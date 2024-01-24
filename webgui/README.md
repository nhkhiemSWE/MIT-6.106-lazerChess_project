SETUP
--------------------------------------------------------------------------------
* Compile `leiserchess` in `player/`
* Start the webserver via:

      $ ./webserver.py

  and leave it running. It should print out `"start Leiserchess at port 5555."`
* In Visual Studio Code, press Ctrl (Cmd on macOS)-Shift-P. In the command window, enter "Forward a Port" and select that option.
* Enter 5555
* In a browser, navigate to http://localhost:5555
* Click 'Start' and enjoy the game! By default, this configuration is human
  versus human. If you select "computer" mode for either player, it defaults to the `../player/leiserchess` binary.
  However, you may specify different bots like this:

      $ python3 webserver.py --player1=path/to/player1binary --player2=path/to/player2binary

* Once you are done, remember to shut down your webserver (Ctrl-C on the
  terminal that you ran python webserver.py).

Sometimes the webserver doesn't die properly, hanging up the port and preventing
a new webserver from starting.  In this case, type

    $ lsof -i :5555

Note the PID of the python job, and type

    $ kill -9 <PID>



PLAYING INSTRUCTIONS
--------------------------------------------------------------------------------
The web interface should be fairly intuitive. Each player takes turns and a turn
ends when you fire the laser.

Movements:
* Each piece can be moved one square at a time simply by doing a drag and drop.
* To rotate a piece simply click on it multiple times until the desired angle of
  rotation is reached.
* The web interface will notify you if you make an invalid move.
* You / Your bot will have 2 minutes to make a move.
    - 2 minute rule is not strictly enforced in this web interface. But for
      autotesting and for the final run of your bot, it WILL be enforced and you
      will lose the game if your bot does not come up with a move within this
      time.
