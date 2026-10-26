# Chess calculator — specification v2.0

| | |
|---|---|
| **Project** | `chess-c99` |
| **Builds on** | Version 5 (`chess.c`, commit `f685113`): selection, preview, BEFORE/AFTER lists, GAINED/LOST/UNCHANGED, F to finalize |
| **Starting plan** | `plans/CHESS9_NEW_CALCULATOR_PLAN.txt` (ChatGPT, 2026-10-24). This document answers its Section 9 questions and makes its Sections 3–7 exact. The same plan in plain words, with the owner's original text, is `docs/PLAN.md`. |
| **Status** | v1.1 **approved in principle by ChatGPT** (review 03: "Design direction approved. Implementation details above should be incorporated before coding Stage 3A."). v1.2 incorporates those details (Sections 3, 4, 5b, 7, 8). The owner agreed that the report appears only on `R`. ChatGPT approved v1.2 (review 05: "APPROVED — Stage 3S may begin"). Stage 3S was implemented as version 6 and approved (review 07: "Stage 3S is approved. Please proceed with Stage 3A only"). Stage 3A was implemented as version 7 and approved (ChatGPT, review 09: "Claude already has my approval to begin Stage 3B"). Stage 3B was implemented as version 8 and approved (review 11: "Stage 3B is APPROVED. Proceed with Stage 3C."). Version 9 made the MATE1 correction asked for in review 11 and was approved (review 13: "Version 9 is approved with no required changes."). Stage 3C was implemented as version 10 and approved (review 15: "Stage 3C is APPROVED without required changes."). The 3D design was approved with clarifications and split into 3D-1 and 3D-2 (review 16); the owner's IF / ELSE IF / ELSE order was approved as two views (review 17). Stage 3D-1 was implemented as version 11 and approved (review 21: "Stage 3D-1 is APPROVED without required changes."). Stage 3D-2 was implemented as version 12 and approved (review 23: "Stage 3D-2 is APPROVED without required changes."). The 3E design, with the owner's ideas 5–8, was approved with changes (reviews 25, 27 and 29 in `docs/reviews/`). Stage 3E was implemented as version 13 and approved (review 29, file `docs/reviews/30`: "Stage 3E, Version 13, is APPROVED without required changes."). The 3F design was approved with clarifications (review 39); Stage 3F was implemented as version 14 and approved (review 41: "STAGE 3F, VERSION 14: APPROVED."). **All planned stages 3A–3F are complete.** Version 15 (2026-10-26) is a beginner-readable rewrite of the same program with no change of behaviour or output (all version 14 tests pass unchanged; `docs/reviews/45_…`); this specification applies to it unchanged. Further development begins with a separately reviewed proposal; the owner's ideas and the flowchart's open questions stay recorded for future research. A separate research cross-reference with the AI Hologram flowchart is in `docs/FLOWCHART_CROSS_REFERENCE.md` (hypotheses only). |
| **Authority for intent** | The owner's own words, quoted in Section 1. |

## 1. What the owner asked for (his words)

> "make the chess game show the piece selected, and the squares available to it. If you select a square you don't go there, but you see how it affects all the other pieces squares they can go to. Then you repeat this for the other side [...] then you go to the square you want by confirming it."

Done: versions 2–5.

> "then there can be a calculator that [...] the shortest route to a goal [...] the distance of two or more side by side or protective pieces is shorter than one piece or less pieces [...] The calculator measures the possible moves and arranges them like this so it says if you move here then the follow up piece with this width or value measure is going to be coming up beside or protectively with you."

> "if there is a step where the sides lists after a move have pieces that can't move, now you balance that can't move list so that multiple versions of many pieces can't move isn't repeated and this goes for counting the other sides pieces that if moved = many pieces can't move [...] here is the calculation for queens scholar which means there is enough pieces can move to block the queen and bishop. Then that counts the way the lists can't move after the enemy moves, that that's defensive, now count for offensive, repeat this but offensively to the enemy."

> "what goal is of course a question [...] if from some square the goal square, then what would be the calculator to another second goal square."

**The TRUE/FALSE motion and the IF / ELSE IF / ELSE order (2026-10-25, verbatim, in order).** Asked whether a goal should be one square or a set of squares:

> "using this is a question of two different logical points, neither solution is false because the solution is neither one it is the function of motion between them with the result of if the motion returned true or false its an if else condition. Because your following my plan so this logical question builds on my plan too so the answer is build them both then use the motion to test true and if false go to false go to if if if falis go to else, you see?"

> "this implies a logic presented to every piece as a result of moving to a square, that movement being a true or false motion. Then the result of having multiple options is every one of those available options has its own true and false motion as well. then you limit this by having a limit to the number of loops you do this so you can capture chess strategies you see?"

> "you know how there was a initial piece and then that being selected presented multiple options. then is there a third list presented to choose from for a true or false and then you select one of those then this changes how the pieces lists move from unchanged because the piece hasn't moved to after the piece moved to a list of true or false options each of the moves available after the pieces moves listed."

> "instead of wording it true false use if for true and else for false thank you"

> "several if conditions is a else if listing the greatest to least with else being the least."

> "the number of moves must make it greater. the less it moves compared to the more it moves, one square = if, 2 squares = else if, 3 squared = else if 4 squares = else."

> "go ask chatgpt write up the idea that after the list is written with how possible moves changed the number of moves possible is listed in order with a value given to every move with the least number of squares moved = if condition, the increment the number of squares moved = else if condition, to the maximum number of squares moved = else."

The owner's later ideas 5 to 8 (the capture-value order, the combined order, captures first, a trade as a summation over several moves) are recorded word for word, with his whole list in his order, in `docs/OWNER_IDEAS.md`.

The interpretations (Claude's design in `docs/reviews/16` and `18`, ChatGPT's reviews 16 and 17 in `docs/reviews/17` and `19`) are kept apart from these words; Section 7 ("3D-1 in detail") is the result.

## 2. Principles

1. **Facts first, judgement later.** The first stages only *measure*: exact sets and counts computed from the rules. No stage before 3F chooses a move or adds weights.
2. **The rules are version 5's.** Every measurement uses version 5's legal-move code. It is verified: its perft counts match the published values on six standard positions (README).
3. **Every number is reproducible.** Each worked example in Section 8 was computed by a program in `tools/spec_examples/` (`examples.c`, `loops.c`, `ladder.c`, `small.c`), each including `chess.c` unchanged; their outputs are the `*_output.txt` files beside them.
4. **Nothing is summed across different units.** Pieces, (piece, square) pairs and squares are counted separately (Section 4).
5. **Preview never changes the real game.** The calculator works on copies of the board, as version 5's preview does.

## 3. Definitions

**Position B.** A board, the side to move, castling rights and the en passant square, as in version 5's `Board`.

**Ply.** One move by one side (a half-move). **All depths are in plies.** "Mate within 3 plies" for White to move means White moves, Black replies, White mates. Plain-language text may add the number of one side's moves in brackets.

**Piece identity.** A piece is named by its colour, type and square in B (for example *White B c4*). Across a move m from B to B' every piece of B is in exactly one of these cases, handled explicitly:
- **unchanged square** — the same square in B';
- **the moved piece** — from m's start square to its end square;
- **the castling rook** — h1 → f1, a1 → d1, h8 → f8 or a8 → d8 when m is a castling move;
- **captured** — on m's end square, or the en passant pawn beside it; it has no square in B';
- **promoted** — the moving pawn, which keeps its identity with the new type (for example *White P e7 → Q e8*).
Version 5's `resolve_piece_after_move` gives the first, third and fourth cases; the calculator adds the second and fifth.

**L(B, p) — legal destinations.** The squares piece p may legally move to in B, **evaluated as if p's side were to move** (version 5's `legal_destination_map`). It includes captures, excludes squares holding a friendly piece, and respects pins and check.

**A(B, p) — controlled squares.** The squares p attacks *or protects* in B: version 5's `piece_attacks`. Pawns control only their two forward diagonals; sliding pieces control up to and including the first occupied square in each direction; a king controls its eight neighbours. A(B, p) includes squares holding friendly pieces and ignores pins (a pinned piece still controls squares).
*L and A are different on purpose:* a pawn controls its diagonals but moves straight; a rook protects the knight beside it but cannot move there.

**Protection link p → q.** p and q have the same colour, q is not a king, and q's square is in A(B, p). (A king cannot be captured, so "protecting the king" has no meaning; the king *can* protect other pieces.)

**Attack link p → e.** e is an enemy piece and e's square is in A(B, p). Attacks on the enemy king are reported as **check**.

**Zero-move piece.** A piece p with L(B, p) empty. Its **cause** is reported, because the causes are not equivalent:
- **BLOCKED** — p has no pseudo-legal move at all (every square it could go to is off the board, holds a friendly piece, or is not reachable by its movement);
- **CHECK** — p has pseudo-legal moves, none is legal, and p's side is in check;
- **PIN** — p has pseudo-legal moves, none is legal, and p's side is not in check (every move would expose its king).
Example (computed): a pawn e4 facing a pawn e5 is BLOCKED; a knight d2 between its king e1 and a bishop b4 is PIN; a rook a1 whose king e1 is checked by a queen e5 is CHECK.

**Hypothetical pass.** In some practice investigations one side "passes" (does not move). **A pass is not a legal chess move.** Every result that uses one is labelled *hypothetical (pass)*. Every claim about defence in a real game uses legal replies only.

**Team reach R(B, side).** The union of L(B, p) over all pieces of that side: the set of squares the side can move *some* piece to.

**Goal.** A square (or set of squares) chosen from outside, by the user or a test. It is a parameter, not always the enemy king.

**Mate-in-one threat.** In position B, side S *threatens mate in one* if, with S made the side to move, S has a legal move after which the other side is in check and has no legal move.

## 4. The three "can't move" counts

The owner's "pieces that can't move" covers three different things. They are counted separately and **never added together**:

| Unit | Name | Counts | Each counted once per |
|---|---|---|---|
| U1 | zero-move pieces | pieces p with L(B, p) empty | piece |
| U2 | lost moves | pairs (p, s) with s in L(B, p) and s not in L(B', p) | (piece, square) pair |
| U3 | denied squares | squares in R(B, side) that are not in R(B', side) | square |

The reverse of U2 is **gained moves** (pairs gained); the reverse of U3 is **opened squares**.

**Which pieces each count covers.**
- U2 and gained moves cover every piece present in both B and B', **including the moved piece** (its pairs are compared between its start and end squares) and the castling rook. This differs from version 5's Step 2 list, which leaves the moved piece out; the Step 2 list itself is unchanged.
- **Captured pieces are reported separately** (a `CAPTURED` line) and are **not** counted in U1 or U2.
- U3 is a team fact and keeps squares lost through a capture, but each denied square is labelled **by capture** when, in B, only pieces that are captured by m could reach it. So a capture never changes the counts silently.
- A promoted pawn is compared as one piece: its pairs before (as a pawn) against its pairs after (as the new piece).

**"Balance so it isn't repeated" — the rule.** For a sequence of positions B0 → B1 → … → Bk, transition t (from B(t−1) to Bt) is charged only with what changes *at that transition*:
- **lost at t** = pairs (or squares) present in B(t−1) and absent in Bt;
- **restored at t** = pairs absent in B(t−1) and present in Bt, that were lost at an earlier transition;
- **gained at t** = pairs present in Bt and absent in B(t−1), not lost earlier.

So a pair still missing is never charged twice, and a pair **lost, restored, then lost again** is charged at each of the two losing transitions, as two separate events (tested in 3A, Section 7). In words:
- for a sequence B → B' → B'', the losses of B → B' and of B' → B'' are reported separately, each against the position just before it;
- a pair lost in B → B' and still missing in B'' is **not** charged again in B' → B'';
- a pair lost in B → B' and back in B'' is reported as **restored** by that reply;
- a square denied to the team counts once, however many pieces lost it.

Which unit each sentence means:
- "pieces that can't move" → **U1** (whole pieces frozen);
- "if moved = many pieces can't move" (one enemy move restricting many of ours) → **U2**, listed by piece and square, with **U3** as the team view;
- "enough pieces can move to block the queen and bishop" → not a count of frozen pieces at all, but the **defence test** of Section 5: does some legal reply stop the threat?

## 5. Defence and offence (one move, one reply)

For a candidate move m by side S in position B, with B' the position after m:

**The move's own report** (both sides): U1, U2 (pairs lost and gained), U3 (squares denied and opened), protection links formed and broken, attack links formed and broken, checks.

**Offence (S's view):** in B', S's mate-in-one threats, S's legal captures (as if S moved again), and the enemy pieces newly attacked or newly unprotected.

**Defence (the reply view):** for every legal reply r of the other side, B'' = B' after r:
- whether S still threatens mate in one in B'' (the reply **stops** or **leaves** the threat);
- the U2 and U3 change of B' → B'' for both sides, with restored pairs;
- protection links of the replying side formed and broken.

The replies are listed **all**, sorted into "stops every mate-in-one threat" and "leaves one", with their square names. The calculator does not choose among them.

**Roles reversed.** "Now count for offensive" is the same report made from the other side's view.

## 5b. The loop (several moves ahead)

The owner (2026-10-25): "step 3 can be a snapshot or loop, now however many loops then you can see other strategies besides scholars mate".

**Snapshot** = Section 5: one move, then every reply. **Loop of depth N** = the same, repeated: the tree of every legal sequence of N plies (half-moves) from position B, with no move skipped.

At every node of the tree the Section 5 measurements are made against the position just before it (charge-once rule of Section 4: a loss is charged at the ply where it happens, a pair still missing later is not charged again, a pair that comes back is reported as restored).

The loop reports, without choosing a move:
- **every sequence ending in checkmate** within N plies, with the mated side;
- for a first move m: whether a mate is **forced** or only **possible**, defined exactly:
  - **possible within N plies**: at least one legal sequence of at most N plies from B reaches checkmate of the defender;
  - **forced within N plies** (side S to move): N ≥ 1 and S has a legal move m such that either m gives checkmate, or N ≥ 3 and *for every* legal reply r there is a mate forced for S within N − 2 plies from the position after m and r. S may choose a *different* continuation after each reply. Counting how many branches contain a mate is evidence, never proof of a forced mate;
  - the report gives "forced within N plies", "possible in k of r replies", or "not found within N plies";
- sequences after which the other side has at least k zero-move pieces (k given by the user).

**Cost.** The number of nodes grows by about 30 per ply (from the start: 20, 400, 8,902, 197,281, 4,865,609 for N = 1 to 5). Stage 3C has no pruning: depth is the user's choice, and the report says the depth searched; when the limit is reached without a result, it says so (`LIMIT N`) instead of implying that nothing exists. Pruning or move ordering is a separate later decision, because it changes what can be found.

## 5c. Method: practice boards first (owner, 2026-10-25)

The loop is developed and tested on **practice boards** with few pieces before the full board, because the tree is small and every result can be checked by hand; pieces are then added back one at a time ("build-up ladder"). The math logic is read from how the measurements of Sections 4–5 change along a known strategy (Scholar's Mate), then checked on other strategies (Fool's Mate) and on sequences that do not end in mate, before it is trusted.

This needs one new ability before 3A: **position setup from text** (FEN), with its legality checks (exactly one king per side, the side not to move not in check, pawns not on the first or last rank, castling and en passant fields consistent).

## 6. Routes to a goal, and "width"

**Route length (one piece).** The smallest number of moves of that piece alone to reach goal G, the other side passing (*hypothetical (pass)*: this measures distance, not a real line of play) and the other pieces staying where they are. Reported per piece. This is the "one long piece" route.

**Arrival support.** When the piece arrives on G: the number of friendly pieces that protect G (protection links to the arriving piece) and of enemy pieces that attack G.

**Coordinated route ("width").** A sequence of moves of two or more friendly pieces after which a piece stands on G **protected** by at least one other friendly piece. Its length is the number of own moves.

**"Shorter" is not one number.** Routes are compared on a **vector**, never a weighted sum:
(own moves to arrive, friendly protectors of G on arrival, enemy attackers of G on arrival, material at risk on arrival).
Route X is *better than* route Y only if it is no worse in every component and better in at least one. Otherwise both are reported. This answers ChatGPT's question 2: the plies, the exchanges and the support stay separate.

**"Coming up beside or protectively with you"** (ChatGPT's question 3): after our move m puts piece p somewhere, a **support follow-up** is a later own move f of another piece q after which q → p is a protection link. The calculator lists, for m, every one-move support follow-up (q, f) and the support it adds. "Beside" alone (adjacent squares) is not a chess fact and is not used.

**Second goal G2.** The same measurements from the position reached at G1 (or from B, if G1 is not reached), so the change from G1 to G2 can be traced.

## 7. Stages

Each stage is a separate new version of `chess.c`, built only after the stage before passes its tests, with the owner's approval. Version 5 stays as it is in the history.

| Stage | Adds | Tests (Section 8) |
|---|---|---|
| **3S** | Position setup from a FEN line (practice boards), with the legality checks of Section 5c; the normal start stays the default | FEN round trip of the E1–E8 positions; illegal FENs refused |
| **3A** | The relation extractor, no scoring: for a previewed move, for both sides, U1/U2/U3 with names, gained/opened, protection and attack links formed and broken, checks, mate-in-one threats. Shown as a text block after the existing preview, every line machine-readable | E1, E2, E3 (move report), E5 |
| **3B** | The reply view: for the previewed move, every legal reply, sorted into stops / leaves the threat, with the B' → B'' changes and restored pairs | E3 (replies), E3b |
| **3C** | The loop of Section 5b: depth N chosen by the user, every sequence, mates found, forced / possible, no pruning; run on practice boards first (5c) | E6, E7, E8, E3 (replies) |
| **3D** | Goals: route length per piece, arrival support, one-move support follow-ups, G1 then G2 | E4 |
| **3E** | Exchange sequences on one square, captures in order, with a stated depth limit and conventional piece values *logged only* (P 1, N 3, B 3, R 5, Q 9) | to be written with 3E |
| **3F** | Only after 3A–3E: comparing candidate moves (`compare`), with the owner's LOOP-1 / LOOP-2 views in place of the Side A / Side B experiment (owner's idea 9, review 39). No result is assumed | see "3F in detail" |

**3A in detail — the report.** Printed only when the user types `R` after a preview (recommended by ChatGPT and Claude, agreed by the owner), so the existing preview output stays exactly as in version 5. The report is **deterministic** (the same position and move always give the same bytes) and **machine-readable**: one fact per line, each line a keyword followed by fields separated by single spaces.

```
CALC BEGIN <move>                      move in coordinates, e.g. d1h5, e7e8q
MOVED <side> <type> <from> <to>
CASTLE <side> <rook-from> <rook-to>     only for castling
CAPTURED <side> <type> <square>         the captured piece, if any (en passant: the pawn's square)
PROMOTED <side> <from> <to> <new-type>  only for promotion
ZERO <side> <type> <square> <BLOCKED|CHECK|PIN> <before|after|both>
LOST <side> <type> <square> <dest> ...  U2: one line per piece, destinations listed;
                                        <square> is the piece's square before the move (its identity)
GAINED <side> <type> <square> <dest> ...   new pairs never lost before
RESTORED <side> <type> <square> <dest> ... pairs lost at an earlier finalized move, now back
DENIED <side> <square> [by-capture]     U3: one line per square
OPENED <side> <square>
PROTECT+ <side> <type> <square> <type> <square>   link formed (protector, protected)
PROTECT- <side> <type> <square> <type> <square>   link broken
ATTACK+ <side> <type> <square> <type> <square>    attacker, attacked enemy
ATTACK- <side> <type> <square> <type> <square>
CHECK <side>                            the side whose king is attacked after the move
MATE1 <side> <move> ...                 that side's mate-in-one moves after the move (threats);
                                        none when the move ends the game (v1.5)
COUNT <side> U1 <n> U2 <n> U3 <n> GAINED <n> RESTORED <n> OPENED <n>   (U1 = zero-move pieces after the move)
CALC END
```

**Fixed order:** White's lines before Black's within each keyword; keywords in the order above; pieces and squares from rank 8 to rank 1, and file a to h within a rank; destinations in the same order. A link line is ordered by the first piece's square, then the second's, in the position where the link exists (after the move for `+`, before it for `-`).

**A move that ends the game (v1.5).** When the move is checkmate or stalemate (the side to move afterwards has no legal move), the report has no `MATE1` lines: there is no further turn in which to threaten anything. All other lines are reported as usual (for a mate: `CHECK` and the `ZERO … CHECK` lines). This matches the reply view's `NO REPLIES` without a `THREAT` line (ChatGPT review 11, point 2). Before version 9, `MATE1` was computed as if the mover could move again and listed meaningless moves.

**Names and memory (v1.3).** In the report every piece is named by its type and its square **before the move** (`Q d1` for the queen moving from d1; a pawn about to promote is still `P`). Behind the names, each piece has a permanent number from the start of tracking (the normal start, `restart`, or `fen`), carried across every finalized move with `follow_piece` (castling rook, captures, en passant, promotion). The numbers hold the memory of the charge-once rule: when a move is finalized with `F`, every pair it removed is remembered as lost; a later move that brings the pair back reports it as `RESTORED` (not `GAINED`), and a pair that stays missing is never charged again. `R` itself changes nothing (the board, the turn, the history and the memory are left as they were). Squares are written `e4`, sides `White`/`Black`, types `P N B R Q K`. **The report contains facts only.** Strategy conclusions (better, worse, should) never appear in 3A–3C; when later stages draw a conclusion it is printed on lines starting `CONCLUSION`, kept apart from the facts.

**Tests before and with 3A.**
- **Regression (the game is unchanged):** before 3S, version 5 is run on fixed input scripts (a full game with previews, `P`, `N` and `F`; castling on both sides; en passant; promotion to each piece; illegal inputs; `claim`, `resign`, `restart`) and its complete output is saved as golden transcripts. Every later version must reproduce them **byte for byte** for the same input (they never type `R`).
- **Report tests:** the 3A report for E1, E2, E3 and E5, line by line; a capture (`CAPTURED`, `DENIED … by-capture`), en passant, castling (`CASTLE`) and promotion (`PROMOTED`); each zero-move cause (`BLOCKED`, `CHECK`, `PIN`).
- **Charge-once test:** a sequence in which a pair is lost, restored, then lost again: reported as two separate `LOST` events and one restoration, never as a repeated charge for the pair while it stays missing.
- **Preview invariants:** after `R`, the real board, the turn and the history are unchanged; `F` still plays the previewed move exactly once.

**3B in detail — the reply view (v1.4).** After a preview, typing `replies` lists every legal reply of the other side; typing `replies <move>` (for example `replies g7g6`, or `replies e2e1q` for a promotion) prints the full 3A report of that one reply. Like `R`, it changes nothing: the board, the turn, the history and the memory stay as they were, and the existing preview output is unchanged. Without a preview the program says so.

```
REPLIES BEGIN <move>                     the previewed move m
THREAT <side> <move> ...                 the mover's mate-in-one moves after m (as MATE1), or
THREAT <side> none                       if m makes no such threat
NO REPLIES <checkmate|stalemate>         only when m ends the game (then there is no THREAT line)
REPLY <reply> <label> White U1 <n> U2 <n> U3 <n> GAINED <n> RESTORED <n> OPENED <n> Black U1 <n> ... OPENED <n>
SUMMARY REPLIES <n> STOPS <n> LEAVES <n> NO-THREAT <n> ALLOWS <n>
REPLIES END
```

- **Labels** (about the mover's mate-in-one moves, after m and the reply): `STOPS` m threatened mate and after the reply there is none; `LEAVES` m threatened mate and at least one remains; `NO-THREAT` m threatened no mate and the reply gives none; `ALLOWS` m threatened no mate but after the reply the mover has one. The labels are facts about mate-in-one only, not advice.
- **Counts** on each `REPLY` line are exactly the `COUNT` lines of `replies <reply>`: the reply's changes from B' (after m) to B'', **with m treated as finalized**, so a pair m took away that the reply gives back is `RESTORED`. Example: E3's 3.Qh5 then 3...g6 shows Black `GAINED 3 RESTORED 2`.
- **Order:** replies sorted by start square, then end square (rank 8 to 1, file a to h), then promotion piece Q, R, B, N.
- **No reply is chosen, scored or played.**

**3F in detail — comparing candidate moves (v2.0).** `compare N m1 m2 [... m6]`: 2 to 6 different legal moves of the side to move S, written like `e2e4` or `e7e8q`, one depth N (1 to 5) used for every bounded part (loop plies, exchange captures, REAL goal plies), and the active goal if any. So every candidate is measured with the same side, the same predicates, the same goal and the same limit; a move of the other side is refused (no opposite perspectives), and there is only one limit (no different search limits). Nothing is chosen or played; the board, turn, history and memory are unchanged.

- **Facts per candidate**, taken only from the existing reports (no second implementation of a chess rule): `R` (COUNT for both sides, CHECK, MATE1 for each side, the number of PROTECT± and ATTACK± lines), `replies` (THREAT, SUMMARY; `GAME-OVER checkmate|stalemate` when the move ends the game), `loop N` (the candidate's FIRST line: FORCED / POSSIBLE k OF r / NOT-FOUND, MATED White n Black n), `routes real N` (each goal square and condition), and 3E (CAPTURE, MATERIAL with BOUNDED, VALUE-ORDER, SQUARES-ORDER, GROUP of `order N`). **Missing values are printed `-`** (for example MATERIAL and VALUE-ORDER for a move that captures nothing; REPLIES for a move that ends the game) and are never treated as zero.
- **Pair lines** `COMPARE <m1> <m2> <fact> <value1> | <value2> <result>`:
  - raw facts (counts, links, check, threats, mated counts, capture): `SAME` / `DIFFERENT` only — no direction for counts or raw values;
  - condition strengths that an approved stage already defines, for S, with the same predicate, target and depth: loop mate `FORCED` > `POSSIBLE` > `NOT-FOUND`; REAL goal `CONDITION IF FORCED` > `IF POSSIBLE` > `ELSE`; `MATERIAL FORCED NONNEGATIVE` > `POSSIBLE NONNEGATIVE`: `STRONGER-CONDITION <m>` / `TIE` / `NOT-COMPARABLE` (a missing value on either side). k OF r is kept in the values; NOT-FOUND is never read as "cannot be reached"; the other side's mates stay in the MATED counts, apart;
  - the owner's IF chains (VALUE-ORDER: captured value over all of S's captures, greatest = IF; SQUARES-ORDER: squares over all of S's moves, fewest = IF; GROUP: the line of `order N`, group 1 FORCED, then group 1 POSSIBLE, then group 2, with the label there): `EARLIER-IN-ORDER <m>` / `SAME-ORDER-LEVEL` / `NOT-COMPARABLE`. An order is a measurement, never evidence of strategic superiority, and takes no part in AGREEMENT.
- **`AGREEMENT <m1> <m2>`**, from the condition strengths only: `ALL-AHEAD <m>` (at least one strict difference and none for the other candidate), `MIXED` (strengths favour different candidates: an incomparable pair, kept), `ALL-TIE` (all applicable strengths tie), `NO-DIRECTED-COMPARISON` (none applicable). Because the loop result is computed for every candidate, NO-DIRECTED-COMPARISON does not occur in this version; it is kept for completeness.
- **Views (the owner's idea 9, an experiment):** `VIEW LOOP-1 SQUARES d ORDER <label> STOP <square> EMPTY|CAPTURE`, `VIEW LOOP-2 VALUE v|none ORDER <label>|none MATERIAL <class>|-`, `NEURON-STATE UNDEFINED`; per pair `VIEWS <m1> <m2> LOOP-1 <m|SAME-LEVEL|NOT-COMPARABLE> LOOP-2 <...> SAME-VIEW|DIFFERENT-VIEW|NOT-COMPARABLE` (LOOP-2 uses the value order, then the material class when the values are at the same level). The views are the owner's proposed correspondence with the flowchart's Loop 1 and Loop 2, a hypothesis only (review 39); no chess fact is labelled sustenance, hunger or thirst. The two readings of "the increment the queen for loop" are in `docs/FLOWCHART_CROSS_REFERENCE.md`, row 15.
- Frame: `COMPARE BEGIN <S> DEPTH <N> CANDIDATES …`, two NOTE lines, `GOAL … REAL DEPTH N` when a goal is active, the CANDIDATE blocks in the typed order, the PAIR blocks (m1–m2, m1–m3, …), `COMPARE END`.
- **Correction found while building 3F:** in version 13, `order N` printed `ORDER IF` for every capture of group 1, because the labelling helper skips negative numbers and group 1 passed the values as negative numbers. Fixed (9 − value); the earlier transcripts did not contain a group with different values, so they are unchanged; a test (X22) now checks it.

**Tests with 3F:** a higher capture value with a weaker material condition (Qxd5 rook POSSIBLE vs Qxb3 pawn FORCED; VALUE-ORDER earlier for the rook; DIFFERENT-VIEW); SAME-ORDER-LEVEL; ALL-TIE; MIXED (a mate possible after Qxd5 vs material forced after exd5); ALL-AHEAD (Qxf7#; GAME-OVER and a missing REPLIES); E3 (POSSIBLE 20 OF 28 vs 21 OF 31: a TIE in strength, k OF r kept; NOT-COMPARABLE material); a goal; promotion, castling, en passant; refused input; invariants and determinism. Transcripts `tests/golden_v14`.

**3E in detail — values, exchanges, the combined order (v1.9).** Values are the conventional P 1, N 3, B 3, R 5, Q 9 (a king is never captured), **logged only**: no weighted score, no move chosen. A nonnegative material result is not proof that a move is good (ChatGPT review 28). An en passant capture counts as a capture on the square the pawn moves to.

- **`exchange <square> N`** (N = 1 to 8 captures): every sequence of legal captures on one square, the side to move (S) first, sides alternating; after each capture the side to move may stop, so every prefix is a sequence. A line without a capture is never a trade (`NO CAPTURES`).

```
EXCHANGE BEGIN <S> <square> LIMIT <N>
NOTE values P 1, N 3, B 3, R 5, Q 9 are logged only, not a score; ...
SEQUENCE <captures> WHITE-TOOK <types|-> BLACK-TOOK <types|-> MATERIAL White <sum> Black <sum> [LIMIT]
FIRST <capture> TAKES <type> VALUE <v> MATERIAL FORCED NONNEGATIVE | MATERIAL POSSIBLE NONNEGATIVE | MATERIAL ELSE ORDER IF|ELSE IF|ELSE [BOUNDED]
SUMMARY SEQUENCES <n>
LIMIT <N>                        when some line was cut by the limit
EXCHANGE END
```

- **The material classes** (idea 8: "a summation over multiple moves"), for each first capture c of S, with M = value S took − value S lost along the line, in the bounded same-square exchange model, with the existential / universal structure of 3C: the side to move chooses between stopping and each legal capture. `MATERIAL FORCED NONNEGATIVE`: S can keep M ≥ 0 at every turn of its own whatever the other side does (the other side may stop or recapture; S may stop). `MATERIAL POSSIBLE NONNEGATIVE`: M ≥ 0 at the end of some line, not forced. `MATERIAL ELSE`: never. `BOUNDED`: the limit cut a line below c, so the class says nothing beyond the limit. In this model the other side may always decline to recapture, so a first capture is at least POSSIBLE; `MATERIAL ELSE` cannot occur for a first capture here (stated, and tested).
- **`order N`** (ideas 7 and 8; N = the exchange limit, 1 to 8): one combined list of every legal move of the side to move, rebuilt in every position. `HEADS CAPTURE` when group 1 is not empty, else `HEADS DISTANCE` (back to the distance list).
  - **Group 1:** captures that are MATERIAL FORCED NONNEGATIVE (listed first), then POSSIBLE NONNEGATIVE. Since a first capture is never MATERIAL ELSE in this model, group 1 holds every legal capture, as review 27 asks ("use all legal captures as the capture group"); the class shows which are forced, and no capture is called a good trade. Each subgroup is ordered by the value taken, greatest first (`ORDER IF`, then `ELSE IF`, smallest `ELSE`), then by fewer squares, then the fixed move order: `GROUP 1 CAPTURE <move> TAKES <type> VALUE <v> SQUARES <d> <class> ORDER <label> [BOUNDED]`.
  - **Group 2:** every other move (and any capture of class ELSE, marked), ordered by squares travelled (fewest = `IF`): `GROUP 2 DISTANCE <move> SQUARES <d> ORDER <label> [TAKES …]`.
  - It is an order of listed facts, never "the best move".
- **`risk on` / `risk off` / `risk`**: with it on, every `ROUTE` line of `routes` ends with `AT-RISK <v>` = the arriving piece's value when at least one enemy piece attacks the goal square on arrival (attacked material, not a guaranteed loss), else `AT-RISK 0`. Off (the default), the version 11 report is unchanged.
- **Scope:** captures and recaptures on one square only. The owner's broader "multiple loops of the different pieces" (trades over several squares and moves) is kept for later stages.

**Tests with 3E:** values; every capture order on a square with both totals; a valuable first capture turning bad (Qxd5 exd5: POSSIBLE) and a low first capture ending 10 to 1 (FORCED); the side to move's choice to stop; the limit (BOUNDED, LIMIT); no captures; en passant; Bxf7+ Kxf7; FORCED checked against an independent minimax for every capture of five positions; no first capture ELSE; the combined order (captures first, FORCED before POSSIBLE, equal values by fewer squares; every legal move once; HEADS DISTANCE without captures; captures disappearing and reappearing over successive positions; NO MOVES); AT-RISK on and off; refused input; invariants. Transcripts `tests/golden_v13`.

**3D-2 in detail — REAL goals, support, the second goal (v1.8).** Every report says PASS (hypothetical: only S moves) or REAL (the other side answers with every legal move); the two are never mixed.

- **`routes real N`** (N = 1 to 6 plies, the user's choice; a goal must be set): for each goal square and each condition (REACH, PROTECTED), every first move m of S gets `CONDITION IF FORCED` (m itself satisfies the condition — `MOVES 1` — or N ≥ 3 and after every legal reply the condition is forced within N − 2 plies, S choosing a possibly different continuation after each reply), `CONDITION IF POSSIBLE k OF r` (not forced, but after k of the r replies the condition can still be satisfied within the depth), or `CONDITION ELSE`. The condition is tested right after each S move. A sequence ends at checkmate or stalemate (a move that mates on another square is ELSE: the game is over). Counting is never proof of forcing.

```
ROUTES BEGIN <S> GOAL <squares> DEPTH <N> REAL
NOTE REAL: <other> answers with every legal move; FORCED = reached whatever <other> does
GOAL <sq> REACH | GOAL <sq> PROTECTED        then one FIRST line per first move:
FIRST <m> CONDITION IF FORCED [MOVES 1] | CONDITION IF POSSIBLE <k> OF <r> | CONDITION ELSE
RESULT <sq> REACH|PROTECTED CONDITION IF FORCED | CONDITION IF POSSIBLE | CONDITION ELSE
SQUARE <sq> REACH <result> PROTECTED <result>   each square
FIRST-IF REACH|PROTECTED <sq> FORCED|POSSIBLE | FIRST-IF … none (ELSE)   the owner's if / else if order
LIMIT <N>                                       when some result is not FORCED
NO MOVES checkmate|stalemate                    instead, when S has no legal move
ROUTES END
```

- **`support`** after a preview of m (PASS: S moves again): every legal move f of another S piece q after which q protects the moved piece and did not before (a new protection link, Section 6 "support follow-up"). `SUPPORT BEGIN <m> PASS`, `NOTE …`, `SUPPORT NOW <type> <sq> PROTECTORS <n>`, then `FOLLOW <f> <type> <from> PROTECTS <type> <sq> PROTECTORS <n>` (protectors after f; a follow-up can block another protector's line, and the count shows it), `SUMMARY FOLLOWS <n>`, `SUPPORT END`.
- **The second goal G2** (Section 6): `goal2 <squares> <L>`, `goal2`, `goal2 off`. With a first goal and a second goal set, `routes` adds, before `ROUTES END`: `SECOND GOAL <squares> LIMIT <L> PASS AFTER <sq>` (the first goal square that is IF, or `none (ELSE)`), then `FROM START SQUARE <sq> REACH … PROTECTED …` measured from the current position, and the same line `FROM <route moves> SQUARE …` from the end of every route to that square that is not DOMINATED (each route once), then `SECOND END`. So the change from G1 to G2 can be traced. PASS only.

**Tests with 3D-2:** REAL depth 1 and 3 from the E3 position (Qh5: reaching f7 FORCED, arriving protected POSSIBLE 26 of 28); all 33 first moves and both conditions checked against an independent count; a mate on another square ends the sequence (ELSE); NO MOVES; support after 1.e4 (exactly 7 moves), 1.d4 (existing links not listed), 3.Qh5 (Bxf7, Be2, g4); the second goal (c4: 0 from the start, 1 after Bxf7, 0 after Qxf7), routes not repeated, first goal not reached; labels, invariants, determinism, refused input. Transcripts `tests/golden_v12`.

**3D-1 in detail — goals, IF / ELSE and the order (v1.7).** PASS mode only (the other side passes; hypothetical, never a real sequence of turns). REAL mode, support follow-ups and the second goal are Stage 3D-2, not yet authorized.

- **Goal:** `goal <squares> <L>`: one square or an ordered set (`goal f7 2`, `goal f7 g7 h7 3`), or `goal king <L>` (the squares next to the enemy king, in board order); `L` = own moves, 1 to 4. `goal` shows it, `goal off` clears it. With no goal and no distance view active, every output is exactly as in version 10.
- **The condition** C on goal square G: REACH = a piece of the side to move (S) stands on G; PROTECTED = REACH and another S piece protects G (Section 3 link). Printed as **CONDITION IF** (holds within the limit) or **CONDITION ELSE** (does not) — the owner's words for true / false. IF never means "good", ELSE never "bad", and IF is not a forced result.
- **PIECE** = this piece alone (the others stay); **SIDE** = any S pieces. Both are reported, separately.
- **PASS legality:** every own move is a legal move with S to move: no jumping over pieces, the own king never left in check, captures remove the captured piece, a king is never captured; en passant is cleared after each own move (the other side passed).
- **The ORDER chain (reading A, route length):** among the move counts present for a goal square, the smallest is `ORDER IF`, each count between is `ORDER ELSE IF`, the largest `ORDER ELSE`; with one distinct count only `IF`; not reached within the limit is `ORDER NONE`. Equal counts get the same label. The order is a measurement, never "better".
- **The distance order (reading B):** `distance on` / `distance off` / `distance`. After selecting a piece, and after a preview (every piece of both sides in the AFTER position, White first, board order), each legal move is listed with `DISTANCE d` = max(|rows|, |files|) = the squares it travels, ordered from the fewest (`ORDER IF`) through each larger distance (`ORDER ELSE IF`) to the most (`ORDER ELSE`), same labelling rules as above. A knight move is marked `JUMP` (one move, displacement 2). Castling is marked `CASTLE <rook from><rook to>` (the king's 2 squares are its distance). A piece of the side not to move is listed as if it were its turn. A piece with no legal move: `NO-MOVES`.

```
routes   (with a goal active)
ROUTES BEGIN <S> GOAL <squares> LIMIT <L> PASS
NOTE PASS is hypothetical: only <S> moves, <other> passes; not a real sequence of turns
GOAL <sq> NOW White <n> Black <n>               pieces of each side attacking / protecting sq now
PIECE <type> <sq> CONDITION IF MOVES <n> ORDER IF|ELSE IF|ELSE      (each S piece, board order)
PIECE <type> <sq> CONDITION ELSE ORDER NONE
ROUTE PIECE <type> <sq> <moves> OWN <n> PROTECTORS <n> ATTACKERS <n> [DOMINATED]   every shortest route
SIDE REACH CONDITION IF MOVES <n> | SIDE REACH CONDITION ELSE
SIDE PROTECTED CONDITION IF MOVES <n> | SIDE PROTECTED CONDITION ELSE
ROUTE SIDE PROTECTED <moves> OWN <n> PROTECTORS <n> ATTACKERS <n> [DOMINATED]      every shortest protected route
... (the block above for each goal square, in order)
SQUARE <sq> REACH CONDITION IF|ELSE PROTECTED CONDITION IF|ELSE   each square
FIRST-IF REACH <sq> | FIRST-IF REACH none (ELSE)   the first IF in the owner's if / else if order
FIRST-IF PROTECTED <sq> | FIRST-IF PROTECTED none (ELSE)
ROUTES END
```

- **Arrival vector** (Section 6): (OWN moves, PROTECTORS of G, ATTACKERS of G), measured on arrival. A route is `DOMINATED` when another route to the same square is no worse in every component and better in at least one; otherwise both stand (E4: Bxf7 in 1 unprotected and Qh5–f7 in 2 protected are both kept). "Material at risk" needs exchange analysis and is left to 3E.
- **Marks in the lists** (the owner's "third list"), with a goal active: after selecting a piece, `GOAL MARKS …` then `MARK <move> PIECE CONDITION IF <sq> MOVES <n> | PIECE CONDITION ELSE  SIDE CONDITION IF <sq> MOVES <n> | SIDE CONDITION ELSE` for each destination (n counts own moves from now, including this one; the first IF square of the set is shown); after a preview, `GOAL MARKS AFTER <move>` with `REACHED <sq> 1` if the previewed move reaches it, and a `MARK <type> <sq> <move> …` line for every move of every S piece in the AFTER position (S moving again); `GOAL MARKS END`.

**Tests with 3D-1:** E4 (route lengths and the ORDER chain), IF and ELSE, ELSE then IF after a later motion, PIECE versus SIDE (Qe2 PIECE ELSE but SIDE IF), single squares and ordered sets with the first IF, goal king, shortest versus protected arrival and DOMINATED, blocked and pinned pieces, the own king, capture and promotion, the distance order (1–4 squares, equal distances, one distance, no moves, knight, pawn, castling, promotion, nothing added or removed), CONDITION and ORDER separate, invariants, refused input. Transcripts `tests/golden_v11`.

**3C in detail — the loop (v1.6).** `loop N` (N from 1 to 6, chosen by the user; there is no default and no pruning) visits every legal sequence of 1 to N plies from the current position. `loop N K` also counts pieces that can't move. `trace <moves>` reports one sequence. Neither changes the game.

```
LOOP BEGIN <S> DEPTH <N> [ZERO <K>]     S = the side to move now
MATE <ply> <move> ... MATED <side>      every sequence ending in checkmate, in the fixed order
PLY <p> POSITIONS <n> MATED White <n> Black <n> STALEMATES <n> [ZERO <n>]
FIRST <m> FORCED                        S forces mate of the other side within N plies with m
FIRST <m> POSSIBLE <k> OF <r>           not forced; after k of m's r replies a mate is still reachable
FIRST <m> NOT-FOUND                     no mate of the other side within N plies after m
        ... each FIRST line ends with MATED White <n> Black <n> (mates below m, either side)
RESULT <S> FORCED|POSSIBLE|NOT-FOUND    FORCED if some first move is FORCED, else POSSIBLE if some is
LIMIT <N>                               when RESULT is not FORCED: the search stopped at N plies
NO MOVES checkmate|stalemate            instead, when S has no legal move
LOOP END
```

- `POSITIONS` at ply p counts the legal sequences of exactly p plies (the standard "perft" number).
- **Forced** and **possible** are the definitions of Section 5b, for a mate of the other side by S. A mate of S itself is listed (`MATE`, the `MATED` counts) but does not make a first move FORCED or POSSIBLE.
- `ZERO <n>` (with `K`): sequences of that length after which the side to move has at least K pieces with no legal move.
- Only checkmate and stalemate end a sequence; draw claims (threefold repetition, fifty moves, insufficient material) are not applied inside the loop.
- Moves are visited in the order of 3B (start square, end square, promotion Q R B N), so the output is the same every time.
- **Per-node measurements:** the loop does not print the Section 5 measurements for every node (5 plies from the start is 4,865,609 positions). `trace e2e4 e7e5 …` prints the 3A report of each move of one sequence, with the memory carried as if each move were finalized, so the charge-once rule applies along the line (`RESTORED` when a lost pair comes back). `TRACE BEGIN <n>`, then for each move `TRACE PLY <i> <move>` and its report, then `TRACE END`.
- **Time:** depth 4 from the start takes about a second; depth 5 from 30 seconds to a few minutes; depth 6 much longer. The program says so before a search of 5 or 6 plies.

**Tests with 3C:** perft counts (start; Kiwipete); E6 (the 8 Fool's Mates at 4 plies); E8 (K+Q v K not found in 1 ply, forced in 3; K+R v K; the Scholar corner possible but not forced in 5); E3 cross-checked with 3B for all 33 first moves (`POSSIBLE k OF r` = `LEAVES` + `ALLOWS` of `REPLIES`); forced checked independently; stalemate, `NO MOVES`, `ZERO`; `trace` equal to `R` in a game with the earlier moves finalized; invariants; refused input. Transcripts `tests/golden_v10`.

**Tests with 3B:** E3's 28 replies (8 `STOPS`: d8e7 d8f6 d8g5 d8h4 e8e7 g8h6 d7d5 g7g6; 20 `LEAVES`); E3b (no threat); a reply that `ALLOWS` a mate (1.e4 f6 2.d4 g5); a check with one answer (1.e4 f6 2.Qh5+, only g7g6); checkmate and stalemate (`NO REPLIES`, no `THREAT` line); promotion replies in Q, R, B, N order; counts equal to the `COUNT` lines of `replies <reply>`; `replies g7g6` equal to the `R` report after 3.Qh5 is played; refused input; the invariants. Golden transcripts of version 8 (`tests/golden_v8`) fix the whole output.

## 8. Worked examples (exact; computed by the programs in `tools/spec_examples/`)

**E1 — the start position.** Each side: 6 zero-move pieces (White a1, c1, d1, e1, f1, h1); 20 legal (piece, square) pairs; team reach 16 squares; 19 protection links; 2 unprotected non-king pieces (the rooks a1, h1).

**E2 — 1.e4.** White: 2 lost pairs (the moved pawn's old moves e2 → e4, e3), 12 gained (the pawn → e5; Qd1 → h5, g4, f3, e2; Ke1 → e2; Bf1 → a6, b5, c4, d3, e2; Ng1 → e2); 2 denied squares (e4, e3); 5 opened (a6, b5, e5, h5, e2). Black: no change. After the move White has 3 zero-move pieces (a1, c1, h1), 30 pairs, reach 19, 15 links (the moved pawn on e4 is unprotected).

**E3 — Scholar's Mate threat.** Position after 1.e4 e5 2.Bc4 Nc6 (FEN `r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/8/PPPP1PPP/RNBQK1NR w KQkq - 2 3`). White threatens no mate yet.
Candidate **3.Qh5**:
- White: 1 lost pair (the queen's old move to h5, where it now stands), 11 gained (the queen → f7, h7, g6, h6, e5, f5, g5, h4, h3 and back to d1; Ke1 → d1), 1 denied square (h5), 7 opened (h7, g6, h6, e5, f5, g5, d1).
- Black: **3 lost pairs: f7 → f6, f7 → f5, h7 → h5.** The f7 pawn lost its moves because **Qh5 pins it** to the king on e8; h5 is now occupied. 2 denied squares (f5, h5).
- White now threatens mate in one: **exactly 1 move, Qh5xf7#**. f7 is attacked by 2 White pieces (Qh5, Bc4) and defended by 1 (Ke8).
- Black has **28 legal replies. 8 stop every mate-in-one: Qe7, Qf6, Qg5, Qh4, Ke7, Nh6, d5, g6. 20 leave one** (including the natural-looking Nf6, Be7, Nge7, a6, d6, h6).
- After 3...g6: White lost 1 pair (Qh5 → f7, because g6 blocks the line h5–g6–f7); Black lost 1 (the g-pawn's old move to g6) and gained 5 (Bf8 → g7, h6; f7 → f6, f5 again: the pin is gone; and the g-pawn → h5: **it now attacks the queen**). In a game where 3.Qh5 was played (finalized), version 7's report lists f7 → f6, f5 as `RESTORED` (lost at 3.Qh5): `GAINED 3 RESTORED 2`; from the same position set up with `fen` there is no history, so all 5 are `GAINED`, 1 denied (g6), 3 opened (g7, f5, h5); Black's protection links rise from 19 to 20 (f7 → g6 and h7 → g6 formed).
- After 3...Nf6: Qxf7# is still available.

**E3b — the alternative 3.Nf3** from the same position: White lost 8 pairs (f2 → f4, f3; Qd1 → h5, g4, f3; the knight's old moves to f3, h3, e2), gained 8 (Ke1 → g1 castling; the knight → e5 (**attacking Black's e5 pawn**), g5, d4, h4 and back to g1; Rh1 → f1, g1); no mate threat; protection links 14 → 17. Both candidates are reported in the same units; nothing says which is better.

**E4 — goals, from the E3 position, White, the other side passing (hypothetical (pass)).**

| Goal | Qd1 | Bc4 | Ng1 | Nb1 | White pieces attacking the goal now |
|---|---|---|---|---|---|
| f7 | 2 moves | 1 move | 3 | 4 | 1 (Bc4) |
| d5 | 3 moves | 1 move | 3 | 2 | 2 (Bc4, Pe4) |

This is the owner's "one piece versus several" in exact terms: **Bc4 reaches f7 in 1 move alone, unprotected** (it would be captured by the king), while the **coordinated route Qh5 then Qxf7 takes 2 moves and arrives protected by Bc4** — and that arrival is mate. By the vector rule of Section 6 neither route is "better" in every component: one is shorter, the other is supported.

**E5 — protection is not mobility** (to be added to the test set with 3A): a pawn protecting a friendly pawn diagonally, and a rook protecting the piece beside it, while neither can move to that square.

**E6 — the loop finds Fool's Mate** (`tools/spec_examples/loops.c`, output `loops_output.txt`). From the start position, sequences ending in checkmate: 0 within 1, 2 or 3 plies; **exactly 8 at 4 plies**, all Fool's Mate (1.f3 or 1.f4 or 1.g4, then ...e5 or ...e6, the other pawn move, ...Qh4#); **347 at 5 plies**. Both match the published counts for chess. The loop was not told to look for this pattern.

**E7 — the build-up ladder** (`ladder.c`, `ladder_output.txt`). White Ke1, Qd1, Bc4; Black Ke8 and Pf7, then Black's pieces added one at a time; White plays Qh5 and Qxf7 with Black passing (**hypothetical (pass)**: a board investigation, not a legal game). Black king's legal squares before Qxf7 / after Qxf7: Pf7 only 4 / 1 (not mate, escape d8); + Pd7 3 / 1 (not mate); + Qd8 2 / 0 (**checkmate**); + Bf8 1 / 0 (checkmate). Black's own queen on d8 is the blocker that makes the mate; Bf8 is not needed.

**E8 — practice boards** (`small.c`, `small_output.txt`). King and queen vs king (`7k/8/5K2/8/8/8/8/Q7 w`): no mate in 1 ply, forced mate within 3 plies (2 White moves; 50 positions searched). King and rook vs king (`6k1/8/6K1/8/8/8/8/R7 w`): mate in 1 ply (12 positions). The Scholar corner with only Ke1, Qd1, Bc4 vs Ke8, Pf7 (`4k3/5p2/8/8/2B5/8/8/3QK3 w`): **no forced mate within 5 plies (3 White moves)** (34,537 positions; legal replies only, no passes). From the full board after 1.e4 e5, 5 plies: 2,961 sequences end in mate (23,027,889 positions, about 2 minutes), Scholar's Mate among them.

## 9. Answers to ChatGPT's Section 9 questions

1. *Which counting unit does each sentence mean?* Section 4: U1 for "pieces that can't move", U2 (with U3) for "one move makes many pieces unable to move", and the defence test, not a count, for "enough pieces can move to block the queen and bishop". All three are kept.
2. *What is a "shorter route" when the opponent can reply?* Not one number: the vector of Section 6, compared without weights. The first route measurement (3D) assumes the opponent passes and says so; replies are added by the reply view (3B), the loop (3C) and the exchange analysis (3E).
3. *What does "coming up beside or protectively" mean?* A support follow-up: a later move after which another friendly piece protects the moved piece (Section 6). Mere adjacency is not used.
4. *Does Stage 3A match the owner's purpose without Gemini's weighted score?* Yes: 3A only reports sets and counts; no weights, no ranking, no move choice. The Gemini experiment stays in `experiments/`.

## 10. Open questions for the owner

1. ~~Should the 3A report print automatically, or only with `R`?~~ **Decided: only with `R`.** The owner (2026-10-25): "you both agree that's a good idea then I agree."
2. For goals: one square at a time, or a set of squares (for example the squares around the enemy king)?
3. ~~In 3E, should piece values be shown at all, or only the exchange sequence?~~ **Decided (owner's ideas 5–8): shown, logged only, with the capture-value order.**
4. ~~For the loop, what depth should be the default?~~ **Decided in 3C: no default.** The user types the depth (1 to 6); the program warns before 5 or 6 plies.

## Revision history

| Version | Date | Change |
|---|---|---|
| 2.0 | 2026-10-25 | Stage 3E approved (review 29). Stage 3F implemented (version 14): `compare N m1 m2 …` with raw facts, STRONGER-CONDITION / TIE / NOT-COMPARABLE, the owner's IF chains (EARLIER-IN-ORDER / SAME-ORDER-LEVEL), AGREEMENT, the LOOP-1 / LOOP-2 views and NEURON-STATE UNDEFINED; correction of the group-1 value labels of `order N`. |
| 1.9 | 2026-10-25 | Stage 3D-2 approved (review 23). The owner's ideas 5–8 (`docs/OWNER_IDEAS.md`). Stage 3E implemented (version 13): values logged only, `exchange <square> N` with MATERIAL FORCED / POSSIBLE NONNEGATIVE / ELSE and BOUNDED, `order N` (captures first, then the distance list), `risk on` (AT-RISK). |
| 1.8 | 2026-10-25 | Stage 3D-1 approved (review 21). Stage 3D-2 implemented (version 12): `routes real N` (FORCED / POSSIBLE / ELSE per first move, both conditions), `support` (new protection links after a preview), `goal2` (the second goal from the start and from the end of each non-dominated route). |
| 1.7 | 2026-10-25 | Stage 3C approved (review 15). The owner's TRUE/FALSE motion and IF / ELSE IF / ELSE words recorded (Section 1). Stage 3D split (review 16); 3D-1 implemented as version 11: goals and ordered sets, CONDITION IF / ELSE, PIECE and SIDE, PASS routes, arrival vector and DOMINATED, marks in the lists, the ORDER chain by moves and the distance order by squares (review 17). |
| 1.6 | 2026-10-25 | Version 9 approved (review 13). Stage 3C implemented (version 10): `loop N`, `loop N K`, `trace`; their line formats; forced / possible / not found per first move; per-node measurements through `trace`; no default depth. |
| 1.5 | 2026-10-25 | Stage 3B approved (review 11). Version 9: no `MATE1` lines after a move that ends the game (checkmate or stalemate), as asked in review 11; the version 7 transcript is unchanged; new transcript `tests/golden_v9`. |
| 1.4 | 2026-10-25 | Stage 3B implemented (version 8): the reply view (`replies`, `replies <move>`), its line format, the labels `STOPS`/`LEAVES`/`NO-THREAT`/`ALLOWS`, `SUMMARY`, `NO REPLIES` without a `THREAT` line, counts with m treated as finalized, and the fixed reply order. |
| 1.3 | 2026-10-25 | Stage 3A implemented (version 7). Report details made exact from the implementation: `RESTORED` lines and the `RESTORED` count; names by square before the move, with permanent numbers behind them carrying the charge-once memory across finalized moves (ChatGPT review 05, point 2); link-line order; E3's 3...g6 with and without history. |
| 1.2.1 | 2026-10-25 | Stage 3S implemented (version 6). E8's king-and-rook FEN corrected from `6k/8/6K1/...` (only 7 squares in its first rank, found by the new FEN tests) to `6k1/8/6K1/...`; the position the example computed was already the intended one (black king g8), so its result is unchanged. |
| 1.2 | 2026-10-25 | ChatGPT's review 03 (approved in principle) incorporated: plies everywhere; explicit identities for the moved, castling, captured and promoted pieces; zero-move causes BLOCKED/CHECK/PIN; hypothetical passes labelled; captured pieces reported separately and `by-capture` denied squares; the exact charge-once rule per transition (lost, restored, gained; lost again = a new event); the exact definition of forced and possible mates and `LIMIT N`; the deterministic 3A report format; regression (golden transcripts of version 5), report, charge-once and preview-invariant tests; facts kept apart from conclusions. |
| 1.1 | 2026-10-25 | The owner's loop idea added (Section 5b, stage 3C, example E6); his method of practice boards first and the build-up ladder (Section 5c, stage 3S, examples E7, E8); goals, exchanges and comparison move to 3D, 3E, 3F; `docs/PLAN.md` written as the plain-words version of the plan. |
| 1.0 | 2026-10-25 | First draft, written by Claude from the owner's words and ChatGPT's plan; worked examples computed against version 5. |
