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

import  java.util.*;
import  java.text.*;

public class PlayGame implements Runnable
{
    Thread t;

    // static  List<PlayGame> pending = Collections.synchronizedList(new LinkedList<PlayGame>());
    static  List<PlayGame> pending = new LinkedList<PlayGame>();
    private Pattach  pW;
    private Pattach  pB;
    public  Integer  drawMoves;
    private Integer  nMoveDrawRule = 200;          // n-move rule - could be any number
    private String   gameRecord = null;        // san game record
    private int      gameno;
    private Player   white;       // white player
    private Player   black;       // black player
    public  String   event;       // Event
    private long[]   acc = new long[2];        // accumulated time for each player
    private long[]   st = new long[2];         // start time for current move
    private long[]   et = new long[2];         // elapsed time for current move
    private int[]    de = new int[2];          // depth achieved
    private long[]   nd = new long[2];         // nodes
    private String   openLine = new String();  // opening line to play
    private int      ctm = 0;
    private StringBuffer san = new StringBuffer();
    private StringBuffer head = new StringBuffer();
    private String verifierInvoke = null;
    private boolean verbose = false;

    // default depth to use for search if nothing else is specified
    private static final int DEFAULT_DEPTH = 4;

    // maximum number of moves to play from opening book
    private static final int MAX_BOOKMOVES = 10;
    // required minimum moves in opening book lines
    private static final int MIN_BOOKMOVES = 2;

    public void  run()
    {
        String s = null;
        int    mvret = 0;
        StringBuffer lst = new StringBuffer();
        String[]   tok;
        Pattach    z;
        Player     who;

        CVerifier gme = new CVerifier(verifierInvoke);

        int        mn = 0;
        String     mv = null;
        String[]   booklst;
        long       ct = 0;       // for fischer time calculation
        long       mvstogo = 0;  // moves to go
        int        moveclock = nMoveDrawRule;  // count down to zero
    
        Date dNow = new Date();
        SimpleDateFormat ft =
            new SimpleDateFormat ("E yyyy.MM.dd 'at' hh:mm:ss a zzz");

        head.append( "[Event \"" + event + "\"]\n");
        head.append( "[Site \"Local\"]\n");
        head.append( "[Date \"" + ft.format(dNow) + "\"]\n");
        head.append( "[Round \"" + gameno + "\"]\n" );
        head.append( "[White \"" + white.name + "\"]\n" );
        head.append( "[Black \"" + black.name + "\"]\n" );

        lst.append("moves");


        pW = new Pattach(white.invoke);
        pW.snd("uci\n");
        pB = new Pattach(black.invoke);
        pB.snd("uci\n");
        pW.waitfor( "uciok" );
        pB.waitfor( "uciok" );

        // send options to white
        for (String sss:white.options) {
            pW.snd(sss + "\n");
        }
        pW.snd("isready\n");
        pW.waitfor("readyok");

        // send options to black
        for (String sss:black.options) {
            pB.snd(sss + "\n");
        }
        pB.snd("isready\n");
        pB.waitfor("readyok");

        if (openLine == null) {
            // no opening book, use starting position
            booklst = new String[0];
        } else {
            booklst = openLine.split(" ");
            if (booklst.length < MIN_BOOKMOVES) {
                System.out.printf("Too short opening line with " + booklst.length + " moves --ok\n");
            }
        }

        gme.setupPosition("");

        ctm = 0;
        moveclock = nMoveDrawRule;

        while (true) {
            String irr = null;    // irregular move or game loss
            int c = ctm & 1;
            if (c == 1) { z = pB; who = black; } else { who = white; z = pW; }

            if (ctm < Math.min(MAX_BOOKMOVES, booklst.length)) {
                mv = booklst[ctm];
                st[c] = System.nanoTime();
                et[c] = st[c] - 1;  // hack,  -1 means book move
                moveclock = nMoveDrawRule;
            } else {
                s = String.format("position startpos %s\n", lst );
                z.snd(s);

                if (c == 0) {

                    if (white.tcTme[0] != 0) {
                        mvstogo = white.tcMvs[0] - ctm / 2;
                        ct = white.tcTme[0];                // time allocated so far
                        while (mvstogo < 1) {
                            mvstogo += white.tcMvs[1];
                            ct += white.tcTme[1];
                        }
                        ct = ct - acc[c];  // how much time left until next time control?

                        if (ct < 1) {
                            irr = "{Black wins due to time forfeit}";
                        }

                        s = String.format("go time %d movestogo %d\n", ct/1000000, mvstogo );
                    } else if (white.nodedepth != 0) {
                        s = String.format("go nodes %d\n", white.nodedepth);
                    } else if (white.depth != 0) {
                        s = String.format("go depth %d\n", white.depth);
                    } else if (white.fisMain != 0) {
                        // fischer
                        ct = white.fisMain + white.fisInc * (ctm / 2) - acc[c];
                        s = String.format("go time %d inc %d\n",
                                          ct / 1000000, white.fisInc/1000000);
                        if (ct < 1) {
                            irr = "{Black wins due to time forfeit}";
                        }
                    } else { // set the default to search depth if nothing is specified
                        s = String.format("go depth %d\n", DEFAULT_DEPTH);
                    }
                } else {

                    if (black.tcTme[0] != 0) {
                        mvstogo = black.tcMvs[0] - ctm / 2;
                        ct = black.tcTme[0];                // time allocated so far
                        while (mvstogo < 1) {
                            mvstogo += black.tcMvs[1];
                            ct += black.tcTme[1];
                        }
                        ct = ct - acc[c];  // how much time left until next time control?

                        if (ct < 1) {
                            irr = "{White wins due to time forfeit}";
                        }

                        s = String.format("go time %d movestogo %d\n", ct / 1000000, mvstogo );
                    } else if (black.nodedepth != 0) {
                        s = String.format("go nodes %d\n", black.nodedepth);
                    } else if (black.depth != 0) {
                        s = String.format("go depth %d\n", black.depth);
                    } else if (black.fisMain != 0) {
                        // fischer
                        ct = black.fisMain + black.fisInc * (ctm / 2) - acc[c];
                        s = String.format("go time %d inc %d\n", ct / 1000000,
                                          black.fisInc/1000000);
                        if (ct < 1) {
                            irr = "{White wins due to time forfeit}";
                        }
                    } else { // set the default to search depth 4
                        s = String.format("go depth 4\n");
                    }
                }

                // Program begins thinking on next move
                // ------------------------------------
                if (irr == null) {
                    z.snd(s);
                    st[c] = System.nanoTime();
                    s = z.waitfor("bestmove");

                    if (s.equals("hasdied")) {
                        if (c == 1) {
                            irr = "{White wins due to program crash}";
                            pW.snd("quit\n");
                            pB.snd("quit\n");
                            pW.cleanup();
                            pB.cleanup();
                            gme.cleanup();
                            san.append( " " + irr);
                            san.append( " 1-0" );
                            head.append( "[Result \"1-0\"]\n" );
                            Counter.decrement();
                            writePgn( head + "\n" + san + "\n\n", this );
                            return;

                        } else {
                            irr = "{Black wins due to program crash}";
                            pB.snd("quit\n");
                            pW.snd("quit\n");
                            pB.cleanup();
                            pW.cleanup();
                            gme.cleanup();
                            san.append( " " + irr);
                            san.append( " 0-1" );
                            head.append( "[Result \"0-1\"]\n" );
                            Counter.decrement();
                            writePgn( head + "\n" + san + "\n\n", this );

                            return;
                        }

                    } else {
                        if (who.tcTme[0] != 0 && ct < 1) {
                            if (c == 1)
                                irr = "{White wins due to time forfeit}";
                            else
                                irr = "{Black wins due to time forfeit}";
                        }
                        et[c] = System.nanoTime();
                        de[c] = z.depthAchieved();
                        nd[c] = z.nodesAchieved();
                        tok = s.split(" ");
                        mv = tok[1];
                    }
                }
            }


	    if (verbose)
                System.out.printf("lastmove: %s\n", mv);

            if (irr == null) {
                lst.append( " " + mv );
            } else {
                lst.append( " " );
            }

            if ((ctm & 1) == 0) {
                mn = mn + 1;
                if (mn != 1 && ((mn-1) % 5) == 0) {
                    san.append("\n");
                } else {
                    if (mn != 1) san.append(" ");
                }
                san.append( String.format("%d.", mn) );
            }

            if (irr == null) {
                mvret = gme.makeToSan( mv );
		if (verbose)
               	    System.out.printf("%s\n\n", gme.getBoard() );
                if (mvret > 0) {
                    moveclock = nMoveDrawRule;
                } else {
                    moveclock = moveclock - 1;
                }

            } else {
                s = null;
            }

            if (mvret == -1 || irr != null) {
                if (irr != null) {
                    System.out.printf("Time forfeit in game %d by player \"%s\"\n",
                                      gameno, who.name );
                    System.out.printf("continuing ...\n");
                    san.append( " " + irr);
                } else {
                    System.out.println(lst);
                    System.out.printf(" Illegal move in game %d\n", gameno );
                    System.out.println( " {Illegal move |" + mv + "| attempted.} " );
                    san.append( " {Illegal move |" + mv + "| attempted.} " );
                }
                et[c] = st[c] + 1;  // to avoid tripping the hung search detection

                pW.snd("quit\n");
                pB.snd("quit\n");
                pW.cleanup();
                pB.cleanup();
                gme.cleanup();
                if ((ctm & 1) == 1) {
                    san.append( " 1-0" );
                    head.append( "[Result \"1-0\"]\n" );
                } else {
                    san.append( " 0-1" );
                    head.append( "[Result \"0-1\"]\n" );
                }
                Counter.decrement();
                writePgn( head + "\n" + san + "\n\n", this );

                return;
            }

            san.append( " " + mv );

            // could this be a problem?
            // Sometimes linux returns negative high resolution times when they are really short.
            if ( et[c] - st[c] > -1 ) {
                san.append( " {" + (et[c] - st[c]) + " " + de[c] + " " + nd[c] + "}" );
                acc[c] += (et[c] - st[c]);
            } else {
                san.append( " {0 " + de[c] + " " + nd[c] + "}" );
                acc[c] += 0;
            }

            // set status as 0, 1, 2, or 3 = regular, draw, mate (white wins), mate (black wins)
            int status = 0;   // ok

            if (gme.status().equals("draw")) {
                status = 1;
            } else if (gme.status().equals("mate - white wins")) {
                status = 2;
            } else if (gme.status().equals("mate - black wins")) {
                status = 3;
            }

            // hard code draw after some limit
            if (status == 0) {

                // in config file this is "adjdicate = n"
                if (ctm > (drawMoves-1) * 2) {
                    status = 1;
                }

                // n-move draw
                if (moveclock <= 0) {
                    status = 1;
                    System.out.printf("%d move draw detected\n", nMoveDrawRule);
                }
            }

            if (status != 0) {
                et[c] = st[c] + 1;  // to avoid tripping the hung search detection
                pW.snd("quit\n");
                pB.snd("quit\n");
                pW.cleanup();
                pB.cleanup();
                gme.cleanup();
                if (status == 2) {
                    san.append( " 1-0" );
                    head.append( "[Result \"1-0\"]\n" );
                } else if (status == 3) {
                    san.append( " 0-1" );
                    head.append( "[Result \"0-1\"]\n" );
                } else {
                    san.append( " 1/2-1/2" );
                    head.append( "[Result \"1/2-1/2\"]\n" );
                }
                Counter.decrement();   // this is how many cpus are currently busy.
                writePgn( head + "\n" + san + "\n\n", this );

                return;
            }
            
            ctm = ctm + 1;
        }
    }


    // write PGN record(s) to text file in SEQUENCE
    // If next game in line is not complete, then stop
    private static void writePgn( String s, PlayGame g )
    {
        g.gameRecord = s;
        synchronized(Harness.pgnwrite) {
            try {
                Harness.pgnwrite.write(g.gameRecord);
                Harness.pgnwrite.flush();
                synchronized(pending) {
                    for (int i = 0; i < pending.size(); i++) {
                        if (pending.get(i).gameno == g.gameno) {
                            pending.remove(i);
                            break;
                        }
                    }
                }
            } catch(Exception e) {
                System.out.println(e);
            }
        }
    }


    // write all remaining PGN record(s) to text file in SEQUENCE
    // If next game in line is not complete, then stop
    public static void writePgn()
    {
        synchronized(Harness.pgnwrite) {
            synchronized(pending) {
                while (pending.size() > 0) {
                    PlayGame p = pending.get(0);
                    try {
                        Harness.pgnwrite.write(p.gameRecord);
                        Harness.pgnwrite.flush();
                        pending.remove(0);
                    } catch(Exception e) {
                        System.out.println(e);
                    }
                }
            }
        }
    }


    public static void oldest()
    {
        long ct = System.nanoTime();
        int  sz = 0, first = 0, last = 0;
        int  pend = 0;

        synchronized(pending) {
            sz = pending.size();
            if (sz > 0) {
                first = pending.get(0).gameno;
                last = pending.get(sz-1).gameno;
                pend = last - first + 1;
            }
        }
        if (sz > 0) {
            System.out.printf("(");
            System.out.printf("%d-%d", first, last);
            System.out.printf(") %d pending\n", pend);
        } else {
            System.out.printf("0 pending\n");
        }

        // handle timeouts, game forfeit, crashes, etc.
        // checks first play in queue only
        // - if there are others they will eventually get discovered
        sz = 0;
        PlayGame game = null;
        synchronized(pending) {
            sz = pending.size();
            if (sz > 0) {
                game = pending.get(0);
            }
        }
        if (sz != 0) {
            long     et = ct - game.st[game.ctm & 1];
            int      pix = game.ctm & 1;   // player index (0 or 1)
            Player   who;

            if (pix == 0) who = game.white; else who = game.black;

            if (who.depth == 0) {
                // Fischer time control only
                if (who.fisMain != 0L) {
                    if (game.et[pix] <= game.st[pix]) {
                        // time allocated for this move
                        long tr = who.fisMain + who.fisInc * (game.ctm / 2) - game.acc[pix];
                        if (et >= tr) {
                            System.out.printf("Game %d player \"%s\" has exhuasted his time\n",
                                              game.gameno, who.name );
                            System.out.printf("move number = %d\n", 1 + (game.ctm / 2));
                            System.out.printf("total main time = %d\n", who.fisMain);
                            System.out.printf("total time allocated = %d\n",
                                              who.fisMain + who.fisInc * (game.ctm / 2));
                            System.out.printf("total time used before move = %d\n", game.acc[pix]);
                            game.san.append(" { " + who.name + " has exhausted time}\n");
                            System.out.println( game.head + "\n" + game.san + "\n\n" );
                        }
                    }
                }
            }
        }
    }


    public void setWhitePlayer(Player p)
    {
        white = p;
    }

    public void setBlackPlayer(Player p)
    {
        black = p;
    }

    public void setOpening(String s)
    {
        openLine = s;
    }

    public void begin()
    {
        t = new Thread(this);
        t.start();
    }

    public void setGameNumber(Integer n)
    {
        gameno = n;
        synchronized(pending) {
            pending.add(this);
        }
    }

    public PlayGame(String verifierInvoke, boolean verbose)
    {
        this.verifierInvoke = verifierInvoke;
        this.verbose = verbose;
    }


}
