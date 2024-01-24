#!/usr/bin/env python3
import subprocess
import multiprocessing as mp
import sys
import random
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
        output = self.search(depth=4)
        return output[-1].split()[-1]

    def kill(self): # Kill the engine
        self.proc.terminate()

def check_heuristics(eng_a,eng_b):
    coeff_a = eng_a.eval()
    coeff_b = eng_b.eval()

    assert len(coeff_a) == len(coeff_b), "The coefficient lists have different sizes,something is very wrong"

    for i in range(len(coeff_a)):
        if coeff_a[i] != coeff_b[i]:
            return False
    
    return True

def worker(arg, q, binpath1, binpath2):
    eng_a = Engine(binpath1)
    eng_b = Engine(binpath2)

    num_games = 50
    max_num_moves = 1000

    for i in range(num_games):
        eng_a.reset_position()
        eng_b.reset_position()
        moves = []
        for j in range(max_num_moves):
            if not check_heuristics(eng_a,eng_b):
                print("Found incompatible heuristics!")
                print(f"Fen string:\n{eng_a.fen()}")
                eng_a.kill()
                eng_b.kill()
                return False
            
            moves.append(eng_a.choose_random_move())
            eng_a.set_position(moves)
            eng_b.set_position(moves)

            status = eng_a.status()
            if "mate" in status or "draw" in status: # Game over, no need to keep evaluating
                break

    # Run 1 game with the best moves
    eng_a.reset_position()
    eng_b.reset_position()
    moves = []
    while True:
        if not check_heuristics(eng_a,eng_b):
            print("Found incompatible heuristics!")
            print(f"Fen string:\n{eng_a.fen()}")
            eng_a.kill()
            eng_b.kill()
            return False
        
        moves.append(eng_a.choose_best_move())
        eng_a.set_position(moves)
        eng_b.set_position(moves)

        status = eng_a.status()
        if "mate" in status or "draw" in status: # Game over, no need to keep evaluating
            break

    eng_a.kill()
    eng_b.kill()
    return True



if len(sys.argv) != 4:
    print("Usage: python3 compare_heuristics.py num_workers correct_binary current_binary")
    print("    num_workers: number of concurrent worker threads running games")
    print("    correct_binary: path to a binary with ~correct~ heuristics, i.e. before changes")
    print("    current_binary: path to the latest binary")
    exit()

num_threads = int(sys.argv[1])
binpath1 = sys.argv[2]
binpath2 = sys.argv[3]

manager = mp.Manager()
q = manager.Queue()
p = mp.Pool(num_threads+1)

jobs = []
for i in range(num_threads):
    job = p.apply_async(worker, (i, q, binpath1, binpath2))
    jobs.append(job)

all_correct = True
for job in jobs:
    all_correct = (all_correct and job.get())
    
if all_correct:
    print("Testing done! Heuristics seem correct!")

q.put('kill')
p.close()
p.join()