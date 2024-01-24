/**
* Copyright (c) 2013--2022 MIT License by 6.172 / 6.106 Staff
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
**/
'use strict';

// Board configuration
const STATICS_HOST = '';
const NUM_COLS = 8;
const NUM_ROWS = 8;


// Must correspond to the strings at the top of player/fen.c
const SPECIAL_BOARDS = {
    'startpos': 'nn6nn/sesw1sesw1sesw/8/8/8/8/NENW1NENW1NENW/SS6SS W',
    'endgame': 'ss7/8/8/8/8/8/8/7NN W'
};


// GUI configuration
const PIECE_SIZE = 500 / NUM_COLS;
const PIECE_MARGIN = 1;
const ANIMATE_DURATION = 800;
const BOARD_WIDTH = NUM_COLS * PIECE_SIZE;
const BOARD_HEIGHT = NUM_ROWS * PIECE_SIZE;

// Constants
const COL_NAMES = ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j'];
const DIRECTIONS = [[-1, 1], [0, 1], [1, 1], [-1, 0], [1, 0], [-1, -1], [0, -1], [1, -1]];
const KING_MAP = { 'nn': 'u', 'ee': 'r', 'ss': 'd', 'ww': 'l' };
const PAWN_MAP = { 'nw': 'u', 'ne': 'r', 'se': 'd', 'sw': 'l' };
const PLAYER_MAP = { 'B': 'black', 'W': 'white' };
const KING_INV_MAP = { 'u': 'nn', 'r': 'ee', 'd': 'ss', 'l': 'ww' };
const PAWN_INV_MAP = { 'u': 'nw', 'r': 'ne', 'd': 'se', 'l': 'sw' };
const PLAYER_INV_MAP = { 'black': 'B', 'white': 'W' };
const KING_LASER_MAP = {
    'u': { 'u': 'x', 'r': 'x', 'd': 'x', 'l': 'x' },
    'r': { 'u': 'x', 'r': 'x', 'd': 'x', 'l': 'x' },
    'd': { 'u': 'x', 'r': 'x', 'd': 'x', 'l': 'x' },
    'l': { 'u': 'x', 'r': 'x', 'd': 'x', 'l': 'x' },
};
const PAWN_LASER_MAP = {
    'u': { 'u': 'x', 'r': 'u', 'd': 'l', 'l': 'x' },
    'r': { 'u': 'x', 'r': 'x', 'd': 'r', 'l': 'u' },
    'd': { 'u': 'r', 'r': 'x', 'd': 'x', 'l': 'd' },
    'l': { 'u': 'l', 'r': 'd', 'd': 'x', 'l': 'x' },
};

const ZAPPED_PIECE_COLOR = {
    'black': 'rgba(160,255,160,0.4)',
    'white': 'rgba(20,255,20,0.4)',
};
const EXPLODING_PIECE_COLOR = {
    'black': 'rgba(75,75,0,0.4)',
    'white': 'rgba(255,200,0,0.4)',
};

const ORI_TO_INDEX = { 'nw': 0, 'ne': 1, 'se': 2, 'sw': 3 };
const LASER_COLORS = [
    '#ff0000', // red
    '#008c00', // green
    '#0000ff', // blue
    '#8000ff', // purple
    '#ff8000', // orange
    '#ffff00', // yellow
    '#80ff00', // lime green
    '#00ffff', // cyan
    '#00bfff', // light blue
    '#ff00ff', // pink
    '#000000', // black
    '#330000', // brown
    '#808080', // grey
];
const KING_LASER_COLOR = {'white': '#c37100', 'black': '#8f4ae0'};

// Piece ID
let pieceCount = 0;

// Options for Gameplay
const CHAIN_REACTION = false;
const BOMBS = false;
const SWAPPING = false;
const STOMPING = true;

const Piece = function (orientation, color, position, board) {
    this.id = 'piece-' + pieceCount++;
    this.name = '';
    this.orientation = orientation;
    this.color = color;
    this.position = position; // Ex: a8
    this.pieceImg = null;
    this.board = board;
    this.blocked = false;

    this.fenText = function () {
        const fen = (this.name == 'king') ? KING_INV_MAP[this.orientation] : PAWN_INV_MAP[this.orientation];
        return ((this.color == 'white') ? fen.toUpperCase() : fen);
    };

    this.imageSrc = function () {
        if (!this.blocked) {
            return STATICS_HOST + 'images/' + this.name + '-' + this.color + '.svg';
        } else {
            return STATICS_HOST + 'images/radio.png';
        }
    };

    // Position relative to board
    const parentPosition = $('#board-parent').position();

    this.left = function () {
        return 2 + parentPosition.left + COL_NAMES.indexOf(this.position[0]) * PIECE_SIZE;
    };

    this.top = function () {
        return 2 + parentPosition.top + (NUM_ROWS - this.position[1] - 1) * PIECE_SIZE;
    };

    this.draw = function () {
        if (this.pieceImg === null) {
            const that = this;
            this.pieceImg = $('<img />');
            this.pieceImg[0].src = this.imageSrc();
            this.pieceImg[0].id = this.id;
            this.pieceImg[0].width = PIECE_SIZE;
            this.pieceImg[0].height = PIECE_SIZE;
            this.pieceImg[0].draggable = true;
            this.pieceImg.on('dragstart', function (e) {
                that.board.setSelectedPiece(that);
            });
            this.pieceImg.click(function (e) {
                that.board.handlePieceClick(that);
            });
            this.pieceImg.addClass('piece');

            this.pieceImg.appendTo('#board-parent');
        }

        this.pieceImg.css({ top: this.top(), left: this.left() });

        const rotate = { 'u': 0, 'd': 180, 'l': 270, 'r': 90 };
        this.pieceImg.css({ rotate: rotate[this.orientation] + 'deg' });
    };

    this.rotate = function (direction, dont_animate) {
        // dont_animate optional argument, default false
        const orientations = ['u', 'r', 'd', 'l'];
        let idx = orientations.indexOf(this.orientation);
        let rotationString;
        let idxDelta;
        if ((direction == 'R') || (direction == 'r')) {
            idxDelta = 1;
            rotationString = '+=90deg';
        } else if ((direction == 'L') || (direction == 'l')) {
            idxDelta = 3;
            rotationString = '-=90deg';
        } else if ((direction == 'U') || (direction == 'u')) {
            idxDelta = 2;
            rotationString = '+=180deg';
        } else {
            return;
        }
        idx = (idx + idxDelta) % 4;
        if (!dont_animate) {
            this.pieceImg.transition({ rotate: rotationString }, ANIMATE_DURATION);
        }
        this.orientation = orientations[idx];
    };

    this.move = function (newPosition, dont_animate) {
        // dont_animate optional argument, default false
        this.position = newPosition;
        $('.piece').css('z-index', '10');
        $(this.pieceImg).css('z-index', '11');
        if (!dont_animate) {
            this.pieceImg.animate({ top: this.top(), left: this.left() }, ANIMATE_DURATION);
        }
    };

    this.isKing = function () {
        return this.name == 'king';
    }

    this.hasColor = function (color) {
        return this.color == color;
    }

    // Laser
    // laserDirection: direction of input laser
    // return: u,d,l,r (output laser), x (dead), s (stop)
    this.laserResult = function (laserDirection) {
        var dir = this.laserMap[this.orientation][laserDirection];
        return dir;
    };

    this.remove = function () {
        this.pieceImg.transition({opacity: 0}, 1000, function() {
            this.remove();
        });
    };

    this.changeColor = function (color) {
        this.color = color;
        this.pieceImg[0].src = this.imageSrc(color);
    }
};

const King = function (orientation, color, position, board) {
    const that = new Piece(orientation, color, position, board);
    that.name = 'king';
    that.laserMap = KING_LASER_MAP;

    return that;
};

const Pawn = function (orientation, color, position, board) {
    const that = new Piece(orientation, color, position, board);
    that.name = 'pawn';
    that.laserMap = PAWN_LASER_MAP;
    return that;
};

const Move = function (text, board) {
    const squares = [];
    for (let i = 0; i + 2 <= text.length; i += 2) {
        const sq = text.slice(i, i + 2);
        squares.push(sq);
    }
    console.log(squares);
    if (squares.indexOf(null) != -1) {
        this.from = null;
        this.mid = null;
        this.to = null;
    } else if (squares.length == 1) {
        this.from = squares[0];
        this.mid = squares[0];
        this.to = squares[0];
    } else if (squares.length == 2) {
        this.from = squares[0];
        this.mid = squares[0];
        this.to = squares[1];
    } else if (squares.length == 3) {
        this.from = squares[0];
        this.mid = squares[1];
        this.to = squares[2];
    } else {
        this.from = null;
        this.mid = null;
        this.to = null;
    }

    if (this.from !== null && text.length % 2 == 1) {
        if (['L', 'R', 'U', 'l', 'r', 'u'].indexOf(text[text.length - 1]) != -1) {
            this.rot = text[text.length - 1].toUpperCase();
        } else {
            this.from = null;
            this.mid = null;
            this.to = null;
            this.rot = null;
        }
    } else {
        this.rot = null;
    }

    if (this.from == this.mid && this.mid == this.to && this.rot === null && (board && !board.getPiece(this.from).name == 'king')) {
        this.from = null;
        this.mid = null;
        this.to = null;
    }

    if (this.from == this.mid && this.from != this.to && this.rot !== null) {
        this.mid = this.to;
    }

    if (this.from != this.mid && this.mid != this.to && this.rot !== null) {
        this.from = null;
        this.mid = null;
        this.to = null;
        this.rot = null;
    }

    if (this.from != this.mid && this.mid != this.to && this.from == this.to) {
        this.from = null;
        this.mid = null;
        this.to = null;
        this.rot = null;
    }

    if (this.from !== null) {
        let okay = true;
    }

    this.isFormed = function () {
        return (this.from !== null);
    };

    this.isDoubleMove = function () {
        return (this.from != this.mid && this.mid != this.to);
    };

    this.isDoubleRot = function () {
        return (this.from != this.to && this.rot !== null);
    };

    this.isDouble = function () {
        return (this.isDoubleMove() || this.isDoubleRot());
    };

    this.isMove = function () {
        return (this.from == this.mid && this.mid != this.to && this.rot === null);
    };

    this.isSelfMove = function () {
        return (this.from == this.mid && this.mid == this.to && this.rot === null);
    };

    this.isRot = function () {
        return (this.from == this.to && this.rot !== null);
    };

    this.isBasic = function () {
        return (this.isMove() || this.isRot());
    };

    this.add = function (addMove) {
        const ret = new Move('');

        if (this.from === null || addMove.from === null) {
            return ret;
        }

        if (this.isDoubleMove() || addMove.isDouble()) {
            return ret;
        }

        if (this.isRot() && !addMove.isRot()) {
            return ret;
        }

        if (this.to != addMove.from) {
            return ret;
        }

        if (this.isMove() && addMove.isMove()) {
            ret.from = this.from;
            ret.mid = this.to;
            ret.to = addMove.to;
            return ret;
        }

        if (this.isMove() && addMove.isRot()) {
            ret.from = this.from;
            ret.mid = this.to;
            ret.to = this.to;
            ret.rot = addMove.rot;
            return ret;
        }

        if ((this.isRot() || this.isDoubleRot()) && addMove.isRot()) {
            const DIR_ADD_MAP = {
                'L': { 'L': 'U', 'R': null, 'U': 'R', 'null': 'L' },
                'R': { 'L': null, 'R': 'U', 'U': 'L', 'null': 'R' },
                'U': { 'L': 'R', 'R': 'L', 'U': null, 'null': 'U' },
                'null': { 'L': 'L', 'R': 'R', 'U': 'U', 'null': null },
            };
            ret.from = this.from;
            ret.mid = this.to;
            ret.to = this.to;
            ret.rot = DIR_ADD_MAP[this.rot][addMove.rot];
            if (ret.rot === null) {
                if (this.from == addMove.to) {
                    // console.trace();
                    ret.from = null;
                    ret.mid = null;
                    ret.to = null;
                } else {
                    ret.mid = ret.from;
                }
            }
            return ret;
        }

        // console.trace();
        return ret;
    };

    this.toString = function () {
        if (this.from === null) {
            return '';
        }

        let ret = this.from;
        if (this.from != this.mid) {
            ret = ret + this.mid;
        }
        if (this.mid != this.to) {
            ret = ret + this.to;
        }
        if (this.rot !== null) {
            ret = ret + this.rot;
        }

        return ret;
    };

    this.revert = function () {
        if (this.from === null) {
            return '';
        }

        let ret = this.to;
        if (this.mid != this.to) {
            ret = ret + this.mid;
        }
        if (this.from != this.mid) {
            ret = ret + this.from;
        }
        if (this.rot !== null) {
            const revertMap = { 'L': 'R', 'R': 'L', 'U': 'U', 'l': 'r', 'r': 'l', 'u': 'u', 'null': '' };
            ret = ret + revertMap[this.rot];
        }

        return ret;
    };
};

const Board = function (dontClear) {
    this.board = {};
    for (let i = 0; i < NUM_COLS; i++) {
        this.board[COL_NAMES[i]] = new Array(NUM_ROWS);
    }

    this.startBoardFen = '';
    this.history = [];
    this.historySnapshot = [];
    this.historyActiveLen = 0;
    this.turn = 'white';
    this.potentialMove = '';
    this.potentialRevertMove = '';
    this.possibleNullMove = '';
    this.victim = null;
    this.skittles_history = [];

    // Timing
    // Selected piece (from the drag-and-drop interface), and also for the
    // click-rotations.
    this.selectedPiece = '';
    this.selectedPieceLeftRotations = 0;

    // Stomped piece2
    this.stompedPiece = null;
    if (this.stompedPiece != null) {
        this.stompedPiece.remove();
        // this.stompedPiece2.remove();
    }
    this.pinned = [];
    this.setSelectedPiece = function (selected_piece) {
        this.selectedPiece = selected_piece;
    };

    this.checkForDraw = function () {
        const countMaterial = function (fen) {
            let empty = 0;
            for (let c of fen) {
                if ('8' >= c && c >= '1') {
                    empty += parseInt(c);
                }
            }
            // no time to do this neatly, you will understand
            return 64 - empty;
        };

	const MATERIAL_DRAW_MOVES = 50;
	const MATERIAL_DRAW_PLY = 2 * MATERIAL_DRAW_MOVES;
        let materialDraw = false;
        if (this.historySnapshot.length >= MATERIAL_DRAW_PLY) {
            materialDraw = true;
            for (let i = 1; i < MATERIAL_DRAW_PLY; i++) {
                // there must be a victim in the past MATERIAL_DRAW_PLY moves
                const fenA = this.historySnapshot[this.historySnapshot.length - i];
                const fenB = this.historySnapshot[this.historySnapshot.length - i - 1];

                if (countMaterial(fenA) != countMaterial(fenB)) {
                    materialDraw = false;
                    break;
                }
            }
            if (materialDraw) {
                console.log('material draw'); // just for a breakpoint
            }
        }

        const REPEAT_DRAW_TIMES = 3;
        let repeats = 0;
        const nowBoard = this.historySnapshot[this.historySnapshot.length - 1];
        for (let i = this.historySnapshot.length - 1; i >= 0; i -= 2) {
            const board = this.historySnapshot[i];

            if (nowBoard == board) {
                repeats++;
            }
        }

        const repeatDraw = repeats >= REPEAT_DRAW_TIMES;

        let reasons = [];
        if (materialDraw) {
            reasons.push('no Pawns zapped for last ' + MATERIAL_DRAW_MOVES + ' moves');
        }
        if (repeatDraw) {
            reasons.push('position repeated ' +
                         repeats + ' times with the same player on move');
        }

        return reasons;
    }

    this.handleDrop = function (x, y) {
        console.log('handleDrop at ' + COL_NAMES[x] + [y]);
        if (this.draggingNew) {
            let pos = COL_NAMES[x] + y;
            console.log("we out! ", pos);

            let piece = new Pawn('u', this.draggingNew, pos, this);
            this.board[pos[0]][pos[1]] = piece;
            piece.draw();
            this.skittles_history_append(this.trimTurn(this.getBoardFen()));
        console.log(this.historySnapshot);
        } else if (this.selectedPiece !== null) {
            $('#move').val(this.selectedPiece.position + COL_NAMES[x] + y);
            $('#move-button').click();
        }
    };

    this.handleDeleteDrop = function() {
        if (this.selectedPiece !== null) {
            this.selectedPiece.remove();

            const hitCol = this.selectedPiece.position[0];
            const hitColId = hitCol.charCodeAt(0) - 'a'.charCodeAt(0);
            const hitRow = 1 * this.selectedPiece.position[1];
            this.board[hitCol][hitRow] = null;
        }
        this.skittles_history_append(this.trimTurn(this.getBoardFen()));
        console.log(this.historySnapshot);
    }

    this.handlePieceClick = function (selected_piece) {
        console.log('handlePieceClick at ' + selected_piece.position);
        this.setSelectedPiece(selected_piece);
        // TODO: Get rid of this ugly hack.
        $('#move').val(this.selectedPiece.position + 'L');
        $('#move-button').click();
    };

    this.getPiece = function (position) {
        const piece = this.board[position[0]][position[1]];
        if (piece) {
            return piece;
        } else {
            return null;
        }
    };


    this.pawnsCollide = function (ori1, ori2, d) {
        let pawnOri1 = PAWN_INV_MAP[ori1];
        let pawnOri2 = PAWN_INV_MAP[ori2];
        if (d < 4) {
            switch (d) {
                case 0: // dirNW
                    return pawnOri1 != 'nw' && pawnOri2 != 'se';
                case 2: // dirNE
                    return pawnOri1 != 'ne' && pawnOri2 != 'sw';
                case 1: // dirN
                    return !((pawnOri1 == 'nw' && pawnOri2 == 'se')
                        || (pawnOri1 == 'ne' && pawnOri2 == 'sw'));
                case 3: // dirW
                    return !((pawnOri1 == 'nw' && pawnOri2 == 'se')
                        || (pawnOri1 == 'sw' && pawnOri2 == 'ne'));
            }
        }

        return this.pawnsCollide(ori2, ori1, 7 - d);
    };

    this.hitPiece = function (currPiece, remove) {
        if (remove) {
            currPiece.remove();
        }

        const hitCol = currPiece.position[0];
        const hitColId = hitCol.charCodeAt(0) - 'a'.charCodeAt(0);
        const hitRow = 1 * currPiece.position[1];
        if (remove) this.board[hitCol][hitRow] = null;

        console.log(">>>>>>> ", EXPLODING_PIECE_COLOR[currPiece.color]);
        this.colorSquare(hitColId, hitRow, EXPLODING_PIECE_COLOR[currPiece.color]);
        if (currPiece.name != 'pawn') {
            return;
        }

        for (let d = 0; d < 8; d++) {
            const nextColId = DIRECTIONS[d][0] + hitColId;
            const nextRow = DIRECTIONS[d][1] + hitRow;
            if (nextColId < 0 || nextColId > 7) {
                continue;
            }
            if (nextRow < 0 || nextRow > 7) {
                continue;
            }

            const nextPiece = this.getPieceCR([nextColId, nextRow]);
            if (nextPiece != null && nextPiece.name == 'pawn' && CHAIN_REACTION
                && this.pawnsCollide(currPiece.orientation, nextPiece.orientation, d)) {
                this.hitPiece(nextPiece, remove);
            }
        }
    };

    // position = [col #, row #]
    this.getPieceCR = function (position) {
        const piece = this.board[COL_NAMES[position[0]]][position[1]];
        if (piece) {
            return piece;
        } else {
            return null;
        }
    };

    // ########## BOARD REP RELATED ##########

    this.trimTurn = function (fen) {
        if (fen) {
            return fen.slice(0, fen.length - 2);
        } else {
            return fen;
        }
    };

    // returns starting board status in FEN notation
    // might use special board names instead
    this.getStartBoardFen = function () {
        if (this.startBoardFen in SPECIAL_BOARDS) {
            return this.startBoardFen;
        } else {
            return 'fen ' + this.startBoardFen;
        }
    };

    // returns current board status in FEN notation
    this.getBoardFen = function () {
        let rep = '';
        for (let r = NUM_ROWS - 1; r >= 0; r--) {
            let blankCount = 0;
            for (let c = 0; c < NUM_COLS; c++) {
                const piece = this.getPiece(COL_NAMES[c] + r);
                if (piece) {
                    if (blankCount != 0) {
                        rep += blankCount;
                    }
                    rep += piece.fenText();
                    blankCount = 0;
                } else {
                    blankCount++;
                }
            }
            if (blankCount != 0) {
                rep += blankCount;
            }
            if (r > 0) {
                rep += '/';
            }
        }
        rep += ' ' + PLAYER_INV_MAP[this.turn];
        return rep;
    };

    // returns starting board status in FEN notation
    // plus move history, only first [step] turns
    this.getBoardFenHistStep = function (step) {
        let rep = this.startBoardFen;
        if (step > this.history.length) {
            step = this.history.length;
        }
        if (step > 0) {
            rep += '#' + this.history.slice(0, step).join('#');
        }
        return rep;
    };

    // returns starting board status in FEN notation
    // plus move history, only first [historyActiveLen] turns
    this.getBoardFenHistCurrent = function () {
        return this.getBoardFenHistStep(this.historyActiveLen);
    };

    // returns starting board status in FEN notation
    // plus move history, all turns
    this.getBoardFenHistFull = function () {
        return this.getBoardFenHistStep(this.history.length);
    };

    this.getHistorySnapshot = function (turn) {
        if (turn >= this.historySnapshot.length) {
            return this.trimTurn(this.getBoardFen());
        } else {
            return this.historySnapshot[turn];
        }
    };

    // takes FEN notation describing ONE row e.g. '10', '4NESE1ss2'
    // returns input separated into tokens of strings and integers
    //     e.g. [4, 'NE', 'SE', 1, 'SS', 2]
    // returns null if input is an invalid string
    this.splitFenRow = function (text) {
        const isDigit = function (char) {
            return '0' <= char && char <= '9';
        };
        const isUppercase = function (char) {
            return 'A' <= char && char <= 'Z';
        };

        const answer = [];
        let colused = 0;
        let i = 0;
        while (i < text.length) {
            if (isDigit(text.charAt(i))) {
                let j = i + 1;
                while (j < text.length && isDigit(text.charAt(j))) {
                    j++;
                }
                const value = parseInt(text.slice(i, j));
                answer.push(value);
                colused += value;
                i = j;
            } else if (text.charAt(i) == '$') {
                i++;
                if (i + 1 < text.length && !isDigit(text.charAt(i + 1)) &&
                    isUppercase(text.charAt(i)) == isUppercase(text.charAt(i + 1)) && text.charAt(i) != text.charAt(i + 1)) {
                    answer.push('$' + text.slice(i, i + 2));
                    colused++;
                    i += 2;
                } else {
                    return null;
                }
            } else {
                if (i + 1 < text.length && !isDigit(text.charAt(i + 1)) &&
                    isUppercase(text.charAt(i)) == isUppercase(text.charAt(i + 1))) {
                    answer.push(text.slice(i, i + 2));
                    colused++;
                    i += 2;
                } else {
                    return null;
                }
            }
        }
        return answer;
    };

    // takes board status in FEN notation (without move history)
    // sets the board accordingly
    // returns true if board valid, false otherwise
    this.setBoardFen = function (text, dontClear) {
        for (let i = 0; i < NUM_COLS; i++) {
            this.board[COL_NAMES[i]] = new Array(NUM_ROWS);
        }
        const isUppercase = function (char) {
            return 'A' <= char && char <= 'Z';
        };

        text = text.replace(/\s+/g, ''); // Strip spaces
        if (text in SPECIAL_BOARDS) {
            text = SPECIAL_BOARDS[text].replace(/\s+/g, '');
        }
        const turntext = text.slice(text.length - 1).toUpperCase();
        if (!(turntext in PLAYER_MAP)) {
            return false;
        }
        const rowtext = text.slice(0, text.length - 1).split('/');
        if (rowtext.length == NUM_ROWS) {
            for (let r = NUM_ROWS - 1; r >= 0; r--) {
                const cols = this.splitFenRow(rowtext[NUM_ROWS - r - 1]);
                if (cols != null) {
                    let c = 0;
                    const board = this.board;
                    const that = this;
                    cols.forEach(function (e) {
                        if (typeof e == 'number') {
                            c += e;
                        } else {
                            let i = 0;
                            let neutral = false;
                            if (e.charAt(0) == '$') {
                                neutral = true;
                                i++;
                                e = e.slice(1);
                            }
                            const color = isUppercase(e.charAt(0)) ? 'white' : 'black';
                            const position = COL_NAMES[c] + r;
                            let piece = null;
                            if (e.charAt(0) == e.charAt(1)) {
                                piece = new King(KING_MAP[e.toLowerCase()], color, position, that);
                            } else {
                                // set gray
                                piece = new Pawn(PAWN_MAP[e.toLowerCase()], neutral ? 'gray' : color, position, that);
                            }
                            board[COL_NAMES[c]][r] = piece;
                            c++;
                        }
                    });
                } else {
                    return false;
                }
            }
        } else {
            return false;
        }
        this.turn = PLAYER_MAP[turntext];
        if (!dontClear) {
            this.clearLaser();
        }
        return true;
    };

    // takes board status in FEN notation (with move history)
    // sets the board accordingly
    // returns true if board valid, false otherwise
    this.setBoardFenHist = function (text) {
        console.log('setBoardFenHist: ' + text);
        text = text.replace(/\s+/g, ''); // Strip spaces
        const textarray = text.split('#');
        if (textarray.length > 0) {
            // Set initial board
            if (!this.setBoardFen(textarray[0])) {
                return false;
            }
            // Set startBoardFen variable
            if (textarray[0] in SPECIAL_BOARDS) {
                this.startBoardFen = textarray[0];
            } else {
                this.startBoardFen = textarray[0].slice(0, textarray[0].length - 1) + ' ' + textarray[0].slice(textarray[0].length - 1);
            }
            // Play moves in history
            this.historyActiveLen = 0;
            this.historySnapshot = [this.trimTurn(this.getBoardFen())];
            for (let i = 1; i < textarray.length; i++) {
                if (!this.isKo(textarray[i], this.turn)) {
                    this.move(textarray[i], null, null, true, true);
                    const victim = this.laserTarget(this.turn);
                    if (victim !== null) {
                        this.hitPiece(victim, false);
                    }

                    this.commitPotentialMove();
                    this.turn = ((this.turn == 'black') ? 'white' : 'black');
                } else {
                    return false;
                }
            }
            return true;
        } else {
            return false;
        }
    };

    // takes board status in PGN notation
    // sets the board accordingly
    // returns true if board valid, false otherwise
    this.setBoardPGN = function (text) {
        const textarray = text.split('\n');
        let tokenCount = 0;
        let whiteMove = null;
        const moves = [];
        const replaceNull = function (str) {
            return (str == 'null') ? '-' : str;
        };
        for (let i = 0; i < textarray.length; i++) {
            const line = textarray[i].trim();
            if (line.length == 0 || line[0] == '[' || line[line.length - 1] == ']') {
                continue;
            }
            const linearray = line.split(' ');
            for (let j = 0; j < linearray.length; j++) {
                const token = linearray[j];
                if (token.length == 0) {
                    continue;
                }
                tokenCount++;
                switch (tokenCount % 9) {
                    case 2: whiteMove = token; break;
                    case 6: if (token == '-' || token.indexOf('-') == -1) {
                        if (whiteMove != 'null' || token != 'null') {
                            moves.push(replaceNull(whiteMove));
                            moves.push(replaceNull(token));
                        }
                        whiteMove = null;
                    }
                        break;
                }
            }
            if (whiteMove !== null && whiteMove != 'null') {
                moves.push(replaceNull(whiteMove));
            }
        }
        const boardString = 'startpos#' + moves.join('#');
        return this.setBoardFenHist(boardString);
    };

    // ########## MOVES RELATED ##########

    // returns true if move valid
    // param: text is the move: 'a3R', 'j0j1', etc.
    // param: turn is the color specifying current player's turn: 'white' or 'black'
    this.isValidMove = function (theMove, turn) {
        return true;
    };

    this.isKo = function (text, turn, lastMove) {
        return false; // 2020 rule: no ko moves
    };

    this.doMove = function (theMove, dont_animate) {
        // dont_animate optional argument, default false
        console.log('doMove: ' + theMove.toString());


        let ret = 0;
        const fromPiece = this.getPiece(theMove.from);
        const midPiece = this.getPiece(theMove.mid);
        const toPiece = this.getPiece(theMove.to);

        console.log(fromPiece, midPiece, toPiece);

        if (!fromPiece) {
            return;
        }

        const fromC = theMove.from[0];
        const fromR = theMove.from[1];
        const midC = theMove.mid[0];
        const midR = theMove.mid[1];
        const toC = theMove.to[0];
        const toR = theMove.to[1];

        // Swap if necessary.
        this.board[fromC][fromR] = midPiece;
        this.board[midC][midR] = toPiece;
        this.board[toC][toR] = fromPiece;
        

        if (theMove.from == theMove.mid) {
            if (theMove.from != theMove.to) {
                ret++;
                if (toPiece) {
                    fromPiece.move(theMove.to, false);
                    toPiece.remove();
                    this.board[midC][midR] = null;
                    if (!fromPiece.isKing()) {
                    } else {
                        fromPiece.move(theMove.to, dont_animate);    
                    }
                } else {
                
                    fromPiece.move(theMove.to, dont_animate);
                }
            }
        }

        if (theMove.rot !== null) {
            ret++;
            fromPiece.rotate(theMove.rot, dont_animate);
        }

        return ret;
    };

    this.move = function (text, successCallback, failureCallback, dont_animate, dontCheckValidity) {

        this.clearLaser();
        var button = document.getElementById("black-show-laser");
        button.value = "Show Lavender Laser";
        var button = document.getElementById("white-show-laser");
        button.value = "Show Tangerine Laser";

        // dont_animate optional argument, default false
        console.log('move: ' + text);
        console.log('move: ' + this.potentialMove + '/' + this.potentialRevertMove);

        dontCheckValidity = true;

        if (text == '' && this.potentialMove == '' && this.potentialRevertMove == '') {
            if (successCallback) {
                successCallback();
            }
            return; // the king might be able to hit something, let's see
        }

        let theMove = new Move(text);
        console.log("theMove: ", theMove);

        if (!dontCheckValidity) {
            if (!this.isValidMove(theMove, this.turn) || this.isKo(theMove, this.turn)) {
                if (failureCallback) {
                    console.log('move: fail ' + text + ' (invalid or Ko)');
                    failureCallback();
                }
                return;
            }
        }

        const currMove = new Move(this.potentialMove);

        this.potentialMove = text;
        this.potentialRevertMove = theMove.revert();

        if (!dont_animate) {
            this.clearLaser();
        }

        const steps = this.doMove(theMove, dont_animate);

        console.log('move: success ' + text);
        console.log('move: ' + this.potentialMove + '/' + this.potentialRevertMove);

        if (successCallback) {
            setTimeout(function () {
                successCallback();
            }, ANIMATE_DURATION * steps);
        }
        this.commitPotentialMove();
    };

    this.skittles_history_append = function(hist) {
        this.historySnapshot = this.historySnapshot.slice(0, this.cur_hist_step + 1);
        this.historySnapshot.push(hist);
        this.cur_hist_step += 1;
        console.log(this.historySnapshot, this.cur_hist_step);
    }

    this.clearPotentialMove = function () {
        this.potentialMove = '';
        this.potentialRevertMove = '';
        this.selectedPiece = null;
        this.selectedPieceLeftRotations = 0;
    };

    this.revertPotentialMove = function () {
        this.move(this.potentialRevertMove, null, null, false, true);
        this.clearPotentialMove();
        this.drawLaser(KING_LASER_COLOR[this.turn], false, false, true);
    };

    this.commitPotentialMove = function () {
        console.log("COMMIT POTENTIAL MOVE");
        let moveText = this.potentialMove;
        if (moveText == '') {
            let king = this.getPieceCR(this.findKing(this.turn));
            moveText = king.position + king.position;
        }
        this.skittles_history_append(this.trimTurn(this.getBoardFen()));
        console.log(this.historySnapshot);
        this.clearPotentialMove();
    };

    this.findKing = function (color) {
        for (let r = NUM_ROWS - 1; r >= 0; r--) {
            for (let c = 0; c < NUM_COLS; c++) {
                const piece = this.getPiece(COL_NAMES[c] + r);
                if (piece !== null && piece.isKing() && piece.hasColor(color)) {
                    return [c, r];
                }
            }
        }
        return null;
    };

    this.findKings = function (color) {
        let kings = [];
        for (let r = NUM_ROWS - 1; r >= 0; r--) {
            for (let c = 0; c < NUM_COLS; c++) {
                const piece = this.getPiece(COL_NAMES[c] + r);
                if (piece !== null && piece.isKing() && piece.hasColor(color)) {
                    kings.push([c, r]);
                }
            }
        }
        return kings;
    };

    this.lookAhead = function (lastMove, unsafeCallback) {
        const myColor = this.turn;
        const theirColor = (myColor == 'white') ? 'black' : 'white';

        // my own laser
        const casualty = this.laserTarget(myColor);
        if (casualty) {
            if ((casualty.color == myColor) && (casualty.name == 'king')) {
                unsafeCallback();
                return;
            }
        }
        // opponent's laser after they move
        const newBoard = new Board();
        newBoard.setBoardFen(this.getBoardFen());
        if (casualty) {
            newBoard.board[casualty.position[0]][casualty.position[1]] = null;
        }
        for (let r = NUM_ROWS - 1; r >= 0; r--) {
            for (let c = 0; c < NUM_COLS; c++) {
                const piece = newBoard.getPieceCR([c, r]);
                if (piece && (piece.color == theirColor)) {
                    // move
                    for (let i = 0; i < DIRECTIONS.length; i++) {
                        const midC = c + DIRECTIONS[i][0];
                        const midR = r + DIRECTIONS[i][1];

                        if (midC < 0 || midC >= NUM_COLS || midR < 0 || midR >= NUM_ROWS) {
                            continue; // out of bound
                        }

                        const midPiece = newBoard.getPieceCR([midC, midR]);

                        if (midPiece === null) {
                            const moveString = COL_NAMES[c] + r + COL_NAMES[midC] + midR;
                            if (!this.isKo(moveString, this.turn, this.historyActiveLen + 1)) { // checks Ko
                                let temp = newBoard.board[COL_NAMES[c]][r];
                                newBoard.board[COL_NAMES[c]][r] = newBoard.board[COL_NAMES[midC]][midR];
                                newBoard.board[COL_NAMES[midC]][midR] = temp;
                                const casualty = newBoard.laserTarget(theirColor);
                                if (casualty && (casualty.color == myColor) && (casualty.name == 'king')) {
                                    unsafeCallback();
                                    return;
                                }
                                temp = newBoard.board[COL_NAMES[c]][r];
                                newBoard.board[COL_NAMES[c]][r] = newBoard.board[COL_NAMES[midC]][midR];
                                newBoard.board[COL_NAMES[midC]][midR] = temp;
                            }
                        } else if (midPiece.color == myColor) {
                            // doublemove
                            for (let i2 = 0; i2 < DIRECTIONS.length; i2++) {
                                const toC = c + DIRECTIONS[i2][0];
                                const toR = r + DIRECTIONS[i2][1];

                                if (toC < 0 || toC >= NUM_COLS || toR < 0 || toR >= NUM_ROWS) {
                                    continue; // out of bound
                                }

                                const toPiece = newBoard.getPieceCR([toC, toR]);

                                if (toPiece === null) {
                                    const moveString = COL_NAMES[c] + r + COL_NAMES[toC] + toR;
                                    if (!this.isKo(moveString, this.turn, this.historyActiveLen + 1)) { // checks Ko
                                        let temp = newBoard.board[COL_NAMES[c]][r];
                                        newBoard.board[COL_NAMES[c]][r] = newBoard.board[COL_NAMES[midC]][midR];
                                        newBoard.board[COL_NAMES[midC]][midR] = newBoard.board[COL_NAMES[toC]][toR];
                                        newBoard.board[COL_NAMES[toC]][toR] = temp;
                                        const casualty = newBoard.laserTarget(theirColor);
                                        if (casualty && (casualty.color == myColor) && (casualty.name == 'king')) {
                                            unsafeCallback();
                                            return;
                                        }
                                        temp = newBoard.board[COL_NAMES[toC]][toR];
                                        newBoard.board[COL_NAMES[toC]][toR] = newBoard.board[COL_NAMES[midC]][midR];
                                        newBoard.board[COL_NAMES[midC]][midR] = newBoard.board[COL_NAMES[c]][r];
                                        newBoard.board[COL_NAMES[c]][r] = temp;
                                    }
                                }
                            }
                            // rotate
                            const orientations = ['u', 'r', 'd', 'l'];
                            const idx = orientations.indexOf(piece.orientation);
                            let temp = newBoard.board[COL_NAMES[c]][r];
                            newBoard.board[COL_NAMES[c]][r] = newBoard.board[COL_NAMES[midC]][midR];
                            newBoard.board[COL_NAMES[midC]][midR] = temp;
                            for (let i2 = 3; i2 >= 0; i2--) {
                                piece.orientation = orientations[(idx + i2) % 4];
                                const newCasualty = newBoard.laserTarget(theirColor);
                                if (newCasualty && (newCasualty.color == myColor) && (newCasualty.name == 'king')) {
                                    unsafeCallback();
                                    return;
                                }
                            }
                            temp = newBoard.board[COL_NAMES[c]][r];
                            newBoard.board[COL_NAMES[c]][r] = newBoard.board[COL_NAMES[midC]][midR];
                            newBoard.board[COL_NAMES[midC]][midR] = temp;
                        }
                    }
                    // rotate
                    const orientations = ['u', 'r', 'd', 'l'];
                    const idx = orientations.indexOf(piece.orientation);
                    for (let i = 3; i >= 0; i--) {
                        piece.orientation = orientations[(idx + i) % 4];
                        const newCasualty = newBoard.laserTarget(theirColor);
                        if (newCasualty && (newCasualty.color == myColor) && (newCasualty.name == 'king')) {
                            unsafeCallback();
                            return;
                        }
                    }
                }
            }
        }
    };

    this.checkBoard = function () {
        let err = false;

        for (let r = NUM_ROWS - 1; r >= 0; r--) {
            for (let c = 0; c < NUM_COLS; c++) {
                const piece = this.getPieceCR([c, r]);
                if (piece && piece.position != COL_NAMES[c] + r) {
                    err = true;
                    console.log(piece.position + '!=' + COL_NAMES[c] + r);
                }
            }
        }
        if (err) {
            alert('bad board');
        }
    };

    // ########## LASER RELATED ##########

    // returns the piece destroyed if the laser is shot
    // null if no piece destroyed
    // does not actually shoot laser -- board representation does not change
    this.laserTargets = function (color) {
        const positions = this.findKings(color);
        if (positions.length === 0) {
            return null;
        }
        let targets = [];
        console.log(positions);
        for (let position of positions) {
            // let position = positions[i];
            let direction = this.getPieceCR(position).orientation;
            const transform = {
                'u': [0, 1],
                'r': [1, 0],
                'd': [0, -1],
                'l': [-1, 0],
            };
    
            let piece = null;
            while (true) {
                position[0] += transform[direction][0];
                position[1] += transform[direction][1];
                if (position[0] < 0 || position[0] >= NUM_COLS || position[1] < 0 || position[1] >= NUM_ROWS) {
                    // out of bounds
                    break;
                }
                piece = this.getPieceCR(position);
                if (piece !== null && piece.blocked == false) {
                    direction = piece.laserResult(direction);
    
                    if (direction == 's') {
                        // Stop
                        break;
                    } else if (direction == 'x') {
                        // Dead
                        targets.push(piece);
                        break;
                    }
                }
            }
        }
        return targets; 
    };

    this.laserBounces = function (color) {
        const position = this.findKing(color);
        if (!position) {
            return null;
        }
        let bounces = [];
        let hitbombs = [];
        let direction = this.getPieceCR(position).orientation;
        const transform = {
            'u': [0, 1],
            'r': [1, 0],
            'd': [0, -1],
            'l': [-1, 0],
        };

        let piece = null;
        while (true) {
            position[0] += transform[direction][0];
            position[1] += transform[direction][1];
            if (position[0] < 0 || position[0] >= NUM_COLS || position[1] < 0 || position[1] >= NUM_ROWS) {
                // out of bounds
                break;
            }
            piece = this.getPieceCR(position);
            if (piece !== null && piece.blocked == false) {
                direction = piece.laserResult(direction);

                if (direction == 's') {
                    // Stop
                    break;
                } else if (direction == 'x') {
                    // Dead
                    // actually consider this a bounce
                    bounces.push(piece);
                    break;
                } else {
                    bounces.push(piece);
                }
            }
        }
        return bounces;
    };

    // this only happens when a move is committed
    this.shootLaser = function () {
        this.victims = this.laserTargets(this.turn);

        this.drawLaser(KING_LASER_COLOR[this.turn]);
    
        if (this.victims.length !== null) {
            for (let victim of this.victims) {
                this.hitPiece(this.victim, true);
            }
        }
    };


    this.shootLaserPreview = function (turn) {
        console.log(turn);
        const newBoard = new Board();
        newBoard.setBoardFen(this.getBoardFen());
        const victims = newBoard.laserTargets(turn);
        newBoard.drawLaser(KING_LASER_COLOR[turn], true, false, true, turn);
        var other_turn = ((turn == "white")? "black" : "white");
        var other_button_name = other_turn + "-show-laser";
        var other_button = document.getElementById(other_button_name);
        if (other_button.value.substring(0, 4) == "Hide") {
            newBoard.drawLaser(KING_LASER_COLOR[other_turn], true, false, true, other_turn);
        }
        if (victims.length !== 0) {
            for (let victim of victims)
                newBoard.hitPiece(victim, false);
        }
    };

    this.shootLaserIdle = function (turn) {
        this.victim = this.laserTargets(turn);
        this.drawLaser(KING_LASER_COLOR[turn], true, false, true, turn);
        if (victims.length !== 0) {
            for (let victim of victims)
                this.hitPiece(victim, false);
        }
    };

    // ########## DRAWING RELATED ##########

    this.drawBoard = function () {
        $('#board-parent img').remove(); // Clear board
        for (let r = NUM_ROWS - 1; r >= 0; r--) {
            for (let c = 0; c < NUM_COLS; c++) {
                const piece = this.getPiece(COL_NAMES[c] + r);
                if (piece) {
                    piece.draw();
                }
            }
        }
    };

    this.posToCoord = function (position) {
        return [PIECE_SIZE / 2 + position[0] * PIECE_SIZE, PIECE_SIZE / 2 + (NUM_ROWS - position[1] - 1) * PIECE_SIZE];
    };

    this.getCtx = function () {
        return document.getElementById('laserCanvas').getContext('2d');
    }

    this.clearLaser = function () {
        if (this.victim) {
            this.victim.remove();
        }
        const ctx = this.getCtx();
        ctx.clearRect(0, 0, BOARD_WIDTH, BOARD_HEIGHT);
    };

    this.colorSquare = function (col, row, fillColor) {
        row = NUM_ROWS - row - 1;
        const fillSize = PIECE_SIZE - 2 * PIECE_MARGIN;
        const ctx = this.getCtx();
        ctx.fillStyle = fillColor;
        ctx.fillRect(col * PIECE_SIZE,
            row * PIECE_SIZE,
            fillSize,
            fillSize);
    };

    this.drawLaser = function (strokeColor, shouldKeep, lineWidth, dashed, turn) {
        const ctx = this.getCtx();
        ctx.strokeStyle = strokeColor;
        ctx.lineWidth = lineWidth ? lineWidth : 5;
        ctx.globalCompositeOperation = 'destination-over';
        if (!shouldKeep) {
            ctx.clearRect(0, 0, BOARD_WIDTH, BOARD_HEIGHT);
        }
        if (dashed !== undefined) {
            ctx.setLineDash([10, 15]);
        } else {
            ctx.setLineDash([]);
        }

        const positions = this.findKings(turn);

        for (let i = 0; i < positions.length; i++) {
            let position = positions[i];
            let coord = this.posToCoord(position);
            ctx.beginPath();
            ctx.moveTo(coord[0], coord[1]);

            let direction = this.getPieceCR(position).orientation;
            const transform = {
                'u': [0, 1],
                'r': [1, 0],
                'd': [0, -1],
                'l': [-1, 0],
            };

            let piece = null;
            let hitsomething = false;
            let skip_direction_change = false;
            let previous_direction = null;
            let bounce_positions = [];
            while (true) {
                if (skip_direction_change) {
                    direction = previous_direction;
                    skip_direction_change = false;
                } else {
                    previous_direction = direction;
                }
                position[0] += transform[direction][0];
                position[1] += transform[direction][1];
                if (position[0] < 0 || position[0] >= NUM_COLS || position[1] < 0 || position[1] >= NUM_ROWS) {
                    // out of bound
                    coord = this.posToCoord(position);
                    ctx.lineTo(coord[0], coord[1]);
                    break;
                }
                piece = this.getPieceCR(position);
                if (piece !== null) {
                    coord = this.posToCoord(position);
                    ctx.lineTo(coord[0], coord[1]);
                    direction = piece.laserResult(direction);

                    if (direction == 's') {
                        // Stop
                        if (hitsomething) {
                            break;
                        } else {
                            hitsomething = true;
                            skip_direction_change = true;
                        }
                        break; // new
                    } else if (direction == 'x') {
                        ctx.fillStyle = ZAPPED_PIECE_COLOR[piece.color];
                        const fillSize = PIECE_SIZE - 2 * PIECE_MARGIN;
                        ctx.fillRect(position[0] * PIECE_SIZE, (NUM_ROWS - position[1] - 1) * PIECE_SIZE, fillSize, fillSize);
                        skip_direction_change = true;
                        hitsomething = true;
                        break;
                    } else {
                        bounce_positions.push([position.slice(), piece.color]);
                    }
                }
            }
            ctx.stroke();
        }
    };
};

