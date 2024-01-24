import argparse
import sys

def parse_pgn_file(file_path):
    games = []
    current_game = []

    with open(file_path, 'r') as file:
        for line in file:
            if line.startswith('1.'):
                current_game.append(line.strip())
            elif line.startswith('[') and current_game:
                games.append(' '.join(current_game))
                current_game = []
        if current_game:
            games.append(' '.join(current_game))

    return games

def process_moves(moves):
    foundOpen = True
    processed_moves = []
    for move in moves.split():
        if '.' in move:
            continue
        if "{" in move:
            foundOpen = True
            continue

        if "}" in move:
            foundOpen = False
            continue

        if foundOpen:
            continue

        processed_moves.append(move)
    return processed_moves

def create_lookup_tables(processed_moves):
    lookup_tables = {}
    for depth in range(1, max(len(moves) for moves in processed_moves) + 1):
        table = []
        for moves in processed_moves:
            if len(moves) >= depth:
                history = ''.join(moves[:depth-1])
                best_move = moves[depth-1]
                table.append((history, best_move))
        lookup_tables[depth] = table
    return lookup_tables

def main():
    parser = argparse.ArgumentParser(description="Generate a lookup table from a chess PGN file.")
    parser.add_argument("file_path", help="Path to the PGN file")
    args = parser.parse_args()

    games = parse_pgn_file(args.file_path)

    all_processed_moves = [process_moves(game) for game in games]

    lookup_tables = create_lookup_tables(all_processed_moves)

    sys.stdout = open("lookup.h", "w")
    print("// Copyright (c) 2022 MIT License by 6.172 / 6.106 Staff\n#ifndef LOOKUP_H\n#define LOOKUP_H\n")
    print(f"#define OPEN_BOOK_DEPTH {len(lookup_tables)}\n")
    print("int lookup_sizes[OPEN_BOOK_DEPTH] = {", end = "")
    for depth, table in lookup_tables.items():
        print(len(table),end = ", ")

    print("};\n")

    for depth, table in lookup_tables.items():
        print(f"const char* lookup_table_depth_{depth}[] = {{", end = " ")
        seen = {}
        for history, best_move in table:
            if history not in seen:
                seen[history] = 1
                print(f'"{history}", "{best_move}"', end = ",\n")
            #print(f'"{history}", "{best_move}"', end = ",\n")
        print("};")


    print(f"const char** lookup_tables[OPEN_BOOK_DEPTH] = {{")
    for depth,table in lookup_tables.items():
        print(f"lookup_table_depth_{depth},")
    print("};")

    print("#endif  // LOOKUP_H")

if __name__ == "__main__":
    main() 