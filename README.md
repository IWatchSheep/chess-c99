# chess-c99

A two-player chess program in C99, built step by step towards a **chess calculator** that measures how a move changes what every piece on the board can do.

The program is `chess.c`. Its history in this repository is the order it was built in: each commit replaced `chess.c` with the next version.

| Version | Commit message starts with | What it adds |
|---|---|---|
| 1 | `Version 1` | Two-player chess with full rules; moves are typed as `e2e4` (promotion `e7e8q`); draw claims, restart, quit |
| 2 | `Version 2` | Select a piece by typing its square (`d2`): the board marks its legal squares, and nothing moves |
| 3 | `Version 3` | After selecting, type a destination (`d4`) to preview it: the BEFORE and AFTER legal squares of every other piece, both sides; nothing moves |
| 4 | `Version 4` | Every move must be selected, previewed, then confirmed with `F`; typed full moves are refused |
| 5 | `Version 5` | For each other piece, the squares it GAINED, LOST and kept (UNCHANGED) |
| 6 | `Version 6` | Calculator stage 3S, practice boards: `fen <FEN>` sets up any possible position (impossible ones are refused with a reason), `fen` prints the current one; everything else is exactly version 5 |
| 7 | `Version 7` | Calculator stage 3A, the fact report: after a preview, `R` lists what the move changes for every piece on both sides (moves lost, gained and given back, pieces that can't move and why, squares the team can no longer reach, protections and attacks made and broken, check, mate-in-one threats, counts); nothing is chosen or scored |
| 8 | `Version 8` | Calculator stage 3B, the reply view: after a preview, `replies` lists every answer the other side has, and whether it stops a mate-in-one threat, with its counts; `replies <move>` shows the full report of one answer; nothing is chosen or scored |
| 9 | `Version 9` | A correction: after a move that ends the game (checkmate or stalemate), `R` no longer lists mate-in-one threats, because there is no next turn |
| 10 | `Version 10` | Calculator stage 3C, the loop: `loop N` looks at every sequence of up to N moves (plies) from the current position and lists every checkmate, and for each of your possible first moves whether a mate is forced or only possible; `trace` shows the report for each move of one line; nothing is chosen or scored |
| 11 | `Version 11` | Calculator stage 3D-1, goals: `goal f7 2` marks every move in your lists IF (the goal square can be reached within the limit) or ELSE; `routes` lists how each piece, alone or together, can reach it, with protection on arrival; results are ordered IF / ELSE IF / ELSE from fewest to most moves; `distance on` orders every move by the squares it travels; only your side moves (hypothetical) |
| 12 | `Version 12` | Calculator stage 3D-2: `routes real 3` tests the goal with the other side answering every move (FORCED or POSSIBLE); `support` lists your next moves that would protect the piece you just previewed; `goal2` measures a second goal from where the first goal's routes end |
| 13 | `Version 13` | Calculator stage 3E, captures: `exchange e5 4` lists every order of captures on one square with what each side took (pawn 1, knight 3, bishop 3, rook 5, queen 9, written down only); `order 4` lists your moves with captures first (greatest piece first), then the rest by distance; `risk on` shows the value of a piece arriving on an attacked goal square |

The current program is version 13. The calculator is planned in `docs/PLAN.md` (plain words) and `docs/CALCULATOR_SPEC.md` (exact); the owner's ideas, in his own words and order, are in `docs/OWNER_IDEAS.md`.

## Build and run

**CLion:** open this folder; CLion reads `CMakeLists.txt` and builds the `chess` program as strict C99. Run it with the green Run button and type into the Run window.

**Command line:**

```sh
gcc -std=c99 -Wall -Wextra -Wpedantic -O2 chess.c -o chess
./chess
```

Widen the console to about 140 columns for the before/after report.

## How to play (version 13)

1. Type the square of one of your pieces, for example `e2`, and press Enter. Its legal squares are listed.
2. Type a destination, for example `e4`. Nothing moves yet: the report shows, for every other piece on both sides, its legal squares BEFORE and AFTER the move, and what it GAINED, LOST and kept.
3. Press `F` and Enter to play the move, `P` to preview another destination, or `N` to choose another piece.

Other commands: `restart`, `quit`, `claim`, `resign`.

**The calculator's report (version 7).** After previewing a move, type `R`. The report lists facts only, one per line, from `CALC BEGIN` to `CALC END`; the normal preview is not changed. For example, after 1.e4 e5 2.Bc4 Nc6, previewing `d1` `h5` and typing `R` shows, among others:

```
ZERO Black P f7 PIN after
LOST Black P f7 f6 f5
MATE1 White h5f7
COUNT Black U1 5 U2 3 U3 2 GAINED 0 RESTORED 0 OPENED 0
```

(the queen pins the f7 pawn, which loses its moves, and White threatens Qxf7 mate). Pieces are named by their square before the move. A move lost earlier in the game that comes back is listed as `RESTORED`, and a lost move is never counted twice. The format is in `docs/CALCULATOR_SPEC.md`, Section 7.

**The reply view (version 8).** After previewing a move, type `replies`. Every legal answer of the other side is listed, one per line, with a label and its counts for both sides. For example, after 1.e4 e5 2.Bc4 Nc6, previewing `d1` `h5` and typing `replies` shows:

```
REPLIES BEGIN d1h5
THREAT White h5f7
REPLY a8b8 LEAVES White U1 4 U2 0 U3 0 GAINED 0 RESTORED 0 OPENED 0 Black U1 5 U2 2 U3 1 GAINED 1 RESTORED 0 OPENED 1
...
REPLY g7g6 STOPS White U1 4 U2 1 U3 0 GAINED 0 RESTORED 0 OPENED 0 Black U1 4 U2 1 U3 1 GAINED 3 RESTORED 2 OPENED 3
...
SUMMARY REPLIES 28 STOPS 8 LEAVES 20 NO-THREAT 0 ALLOWS 0
REPLIES END
```

The labels are: `STOPS` (the threatened mate is gone), `LEAVES` (a mate in one remains), `NO-THREAT` (there was no threat and the answer gives none) and `ALLOWS` (there was no threat, but after this answer the mover has a mate in one). If the move ends the game the view says `NO REPLIES checkmate` or `NO REPLIES stalemate`. Type `replies g7g6` to see the full report of one answer. Nothing is played; press `F` as usual to play your move.

**Goals (version 11).** Type `goal`, a square and a limit of your own moves, for example `goal f7 2` (or several squares, `goal f7 g7 h7 3`, or `goal king 2` for the squares next to the enemy king). While a goal is set, selecting a piece marks each of its moves, and previewing marks every move of your pieces afterwards: `CONDITION IF` (the goal can be reached within the limit, by this piece = `PIECE`, or by any of your pieces = `SIDE`) or `CONDITION ELSE`. Type `routes` for the full list: for each of your pieces the fewest moves to the goal, every shortest route with how many of your pieces protect the square on arrival and how many enemy pieces attack it, and the routes where pieces work together to arrive protected. Results are ordered by moves: the fewest `ORDER IF`, then `ORDER ELSE IF`, the most `ORDER ELSE`; not reached is `NONE`. For example, after 1.e4 e5 2.Bc4 Nc6, `goal f7 4` then `routes` shows Bc4 1 move (unprotected), Qd1 2 moves (via h5 or f3, protected by the bishop), Ng1 3, Nb1 4. Only your side moves here (the other side "passes"), so these are distances, not real games. `goal off` clears the goal.

**Real answers, support, a second goal (version 12).** With a goal set, `routes real 3` (1 to 6 half-moves) tests it with the other side answering with every legal move: each of your first moves is `CONDITION IF FORCED` (you reach the goal whatever they do), `CONDITION IF POSSIBLE k OF r` (still reachable after k of their r answers) or `CONDITION ELSE`. For example, after 1.e4 e5 2.Bc4 Nc6 with `goal f7 2`, `routes real 3` shows that after 3.Qh5 reaching f7 is forced, but arriving there protected is only possible (26 of 28 answers; ...g6 and ...d5 block). After previewing a move, `support` lists your next moves that would protect the piece you just moved (after 1.e4: d3, f3, Nc3, Qg4, Qf3, Qe2, Bd3). `goal2 c4 2` sets a second goal; `routes` then measures it from the current position and from the end of each route to the first goal.

**Captures (version 13).** Type `exchange` and a square with a limit, for example `exchange d5 4`: every order of captures on that square, each side allowed to stop after any capture, with what each side took and its total (pawn 1, knight 3, bishop 3, rook 5, queen 9; written down only, never a score). Each first capture is marked `MATERIAL FORCED NONNEGATIVE` (you come out even or better whatever the other side does), `MATERIAL POSSIBLE NONNEGATIVE` (only in some lines) or `MATERIAL ELSE`; `BOUNDED` means the limit cut a line. `order 4` lists all your moves in one list: captures first (greatest piece first, those that are forced to come out even or better before the others), then the other moves by distance; when there is nothing to capture it says `HEADS DISTANCE`. `risk on` adds `AT-RISK` to the routes. These are measurements, not advice: coming out even in material is not proof a move is good.

Type `distance on` to see, after selecting a piece and after a preview, every move with the number of squares it travels, from the fewest (`ORDER IF`) to the most (`ORDER ELSE`); `distance off` turns it off.

**The loop (version 10).** Type `loop` and a depth, for example `loop 3`. Every sequence of up to 3 moves (plies) from the current position is looked at, nothing skipped. You get every sequence that ends in checkmate, how many positions each ply has, and for each move you could play now: `FORCED` (you can force mate within the depth whatever the other side does), `POSSIBLE k OF r` (a mate can still happen after k of the other side's r answers) or `NOT-FOUND`. For example, from the start `loop 4` finds exactly the 8 ways of Fool's Mate; after 1.e4 e5 2.Bc4 Nc6, `loop 3` shows `FIRST d1h5 POSSIBLE 20 OF 28` (the same 20 answers the reply view marks `LEAVES`). `loop 3 6` also counts the sequences after which the side to move has at least 6 pieces that can't move. Depth 4 takes about a second, depth 5 up to a few minutes.

Type `trace` and a list of moves, for example `trace e2e4 e7e5 f1c4 b8c6 d1h5 g7g6`, to see the `R` report of every move in that line, as if each were played: moves lost earlier and given back show as `RESTORED`. Nothing is played.

**Practice boards (version 6).** Type `fen` and a position in standard chess notation (FEN) to set it up, for example king and queen against king:

```
fen 7k/8/5K2/8/8/8/8/Q7 w - - 0 1
```

Type `fen` alone to print the current position. A position that cannot happen in a real game (two kings of one colour, a pawn on the last rank, the side not to move in check, too many pieces, …) is refused with the reason, and the board stays as it was. A position where the side to move is in check is allowed. The game still starts from the normal position.

## Tests

In CLion: **Run → All CTests** (or `ctest` in the build folder). They are:

- `calc_tests` (`tests/calc_tests.c`): 31 checks of the report against the worked examples of the specification (the whole 1.e4 report line by line; 3.Qh5 with the pin and the one mate threat; 3...g6 with and without the game's history; 3.Nf3), protection versus mobility, capture (with squares denied by the capture), en passant, castling, promotion, each reason a piece can't move, the count-once rule over a lost / given back / lost again sequence, that `R` changes nothing, and (version 9) that a move ending the game has no mate-in-one lines.
- `replies_tests` (`tests/replies_tests.c`): 22 checks of the reply view: Scholar's Mate (28 answers, exactly the 8 that stop the mate), 3.Nf3 (no threat), an answer that allows a mate, a check with one answer, checkmate and stalemate, promotion answers in Q, R, B, N order, the fixed order, counts that agree with the full report, bad input refused, and that nothing changes.
- `loop_tests` (`tests/loop_tests.c`): 28 checks of the loop and trace: perft counts from the start and Kiwipete, the 8 Fool's Mates, the practice boards (forced or not), agreement with the reply view for all 33 first moves of the Scholar position, forced versus possible, stalemates, pieces that can't move, trace equal to `R` with the earlier moves played, bad input, and that nothing changes.
- `goal_tests` (`tests/goal_tests.c`): 34 checks of goals, IF / ELSE and the order: the specification's example E4, IF then ELSE, one piece versus the whole side, sets of squares, protected arrival, blocked and pinned pieces, the king's safety, capture and promotion, the distance order (equal distances, knight, castling, promotion), and that nothing changes.
- `real_tests` (`tests/real_tests.c`): 16 checks of version 12: real-answer goal tests from the Scholar position checked against an independent count for all 33 first moves, a checkmate that ends the line, support moves after 1.e4, 1.d4 and 3.Qh5, the second goal, labels, and that nothing changes.
- `exchange_tests` (`tests/exchange_tests.c`): 21 checks of version 13: values, capture orders with both totals, a valuable capture that turns bad and a small capture that ends well, stopping, limits, en passant, an independent check of the forced results, the combined order and how it switches between captures and distance from one position to the next, AT-RISK, and that nothing changes.
- `unit_tests` (`tests/chess_tests.c`): 63 checks of the FEN setup (21 refusals with their exact reasons, positions in check accepted, round trips, perft from loaded positions) and of following each piece across a move (castling rooks, captures, en passant, promotion).
- `golden_01` … `golden_07` (`tests/golden/`): version 5's complete output on seven fixed input scripts (a game with previews, `P`, `N`, `F`; all four castlings; en passant both ways; promotion to each piece; bad inputs; restart and resign; a threefold-repetition claim). Every later version must print **exactly** the same for the same input; this proves the game itself has not changed.
- `golden_08_report` (`tests/golden_v7/`): version 7's output for a script with ten `R` reports (Scholar's Mate line, the lost / given back / lost again sequence, a promotion, castling, and `R` with nothing to report), so later versions cannot change the report silently.
- `golden_09_replies`, `golden_10_replies_endings` (`tests/golden_v8/`): version 8's output for the reply view (Scholar's Mate, 3.Nf3, one answer in detail, bad input, checkmate, stalemate).
- `golden_11_report_endings` (`tests/golden_v9/`): version 9's `R` report after a stalemate and two checkmates.
- `golden_12_loop`, `golden_13_loop_boards` (`tests/golden_v10/`): version 10's loop and trace output.
- `golden_14_goal`, `golden_15_goal_boards`, `golden_16_distance` (`tests/golden_v11/`): version 11's goals, routes, marks and distance order.
- `golden_17_real`, `golden_18_real_boards` (`tests/golden_v12/`): version 12's real-answer routes, support and second goal.
- `golden_19_exchange`, `golden_20_exchange_scholar` (`tests/golden_v13/`): version 13's exchanges, order and AT-RISK.

## Verified (2026-10-25, versions 1–5; version 6 below)

- Every version and every file in `originals/` and `experiments/` builds with `gcc -std=c99 -Wall -Wextra -Wpedantic -Werror`, and runs under AddressSanitizer and UndefinedBehaviorSanitizer with no report.
- **The chess rules are correct in every version.** The number of legal move sequences ("perft") matches the published values on six standard test positions (start position to depth 4: 197,281; "Kiwipete" to depth 3: 97,862; positions 3 to 6 of the standard set), about 1.5 million positions in all. This covers castling, en passant, promotion, pins, check and checkmate.
- Scholar's Mate (1.e4 e5 2.Bc4 Nc6 3.Qh5 Nf6 4.Qxf7#) ends in checkmate in every version.

**Version 6** builds with `-Werror` and runs clean under the sanitizers; it reproduces all seven version 5 transcripts byte for byte; the six perft counts are unchanged; all 63 unit checks pass. Changing one word of the program's output makes all seven transcript tests fail, so they really check.

**Version 7** builds with `-Werror` and runs clean under the sanitizers; it still reproduces all seven version 5 transcripts byte for byte; the six perft counts are unchanged; all 63 unit checks and all 27 report checks pass. Its report numbers for 1.e4, 3.Qh5, 3...g6 and 3.Nf3 equal those computed independently by `tools/spec_examples/examples.c`.

**Version 8** builds with `-Werror` and runs clean under the sanitizers; all eight earlier transcripts (seven of version 5, one of version 7) are reproduced byte for byte; the six perft counts and the outputs of `tools/spec_examples/` are unchanged; all 63 unit checks, 27 report checks and 22 reply checks pass. On Windows the transcript tests used to fail because the game's `chcp` call (since version 1) used up the input file; that call now runs only in a real console window.

**Version 9** changes only the `R` report after a move that ends the game. All ten earlier transcripts, including version 7's report transcript, are reproduced byte for byte; 14 CTest tests pass (63 unit, 31 report, 22 reply checks, eleven transcripts).

**Version 10** adds the loop and trace. All eleven earlier transcripts are reproduced byte for byte; 17 CTest tests pass (63 unit, 31 report, 22 reply, 28 loop checks, thirteen transcripts). The loop's position counts equal the published perft numbers, including 4,865,609 at 5 plies from the start, where it finds 347 mates; after 1.e4 e5 it finds 2,961 mates at 5 plies, as in the specification's example E8.

**Version 11** adds goals and the distance order. All thirteen earlier transcripts are reproduced byte for byte; 21 CTest tests pass (63 unit, 31 report, 22 reply, 28 loop, 34 goal checks, sixteen transcripts). The route lengths equal the specification's example E4.

**Version 12** adds real-answer goal tests, support moves and the second goal. All sixteen earlier transcripts are reproduced byte for byte; 24 CTest tests pass (63 unit, 31 report, 22 reply, 28 loop, 34 goal, 16 real checks, eighteen transcripts).

**Version 13** adds captures, values and the combined order. All eighteen earlier transcripts are reproduced byte for byte; 27 CTest tests pass (63 unit, 31 report, 22 reply, 28 loop, 34 goal, 16 real, 21 exchange checks, twenty transcripts).

## Code map (optional)

[Graphify](https://github.com/Graphify-Labs/graphify) can draw a map of the functions and who calls whom, on your own PC with no account:

```
graphify extract . --code-only
graphify cluster-only . --no-label
start graphify-out\graph.html
```

`.graphifyignore` leaves out the old copies in `originals/` and `experiments/`, and `graphify-out/` is ignored by Git.

## Other folders

- `plans/`: the plan for the calculator (`CHESS9_NEW_CALCULATOR_PLAN.txt`, the agreed starting point) and an earlier brief. The calculator is built stage by stage from this plan (see `docs/CALCULATOR_SPEC.md`, Section 7).
- `experiments/chess_06_gemini_calculator.c`: an experimental Side A / Side B strategy calculator written with Gemini. It is kept for the record; it is **not** the agreed calculator design.
- `originals/`: the files `chess(1).c` to `chess(10).c` as they were received, for traceability. `chess(9).c` is version 5 and `chess(10).c` is the experiment; the numbered files do not each match one version.
