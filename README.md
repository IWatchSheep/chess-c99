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

The current program is version 5.

## Build and run

**CLion:** open this folder; CLion reads `CMakeLists.txt` and builds the `chess` program as strict C99. Run it with the green Run button and type into the Run window.

**Command line:**

```sh
gcc -std=c99 -Wall -Wextra -Wpedantic -O2 chess.c -o chess
./chess
```

Widen the console to about 140 columns for the before/after report.

## How to play (version 5)

1. Type the square of one of your pieces, for example `e2`, and press Enter. Its legal squares are listed.
2. Type a destination, for example `e4`. Nothing moves yet: the report shows, for every other piece on both sides, its legal squares BEFORE and AFTER the move, and what it GAINED, LOST and kept.
3. Press `F` and Enter to play the move, `P` to preview another destination, or `N` to choose another piece.

Other commands: `restart`, `quit`, `claim`, `resign`.

## Verified (2026-10-25)

- Every version and every file in `originals/` and `experiments/` builds with `gcc -std=c99 -Wall -Wextra -Wpedantic -Werror`, and runs under AddressSanitizer and UndefinedBehaviorSanitizer with no report.
- **The chess rules are correct in every version.** The number of legal move sequences ("perft") matches the published values on six standard test positions (start position to depth 4: 197,281; "Kiwipete" to depth 3: 97,862; positions 3 to 6 of the standard set), about 1.5 million positions in all. This covers castling, en passant, promotion, pins, check and checkmate.
- Scholar's Mate (1.e4 e5 2.Bc4 Nc6 3.Qh5 Nf6 4.Qxf7#) ends in checkmate in every version.

## Other folders

- `plans/`: the plan for the calculator (`CHESS9_NEW_CALCULATOR_PLAN.txt`, the agreed starting point) and an earlier brief. **The calculator is still at the planning stage.**
- `experiments/chess_06_gemini_calculator.c`: an experimental Side A / Side B strategy calculator written with Gemini. It is kept for the record; it is **not** the agreed calculator design.
- `originals/`: the files `chess(1).c` to `chess(10).c` as they were received, for traceability. `chess(9).c` is version 5 and `chess(10).c` is the experiment; the numbered files do not each match one version.
