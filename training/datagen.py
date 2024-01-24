#!/usr/bin/env python3
import subprocess
import multiprocessing as mp
import sys
processes = []
class Engine:
    def __init__(self, path):
        self.proc = subprocess.Popen(path, stdout=subprocess.PIPE, stdin=subprocess.PIPE, bufsize=1, universal_newlines=True)
        processes.append(self.proc)
        self.commands = []
    def send_command(self, cmd):
        self.commands.append(cmd)
        self.proc.stdin.write(cmd)
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
        

    def make_move(self, move):
        self.send_command(f"move {move}\n")
        while True:
            line = self.proc.stdout.readline().strip()
            if "victims" in line:
                return int(line.split()[-1])
            

    def kill(self):
        self.proc.terminate()

import numpy as np
def writer(q):
    with open("games_tuning_output.txt", 'a') as f:
        print("Started writer")
        while True:
            m = q.get()
            if m=="kill":
                break
            f.write(str(m) + "\n")
            f.flush()
def worker(arg, q):
    try:
        eng_a = Engine("../player/leiserchess")
        eng_b = Engine("../player/leiserchess")
        for i in range(900000):
            print("Starting", i)
            eng_a.reset_position()
            eng_b.reset_position()
            engs = [eng_a, eng_b]
            moves = []
            cur_turn = 0
            positions = []
            while True:
                if len(moves):
                    engs[cur_turn%2].set_position(moves)
                output = engs[cur_turn%2].search(time=3000, inc=0.3)
                best_move = output[-1].split()[-1]
                score = int(output[-2].split()[3])
                moves.append(best_move)
                eval_coefficients = list(map(int, engs[cur_turn%2].eval()))
                num_victims = engs[cur_turn%2].make_move(best_move)
                if num_victims == 0 and abs(score)<3000: # quiescent position and not mated
                    positions.append(np.array(eval_coefficients))
                status = engs[cur_turn%2].status()
                if "mate" in status or "draw" in status:
                    if "mate" in status:
                        if cur_turn%2:
                            result = 1
                        else:
                            result = 0
                    else:
                        result = 0.5
                    if len(positions)>0:
                        positions = np.stack(positions)
                        positions = np.hstack((positions, result*np.ones((positions.shape[0], 1))))
                        print(positions, result)
                    break
                cur_turn += 1

            for row in positions:
                q.put(",".join(map(str, list(row))))
        raise ValueError
    except:
        raise AssertionError
        try:
            eng_a.kill()
            eng_b.kill()
        except:
            pass

if len(sys.argv) != 2:
    print("Usage: python3 datagen.py num_workers")
    print("    num_workers: number of concurrent worker threads running games")
    exit()
num_threads = int(sys.argv[1])

manager = mp.Manager()
q = manager.Queue()
p = mp.Pool(num_threads+1)

w = p.apply_async(writer, (q,))
jobs = []
for i in range(num_threads):
    job = p.apply_async(worker, (i, q))
    jobs.append(job)

for job in jobs:
    job.get()
q.put('kill')
p.close()
p.join()
