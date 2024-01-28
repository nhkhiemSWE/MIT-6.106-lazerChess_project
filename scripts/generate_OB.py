#!/usr/bin/env python3
import subprocess
import multiprocessing as mp
import sys
import random
import threading
import pprint
processes = []

# Engine class from datagen.py
class Engine:
    def __init__(self, path):
        self.proc = subprocess.Popen(path, stdout=subprocess.PIPE, stdin=subprocess.PIPE, bufsize=1, universal_newlines=True)
        processes.append(self.proc)
        self.commands = []

    def send_command(self, cmd):    # use to implement any UCI command
        self.commands.append(cmd)
        self.proc.stdin.write(cmd)

    # Implemented commands
    def set_option(self, name, value):
        self.send_command(f"setoption name {name} value {value}\n")
    def set_position(self, move_list):
        self.send_command("position startpos moves " + " ".join(move_list) + "\n")
    def reset_position(self):
        self.send_command("position startpos\n")
    def search(self, depth=None, time=None, inc = None):
        if depth: self.send_command(f"go depth {depth}\n")
        else: self.send_command(f'go time {time} inc {inc}\n')
        in_output = False
        output = []
        while True:
            line = self.proc.stdout.readline().strip()
            cols = line.split()
            if cols[0] == "info":
                in_output = True
            if not in_output:
                continue
            output.append(line)
            if cols[0]=="bestmove": break
        return output
    def status(self):
        self.send_command(f"status\n")
        line = self.proc.stdout.readline().strip()
        return line
    def eval(self):
        self.send_command(f"eval\n")
        coefficients = self.proc.stdout.readline().strip()
        weights = []
        while True:
            coefficients = coefficients.split()
            if coefficients[0] == "info":
                break
            if coefficients[0] == "Total":
                weights.append(coefficients[4])
            coefficients = self.proc.stdout.readline().strip()
        return weights
    def generate_moves(self):
        self.send_command(f"generate\n")

        line = self.proc.stdout.readline().strip()
        return line.split()[1:]
    def make_move(self, move):
        self.send_command(f"move {move}\n")
        while True:
            line = self.proc.stdout.readline().strip()
            if "victims" in line:
                return int(line.split()[-1])
    def fen(self):
        self.send_command("fen\n")
        line = self.proc.stdout.readline().strip()
        return line

    def choose_random_move(self):
        possible_moves = self.generate_moves()
        return random.choice(possible_moves)
    def choose_best_move(self):
        output = self.search(depth=11)
        return output[-1].split()[-1]

    def kill(self): # Kill the engin
        self.proc.terminate()

def main():
    sys.stdout = open("lookup.h", "w")
    bin_path = sys.argv[1]
    depth = int(sys.argv[2])
    eng = Engine(bin_path)
    # depth1= eng.generate_moves()
    depth1= ['e1d2'] #, 'e1e2', 'e1f2', 'e1f1', 'e1d1', 'e1R', 'e1U', 'e1L', 'g1f2', 'g1g2', 'g1h2', 'g1h1', 'g1f1', 'g1R', 'g1U', 'g1L', 'h0g1', 'h0h1', 'h0g0', 'h0R', 'h1g2', 'h1h2', 'h1R', 'h1U', 'h1L']
    books = []
    print("// Copyright (c) 2022 MIT License by 6.172 / 6.106 Staff\n#ifndef LOOKUP_H\n#define LOOKUP_H\n")
    print(f"#define OPEN_BOOK_DEPTH {depth}\n")
    print("int lookup_sizes[OPEN_BOOK_DEPTH] = {", end = "")
    for i in range(depth):
        print(len(depth1),end = ", ")
        books.append([])
    print("};\n")

    for i in range(len(depth1)):
        history = depth1[i]
        eng.make_move(history)
        for i in range(1,depth):
            best_move = eng.choose_best_move()
            books[i].append(history)
            books[i].append(best_move)
            history += best_move
            eng.make_move(best_move)
        eng.reset_position()

    for j in range(2, depth + 1):
        print(f"const char* lookup_table_depth_{j}[] = {{", end = " ")
        for i in range(len(depth1)):
            history = books[j-1][i*2]
            best_move = books[j-1][i*2+1]
            print(f'"{history}", "{best_move}"', end = ",\n")
        print("};")

    print(f"const char** lookup_tables[OPEN_BOOK_DEPTH] = {{")
    for i in range(2,depth + 1):
        print(f"lookup_table_depth_{i},")
    print("};")

    print("#endif  // LOOKUP_H")

if __name__ == "__main__":
    main() 





