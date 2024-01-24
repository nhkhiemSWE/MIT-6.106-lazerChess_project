/**
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
**/
'use strict';

/* global Board, NUM_ROWS, NUM_COLS, PIECE_SIZE, COL_NAMES */


const CANVAS_Z_INDEX = 1000;
var GLOBAL_BOARD;

$(document).ready(function () {
    let board = new Board();
    GLOBAL_BOARD = board;
    let player1 = 'human';
    let player2 = 'human';
    let gameStarted = false;

    const setup = () => {
        // Draw board label
        const wrapperPosition = $('#board-wrapper').position();
        let left = wrapperPosition.left;
        let top = wrapperPosition.top + 20;
        for (let i = NUM_ROWS; i > 0; i--) {
            const label = $('<div>' + (i - 1) + '</div>');
            label.addClass('board-label');
            label.appendTo('#board-wrapper');
            label.css({ top: top + (NUM_ROWS - i) * PIECE_SIZE, left: left });
        }

        left += 40;
        top -= 10;
        for (let i = 0; i < NUM_COLS; i++) {
            const label = $('<div>' + COL_NAMES[i] + '</div>');
            label.addClass('board-label');
            label.appendTo('#board-wrapper');
            label.css({ top: top + NUM_ROWS * PIECE_SIZE, left: left + i * PIECE_SIZE });
        }

        // Draw lines
        const bCanvas = document.getElementById('cboard');
        const ctx = bCanvas.getContext('2d');

        ctx.fillStyle = '#F0F0F0'; // board color
        ctx.fillRect(0, 0, PIECE_SIZE * NUM_COLS, PIECE_SIZE * NUM_ROWS);
        ctx.strokeStyle = '#A0A0A0'; // line color
        for (let i = 1; i < NUM_COLS; i++) {
            ctx.moveTo(PIECE_SIZE * i, 0);
            ctx.lineTo(PIECE_SIZE * i, PIECE_SIZE * NUM_ROWS);
        }
        for (let i = 1; i <= NUM_ROWS; i++) {
            ctx.moveTo(0, PIECE_SIZE * i);
            ctx.lineTo(PIECE_SIZE * NUM_COLS, PIECE_SIZE * i);
        }
        ctx.stroke();

        // Create laser canvas
        const laserCanvas = document.createElement('canvas');
        laserCanvas.height = PIECE_SIZE * NUM_ROWS;
        laserCanvas.width = PIECE_SIZE * NUM_COLS;
        laserCanvas.id = 'laserCanvas';
        laserCanvas.style.position = 'absolute';
        laserCanvas.style.zIndex = CANVAS_Z_INDEX;
        laserCanvas.style.pointerEvents = 'none';

        const parent = $('#board-parent').position();
        $('#board-parent').append(laserCanvas);
        $('#laserCanvas').css({ top: parent.top + 3, left: parent.left + 3 });

        // Handling drag-and-drop.
        $('#board-parent').on('dragover', function (e) {
            e.preventDefault();
        });
        $('#board-parent').on('dragenter', function (e) {
            e.preventDefault();
        });
        $('#board-parent').on('drag', function (e) {
            $('.piece').css('z-index', '10');
            $(e.originalEvent.target).css('z-index', '11');
        });
        $('#board-parent').on('drop', function (e) {
            e.preventDefault();

            const xOffset = e.originalEvent.clientX - laserCanvas.getBoundingClientRect().left;
            const yOffset = e.originalEvent.clientY - laserCanvas.getBoundingClientRect().top;

            const x = Math.floor(xOffset / PIECE_SIZE);
            const y = NUM_ROWS - 1 - Math.floor(yOffset / PIECE_SIZE);

            board.handleDrop(x, y);
            updateUI();
        });
        $('#board-parent').on('click', function () {
            board.clearLaser();
        });

        // $('#skittles-delete-zone').on('dragover', function(e) {
        //     e.preventDefault();
        //     $(this).css('background-color', 'black');
        // });
        // $('#skittles-delete-zone').on('dragleave', function(e) {
        //     e.preventDefault();
        //     $(this).css('background-color', 'red');
        // });
        // $('#skittles-delete-zone').on('drop', function(e) {
        //     e.preventDefault();
        //     $(this).css('background-color', 'red');
        //     board.handleDeleteDrop();
        //     updateUI();
        // })

        // $('#skittles-add-white')[0].draggable = true;

        // $('#skittles-add-white img').on('dragstart', function(e) {
        //     board.draggingNew = 'white';
        // });
        // $('#skittles-add-white img').on('dragend', function(e) {
        //     e.preventDefault();
        //     board.draggingNew = null;
        // });
        // $('#skittles-add-gray img').on('dragstart', function(e) {
        //     board.draggingNew = 'gray';
        // });
        // $('#skittles-add-gray img').on('dragend', function(e) {
        //     e.preventDefault();
        //     board.draggingNew = null;
        // });
        // $('#skittles-add-black img').on('dragstart', function(e) {
        //     board.draggingNew = 'black';
        // });
        // $('#skittles-add-black img').on('dragend', function(e) {
        //     e.preventDefault();
        //     board.draggingNew = null;
        // });

        // $('#skittles-pause').on('click', function(e) {
        //     if (!board.skittlesState) {
        //         board.skittlesState = 'play';
        //     }

        //     if (board.skittlesState == 'play') {
        //         board.skittlesState = 'pause';
        //         $(this).text('Play');
        //     } else {
        //         board.skittlesState = 'play';
        //         $(this).text('Pause');
        //     }
        // });

        // Create board
        board.setBoardFenHist('startpos');
        board.drawBoard();
        enableHistoryButtons(true);
        updateUI();
    };

    setup();

    $('#move').keypress((event) => {
        if (event.which == 13) {
            event.preventDefault();
            move();
        }
    });

    function setBoardFenHist() {
        const text = $('#fen-text').val();
        const newboard = new Board();
        if (newboard.setBoardFenHist(text)) {
            board = newboard;
            board.drawBoard();
            enableHistoryButtons(true);
            $('#move-button').click(); // attempt to null move just in case
            updateUI();
        } else {
            warning('STATE INVALID');
        }
    };

    function drawBoardFromState(newboard) {
        if (newboard.historyActiveLen > 0) {
            const boardWithoutLastMove = newboard.getBoardFenHistStep(newboard.historyActiveLen - 1);
            const lastMove = newboard.history[newboard.historyActiveLen - 1];
            const newerboard = new Board();
            newerboard.setBoardFenHist(boardWithoutLastMove);
            newerboard.move(lastMove, null, null, true);
            newerboard.commitPotentialMove();
            newerboard.history = newboard.history;
            newerboard.blacktime = newboard.blacktime;
            newerboard.whitetime = newboard.whitetime;
            board = newerboard;
        } else {
            board = newboard;
        }
        board.drawBoard();
        if (board.historyActiveLen > 0) {
            board.shootLaserIdle();
        }
    }

    function setBoardPGN() {
        const text = $('#pgn-text').val();
        const newboard = new Board();
        if (newboard.setBoardPGN(text)) {
            drawBoardFromState(newboard);
            enableHistoryButtons(true);
            updateUI();
        } else {
            warning('PGN INVALID');
        }
    };

    function stepTo(step) {
        const text = board.getBoardFenHistStep(step);
        const newboard = new Board();
        if (newboard.setBoardFenHist(text)) {
            newboard.history = board.history;
            newboard.blacktime = board.blacktime;
            newboard.whitetime = board.whitetime;
            drawBoardFromState(newboard);
            enableHistoryButtons(true);
            updateUI();
        } else {
            warning('ERROR');
        }
    };

    function stepFirst() {
        stepTo(0);
    }

    function stepPrev() {
        stepTo(board.historyActiveLen - 1);
    }

    function stepNext() {
        stepTo(board.historyActiveLen + 1);
    }

    function stepLast() {
        stepTo(board.history.length);
    }

    function undo() {
        // undo button usable while game active
        // similar to stepTo(board.historyActiveLen - 2) in some sense
        const text = board.getBoardFenHistStep(board.historyActiveLen - 2);
        const newboard = new Board();
        if (newboard.setBoardFenHist(text)) {
            newboard.blacktime = board.blacktime;
            newboard.whitetime = board.whitetime;
            board = newboard;
            board.clearPotentialMove();
            board.drawBoard();
            updateUI();
        } else {
            warning('ERROR');
        }
    }

    function move() {
        const text = $('#move').val();
        const moveUnsafeCallback = function () {
            // warning('MOVE UNSAFE', true);
        };
        const tryEnableLaserButton = function () {
            let ok = false;
            if (board.potentialMove == '') {
                // if firing laser can kill some piece
                // we allow null move as long as the board change
                const potentialVictims = board.laserTargets(board.turn);
                if (potentialVictims.length !== 0) {
                    ok = true;
                }
            } else {
                ok = true;
            }

            if (ok) {
                $('#laser-button').removeAttr('disabled');
                $('#laser-button').addClass('highlight');
                console.log('#laser-button enabled');
            }
            return ok;
        }

        const moveInvalidCallback = function () {
            warning('MOVE INVALID');
            tryEnableLaserButton();
        };
        const moveAnimatedCallback = function (lastMove) {
            return function () {
                $('#cancel-button').removeAttr('disabled');
                $('#move').attr('disabled', 'disabled');
                $('#move-button').attr('disabled', 'disabled');
                $('#undo-button').attr('disabled', 'disabled');
                $('#clock-stop-button').attr('disabled', 'disabled');
                $('#clock-reset-button').attr('disabled', 'disabled');
                if (tryEnableLaserButton()) {
                    board.lookAhead(lastMove, moveUnsafeCallback);
                    board.shootLaserPreview();
                }
            };
        }(text);
        $('#laser-button').attr('disabled', 'disabled');
        $('#laser-button').removeClass('highlight');
        console.log('#laser-button disabled');
        board.move(text, moveAnimatedCallback, moveInvalidCallback, false, false);
        $('#move').val('');
    };

    function cancel() {
        $('#cancel-button').attr('disabled', 'disabled');
        $('#laser-button').attr('disabled', 'disabled');
        $('#laser-button').removeClass('highlight');
        $('#move').removeAttr('disabled');
        $('#move-button').removeAttr('disabled');
        $('#undo-button').removeAttr('disabled');
        $('#clock-stop-button').removeAttr('disabled');
        $('#clock-reset-button').removeAttr('disabled');
        clearWarning();
        board.revertPotentialMove();

        // This messes up other state.
        // The downside is that if the user cancels when they could have null moved
        // they will need to rotate 360 to get the option to null move again
        // $('#move-button').click(); // attempt to null move just in case
    }

    function shootLaser() {
        $('#cancel-button').attr('disabled', 'disabled');
        $('#laser-button').attr('disabled', 'disabled');
        $('#laser-button').removeClass('highlight');
        $('#move').removeAttr('disabled');
        $('#move-button').removeAttr('disabled');
        $('#clock-stop-button').removeAttr('disabled');
        $('#clock-reset-button').removeAttr('disabled');
        $('#undo-button').removeAttr('disabled');
        clearWarning();
        board.commitPotentialMove();
        board.shootLaser();
    }

    function timeToString(time) {
        const negative = (time < 0);
        if (negative) {
            time = -time;
        }
        let seconds = time % 60;
        let minutes = (time - seconds) / 60;
        if (negative) {
            minutes = '-' + minutes;
        }
        if (seconds < 10) {
            seconds = '0' + seconds;
        }
        return minutes + ':' + seconds;
    }

    function updateTime() {
        if (!gameStarted) {
            return;
        }
        if (board.turn == 'black') {
            board.blacktime--;
        } else {
            board.whitetime--;
        }

        let whitetext = timeToString(board.whitetime);
        let blacktext = timeToString(board.blacktime);

        if (board.turn == 'black') {
            blacktext = '<b>' + blacktext + '<b/>';
        } else {
            whitetext = '<b>' + whitetext + '<b/>';
        }

        $('#whitetime').html(whitetext);
        $('#blacktime').html(blacktext);
        setTimeout(updateTime, 1000);
    }

    function enableHistoryButtons(enabled) {
        if (!enabled || board.historyActiveLen == 0) {
            $('#hist-first-button').attr('disabled', 'disabled');
            $('#hist-prev-button').attr('disabled', 'disabled');
        } else {
            $('#hist-first-button').removeAttr('disabled');
            $('#hist-prev-button').removeAttr('disabled');
        }
        if (!enabled || board.historyActiveLen == board.history.length) {
            $('#hist-next-button').attr('disabled', 'disabled');
            $('#hist-last-button').attr('disabled', 'disabled');
        } else {
            $('#hist-next-button').removeAttr('disabled');
            $('#hist-last-button').removeAttr('disabled');
        }
        if (enabled) {
            if (board.findKings('black').lenth === 0 || board.findKings('white').length === 0) {
                $('#clock-start-button').attr('disabled', 'disabled');
            } else {
                $('#clock-start-button').removeAttr('disabled');
            }
            $('#clock-stop-button').attr('disabled', 'disabled');
            $('#fen-text').removeAttr('disabled');
            $('#fen-button').removeAttr('disabled');
            $('#pgn-button').removeAttr('disabled');
            $('#bot-name1').removeAttr('disabled');
            $('#bot-name2').removeAttr('disabled');
        } else {
            $('#clock-start-button').attr('disabled', 'disabled');
            $('#clock-stop-button').removeAttr('disabled');
            $('#fen-text').attr('disabled', 'disabled');
            $('#fen-button').attr('disabled', 'disabled');
            $('#pgn-button').attr('disabled', 'disabled');
            $('#bot-name1').attr('disabled', 'disabled');
            $('#bot-name2').attr('disabled', 'disabled');
        }
    }

    function updateUI() {
        // HISTORY pane
        $('#move-no').html('');
        $('#black-move').html('');
        $('#white-move').html('');
        for (let i = 0; i < board.history.length; i++) {
            // turn number
            if (i % 2 == 0) {
                const mv = 1 + Math.floor(i / 2);
                const moveNumItem = $('<div>');
                moveNumItem.html(mv + '.&nbsp;');
                if (i >= board.historyActiveLen) {
                    moveNumItem.addClass('historyInactive');
                }
                $('#move-no').append(moveNumItem);
            }
            // moves
            const historyItem = $('<div>');
            historyItem.text(board.history[i]);
            if (i >= board.historyActiveLen) {
                historyItem.addClass('historyInactive');
            }
            if (i % 2 == 0) {
                $('#black-move').append(historyItem);
            } else {
                $('#white-move').append(historyItem);
            }
        }
        // REP pane
        $('#fen-rep-hist').text(board.getBoardFenHistCurrent());
        $('#fen-rep-nohist').text(board.getBoardFen());
        $('#board-history').scrollTop($('#board-history')[0].scrollHeight);
    }

    // Board action when player is switched
    $('#board-parent').bind('switchPlayer', () => {
        board.checkBoard();
        updateUI();
        const drawReason = board.checkForDraw();
        if (drawReason.length > 0) {
            alert('Draw: ' + drawReason);
            stopTime();
            return;
        }
        if (board.findKings('black').length === 0) {
            alert('Tangerine wins!');
            stopTime();
            return;
        }
        if (board.findKings('white').length === 0) {
            alert('Lavender wins!');
            stopTime();
            return;
        }
        if (board.findKings('black').length < board.findKings('white').length && board.turn == 'white') {
            alert('Tangerine wins!');
            stopTime();
            return;
        }
        if (board.findKings('white').length < board.findKings('black').length && board.turn == 'black') {
            alert('Lavender wins!');
            stopTime();
            return;
        }
        if ((board.turn == 'white' && player1 != 'human') || (board.turn == 'black' && player2 != 'human')) {
            // bot's turn
            $('#move').attr('disabled', 'disabled');
            $('#move-button').attr('disabled', 'disabled');
            $('#undo-button').attr('disabled', 'disabled');
            const botName = (board.turn == 'white') ? player1 : player2;
            let args = {};
            if (board.turn == 'white') {
                args = {
                    position: board.getStartBoardFen(),
                    moves: board.history.join(' '),
                    gotime: board.whitetime * 1000,
                    goinc: board.whiteinc * 1000,
                    player: botName,
                };
            } else {
                args = {
                    position: board.getStartBoardFen(),
                    moves: board.history.join(' '),
                    gotime: board.blacktime * 1000,
                    goinc: board.blackinc * 1000,
                    player: botName,
                };
            }
            $.post('/move/', args, (data) => {
                const monitorBlock = $('<div>');
                $('#monitor').append(monitorBlock);
                const srvCall = () => {
                    const svcCallArgs = { reqid: data.reqid };
                    $.post('/poll/', svcCallArgs, (data) => {
                        if (!gameStarted) {
                            return;
                        }
                        if (data.move != 'None') {
                            const cb = function (move) {
                                return function () {
                                    warning('BOT MADE AN INVALID MOVE ' + move + '!');
                                };
                            };
                            let sel = '#info-block-' + board.turn;
                            let html = data.info.replace(/cp -?\d+/g, "<b>$&</b>");
                            html = html.replace(/bestmove \w+/g, "<b>$&</b>");
                            $(sel).append($('<span/>', {
                                class: board.turn + 'info',
                                html: html
                            }));
                            $(sel).scrollTop($(sel)[0].scrollHeight);
                            board.move(data.move, shootLaser, cb(data.move));
                        } else {
                            srvCall();
                        }
                    }, 'json');
                };
                srvCall();
            }, 'json');
            return;
        }
        // human's turn
        $('#move').removeAttr('disabled');
        $('#move-button').removeAttr('disabled');
        $('#undo-button').removeAttr('disabled');
        enableHistoryButtons(false);

        $('#move-button').click(); // attempt to null move just in case

        updateUI();
    });

    function submitCommand(cmd, player) {
        let args = {
            player: player,
            command: cmd
        }
        $.post("/uci/", args, (data) => {
            console.log(data);
            let turn = player == "player1" ? "white" : "black";
            $('#info-block-' + turn).append($('<span/>', {
                class: turn + 'info',
                html: data.output + '\n'
            }));
        }, 'json');
    }

    function handleCommandClick(player) {
        let sel = (player == "player1") ? "#uci-text-p1" : "#uci-text-p2";
        let uci = $(sel).val();
        submitCommand(uci, player);
    }

    // Show warning message
    // The message stays on for 2 seconds, unless stayOn is set
    function warning(message, stayOn) {
        $('#move-text').text(message);
        $('#move-text').removeClass('standard-move-text');
        $('#move-text').addClass('warning-move-text');
        if (!stayOn) {
            setTimeout(function () {
                clearWarning();
            }, 2000);
        }
    }

    function clearWarning() {
        $('#move-text').text('Move');
        $('#move-text').removeClass('warning-move-text');
        $('#move-text').addClass('standard-move-text');
    }

    // Set MOVE button actions
    $('#move-button').click(move);
    $('#undo-button').click(undo);
    $('#cancel-button').click(cancel);
    $('#laser-button').click(shootLaser);

    // Set CLOCK button actions
    $('#clock-start-button').click(() => {
        player1 = $('#bot-name1').val();
        player2 = $('#bot-name2').val();
        board.history = board.history.slice(0, board.historyActiveLen);
        board.historySnapshot = board.historySnapshot.slice(0, board.historyActiveLen + 1);
        enableHistoryButtons(false);
        updateUI();
        gameStarted = true;
        updateTime();
        $('#board-parent').trigger('switchPlayer');
    });
    const stopTime = () => {
        gameStarted = false;
        enableHistoryButtons(true);
        $('#move').attr('disabled', 'disabled');
        $('#move-button').attr('disabled', 'disabled');
        $('#undo-button').attr('disabled', 'disabled');
    };

    const resetTime = () => {
        stopTime();
        board.blacktime = board.DEFAULT_START_TIME;
        board.whitetime = board.DEFAULT_START_TIME;
        $('#whitetime').html(timeToString(board.whitetime));
        $('#blacktime').html(timeToString(board.blacktime));
    };

    $('#clock-stop-button').click(stopTime);
    $('#clock-reset-button').click(resetTime);

    // Set HISTORY button actions
    $('#hist-first-button').click(stepFirst);
    $('#hist-prev-button').click(stepPrev);
    $('#hist-next-button').click(stepNext);
    $('#hist-last-button').click(stepLast);

    // Set REPRESENTATION button actions
    $('#fen-button').click(setBoardFenHist);
    $('#pgn-button').click(setBoardPGN);
    $('#uci-button-p1').click(function() {
        handleCommandClick("player1");
    });
    $('#uci-button-p2').click(function() {
        handleCommandClick("player2");
    });
});

