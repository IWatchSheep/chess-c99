# Independent review for Claude: AI Hologram Version 3, C99

Checked on this Linux host during the evening of 25 October 2026 (America/Vancouver). Compiler: gcc 13.3.0, `gcc -std=c99 -Wall -Wextra -Wpedantic -Werror -O2`. AddressSanitizer and UndefinedBehaviorSanitizer were not available in this run and were not used. The preserved chess file was not modified.

Labels used below:

- **VERIFIED** — observed in the saved source, or reproduced by `review/independent_checks.c` on this host.
- **REPORTED** — stated by an earlier audit. Not re-run here.
- **PROPOSAL** — a future change. Not applied.
- **UNRESOLVED** — the owner has not defined it. Do not invent an executable meaning.
- **INCORRECT** — the earlier claim does not match the source or this run.

This review does not create the Version 3 repository and does not implement a grid, a flowchart, or a patched chess program.

## 1. What was actually reviewed

The upload is a set of transport files, not a compilable project. Files 02 through 18 match the SHA-256 list in file 19. One hundred six preserved spans inside those files matched their declared byte counts and hashes. File 01, the chess source, was not in the upload. It arrived later as a chat paste.

File 00 in the manifest is recorded as empty (`e3b0c442…`, 0 bytes). The delivered file 00 is the 2,113-byte read-first note, SHA-256 `adbc23d4d126e0ee3142b5d2fd62e4db113be6a2c95728414ef49ae264571424`. That is a packaging discrepancy. The note itself was read.

The chat paste used Unix newlines and had no final newline. Restoring CRLF and the final newline produces the manifest identity:

| Item | Result |
| --- | --- |
| Path | `preserved/007_chess.c` |
| Bytes | 236,153 |
| Physical lines | 6,917, each ending in CRLF |
| SHA-256 | `549f15c1e6fa46fe47aaa566f05bce30a62d562ac2433f4a0c626c802814fcc6` |
| Match to file 19 | **VERIFIED** |

No chess rule was edited to force that hash. The only change from the paste is the newline encoding the manifest already described.

Also read, at the level needed for the questions in file 18: the living handoff header and LOG-0026 (file 03), the owner C99 correction and the older C++11 planning text (file 04), the statement-approval and opportunity boundaries (file 06), the grid-audit summary (file 07, source 040), Draft 08’s approval record (file 09), ChatGPT’s syntax-only audit (file 10), Google rounds 1–3 (files 11–17), the original CMake (file 02), and the question list (file 18). Historical C++ bodies in files 05 and 08 were inventoried, not re-certified line by line. `calculator-complete-v14` is not in this package. The v2 grid repository is not in this package.

The archived CMake (file 02, 176 bytes, SHA-256 `fac1666223a505afaabc8de1bf06ca9b70c9a09158bb828d6cd0f5c57291975f`) is:

```cmake
cmake_minimum_required(VERSION 3.20)
project(chess_c99 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
add_executable(chess chess.c)
```

Google’s BLD-01, a duplicate `add_executable(chess 007_chess.c 007_chess.c)`, describes a live CLion file that is not file 02. **UNVERIFIED** against anything shipped here. File 02 itself is a single executable line. **VERIFIED**.

`preserved/007_chess.c` compiles clean with the warning flags above. **VERIFIED**.

## 2. Google’s three rounds

Google’s work improved. Round 1 treated several contract gaps as unconditional defects and proposed a `free` inside `reset_board`. Round 2 quoted the draw functions more carefully but still said a NULL reason crashes immediately. Round 3 correctly narrowed both points. Those corrections should be kept.

Round 3 then overstated what the passing checks mean. Its own ledger says the “172 checks” are one compiler invocation, 64 earlier automated checks, 15 CLI commands, 11 reproducer checks, 75 new harness checks, and 6 crash subprocesses. Sanitizers were unavailable on their Windows MinGW. The v14 golden transcripts were not run. Perft stopped at depth 4 for the start position and earlier for the other positions. Calling that “zero leaks, zero overflows, zero crashes” is **INCORRECT** as a universal claim. It is a report about the paths they executed.

What this host reproduced, separately, is in section 3. Google’s Windows exit codes `0xC0000374` and `0xC0000005` stay **REPORTED**. On this Linux host the same classes of bad pointer crash with signal 11 (SIGSEGV). Do not treat those as the same numeric code.

### Census mistakes that should not be copied forward

`text_add` lexical matches of `\btext_add\s*\(`: **211**, **VERIFIED**. One of those is the definition at line 3166. Call sites: **210**. Round 3 called all 211 “call sites.” That count includes the definition.

Round 3 also said every call produces a line under 120 characters, with a measured maximum of 110. For the ordinary preview `e2e4`, `replies_report` emits this 121-character line:

`REPLY d7d5 NO-THREAT White U1 3 U2 0 U3 0 GAINED 1 RESTORED 0 OPENED 1 Black U1 3 U2 2 U3 1 GAINED 11 RESTORED 0 OPENED 5`

**VERIFIED**. The “maximum 110” claim is **INCORRECT** even for one legal move from the opening. The line is still far below 511. Truncation was not observed on this report. Absence of truncation in general was not proved.

Shallow `Board` copies. Round 3 said 34 copies and named several functions that are not the functions at those lines:

| Line | Round 3 name | Function actually there |
| --- | --- | --- |
| 2179 | `print_board` | `legal_destination_map` |
| 2328 | `print_board_preview` | `list_piece_moves` |
| 2633 | `command_preview` | `show_step2_after` |
| 3310 | `calc_report` | `zero_move_cause` (`calc_report`’s copy is line 3631) |
| 3402 | `replies_report` | `report_mates` (`replies_report` begins at line 3840) |

Line 2034, `Board after_move = {0}`, is a zero-initialization. The alias is the later assignment `after_move = *b` at line 2041.

This source has **33** declarations of the form `Board name = *other` and **3** later assignments that alias `history`: lines 2041, 3830 (`*after = *b` in `position_after`), and 4239 (`board = *b` in `trace_report`). That is 36 aliasing sites. The figure 34 is **INCORRECT**.

`record_position` is called from exactly three places: `reset_board` (line 1527), successful `load_fen` (line 3036), and `main` after F finalizes a move (line 6763). **VERIFIED**. `free(game.history)` at line 6915 is the only release of that buffer. Other `free` calls release move lists and report text. **VERIFIED** by search, not by a sanitizer.

Round 3’s statement that those copies do not call `record_position` or `free` on the shared buffer matches this call graph. **VERIFIED** as a reading of the current program. It is not a promise about a future caller.

`dead_position` returning false for king and two knights versus king was described as a compliance gap. That description is **INCORRECT**. King and two knights versus king is not dead under FIDE; mate is possible. This host confirmed the function returns false for that material and true for king versus king, king and bishop, king and knight, and same-color bishops. See section 3.

## 3. Chess findings

### A. Board memory

`Board` owns `Position *history`, `history_size`, and `history_capacity` (the struct is in section 3 of the source).

`reset_board` (line 1483) fills the squares, sets White to move, clears castling and en passant, zeroes the clocks, sets `history_size` to 0, and calls `record_position`. It does not write `history` or `history_capacity`.

`record_position` (line 1909) allocates 32 entries when `size == capacity`, then doubles. It copies the key with `strcpy` into `Position.key[80]`.

`main` (line 6327) does `Board game = {0}` before the first `reset_board`. Restart calls `reset_board` again. Exit frees `game.history` once. `free(NULL)` is safe if reset never allocated, which it does allocate on the success path.

**VERIFIED** on a zeroed board:

- First reset: capacity 32, size 1, non-NULL history.
- Second reset: same pointer, same capacity, size 1 again.
- Forty further `record_position` calls grow the buffer; the pointer stays valid.
- A second `free` after the pointer is set to NULL does not crash.

**VERIFIED** crash, signal 11: a board filled with the byte `0x5A`, then `reset_board`. `record_position` follows the garbage pointer. This is not a bug in `main`, because `main` zeroes the board. It is a real contract hole. `reset_board` is only safe on storage that is zeroed or that already owns a live history buffer.

A `free` of that same `0x5A` pointer also crashes with signal 11. Putting `free(b->history)` inside `reset_board`, which Round 1 proposed, is unsafe on raw stack memory. **Do not add that free.** Round 2 and Round 3 were right to reject it.

Shallow copy, **VERIFIED** with `e2e4`:

- The parent’s pointer, size, and capacity stay unchanged.
- The copy’s `history` pointer equals the parent’s pointer.
- The copy’s `history_size` becomes 0, because a pawn move clears the size on the `Board` it is given.

`make_move` does not call `record_position`. After `e2e4`, size is 0 until the caller records. `main` does record after F. A caller that only uses `make_move` will not detect repetition. That is the current contract, not a silent CLI bug.

`load_fen` failure returns before `*b` is replaced. **VERIFIED**: a malformed string leaves the pointer, the size, and White-to-move alone. Success copies the existing pointer and capacity into the new board, sets size to 0, replaces `*b`, and records one key. **VERIFIED** for the start FEN, including a round trip through `board_to_fen`.

`load_fen` on a NULL `history` allocates, which is what a zeroed board does. `load_fen` on a dangling pointer left behind after `free` writes into freed memory. Callers must NULL the pointer when they free. The preserved `main` frees only at exit, so this is a reuse rule for tests and for Version 3, not a bug in the interactive loop.

**PROPOSAL**, only after the owner agrees, and only in a copy of the program:

```c
void board_init(Board *b) {
    memset(b, 0, sizeof *b); /* raw storage, once */
    reset_board(b);
}

void board_reset(Board *b) {
    reset_board(b);          /* already initialized; no free */
}

void board_free(Board *b) {
    free(b->history);        /* free(NULL) is safe; a second call is safe */
    b->history = NULL;
    b->history_size = 0;
    b->history_capacity = 0;
}
```

`board_init` must not be called twice: the `memset` drops the old pointer without freeing it. `board_free` must not be called on a board that was never initialized. Document that a copied `Board` does not own `history`. A copy that might record or reset must detach first (`history = NULL`, size 0, capacity 0). The perft helper in the independent checks does that.

Out-of-memory paths in `append_move`, `record_position`, and `text_add` call `exit` without freeing. That is acceptable for this CLI. It should be stated, not described as a proved leak-free library.

### B. Draw pointers

`automatic_draw` (line 2004) writes `*reason` only when it returns true: `"75-move rule"` at halfmove 150, `"fivefold repetition"` at five matching keys, or `"dead position"`. The false return does not touch `*reason`.

`claimable_draw` (line 2031) returns false with no write when an intended move is illegal. Otherwise it may copy the board and `make_move`, then write `"50-move rule"` at halfmove 100 or `"threefold repetition"`. The repetition threshold is 2 when the caller supplied an intended move and 3 otherwise. A pawn move or capture sets the copy’s `history_size` to 0, so an irreversible move cannot claim threefold through old keys. That matches the rule that those moves change the placement.

**VERIFIED**:

- Opening position, NULL reason: both functions return false and do not crash.
- Opening position, non-NULL reason: the pointer is left as the caller set it.
- Halfmove 150, non-NULL: true, `"75-move rule"`.
- Halfmove 100, non-NULL: true, `"50-move rule"`.
- Halfmove 150 or 100 with a NULL reason: signal 11. The crash is on the true path only.
- Knight maneuver `g1f3 g8f6 f3g1 f6g8`, recorded after each `make_move`: two occurrences of the start key are not claimable. The same maneuver again makes the third occurrence claimable as `"threefold repetition"`.

A guard of the form `if (reason) *reason = "..."` on those true paths preserves the boolean result and the string for every current caller. `main` passes `&draw_reason`. The guard is unnecessary inside the preserved monolith. It is the right contract if these functions become a reusable API. `assert(reason)` is the wrong contract: it disappears under `NDEBUG` and still crashes in debug.

`game_over` (line 2068) writes `*stalemate = false` on every call, before it decides anything. A NULL stalemate pointer crashes even in the opening. **VERIFIED**, signal 11. Round 3 under-discussed this pointer.

A NULL draw-reason pointer is safe in the opening, because `automatic_draw` returns false and does not write. The same NULL crashes when the 75-move rule holds and the side to move still has a move. **VERIFIED**, signal 11.

`load_fen` writes `*reason` on every path, including success (`*reason = NULL`) and a missing string. A NULL reason pointer is never safe. **VERIFIED**, signal 11 on `load_fen(b, NULL, NULL)`. Google did not flag this. If the reason argument stays required, say so in the comment. If it becomes optional, every write needs the same `if (reason)` guard, including the success path.

### C. `text_add`

`text_add` (line 3166) formats into `char line[512]` with `vsnprintf`. The write into that array is bounded. A negative `vsnprintf` result returns without appending. The stored length is `strlen(line)`, so a formatted result of 512 or more characters is stored as 511 characters plus a NUL.

**VERIFIED**: 50 and 511 characters are kept; 699 characters of one `%s` are stored as 511.

`text_raw` feeds `text_add` in chunks of at most 255 bytes, so that helper does not cross the limit. `replies_report` line 3861 does not:

```c
text_add(out, "THREAT%s", threats.text + strlen("MATE1"));
```

The `%s` is an entire mate list. Nothing in the type system keeps it under 511. The `e2e4` report did not get close. A position with a very long `MATE1` list could truncate. **PROPOSAL**: leave `007_chess.c` unchanged. Document the 511-character staging limit. Consider a two-pass `va_copy` implementation only after a real report loses text and the owner accepts the extra complexity. The two-pass form is valid C99 and is harder for a beginner to read. Round 1’s patch, which ignored the existing `realloc` path, should not be revived.

`Candidate` fields and `line_after` also truncate into fixed buffers (256 bytes, and 48 for some goal text). That is a separate display limit, not the 512-byte staging buffer.

### D. Rules that were executed here

All of the following passed in `review/independent_checks.c` (99 checks, 0 failures) plus one interactive run of the compiled program.

| Check | Result |
| --- | --- |
| Start perft, depths 1–4 | 20, 400, 8,902, 197,281 |
| Kiwipete, depths 1–3 | 48, 2,039, 97,862 |
| Position 3, depths 1–3 | 14, 191, 2,812 |
| Google’s position-4 FEN, depths 1–2 | 6, 264 |
| Position 5, depths 1–2 | 44, 1,486 |
| Position 6, depths 1–2 | 46, 2,079 |
| Fool’s mate `f3 e5 g4 Qh4` | checkmate, White in check, not stalemate |
| `7k/5Q2/6K1/8/8/8/8/8 b - - 0 1` | stalemate |
| `k7/2Q5/K7/8/8/8/8/8 b - - 0 1` | stalemate |
| En passant `d6` from the `e5` / `d5` fixture | legal; d5 pawn removed; key records the ep square |
| Kingside castle through a bishop attack on f1 | illegal |
| Queenside castle in that same position | legal |
| Promotion `a7a8n` | knight on a8 |
| `order_label` on 1, 3, 5 | `IF`, `ELSE IF`, `ELSE` |
| Interactive `e2`, `e4`, `r`, `replies`, `support`, `f`, `fen` | preview, reports, then `... b KQkq e3 0 1` |

The commonly copied position-4 rank `Pp1P2PPP` sums to 9 squares. This loader rejects that FEN with “a rank does not have exactly 8 squares.” **VERIFIED**. Google’s different FEN (`Pp1P2PP` and `R2Q1RK1 w kq`) does load, and its reported 6 / 264 node counts are **VERIFIED** for that string. This review does not rename that string to “standard position 4.”

Perft depths 5 and 6 were not run. The v14 transcript diff was not run. Those remain **UNRESOLVED** as equivalence claims. The node counts above are enough to trust move generation for the positions that were searched.

`order_label` (line 4620) ranks a number against other non-negative numbers. It is a display label inside the calculator. It is not the unapproved opportunity IF/ELSE, and it does not choose a move.

Other limits, from reading the source, not elevated to defects:

- FEN castling with neither `K` nor `Q` sets `white_king_moved`, even if the king piece has not moved. The repetition key stores those rights, so two such positions still compare equal. The program cannot represent “king unmoved, both rooks gone” as different from “king moved.”
- `load_fen` rejects a king attacked by more than two pieces, and rejects a position where the side not to move is in check. Composed diagrams outside that set will not load. Triple check cannot arise from one legal move.
- `dead_position` is not a complete insufficient-material proof. Blocked positions are outside it. King and two knights versus king correctly stays playable.
- `compare_report` uses its depth argument both as the search depth and as the exchange limit. `parse_compare` accepts depths 1 through 5. `LOOP_MAX_DEPTH` is 6. That coupling is real (section 20 of the source). It is not a rules bug.
- `LoopState`, `RouteState`, `ExchangeState`, the trace `Tracker`, and `goal_show_risk` are static or global. The program is a single-threaded CLI.
- PASS mode (`pass_play`) lets the same side move again and clears en passant. The report text already says this is hypothetical.

The interactive two-step path is the supported way to move. Direct four- and five-character moves are rejected on purpose.

## 4. Beginner C99 architecture

Version 3, when the owner asks for it, should be ISO C99, CMake `CMAKE_C_STANDARD 99` with extensions off, and the same warning bar used here. The 2026-10-25 owner sentence in file 04 is the language decision:

> I didn't agree the new repository would be written in c++ I intended to write it all in c99 that a beginner could read.

File 04’s earlier pages that say C++11 are history. Do not edit them. Do not describe Version 3 as C++11.

Two programs, not one rules engine.

**Environment A, chess facts.** Start from this calculator. It measures legal moves, checks, mates, captures, and report lines. It does not choose a move. Keep that. A beginner-readable shape is still one program with named functions and section comments, which this file already has. A large split into many tiny files will not help a beginner. If the file is divided later, four units are enough: board lifetime and FEN, move rules, report text, and `main`. Do that only in a new tree, after the lifecycle tests exist.

Add `board_init`, `board_reset`, and `board_free` as in section 3A. Do not free inside reset. Draw guards only if the functions are published as optional-output APIs. Leave `text_add` alone until a real line exceeds 511 characters.

**Environment B, grid.** Claude’s audit 46 (file 07) describes `ai-hologram-v2` as a 10 by 10 grid, one agent, one object, one destination, actions N/E/S/W, outcomes Moved / Pushed / Blocked / InvalidState, and C0 / C1 agents, written in C++17. That description is **REPORTED**. The grid source is not here, so a C99 grid must not be invented from the summary. Archive the real v2 tree first. Then write a separate beginner C99 program and its own tests. Do not share types with the chess calculator. The flowchart’s C1 is not v2’s C1.

Neither environment implements Draft 08, Draft 12, D3, D5, or the opportunity sheet.

## 5. Flowcharts and open dependencies

These boundaries agree across the living handoff (file 03, including LOG-0026 at the entry dated 2026-10-23) and the statement file (file 06). They are conceptual. They are not C.

- Draft 08 is the reviewed flowchart baseline. Grok’s approval is recorded. It is not an executable diagram. D1 is a conceptual bridge; the two spatial increments are not proved to be the same operation. D2 is resolved only as scope: the four conditions belong inside the Stage B loop, and those four are still undefined.
- Draft 12 Candidate 1 corrected v2 is the owner-accepted conceptual working model. Draft 08 stays the baseline. Draft 11 Corrected Candidate 4.1 is the previous working candidate. The one-passage join is a guess. The external-board account is “might be.” Section 4’s temporal account and the meaning of “moves” stay open.
- The owner’s words for the separate statements are “i approve” (file 06). ChatGPT’s phrase “As the owner, I approve” is not the acceptance. The statements assembly still contains an older “UNAPPROVED” label. That label is history. The approval does not paste the statements into Draft 08 or Draft 12.
- Draft 13 is a review candidate only.
- D3 (what EQUALS and EQUALLY MATCHED do) is **UNRESOLVED**. D5 (when the increment happens and what it changes) is **UNRESOLVED**. D4 (the mathematics) is deferred. Question A is **UNRESOLVED**.
- The opportunity IF/ELSE sheet is separate and unapproved. What “opportunity” means is unstated. `O(m,s)` is withdrawn. Do not answer it.
- The calculator’s `order_label` strings `IF` / `ELSE IF` / `ELSE` are a ranking of numbers. They are not this sheet.
- The state names INIT, OBSERVE, COPY_GUARD, EVIDENCE_OPEN, and TERMINATED belong to a different program’s vocabulary. They were not found in `ai-hologram-v2` by the earlier audit. Do not import them into Version 3 as if they were v2.

King-begins-at-0 is a separate agreement about the conceptual number line. It is not a chess rank. It does not rewrite Draft 07 or Draft 02.

## 6. Contradictions to keep visible

Do not “fix” old documents so that they appear to have always said the later decision.

| Earlier record | Later record | How to read it |
| --- | --- | --- |
| File 04, 2026-10-24, Version 3 in C++11 | File 04 source 001, 2026-10-25, owner: the new repository is C99 a beginner can read | C99 wins for new work. The C++11 pages stay as history. |
| Some v11 wording that Draft 12 was not approved | LOG-0026, owner accepts Draft 12 corrected v2 | Draft 12 corrected v2 is the working conceptual model. The older sentence stays inside the historical copy. |
| Statements assembly says UNAPPROVED | Owner’s “i approve” in file 06 | The statements are approved. The assembly file was not rewritten. The opportunity sheet was not approved. |
| Living handoff once said a standalone Draft 08 file was missing | This package embeds Draft 08 as source 066 in file 09 | “Missing” is outdated for this package. |
| Google Round 1: free history inside reset; NULL reason always crashes | Round 3, and this run | Do not free inside reset. NULL crashes only when the function writes through the pointer. |
| Google Round 3: 34 copies, named `print_board` and `calc_report` at the lines above; all `text_add` lines under 120; zero leaks | This source and this run | 36 alias sites; the names at those lines are wrong; an `e2e4` reply line is 121 characters; leaks were not measured with a sanitizer. |
| Google: king and two knights versus king is a dead-position gap | `dead_position` and FIDE | Not a gap. The function correctly leaves it playable. |
| Calculator `order_label` prints IF / ELSE | Opportunity sheet, unapproved | Different mechanisms. Do not merge them. |
| Flowchart C1 / Experiment 1 / v0.6 | v2 grid’s C0 and C1 agents | Different programs. |

ChatGPT’s audit 072 (file 10) was a source reading plus `gcc -std=c99 -fsyntax-only`. It did not run perft, v14, or sanitizers. Its correction of “complete FIDE engine” and “already verified” still stands. This run goes further on perft and on the lifecycle crashes, and still does not certify the whole of FIDE or the v14 tag.

## 7. Next tests, in order

Do these before any edit to a working copy. The preserved file stays untouched.

1. Diff `preserved/007_chess.c` against the git tag `calculator-complete-v14` when that repository is available. This package cannot do that diff.
2. Re-run `review/independent_checks.c` after any copy is made. The dirty-stack and NULL-pointer cases must stay in subprocesses.
3. Run perft depth 5 and 6 only if the owner wants those numbers. Depth 4 already matches the published start-position count.
4. Run the v14 golden transcripts when the tag is present. Do not treat the 15-command CLI script as a substitute.
5. Build one copy with ASan and UBSan if the host has them. This host did not. “No leak” is not known until that build runs.
6. Add a position whose `MATE1` list is longer than 511 characters before changing `text_add`. If no such position appears in real reports, leave the function.
7. When the v2 grid repository is in hand, archive it, then write grid tests from that source. Do not write them from audit 46 alone.

## 8. Plan for Claude

1. Read this file, then file 03’s header and LOG-0026, then file 04 source 001. Stop if a later owner note contradicts them.
2. Do not create the Version 3 repository, do not open a patch against `007_chess.c`, and do not rewrite historical documents.
3. Treat Google’s 172 checks as **REPORTED**. Treat section 3 of this file as **VERIFIED** on one Linux host. Where they differ, prefer this file’s source lines and the independent program.
4. If the owner approves a lifecycle experiment: copy the chess source, add `board_init` / `board_reset` / `board_free` exactly as specified in section 3A, and run the independent checks plus a test that `board_init` is not used on a live board. Do not free inside `board_reset`.
5. If the owner approves optional draw pointers: add `if (reason)` only on the true-path writes in `automatic_draw` and `claimable_draw`. Decide explicitly whether `game_over`’s stalemate pointer and `load_fen`’s reason pointer are required or optional. Test NULL and non-NULL. The strings for non-NULL callers must stay `"75-move rule"`, `"fivefold repetition"`, `"dead position"`, `"50-move rule"`, and `"threefold repetition"`.
6. Do not replace `text_add` in the same change. It is a separate decision.
7. Do not implement D3, D5, Question A, the opportunity sheet, `O(m,s)`, or a shared chess/grid rules engine.
8. Do not start the C99 grid until the real v2 tree is archived and the owner says to reimplement it.
9. Keep `order_label` described as a numeric ranking. Do not cite it as the opportunity mechanism.

## 9. Questions that need the owner

1. May a later experiment add `board_init`, `board_reset`, and `board_free` in a copy, leaving `preserved/007_chess.c` unchanged?
2. Should `automatic_draw` and `claimable_draw` accept a NULL reason, or must callers always pass a pointer? The same question, separately, for `game_over`’s stalemate pointer and for `load_fen`’s reason pointer.
3. Should `text_add` stay on the 511-character staging buffer until a real report loses text?
4. Is the single beginner file still the preferred shape of the calculator, or is a four-file split wanted?
5. Where is `ai-hologram-v2`, and is the archive already made? This package does not contain it.
6. Is `calculator-complete-v14` the tag that must match this 236,153-byte file? The manifest hash matches. The tag itself was not compared.
7. Question A, D3, D5, the meaning of “opportunity,” and the one-passage join stay with the owner. No implementation should close them.

## How to repeat the checks

From the repository root, the preserved file and the harness:

```bash
gcc -std=c99 -Wall -Wextra -Wpedantic -Werror -O2 \
    -o /tmp/chess-checks preserved/007_chess.c
gcc -std=c99 -Wall -Wextra -Wpedantic -Werror -O2 \
    -o /tmp/chess-checks-suite review/independent_checks.c
/tmp/chess-checks-suite
```

The suite includes the preserved source and renames its `main`. A non-zero status means a failed check. Child modes of the suite are expected to die with a signal; the parent records that.
