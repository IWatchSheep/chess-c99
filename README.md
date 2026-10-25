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

The current program is version 7. The calculator is planned in `docs/PLAN.md` (plain words) and `docs/CALCULATOR_SPEC.md` (exact).

## Build and run

**CLion:** open this folder; CLion reads `CMakeLists.txt` and builds the `chess` program as strict C99. Run it with the green Run button and type into the Run window.

**Command line:**

```sh
gcc -std=c99 -Wall -Wextra -Wpedantic -O2 chess.c -o chess
./chess
```

Widen the console to about 140 columns for the before/after report.

## How to play (version 7)

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

**Practice boards (version 6).** Type `fen` and a position in standard chess notation (FEN) to set it up, for example king and queen against king:

```
fen 7k/8/5K2/8/8/8/8/Q7 w - - 0 1
```

Type `fen` alone to print the current position. A position that cannot happen in a real game (two kings of one colour, a pawn on the last rank, the side not to move in check, too many pieces, …) is refused with the reason, and the board stays as it was. A position where the side to move is in check is allowed. The game still starts from the normal position.

## Tests

In CLion: **Run → All CTests** (or `ctest` in the build folder). They are:

- `calc_tests` (`tests/calc_tests.c`): 27 checks of the report against the worked examples of the specification (the whole 1.e4 report line by line; 3.Qh5 with the pin and the one mate threat; 3...g6 with and without the game's history; 3.Nf3), protection versus mobility, capture (with squares denied by the capture), en passant, castling, promotion, each reason a piece can't move, the count-once rule over a lost / given back / lost again sequence, and that `R` changes nothing.
- `unit_tests` (`tests/chess_tests.c`): 63 checks of the FEN setup (21 refusals with their exact reasons, positions in check accepted, round trips, perft from loaded positions) and of following each piece across a move (castling rooks, captures, en passant, promotion).
- `golden_01` … `golden_07` (`tests/golden/`): version 5's complete output on seven fixed input scripts (a game with previews, `P`, `N`, `F`; all four castlings; en passant both ways; promotion to each piece; bad inputs; restart and resign; a threefold-repetition claim). Every later version must print **exactly** the same for the same input; this proves the game itself has not changed.
- `golden_08_report` (`tests/golden_v7/`): version 7's output for a script with ten `R` reports (Scholar's Mate line, the lost / given back / lost again sequence, a promotion, castling, and `R` with nothing to report), so later versions cannot change the report silently.

## Verified (2026-10-25, versions 1–5; version 6 below)

- Every version and every file in `originals/` and `experiments/` builds with `gcc -std=c99 -Wall -Wextra -Wpedantic -Werror`, and runs under AddressSanitizer and UndefinedBehaviorSanitizer with no report.
- **The chess rules are correct in every version.** The number of legal move sequences ("perft") matches the published values on six standard test positions (start position to depth 4: 197,281; "Kiwipete" to depth 3: 97,862; positions 3 to 6 of the standard set), about 1.5 million positions in all. This covers castling, en passant, promotion, pins, check and checkmate.
- Scholar's Mate (1.e4 e5 2.Bc4 Nc6 3.Qh5 Nf6 4.Qxf7#) ends in checkmate in every version.

**Version 6** builds with `-Werror` and runs clean under the sanitizers; it reproduces all seven version 5 transcripts byte for byte; the six perft counts are unchanged; all 63 unit checks pass. Changing one word of the program's output makes all seven transcript tests fail, so they really check.

**Version 7** builds with `-Werror` and runs clean under the sanitizers; it still reproduces all seven version 5 transcripts byte for byte; the six perft counts are unchanged; all 63 unit checks and all 27 report checks pass. Its report numbers for 1.e4, 3.Qh5, 3...g6 and 3.Nf3 equal those computed independently by `tools/spec_examples/examples.c`.

## Other folders

- `plans/`: the plan for the calculator (`CHESS9_NEW_CALCULATOR_PLAN.txt`, the agreed starting point) and an earlier brief. **The calculator is still at the planning stage.**
- `experiments/chess_06_gemini_calculator.c`: an experimental Side A / Side B strategy calculator written with Gemini. It is kept for the record; it is **not** the agreed calculator design.
- `originals/`: the files `chess(1).c` to `chess(10).c` as they were received, for traceability. `chess(9).c` is version 5 and `chess(10).c` is the experiment; the numbered files do not each match one version.
