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


const CANVAS_Z_INDEX = 1000;
var GLOBAL_BOARD;

$(document).ready(function () {
    let board = new Board();
    GLOBAL_BOARD = board;

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

        $('#skittles-delete-zone').on('dragover', function(e) {
            e.preventDefault();
            $(this).css('background-color', 'black');
        });
        $('#skittles-delete-zone').on('dragleave', function(e) {
            e.preventDefault();
            $(this).css('background-color', 'red');
        });
        $('#skittles-delete-zone').on('drop', function(e) {
            e.preventDefault();
            $(this).css('background-color', 'red');
            board.handleDeleteDrop();
            updateUI();
        })

        $('#skittles-add-white')[0].draggable = true;

        $('#skittles-add-white img').on('dragstart', function(e) {
            board.draggingNew = 'white';
        });
        $('#skittles-add-white img').on('dragend', function(e) {
            e.preventDefault();
            board.draggingNew = null;
        });
        $('#skittles-add-black img').on('dragstart', function(e) {
            board.draggingNew = 'black';
        });
        $('#skittles-add-black img').on('dragend', function(e) {
            e.preventDefault();
            board.draggingNew = null;
        });

        $('#skittles-pause').on('click', function(e) {
            if (!board.skittlesState) {
                board.skittlesState = 'play';
            }

            if (board.skittlesState == 'play') {
                board.skittlesState = 'pause';
                $(this).text('Play');
            } else {
                board.skittlesState = 'play';
                $(this).text('Pause');
            }
        });

        // Create board
        board.setBoardFenHist('startpos');
        board.cur_hist_step = 0;
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
        console.log("step to", step);
        console.log(board.history);
        return;
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
            warning('MOVE UNSAFE', true);
        };
        const tryEnableLaserButton = function () {
            return false;
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
        updateUI();
    };

    function cancel() {
        $('#cancel-button').attr('disabled', 'disabled');
        $('#laser-button').attr('disabled', 'disabled');
        $('#laser-button').removeClass('highlight');
        console.log('#laser-button disabled');
        $('#move').removeAttr('disabled');
        $('#move-button').removeAttr('disabled');
        $('#undo-button').removeAttr('disabled');
        $('#clock-stop-button').removeAttr('disabled');
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
        console.log('#laser-button disabled');
        $('#clock-stop-button').removeAttr('disabled');
        $('#clock-reset-button').removeAttr('disabled');
        clearWarning();
        board.commitPotentialMove();
        board.shootLaser();
    }

    function enableHistoryButtons(enabled) {
        if (enabled && !board.historyActiveLen == 0) {
            $('#hist-first-button').removeAttr('disabled');
            $('#hist-prev-button').removeAttr('disabled');
        }
        if (enabled && board.historyActiveLen == board.history.length) {
            $('#hist-next-button').removeAttr('disabled');
            $('#hist-last-button').removeAttr('disabled');
        }
        if (enabled) {
            if (board.findKing('black') === null || board.findKing('white') === null) {
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
        for (let i = 0; i < board.history.length; i++) {
            // turn number
            if (i % 2 == 0) {
                const mv = 1 + Math.floor(i / 2);
                const moveNumItem = $('<div>');
                moveNumItem.html(mv + '.&nbsp;');
                if (i >= board.historyActiveLen) {
                    moveNumItem.addClass('historyInactive');
                }
            }
            // moves
            const historyItem = $('<div>');
            historyItem.text(board.history[i]);
            if (i >= board.historyActiveLen) {
                historyItem.addClass('historyInactive');
            }
        }
        // REP pane
        $('#fen-rep-hist').text(board.getBoardFenHistCurrent());
        $('#fen-rep-nohist').text(board.getBoardFen());

        if (board.historySnapshot.length >= 2) {
            if (board.cur_hist_step > 0) {
                $('#skittles-hist-first-button').removeAttr('disabled');
                $('#skittles-hist-prev-button').removeAttr('disabled');
            } else {
                $('#skittles-hist-first-button').attr('disabled', 'disabled');
                $('#skittles-hist-prev-button').attr('disabled', 'disabled');
            }
            if (board.cur_hist_step <= board.historySnapshot.length - 2) {
                $('#skittles-hist-last-button').removeAttr('disabled');
                $('#skittles-hist-next-button').removeAttr('disabled');
            } else {
                $('#skittles-hist-last-button').attr('disabled', 'disabled');
                $('#skittles-hist-next-button').attr('disabled', 'disabled');
            }
        } else {
            $('#skittles-hist-first-button').attr('disabled', 'disabled');
            $('#skittles-hist-last-button').attr('disabled', 'disabled');
            $('#skittles-hist-prev-button').attr('disabled', 'disabled');
            $('#skittles-hist-next-button').attr('disabled', 'disabled');
        }
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

    function black_toggle_laser() {
        var this_button = document.getElementById("black-show-laser");
        if (this_button.value == "Show Lavender Laser") {
            this_button.value = "Hide Lavender Laser";
            board.shootLaserPreview('black');
        } else {
            this_button.value = "Show Lavender Laser";
            board.clearLaser();
            if (document.getElementById("white-show-laser").value.substring(0,4) == "Hide") {
                board.shootLaserPreview('white');
            }
        }
    }

    function white_toggle_laser() {
        var this_button = document.getElementById("white-show-laser");
        if (this_button.value == "Show Tangerine Laser") {
            this_button.value = "Hide Tangerine Laser";
            board.shootLaserPreview('white');
        } else {
            this_button.value = "Show Tangerine Laser";
            board.clearLaser();
            if (document.getElementById("black-show-laser").value.substring(0,4) == "Hide") {
                board.shootLaserPreview('black');
            }
        }
    }

    function skittles_set_board_state(text) {
        const newboard = new Board();
        if (newboard.setBoardFenHist(text)) {
            newboard.historySnapshot = board.historySnapshot;
            newboard.cur_hist_step = board.cur_hist_step;
            board = newboard;
            board.drawBoard();
            enableHistoryButtons(true);
            $('#move-button').click(); // attempt to null move just in case
            updateUI();
        } else {
            warning('STATE INVALID');
        }
    }

    function skittles_goto_board_step(step) {
        console.log("Go to board step ", step, " / ", board.historySnapshot.length - 1);
        if (step < 0 || board.historySnapshot.length <= step) {
            return;
        }
        board.cur_hist_step = step;
        const text = board.historySnapshot.at(step) + " W";
        skittles_set_board_state(text);
    }

    function skittles_hist_first() {
        skittles_goto_board_step(0);
    }

    function skittles_hist_last() {
        skittles_goto_board_step(board.historySnapshot.length - 1);
    }

    function skittles_hist_prev() {
        skittles_goto_board_step(board.cur_hist_step - 1);
    }

    function skittles_hist_next() {
        skittles_goto_board_step(board.cur_hist_step + 1);
    }

    // Set MOVE button actions
    $('#move-button').click(move);
    $('#undo-button').click(undo);
    $('#cancel-button').click(cancel);
    $('#laser-button').click(shootLaser);

    // Set HISTORY button actions
    $('#hist-first-button').click(stepFirst);
    $('#hist-prev-button').click(stepPrev);
    $('#hist-next-button').click(stepNext);
    $('#hist-last-button').click(stepLast);

    // Set REPRESENTATION button actions
    $('#fen-button').click(setBoardFenHist);
    $('#pgn-button').click(setBoardPGN);
    // Set skittles button actions
    $('#black-show-laser').click(black_toggle_laser);
    $('#white-show-laser').click(white_toggle_laser);
    // $('#skittles-undo-button').click(skittles_undo);
    $('#skittles-hist-first-button').click(skittles_hist_first);
    $('#skittles-hist-last-button').click(skittles_hist_last);
    $('#skittles-hist-prev-button').click(skittles_hist_prev);
    $('#skittles-hist-next-button').click(skittles_hist_next);
});

