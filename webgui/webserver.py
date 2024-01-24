#!/usr/bin/python3
'''
 * Copyright (c) 2022 MIT License by 6.172 / 6.106 Staff
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
'''

import sys
import string
import cgi
import time
import os
import subprocess
import socket
import json
import urllib.parse
import argparse
import select
from http.server import BaseHTTPRequestHandler, HTTPServer

DEFAULT_PLAYER = "../player/leiserchess"


class Player:
    def __init__(self, binary):
        self._proc = subprocess.Popen(
            binary, stdin=subprocess.PIPE, stdout=subprocess.PIPE, universal_newlines=True,bufsize=1)
        self._poll = select.poll()
        self._poll.register(self._proc.stdout, select.POLLIN)
        self._proc.stdin.write('uci\n')
        self._proc.stdin.flush()
        print('Waiting for uciok from bot: {}'.format(binary))
        while True:
            line = self._proc.stdout.readline().strip()
            if line == "uciok":
                return

    def play(self, position, moves, time, inc):
        if len(moves) > 0:
            args = \
                'position ' + position + ' ' + \
                'moves ' + moves + '\n' + \
                'go ' + \
                'time ' + time + ' ' + \
                'inc ' + inc + '\n'
        else:
            args = \
                'position ' + position + '\n' + \
                'go ' + \
                'time ' + time + ' ' + \
                'inc ' + inc + '\n'

        print(args)
        self._proc.stdin.write(args)
        self._proc.stdin.flush()

        info = ''

        while True:
            line = self._proc.stdout.readline().strip()
            if not line:
                raise RuntimeError("ERROR: Leiserchess Player Terminated")
            print(line)

            info += line + '\n'

            if line.startswith("bestmove "):
                return line.split(" ")[1], info

    def command(self, cmd):
        self._proc.stdin.write(cmd + '\n')
        self._proc.stdin.flush()

        info = self._proc.stdout.readline().strip()
        timeout = time.time() + 1
        while time.time() < timeout:
            res = self._poll.poll(0)
            if res:
                line = self._proc.stdout.readline().strip()
                info += line + '\n'
                timeout = time.time() + 1
                if not line:
                    return info

        return info

class Webserver:
    def __init__(self, port, player1, player2):
        # Set up player
        self.players = (Player(player1), Player(player2))

        self.info = ''
        self.next_req_id = 0
        self.pending_requests = dict()

        try:
            server = HTTPServer(('', port), self.build_leiserchess_handler())
            print('Starting Leiserchess...')
            #local_ip = socket.gethostbyname(socket.gethostname())
            local_ip = '127.0.0.1'
            print('Visit http://{}:{}'.format(local_ip, port))
            server.serve_forever()
        except KeyboardInterrupt:
            print('keyboard interrupt received, shutting down server')
            server.socket.close()

    def add_pending_request(self, return_move):
        self.pending_requests[self.next_req_id] = return_move
        self.next_req_id += 1

    def pop_pending_request(self, reqid):
        req = self.pending_requests[reqid]
        del self.pending_requests[reqid]
        return req

    def get_player(self, player_str):
        if player_str == "player1":
            player_id = 0
        elif player_str == "player2":
            player_id = 1
        else:
            raise ValueError('invalid player_str: {}'.format(player_str))

        return self.players[player_id]

    def build_leiserchess_handler(self):
        webserver = self

        class LeiserchessHandler(BaseHTTPRequestHandler):
            def do_GET(self):
                return webserver.do_GET(self)

            def do_POST(self):
                return webserver.do_POST(self)

        return LeiserchessHandler

    def do_GET(self, request: BaseHTTPRequestHandler):
        if request.path == "/":
            request.path = "/index.html"

        content_types = {
            ".html": "text/html",
            ".js": "application/javascript",
            ".css": "text/css",
            ".png": "image/png",
            ".svg": "image/svg+xml",
            ".ico": "image/x-icon",
            ".pdf": "application/pdf",
        }

        try:
            for suffix, contentType in content_types.items():
                if request.path.endswith(suffix):
                    path = os.getcwd() + request.path
                    if not in_directory(path, os.getcwd()):
                        raise IOError

                    with open(path, "rb") as f:
                        request.send_response(200)
                        request.send_header('Content-type', contentType)
                        request.end_headers()
                        request.wfile.write(f.read())
                        request.wfile.flush()
                    return

            raise IOError
        except IOError:
            request.send_error(404, 'File Not Found: {}'.format(request.path))

    def do_POST(self, request: BaseHTTPRequestHandler):
        ctype, pdict = cgi.parse_header(request.headers.get('content-type'))
        if ctype == 'multipart/form-data':
            postvars = cgi.parse_multipart(request.rfile, pdict)
        elif ctype == 'application/x-www-form-urlencoded':
            length = int(request.headers.get('content-length'))
            postvars = urllib.parse.parse_qs(request.rfile.read(
                length).decode('utf8'), keep_blank_values=1)
        else:
            postvars = {}

        if request.path.startswith("/move/"):
            print(postvars)

            player_str = postvars['player'][0]
            position = postvars['position'][0]
            moves = postvars['moves'][0]
            gotime = postvars['gotime'][0]
            goinc = postvars['goinc'][0]

            player = self.get_player(player_str)

            return_move, new_info = player.play(position, moves, gotime, goinc)
            self.info = new_info

            request.send_response(200)
            request.send_header('Content-type', "text/json")
            request.end_headers()

            output = {
                "move": return_move,
                "reqid": str(self.next_req_id)
            }
            request.wfile.write(json.dumps(output).encode('utf8'))
            request.wfile.flush()

            self.add_pending_request(return_move)
            return

        if request.path.startswith("/poll/"):
            return_move = self.pop_pending_request(int(postvars['reqid'][0]))
            request.send_response(200)
            request.send_header('Content-type', "text/json")
            request.end_headers()

            output = {
                "move": return_move,
                "info": self.info
            }
            request.wfile.write(json.dumps(output).encode('utf8'))
            request.wfile.flush()
            return

        if request.path.startswith("/uci/"):
            print(postvars)
            cmd = postvars['command'][0]
            player_str = postvars['player'][0]
            player = self.get_player(player_str)
            print("starting")
            output = player.command(cmd)
            print("output ", output)
            request.send_response(200)
            request.send_header('Content-type', "text/json")
            request.end_headers()

            output = {
                "output": output
            }
            request.wfile.write(json.dumps(output).encode('utf8'))
            request.wfile.flush()
            return

        request.send_error(404, 'Invalid request: {}'.format(request))


# from http://stackoverflow.com/questions/3812849/how-to-check-whether-a-directory-is-a-sub-directory-of-another-directory
def in_directory(file, directory):
    # make both absolute
    directory = os.path.realpath(directory)
    file = os.path.realpath(file)

    # return true, if the common prefix of both is equal to directory
    # e.g. /a/b/c/d.rst and directory is /a/b, the common prefix is /a/b
    return os.path.commonprefix([file, directory]) == directory


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default=5555, type=int,
                        help="Port to run server")
    parser.add_argument("--player1", default=DEFAULT_PLAYER,
                        type=str, help="Player 1 binary")
    parser.add_argument("--player2", default=DEFAULT_PLAYER,
                        type=str, help="Player 2 binary")

    args = parser.parse_args()
    Webserver(**vars(args))


if __name__ == '__main__':
    main()
