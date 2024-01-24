#!/usr/bin/env python3
import subprocess
import multiprocessing as mp
import sys
import random
import threading
processes = []

# class ThreadWithReturnValue(Thread):
#     def __init__(self, group=None, target=None, name=None,
#                  args=(), kwargs={}, Verbose=None):
#         Thread.__init__(self, group, target, name, args, kwargs, Verbose)
#         self._return = None
#     def run(self):
#         if self._Thread__target is not None:
#             self._return = self._Thread__target(*self._Thread__args,
#                                                 **self._Thread__kwargs)
#     def join(self):
#         Thread.join(self)
#         return self._return

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
        output = self.search(depth=10)
        return output[-1].split()[-1]

    def kill(self): # Kill the engine
        self.proc.terminate()

def generate_ob(list_move, eng):
    print("entering thread")
    book2 = []
    book3 = []
    depth3 = []
    for i in  range(len(list_move)):
        history = list_move[i]
        save = history
        eng.make_move(history)
        best_move = eng.choose_best_move()
        book2.append(history)
        book2.append(best_move) 

        save += " " + best_move
        history += best_move
        eng.make_move(best_move)
        next_best = eng.choose_best_move()
        book3.append(history)
        book3.append(next_best)

        depth3.append(save + " " + next_best)
        eng.reset_position()
        print(f'"{i + 1}/{len(list_move)} done"')
    
    print(book2)
    print(book3)
    print(depth3)
    print("done")
    # return [next_depth_book, book3]

def main():
    # sys.stdout = open("lookup.h", "w")
    bin_path = sys.argv[1]
    # print("// Copyright (c) 2022 MIT License by 6.172 / 6.106 Staff\n#ifndef LOOKUP_H\n#define LOOKUP_H\n")
    # print(f"#define OPEN_BOOK_DEPTH {3}\n")
    # print("int lookup_sizes[OPEN_BOOK_DEPTH] = {", end = "")
    # print("ADJUST LATER")
    # print("};\n")
    eng1 = Engine(bin_path)
    eng2 = Engine(bin_path)
    eng3 = Engine(bin_path)
    eng4 = Engine(bin_path)
    eng5 = Engine(bin_path)
    eng6 = Engine(bin_path)

    depth1_1 = ['a0a1', 'a0b1', 'a0b0', 'a0R', 'a1a2']
    depth1_2 = ['a1b2', 'a1R', 'a1U', 'a1L']
    depth1_3 = ['b1a2', 'b1b2', 'b1c2', 'b1c1']
    depth1_4 = ['b1a1', 'b1R', 'b1U', 'b1L']
    depth1_5 = ['d1c2', 'd1d2', 'd1e2', 'd1e1']
    depth1_6 = ['d1c1', 'd1R', 'd1U', 'd1L']
   
   
    # depth1= ['e1d2', 'e1e2', 'e1f2', 'e1f1', 'e1d1', 'e1R', 'e1U', 'e1L', 'g1f2', 'g1g2', 'g1h2', 'g1h1', 'g1f1', 'g1R', 'g1U', 'g1L', 'h0g1', 'h0h1', 'h0g0', 'h0R', 'h1g2', 'h1h2', 'h1R', 'h1U', 'h1L']

    t1 = threading.Thread(target=generate_ob, args = [depth1_1, eng1])
    t1.start()
    t2 = threading.Thread(target=generate_ob, args = [depth1_2, eng2])
    t2.start()
    t3 = threading.Thread(target=generate_ob, args = [depth1_3, eng3])
    t3.start()

    t4 = threading.Thread(target=generate_ob, args = [depth1_4, eng4])
    t4.start()
    t5 = threading.Thread(target=generate_ob, args = [depth1_5, eng5])
    t5.start()
    t6 = threading.Thread(target=generate_ob, args = [depth1_6, eng6])
    t6.start()


    # for i in  range(len(depth1)):
    #     history = depth1[i]
    #     eng.make_move(history)
    #     print("done")
    #     best_move = eng.choose_best_move()
    #     book2.append(history)
    #     book2.append(best_move) 

    #     history += best_move
    #     eng.make_move(best_move)
    #     next_best = eng.choose_best_move()
    #     book3.append(history)
    #     book3.append(next_best)
    #     depth3.append(history+next_best)
    #     eng.reset_position()
    #     print(book2)

    # print(f"const char* lookup_table_depth_{2}[] = {{", end = " ")
    # for i in range(len(depth1)):
    #     history = book2[i*2]
    #     best_move = book2[i*2+1]
    #     print(f'"{history}", "{best_move}"', end = ",\n")
    
    # # for i in range(len(depth1)):
    # #     history = depth1_b[i*2]
    # #     best_move = depth1_b[i*2+1]
    # #     print(f'"{history}", "{best_move}"', end = ",\n")
    # print("};")


    # print(f"const char* lookup_table_depth_{3}[] = {{", end = " ")
    # for i in range(len(depth1)):
    #     history = book3[i*2]
    #     best_move = book3[i*2+1]
    #     print(f'"{history}", "{best_move}"', end = ",\n")
    # # for i in range(len(depth1)):
    # #     history = book2_b[i*2]
    # #     best_move = book2_b[i*2+1]
    # #     print(f'"{history}", "{best_move}"', end = ",\n")
    # print("};")

    # print(f"const char** lookup_tables[OPEN_BOOK_DEPTH] = {{")
    # for i in range(3):
    #     print(f"lookup_table_depth_{i},")
    # print("};")

    # print("#endif  // LOOKUP_H")

if __name__ == "__main__":
    main() 





