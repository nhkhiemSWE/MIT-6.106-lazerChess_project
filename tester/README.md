QUICK START GUIDE 
--------------------------------------------------------------------------------
Only programs which support the UCI protocol can be used. (See section "Gotchas"
at end for typical problems.)

1. Run `make` to compile all dependencies

2. Write a configuration file called `mytest.txt`. An example can be found in
   `tests/basic.txt`. The configuration file MUST end with '.txt' extension.

3. Run the autotester with

        $ make test TEST_FILE=../tests/mytest.txt

    (where `../tests/mytest.txt` is the name of your test configuration file)

4. Game results will be stored in `../tests/mytest.pgn`.
   (note: Results will be found in the same directory in which you store the
   configuration file, not where you invoked `lauto.jar`.)
   
   In your terminal, you will see the results of calling `./pgn/pgnrate.tcl` with the
   scores for your bots.

CONFIGURATION FILE SYNTAX
--------------------------------------------------------------------------------
Here is a minimal configuration file example:

    ---------[ minimal_config.txt ]---------

    cpus = 12
    title = My funky test
    game_rounds = 50
    adjudicate = 200
 
    # now we have the player definitions
    # --
 
    player = depth_1
    invoke = ../player/myLeiserchess
    depth = 1
 
    player = depth_2
    invoke = ../player/myLeiserchess
    depth = 2
 
    player = depth_3
    invoke = ../player/myLeiserchess
    depth = 3

    ----------------[ snip ]----------------


Note that each player section must begin with:

    player = <player name>
    invoke = <player binary path>

all options beyond that can be in arbitrary order.
The header must have at least:

    cpus = <number of games to play concurrently>

The title is optional and is placed in the PGN files as the PGN event tag. The
book specifies the opening book moves; currently we only provide `book.dta`.

The number of game rounds to play (i.e., `game_rounds`) is optional. If left
out, 1000 total games are played by default.

`adjudicate = N` is optional and defaults to 400. It defines the number of moves
to be played before declaring a game as drawn. A move is 2 ply or 1 move by each
side.

The tester supports several types of level of play and any option defined by a
given program via the UCI protocol (for leiserchess, use 'uci' command to see
supported options). A program could expose hundreds of user configurable options
this way and they can all be tested.

Example:

    player = myLeiserchess-test
    invoke = ../player/leiserchess
    hash = 64
    lmr_r1 = 5



COMMON OPTIONS
--------------------------------------------------------------------------------

- `depth = N`
   specify a fixed depth for all moves

- `fis = main inc`
  - `main`: main time in seconds
  - `inc`: fischer increment in seconds. can be integer or decimal



FAMILY OPTION
--------------------------------------------------------------------------------
The tester also flexibly supports a mode where you can prevent some programs
from playing others. This can be used when testing several version of your
program against foreign programs for instance where you do not want different
versions of your own program to play each other. This is specified by the
`family` option:

    family = myprogram

All programs belong to a family, but if it is not specified it becomes the same
as the player name. The rule is that no program plays another program in the
same "family."

Example:

    -----------[ family example ]-----------

    cpus = 4
    title = foreign program test

    # now we have the player definitions
    # --

    player = Master 
    invoke = leiserchessPro 
    fis = 6 0.1

    player = Joes_program
    invoke = joe
    fis = 6 0.1

    player = Fred
    invoke = fred_khet
    fis = 6 0.1

    player = myPlayer1.0 
    invoke = leiserchess.1.0
    fis = 6 0.1
    family = myLeiserchess 

    player = myPlayer1.1
    invoke = leiserchess.1.1
    fis = 6 0.1
    family = myLeiserchess 

    player = myPlayer1.2
    invoke = leiserchess.1.2
    fis = 6 0.1
    family = myLeiserchess

    ----------------[ snip ]----------------



GOTCHAS
--------------------------------------------------------------------------------
Some typical problems that could stall you:

1. Player names must not include spaces. It may work if quoted but this has not
   been tested. This may also be true of the "invoke" line.

2. The equal ('=') character must not appear twice in any line. Do not create
   program options in your program that use the '=' character. (Note to course
   staff: needs to be tested and fixed)

3. A bug in earlier versions of the autotester does not let you properly test
   with cpus = 1. This should be fixed, but if you're still experiencing issues,
   make a post on Piazza.

4. Option names are case sensitive, even though the UCI protocol does not
   require that. The tester does. So if your program defines an option the
   configuration must match case exactly.

   The rationale for this "bug" is that when the tester was originally designed
   for chess some chess programs did not honor the case insensitive rule and
   thus there is no harm to specify options the way the user program specifies
   them.

5. Due to a bug in the tester, if your program publishes an option with spaces
   in it, there must never be more than one consecutive space.
  
6. UCI protocol specifies that an option name or value cannot contain the words,
   "name" or "value" - this is a wart in the original UCI protocol that UCI is
   based on. The protocol uses the tokens "name" and "value" as delimiters when
   parsing so you cannot use them.
    
   So do not publish an option name such as: "anibus value"
