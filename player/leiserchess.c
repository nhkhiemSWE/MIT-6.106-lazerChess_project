// Copyright (c) 2022 MIT License by 6.172 / 6.106 Staff

#include <ctype.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#if PARALLEL
#include <cilk/cilk.h>
#include <cilk/reducer.h>
#endif

#include "end_game.h"
#include "eval.h"
#include "fen.h"
#include "lookup.h"
#include "move_gen.h"
#include "options.h"
#include "search.h"
#include "tbassert.h"
#include "tt.h"
#include "util.h"

char VERSION[] = "1060";

#define INF_TIME 99999999999.0
#define INF_DEPTH 999  // if user does not specify a depth, use 999

// if the time remain is less than this fraction, dont start the next search
// iteration
#define RATIO_FOR_TIMEOUT 0.5

// -----------------------------------------------------------------------------
// file I/O
// -----------------------------------------------------------------------------

static FILE* OUT;
static FILE* IN;

// -----------------------------------------------------------------------------
// Printing helpers
// -----------------------------------------------------------------------------

void lower_case(char* s) {
  int i;
  int c = strlen(s);

  for (i = 0; i < c; i++) {
    s[i] = tolower(s[i]);
  }

  return;
}

// Returns victims or NO_VICTIMS if no victims or -1 if illegal move
// makes the move described by 'mvstring'
victims_t make_from_string(position_t* old, position_t* p,
                           const char* mvstring) {
  sortable_move_t lst[MAX_NUM_MOVES];
  move_t mv = NULL_MOVE;
  // make copy so that mvstring can be a constant
  char string[MAX_CHARS_IN_MOVE];
  int move_count = generate_all(old, lst);

  snprintf(string, MAX_CHARS_IN_MOVE, "%s", mvstring);
  lower_case(string);

  for (int i = 0; i < move_count; i++) {
    char buf[MAX_CHARS_IN_MOVE];
    move_to_str(get_move(lst[i]), buf, MAX_CHARS_IN_MOVE);
    lower_case(buf);

    if (strcmp(buf, string) == 0) {
      mv = get_move(lst[i]);
      break;
    }
  }
  return (move_eq(mv, NULL_MOVE)) ? ILLEGAL() : actually_make_move(old, p, mv);
}

typedef enum {
  NONWHITESPACE_STARTS,  // next nonwhitespace starts token
  WHITESPACE_ENDS,       // next whitespace ends token
  QUOTE_ENDS             // next double-quote ends token
} parse_state_t;

// -----------------------------------------------------------------------------
// UCI search (top level scout search call)
// -----------------------------------------------------------------------------

static move_t bestMoveSoFar;
static char theMove[MAX_CHARS_IN_MOVE];

static pthread_mutex_t entry_mutex;
static uint64_t node_count_serial;

typedef struct {
  position_t* p;
  int depth;
  double tme;
} entry_point_args;

typedef struct {
  const char* lookup_best_move;
} entry_point_ret;

// Returns the `best_move` if found starting from a
// given position `p`. Otherwise returns NULL
const char* try_lookup_table(position_t* p) {
  const char* best_move = NULL;

  move_t moves[OPEN_BOOK_DEPTH] = {NULL_MOVE};
  char move_list[OPEN_BOOK_DEPTH * 10] = {0};

  // Extract history from `p`
  position_t* current_move = p;
  for (int move_index = p->ply; move_index > 0; move_index--) {
    move_t tail = current_move->last_move;
    moves[move_index - 1] = tail;
    current_move = current_move->history;
  }

  // Convert to human readable index. Ie: "move1move2move3"
  for (int move_index = 0; move_index < p->ply; move_index++) {
    char hr_move[10];
    move_to_str(moves[move_index], hr_move, 10);
    strcat(move_list, hr_move);
  }

  // Search the table for this index
  const char** table = lookup_tables[p->ply];
  int table_size = lookup_sizes[p->ply];
  for (int i = 0; i < table_size; i += 2) {
    if (strcmp(move_list, table[i]) == 0) {
      best_move = table[i + 1];

      // wow, such speed, such depth!
      fprintf(
          OUT,
          "info depth +inf move_no 1 time (microsec) 0 nodes +inf nps +inf\n");

      // This unlock will allow the main thread lock/unlock in
      // UCIBeginSearch to proceed
      pthread_mutex_unlock(&entry_mutex);
      break;
    }
  }

  return best_move;
}

#define NUM_PARALLEL 128
#ifdef PARALLEL
int ncores=0;
#endif

typedef struct {
  uint64_t node_count_serial;
  char padding[64];
} node_count_with_padding;
node_count_with_padding node_count_serial_all[NUM_PARALLEL];

typedef struct {
  move_t killer __KMT_dim__;
  char padding[64];
} killer_with_padding;
killer_with_padding killer_all[NUM_PARALLEL];

typedef struct {
  int best_move_history __BMH_dim__;
  char padding[64];
} best_move_with_padding;
best_move_with_padding best_move_history_all[NUM_PARALLEL];

typedef struct {
  sortable_move_t move_list[MAX_NUM_MOVES];
  char padding[64];
} move_list_with_padding;
move_list_with_padding move_list_all[NUM_PARALLEL];

void entry_point(entry_point_args* args, entry_point_ret* ret) {
  bestMove subpv[MAX_PLY_IN_SEARCH];
  for (int i = 0; i < MAX_PLY_IN_SEARCH; ++i) {
    subpv[i].score = -INF;
    subpv[i].move = NULL_MOVE;
    subpv[i].has_been_set = false;
    subpv[i].mutex = 0;
  }

  int depth = args->depth;
  position_t* p = args->p;
  double tme = args->tme;

  double et = 0.0;

  // start time of search
  init_abort_timer(tme);

  // init_best_move_history();
  tt_age_hashtable();

  init_tics();

  // Try using the move lookup table if our ply is less than the depth of the
  // table
  if (p->ply < OPEN_BOOK_DEPTH && USE_OB) {
    const char* lookup_best_move = try_lookup_table(p);

    // A best move from the lookup table was found. Return immediately!
    if (lookup_best_move) {
      ret->lookup_best_move = lookup_best_move;
      // return;
    }
  }

  for (int i = 0; i< NUM_PARALLEL; i ++) {
    node_count_serial_all[i].node_count_serial = 0;
  }

  // Iterative deepening
  for (int d = 1; d <= depth; d++) {
    reset_abort();

    // Unleash wrath!
    #ifdef PARALLEL
      cilk_for (int i = 0; i< ncores; i ++) {
        searchRoot(p, -INF, INF, d, 0, subpv, &node_count_serial_all[i].node_count_serial, OUT, i, 
                    move_list_all[i].move_list, killer_all[i].killer, best_move_history_all[i].best_move_history);
      }
    #else
      for (int i = 0; i< 1; i ++) {
        searchRoot(p, -INF, INF, d, 0, subpv, &node_count_serial_all[i].node_count_serial, OUT, i, 
                    move_list_all[i].move_list, killer_all[i].killer, best_move_history_all[i].best_move_history);
      }
    #endif

    // searchRoot(p, -INF, INF, d, 0, subpv, &node_count_serial, OUT);

    et = elapsed_time();
    for (int i = 0; i < MAX_PLY_IN_SEARCH; i ++) {
      if (subpv[i].has_been_set) {
        bestMoveSoFar = subpv[i].move;
      }
    }

    if (!should_abort()) {
      // print something?
    } else {
      break;
    }

    // don't start iteration that you cannot complete
    if (et > tme * RATIO_FOR_TIMEOUT) {
      break;
    }
  }

  // This unlock will allow the main thread lock/unlock in UCIBeginSearch to
  // proceed
  pthread_mutex_unlock(&entry_mutex);

  return;
}

// Makes call to entry_point -> make call to searchRoot -> searchRoot in
// search.c
void UciBeginSearch(position_t* p, int depth, double tme) {
  // Setup for the barrier
  pthread_mutex_lock(&entry_mutex);

  entry_point_args args;
  args.depth = depth;
  args.p = p;
  args.tme = tme;
  node_count_serial = 0;

  entry_point_ret ret;

  // If non-NULL, means that lookup table was used to determine best move
  ret.lookup_best_move = NULL;

  entry_point(&args, &ret);

  // Check if `entry_point` found a best move using the lookup table
  if (ret.lookup_best_move) {
    fprintf(OUT, "bestmove %s\n", ret.lookup_best_move);
  } else {
    char bms[MAX_CHARS_IN_MOVE];
    move_to_str(bestMoveSoFar, bms, MAX_CHARS_IN_MOVE);
    snprintf(theMove, MAX_CHARS_IN_MOVE, "%s", bms);
    fprintf(OUT, "bestmove %s\n", bms);
  }
  return;
}

void UciBeginSearchMove(position_t* p, int depth, double tme, char* bms) {
    // Setup for the barrier
  pthread_mutex_lock(&entry_mutex);

  entry_point_args args;
  args.depth = depth;
  args.p = p;
  args.tme = tme;
  node_count_serial = 0;

  entry_point_ret ret;

  // If non-NULL, means that lookup table was used to determine best move
  ret.lookup_best_move = NULL;

  entry_point(&args, &ret);
  // Check if `entry_point` found a best move using the lookup table
  if (ret.lookup_best_move) {
    fprintf(OUT, "bestmove %s\n", ret.lookup_best_move);
    strcpy(bms,ret.lookup_best_move);
  } else {
    // char bms[MAX_CHARS_IN_MOVE];
    move_to_str(bestMoveSoFar, bms, MAX_CHARS_IN_MOVE);
    snprintf(theMove, MAX_CHARS_IN_MOVE, "%s", bms);
    fprintf(OUT, "bestmove %s\n", bms);
  }
  fprintf(OUT, "Done Searching! Move %s \n", bms);
  fprintf(OUT, "\n");
  return;
}

// -----------------------------------------------------------------------------
// argparse help
// -----------------------------------------------------------------------------

// print help messages in uci
void help() {
  fprintf(OUT, "info eval      - Evaluate current position.\n");
  fprintf(OUT, "info display   - Display current board state.\n");
  fprintf(OUT, "info generate  - Generate all possible moves.\n");
  //ADDED features
  fprintf(OUT, "info fen       - print out the FEN string representation of the current game position.\n");
  fprintf(OUT, "info next <depth>\n");
  fprintf(OUT, "info           - Make the best move for the current player which suggested after searching at <depth>.\n");
  fprintf(OUT, "info             Sample usage: \n");
  fprintf(OUT, "info                 next 3: search at depth 3 to find the best move, and make the move\n");
  fprintf(OUT, "info undo      - Undo the previous move.\n");
  //end ADDED features
  fprintf(OUT,
          "info status    - Display game status as of last move. Possible "
          "values are:\n");
  fprintf(
      OUT,
      "info               mate - white wins, mate - black wins, draw, ok\n");
  fprintf(
      OUT,
      "info go        - Search from current state. Possible arguments are:\n");
  fprintf(OUT,
          "info               depth <depth>:     search until depth <depth>\n");
  fprintf(OUT,
          "info "
          "              time <time_limit>: search assume you have <time>"
          " amount of time\n");
  fprintf(OUT, "info                                  for the whole game.\n");
  fprintf(OUT,
          "info "
          "              inc <time_inc>:    set the Fischer time increment"
          " for the search\n");
  fprintf(
      OUT,
      "info             Both time arguments are specified in milliseconds.\n");
  fprintf(OUT, "info             Sample usage: \n");
  fprintf(OUT, "info                 go depth 4: search until depth 4\n");
  fprintf(OUT, "info help      - Display help (this info).\n");
  fprintf(OUT,
          "info "
          "isready   - Ask if the UCI engine is ready, if so it echoes"
          " \"readyok\".\n");
  fprintf(OUT,
          "info "
          "            This is mainly used to synchronize the engine with the"
          " GUI.\n");
  fprintf(OUT, "info move      - Make a move for current player.\n");
  fprintf(OUT, "info             Sample usage: \n");
  fprintf(OUT, "info                 move j0j1: move a piece from j0 to j1\n");
  fprintf(OUT,
          "info "
          "perft     - Output the number of possible moves up to a given"
          " depth.\n");
  fprintf(OUT, "info             Used to verify move the generator.\n");
  fprintf(OUT, "info             Sample usage: \n");
  fprintf(OUT,
          "info "
          "                depth 3: generate all possible moves for"
          " depth 1--3\n");
  fprintf(OUT,
          "info "
          "position  - Set up the board using the fenstring given.  Possible"
          " arguments are:\n");
  fprintf(OUT,
          "info "
          "              startpos:     set up the board with default starting"
          " position.\n");
  fprintf(OUT,
          "info "
          "              endgame:      set up the board with endgame"
          " configuration.\n");
  fprintf(OUT,
          "info "
          "              fen <string>: set up the board using the given"
          " fenstring <string>.\n");
  fprintf(OUT,
          "info "
          "                            See doc/engine-interface.txt for more"
          " info \n");
  fprintf(OUT, "info                             on fen notation.\n");
  fprintf(OUT, "info             Sample usage: \n");
  fprintf(OUT,
          "info "
          "                position endgame: set up the board so that only"
          " monarchs remain\n");
  fprintf(OUT, "info quit      - Quit this program\n");
  fprintf(OUT,
          "info "
          "setoption - Set configuration options used in the engine,"
          " the format is: \n");
  fprintf(OUT, "info             setoption name <name> value <val>.\n");
  fprintf(OUT,
          "info "
          "            Use the comment \"uci\" to see possible options and"
          " their current values\n");
  fprintf(OUT, "info             Sample usage: \n");
  fprintf(OUT,
          "info "
          "                setoption name fut_depth value 4: set"
          " fut_depth to 4\n");
  fprintf(OUT, "info uci       - Display UCI version and options\n");
  fprintf(OUT, "\n");
}

// Get next token in s[] and put into token[]. Strips quotes.
// Side effects modify ps[].
int parse_string_q(char* s, char* token[]) {
  int token_count = 0;
  parse_state_t state = NONWHITESPACE_STARTS;

  while (*s != '\0') {
    switch (state) {
      case NONWHITESPACE_STARTS:
        switch (*s) {
          case ' ':
          case '\t':
          case '\n':
          case '\r':
          case '#':
            *s = '\0';
            break;
          case '"':
            state = QUOTE_ENDS;
            *s = '\0';
            if (*(s + 1) == '\0') {
              fprintf(stderr, "Input parse error: no end of quoted string\n");
              return 0;  // Parse error
            }
            token[token_count++] = s + 1;
            break;
          default:  // nonwhitespace, nonquote
            state = WHITESPACE_ENDS;
            token[token_count++] = s;
        }
        break;

      case WHITESPACE_ENDS:
        switch (*s) {
          case ' ':
          case '\t':
          case '\n':
          case '\r':
          case '#':
            state = NONWHITESPACE_STARTS;
            *s = '\0';
            break;
          case '"':
            fprintf(stderr, "Input parse error: misplaced quote\n");
            return 0;  // Parse error
            break;
          default:  // nonwhitespace, nonquote
            break;
        }
        break;

      case QUOTE_ENDS:
        switch (*s) {
          case ' ':
          case '\t':
          case '\n':
          case '\r':
          case '#':
            break;
          case '"':
            state = NONWHITESPACE_STARTS;
            *s = '\0';
            if (*(s + 1) != '\0' && *(s + 1) != ' ' && *(s + 1) != '\t' &&
                *(s + 1) != '\n' && *(s + 1) != '\r') {
              fprintf(stderr,
                      "Input parse error: quoted string must be followed "
                      "by white space\n");
              fprintf(stderr, "ASCII char: %d\n", (int)*(s + 1));
              return 0;  // Parse error
            }
            break;
          default:  // nonwhitespace, nonquote
            break;
        }
        break;
    }
    s++;
  }
  if (state == QUOTE_ENDS) {
    fprintf(stderr, "Input parse error: no end quote on quoted string\n");
    return 0;  // Parse error
  }

  return token_count;
}

void init_options() {
  for (int j = 0; iopts[j].name[0] != 0; j++) {
    tbassert(iopts[j].min <= iopts[j].dfault, "min: %d, dfault: %d\n",
             iopts[j].min, iopts[j].dfault);
    tbassert(iopts[j].max >= iopts[j].dfault, "max: %d, dfault: %d\n",
             iopts[j].max, iopts[j].dfault);
    *iopts[j].var = iopts[j].dfault;
  }
}

void print_options() {
  for (int j = 0; iopts[j].name[0] != 0; j++) {
    fprintf(OUT, "option name %s type spin value %d default %d min %d max %d\n",
            iopts[j].name, *iopts[j].var, iopts[j].dfault, iopts[j].min,
            iopts[j].max);
  }
  return;
}

// -----------------------------------------------------------------------------
// main - implements the UCI protocol. The command line interface is
// described in doc/engine-interface.txt
// -----------------------------------------------------------------------------
//
// Note: Prefix any printed lines with the token "info" if the line
// is intended for a human operator should be ignored by an automatic
// agent using UCI to communicate with this bot.

int main(int argc, char* argv[]) {
  #ifdef PARALLEL
    ncores = __cilkrts_get_nworkers();
    if (ncores > NUM_PARALLEL) {
      ncores = NUM_PARALLEL;
    }
  #endif

  position_t* gme = (position_t*)malloc(sizeof(position_t) * MAX_PLY_IN_GAME);

  setbuf(stdout, NULL);
  setbuf(stdin, NULL);

  OUT = stdout;

  if (argc > 1) {
    const char* fname = argv[1];
    IN = fopen(fname, "r");
    if (IN == NULL) {
      fprintf(OUT, "Could not open file: %s\n", fname);
      return -1;
    }
  } else {
    IN = stdin;
  }

  init_options();
  init_zob();

  char** tok =
      (char**)malloc(sizeof(char*) * MAX_CHARS_IN_TOKEN * MAX_PLY_IN_GAME);
  int ix = 0;  // index of which position we are operating on

  // input string - last message from UCI interface
  // big enough to support 4000 moves
  char* istr = (char*)malloc(sizeof(char) * 24000);

  tt_make_hashtable(HASH);             // initial hash table
  fen_to_pos(&gme[ix], STARTPOS_FEN);  // initialize with the starting position

  //  Check to make sure we don't loop infinitely if we don't get input.
  bool saw_input = false;
  double start_time = milliseconds();

  while (true) {
    int n;

    if (fgets(istr, 20478, IN) != NULL) {
      int token_count = parse_string_q(istr, tok);

      if (token_count == 0) {  // no input
        if (!saw_input && milliseconds() - start_time > 1000 * 5) {
          fprintf(OUT,
                  "info Received no commands after 5 seconds, terminating "
                  "program\n");
          break;
        }
        continue;
      } else {
        saw_input = true;
      }

      if (strcmp(tok[0], "quit") == 0) {
        break;
      }

      if (strcmp(tok[0], "undo") == 0) {
        if (ix == 0) {
          // display(&gme[ix]);
        } else {
          ix --;
          // display(&gme[ix]);
        }
        continue;
      }

      if (strcmp(tok[0], "bitboard") == 0) {
        bitboardDisplay(&gme[ix]);
        continue;
      }

      // if (strcmp(tok[0], "lasermap") == 0) {
      //   laserMapDisplay(&gme[ix]);
      //   continue;
      // }

      if (strcmp(tok[0], "next") == 0) {
        if (token_count > 2) {
          fprintf(OUT, "info Only reqquires two arguments.   Use 'help' to see valid command.\n");
        } else {
          int depth = strtol(tok[1], (char**)NULL, 10);;
          double goal = INF_TIME;
          double tme = 0.0;
          double inc = 0.0;
          char bms[MAX_CHARS_IN_MOVE];
          if (depth < INF_DEPTH) {
            UciBeginSearchMove(&gme[ix], depth, INF_TIME, bms);
          } else {
            goal = tme * 0.02;   // use about 1/50 of main time
            goal += inc * 0.80;  // use most of increment
            // sanity check,  make sure that we don't run ourselves too low
            if (goal > tme / 10.0) {
              goal = tme / 10.0;
            }
              UciBeginSearchMove(&gme[ix], INF_DEPTH, goal, bms);
          }
          victims_t victims = make_from_string(&gme[ix], &gme[ix + 1], bms);
          if (is_ILLEGAL(victims)) {
            fprintf(OUT, "info Illegal move %s\n", bms);
            fprintf(OUT,
                    "move victims -1\n");  // DO NOT CHANGE THIS LINE!!! needs to
                                          // be this format for the autotester
          } else {
            ix++;
            display(&gme[ix]);
            fprintf(OUT, "move victims %d\n",
                    victims.count);  // DO NOT CHANGE THIS LINE!!! needs to
                                    // be this format for the autotester
          }
        }
        continue;
      }

      if (strcmp(tok[0], "position") == 0) {
        n = 0;
        if (token_count < 2) {  // no input
          fprintf(OUT,
                  "info Second argument required.  Use 'help' to see valid "
                  "commands.\n");
          continue;
        }

        if (strcmp(tok[1], "startpos") == 0) {
          ix = 0;
          fen_to_pos(&gme[ix], STARTPOS_FEN);
          n = 2;
        } else if (strcmp(tok[1], "endgame") == 0) {
          ix = 0;
          fen_to_pos(&gme[ix], ENDGAME_FEN);
          n = 2;
        } else if (strcmp(tok[1], "fen") == 0) {
          if (token_count < 3) {  // no input
            fprintf(OUT, "info Third argument (the fen string) required.\n");
            continue;
          }
          ix = 0;
          n = 3;
          char fen_tok[MAX_FEN_CHARS];
          strncpy(fen_tok, tok[2], MAX_FEN_CHARS);
          if (token_count >= 4 &&
              (strncmp(tok[3], "B", 1) == 0 || strncmp(tok[3], "W", 1) == 0)) {
            n++;
            strncat(fen_tok, " ", MAX_FEN_CHARS - strlen(fen_tok) - 1);
            strncat(fen_tok, tok[3], MAX_FEN_CHARS - strlen(fen_tok) - 1);
          }
          fen_to_pos(&gme[ix], fen_tok);
        }

        int save_ix = ix;
        if (token_count > n + 1) {
          for (int j = n + 1; j < token_count; j++) {
            victims_t victims =
                make_from_string(&gme[ix], &gme[ix + 1], tok[j]);
            if (is_ILLEGAL(victims)) {
              fprintf(OUT, "info string Move %s is illegal\n", tok[j]);
              ix = save_ix;
              // breaks multiple loops.
              goto next_command;
            } else {
              ix++;
            }
          }
        }

      next_command:
        continue;
      }

      if (strcmp(tok[0], "reset") == 0) {
        fprintf(OUT, "*****************************************************\n");
        continue;
      }

      if (strcmp(tok[0], "key") == 0){
        fprintf(OUT, "position->key: %" PRIu64 ", computed-key: %" PRIu64 "\n", (&gme[ix])->key,
           compute_zob_key(&gme[ix]));
        continue;
      }

      if (strcmp(tok[0], "fen") == 0) {
        char out_fen[MAX_FEN_CHARS];
        pos_to_fen(&gme[ix], out_fen);
        fprintf(OUT, "%s \n", out_fen);
        continue;
      }
      if (strcmp(tok[0], "moves") == 0) {
        if (token_count < 2) {  // no input
          fprintf(OUT, "info Second argument (move position) required.\n");
          continue;
        }
        victims_t victims;
        for (int i = 1; i <= token_count; i ++) {
          victims = make_from_string(&gme[ix], &gme[ix + 1], tok[i]);
          if (is_ILLEGAL(victims)) {
          fprintf(OUT, "info Illegal move %s\n", tok[1]);
          fprintf(OUT,
                  "move victims -1\n");  // DO NOT CHANGE THIS LINE!!! needs to
                                         // be this format for the autotester
        } else {
          ix++;
        }
        if (player_wins(&gme[ix], BLACK))
          fprintf(OUT, "status mate - black wins\n");
        else if (player_wins(&gme[ix], WHITE))
          fprintf(OUT, "status mate - white wins\n");
        else if (is_draw(&gme[ix]))
          fprintf(OUT, "status draw\n");
        }
        display(&gme[ix]);
        fprintf(OUT, "move victims %d\n",
        victims.count);  // DO NOT CHANGE THIS LINE!!! needs to
                          // be this format for the autotester
        continue;
      }

      if (strcmp(tok[0], "move") == 0) {
        victims_t victims = make_from_string(&gme[ix], &gme[ix + 1], tok[1]);
        if (token_count < 2) {  // no input
          fprintf(OUT, "info Second argument (move position) required.\n");
          continue;
        }
        if (is_ILLEGAL(victims)) {
          fprintf(OUT, "info Illegal move %s\n", tok[1]);
          fprintf(OUT,
                  "move victims -1\n");  // DO NOT CHANGE THIS LINE!!! needs to
                                         // be this format for the autotester
        } else {
          ix++;
          display(&gme[ix]);
          color_t c = color_to_move_of(&gme[ix - 1]);
          printf("color of move: %d and removed color %d (WHITE) vs %d (BLACK) \n", c, victims.removed_color[WHITE], victims.removed_color[BLACK]);
          printf("is this move blunder? %d \n", victims.removed_color[c] && !victims.removed_color[opp_color(c)]);
          fprintf(OUT, "move victims %d\n",
                  victims.count);  // DO NOT CHANGE THIS LINE!!! needs to
                                   // be this format for the autotester
        }
        if (player_wins(&gme[ix], BLACK))
          fprintf(OUT, "status mate - black wins\n");
        else if (player_wins(&gme[ix], WHITE))
          fprintf(OUT, "status mate - white wins\n");
        else if (is_draw(&gme[ix]))
          fprintf(OUT, "status draw\n");
        continue;
      }

      if (strcmp(tok[0], "uci") == 0) {
        // TODO(you): Change the name & version once you start modifying the
        // code!
        fprintf(OUT, "id name %s version %s\n", "Leiserchess", VERSION);
        fprintf(OUT, "id author %s\n",
                "Don Dailey, Charles E. Leiserson, and the staff of MIT 6.106");
        print_options();
        printf("uciok\n");
        continue;
      }

      if (strcmp(tok[0], "status") == 0) {
        if (player_wins(&gme[ix], BLACK))
          fprintf(OUT, "status mate - black wins\n");
        else if (player_wins(&gme[ix], WHITE))
          fprintf(OUT, "status mate - white wins\n");
        else if (is_draw(&gme[ix]))
          fprintf(OUT, "status draw\n");
        else
          fprintf(OUT, "status ok\n");
        continue;
      }

      if (strcmp(tok[0], "isready") == 0) {
        fprintf(OUT, "readyok\n");
        continue;
      }

      if (strcmp(tok[0], "setoption") == 0) {
        int sostate = 0;
        char name[MAX_CHARS_IN_TOKEN];
        char value[MAX_CHARS_IN_TOKEN];

        strncpy(name, "", MAX_CHARS_IN_TOKEN);
        strncpy(value, "", MAX_CHARS_IN_TOKEN);

        for (int i = 1; i < token_count; i++) {
          if (strcmp(tok[i], "name") == 0) {
            sostate = 1;
            continue;
          }
          if (strcmp(tok[i], "value") == 0) {
            sostate = 2;
            continue;
          }
          if (sostate == 1) {
            // we subtract 1 from the length to account for the
            // additional terminating '\0' that strncat appends
            strncat(name, " ", MAX_CHARS_IN_TOKEN - strlen(name) - 1);
            strncat(name, tok[i], MAX_CHARS_IN_TOKEN - strlen(name) - 1);
            continue;
          }

          if (sostate == 2) {
            strncat(value, " ", MAX_CHARS_IN_TOKEN - strlen(value) - 1);
            strncat(value, tok[i], MAX_CHARS_IN_TOKEN - strlen(value) - 1);
            if (i + 1 < token_count) {
              strncat(value, " ", MAX_CHARS_IN_TOKEN - strlen(value) - 1);
              strncat(value, tok[i + 1],
                      MAX_CHARS_IN_TOKEN - strlen(value) - 1);
              i++;
            }
            continue;
          }
        }

        lower_case(name);
        lower_case(value);

        // see if option is in the configurable integer parameters
        {
          bool recognized = false;
          for (int j = 0; iopts[j].name[0] != 0; j++) {
            char loc[MAX_CHARS_IN_TOKEN];

            snprintf(loc, MAX_CHARS_IN_TOKEN, "%s", iopts[j].name);
            lower_case(loc);
            if (strcmp(name + 1, loc) == 0) {
              recognized = true;
              int v = strtol(value + 1, (char**)NULL, 10);
              if (v < iopts[j].min) {
                v = iopts[j].min;
              }
              if (v > iopts[j].max) {
                v = iopts[j].max;
              }
              fprintf(OUT, "info setting %s to %d\n", iopts[j].name, v);
              *(iopts[j].var) = v;

              if (strcmp(name + 1, "hash") == 0) {
                tt_resize_hashtable(HASH);
                fprintf(OUT,
                        "info string Hash table set to %d records of "
                        "%zu bytes each\n",
                        tt_get_num_of_records(), tt_get_bytes_per_record());
                fprintf(OUT, "info string Total hash table size: %zu bytes\n",
                        tt_get_num_of_records() * tt_get_bytes_per_record());
              }
              if (strcmp(name + 1, "reset_rng") == 0) {
                fprintf(OUT, "info string reset the rng\n");
                // if setting the random seed we need to reinit the zob
                init_zob();
              }
              break;
            }
          }
          if (!recognized) {
            fprintf(OUT, "info string %s not recognized\n", name + 1);
          }
          continue;
        }
      }

      if (strcmp(tok[0], "help") == 0) {
        help();
        continue;
      }

      if (strcmp(tok[0], "monarch") == 0) {
        if (token_count != 2) {
          fprintf(OUT, "info Only reqquires two arguments.   Use 'help' to see valid command.\n");
          continue;
        } 
        if (strcmp(tok[1], "white") == 0) {
          square_t mloc0 = (&gme[ix])->monarch_loc[WHITE][0];
          square_t mloc1 = (&gme[ix])->monarch_loc[WHITE][1];
          fprintf(OUT, "%d - %d and %d - %d \n", fil_of(mloc0), rnk_of(mloc0), fil_of(mloc1), rnk_of(mloc1));
        } 
        else if (strcmp(tok[1], "black") == 0) {
          square_t mloc0 = (&gme[ix])->monarch_loc[BLACK][0];
          square_t mloc1 = (&gme[ix])->monarch_loc[BLACK][1];
          fprintf(OUT, "%d - %d and %d - %d \n", fil_of(mloc0), rnk_of(mloc0), fil_of(mloc1), rnk_of(mloc1));
        } else {
          fprintf(OUT, "Wrong syntax! Use 'help' to see valid commands. \n");
        }
        continue;
      }

      if (strcmp(tok[0], "display") == 0) {
        display(&gme[ix]);
        continue;
      }

      sortable_move_t lst[MAX_NUM_MOVES];
      if (strcmp(tok[0], "generate") == 0) {
        int num_moves = generate_all(&gme[ix], lst);
        for (int i = 0; i < num_moves; ++i) {
          if (i == 0) {
            fprintf(OUT, "info ");
          }
          char buf[MAX_CHARS_IN_MOVE];
          move_to_str(get_move(lst[i]), buf, MAX_CHARS_IN_MOVE);
          fprintf(OUT, "%s ", buf);
        }
        fprintf(OUT, "\n");
        continue;
      }

      if (strcmp(tok[0], "eval") == 0) {
        if (token_count == 1) {  // evaluate current position
          score_t score = eval(&gme[ix], true);
          fprintf(OUT, "info score cp %d\n", score);
        } else {  // get and evaluate move
          victims_t victims = make_from_string(&gme[ix], &gme[ix + 1], tok[1]);
          if (is_ILLEGAL(victims)) {
            fprintf(OUT, "info Illegal move\n");
          } else {
            // evaluated from opponent's pov
            score_t score = -eval(&gme[ix + 1], true);
            fprintf(OUT, "info score cp %d\n", score);
          }
        }
        continue;
      }

      if (strcmp(tok[0], "go") == 0) {
        double tme = 0.0;
        double inc = 0.0;
        int depth = INF_DEPTH;
        double goal = INF_TIME;

        // process various tokens here
        for (int n = 1; n < token_count; n++) {
          if (strcmp(tok[n], "depth") == 0) {
            n++;
            depth = strtol(tok[n], (char**)NULL, 10);
            continue;
          }
          if (strcmp(tok[n], "time") == 0) {
            n++;
            tme = strtod(tok[n], (char**)NULL);
            continue;
          }
          if (strcmp(tok[n], "inc") == 0) {
            n++;
            inc = strtod(tok[n], (char**)NULL);
            continue;
          }
        }

        if (depth < INF_DEPTH) {
          UciBeginSearch(&gme[ix], depth, INF_TIME);
        } else {
          goal = tme * 0.02;   // use about 1/50 of main time
          goal += inc * 0.80;  // use most of increment
          // sanity check,  make sure that we don't run ourselves too low
          if (goal > tme / 10.0) {
            goal = tme / 10.0;
          }
          UciBeginSearch(&gme[ix], INF_DEPTH, goal);
        }
        continue;
      }

      if (strcmp(tok[0], "perft") == 0) {  // Sanity check for move generator
                                           // 0 1
                                           // 1 66
                                           // 2 4226
                                           // 3 267674
                                           // 4 17024694
                                           // 5 1071907988
        int depth = 5;
        if (token_count >= 2) {  // Takes a depth argument to test deeper
          depth = strtol(tok[1], (char**)NULL, 10);
        }
        do_perft(&gme[ix], depth);
        continue;
      }

      if (strcmp(tok[0], "test") == 0) {
        test_ptouch(&gme[ix]);
        continue;
      }

      fprintf(OUT,
              "info Illegal command.  Use 'help' to see possible options.\n");
      continue;
    }
  }

  tt_free_hashtable();
  return 0;
}
