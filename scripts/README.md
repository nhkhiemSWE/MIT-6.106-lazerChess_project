OVERVIEW

Different scripts that we can use to automate our process:

compare_heuristics.py

Usage: python3 compare_heuristics.py num_workers correct_binary current_binary
Runs eval many times on the different states for both binaries and compares the values
It runs many games, starting from the first move and making random moves until the game is over (or a lot of moves are made)
It also runs one game with all moves chosen from a depth 5 exploration
If the values at any point are not equal, it reports it
If it runs all games successfully, it gives a confirming message
generate_opening_book.py:

Generate a lookup table from a chess PGN file. It generates a whole lookup.h file
Run by using python3 generate_opening_book.py ../tests/game_file.pgn where game_file.pgn is the pgn you are using to generate the opening book
To use, copy lookup.h into player and make use USE_OB is enabled
generate_tables:

Generates constant tables to use instead of some functions, for example qi_at()
Just copy the output into the relevant .c file
Make sure parameters like ARR_WIDTH are correct