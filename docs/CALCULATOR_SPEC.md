# Chess calculator — specification v1.1 (for review)

| | |
|---|---|
| **Project** | `chess-c99` |
| **Builds on** | Version 5 (`chess.c`, commit `f685113`): selection, preview, BEFORE/AFTER lists, GAINED/LOST/UNCHANGED, F to finalize |
| **Starting plan** | `plans/CHESS9_NEW_CALCULATOR_PLAN.txt` (ChatGPT, 2026-10-24). This document answers its Section 9 questions and makes its Sections 3–7 exact. The same plan in plain words, with the owner's original text, is `docs/PLAN.md`. |
| **Status** | **Draft for review by ChatGPT and the owner.** No code is written until it is agreed. |
| **Authority for intent** | The owner's own words, quoted in Section 1. |

## 1. What the owner asked for (his words)

> "make the chess game show the piece selected, and the squares available to it. If you select a square you don't go there, but you see how it affects all the other pieces squares they can go to. Then you repeat this for the other side [...] then you go to the square you want by confirming it."

Done: versions 2–5.

> "then there can be a calculator that [...] the shortest route to a goal [...] the distance of two or more side by side or protective pieces is shorter than one piece or less pieces [...] The calculator measures the possible moves and arranges them like this so it says if you move here then the follow up piece with this width or value measure is going to be coming up beside or protectively with you."

> "if there is a step where the sides lists after a move have pieces that can't move, now you balance that can't move list so that multiple versions of many pieces can't move isn't repeated and this goes for counting the other sides pieces that if moved = many pieces can't move [...] here is the calculation for queens scholar which means there is enough pieces can move to block the queen and bishop. Then that counts the way the lists can't move after the enemy moves, that that's defensive, now count for offensive, repeat this but offensively to the enemy."

> "what goal is of course a question [...] if from some square the goal square, then what would be the calculator to another second goal square."

## 2. Principles

1. **Facts first, judgement later.** The first stages only *measure*: exact sets and counts computed from the rules. No stage before 3F chooses a move or adds weights.
2. **The rules are version 5's.** Every measurement uses version 5's legal-move code. It is verified: its perft counts match the published values on six standard positions (README).
3. **Every number is reproducible.** Each worked example in Section 8 was computed by a program in `tools/spec_examples/` (`examples.c`, `loops.c`, `ladder.c`, `small.c`), each including `chess.c` unchanged; their outputs are the `*_output.txt` files beside them.
4. **Nothing is summed across different units.** Pieces, (piece, square) pairs and squares are counted separately (Section 4).
5. **Preview never changes the real game.** The calculator works on copies of the board, as version 5's preview does.

## 3. Definitions

**Position B.** A board, the side to move, castling rights and the en passant square, as in version 5's `Board`.

**Piece identity.** A piece is named by its colour, type and square in B (for example *White B c4*). After a move m, the same piece is found with version 5's `resolve_piece_after_move` (it follows the castling rook and removes captured pieces, including en passant). A promoted pawn keeps its identity with the new type.

**L(B, p) — legal destinations.** The squares piece p may legally move to in B, **evaluated as if p's side were to move** (version 5's `legal_destination_map`). It includes captures, excludes squares holding a friendly piece, and respects pins and check.

**A(B, p) — controlled squares.** The squares p attacks *or protects* in B: version 5's `piece_attacks`. Pawns control only their two forward diagonals; sliding pieces control up to and including the first occupied square in each direction; a king controls its eight neighbours. A(B, p) includes squares holding friendly pieces and ignores pins (a pinned piece still controls squares).
*L and A are different on purpose:* a pawn controls its diagonals but moves straight; a rook protects the knight beside it but cannot move there.

**Protection link p → q.** p and q have the same colour, q is not a king, and q's square is in A(B, p). (A king cannot be captured, so "protecting the king" has no meaning; the king *can* protect other pieces.)

**Attack link p → e.** e is an enemy piece and e's square is in A(B, p). Attacks on the enemy king are reported as **check**.

**Zero-move piece.** A piece p with L(B, p) empty.

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

**"Balance so it isn't repeated" — the rule.** A loss is charged once, at the transition where it happens:
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
- for a first move m: whether a mate (or another outcome) is **forced** — reached against *every* reply — or only **possible** if the other side errs; reported as "forced within N plies", "possible in k of r replies", or "not found within N plies";
- sequences after which the other side has at least k zero-move pieces (k given by the user).

**Cost.** The number of nodes grows by about 30 per ply (from the start: 20, 400, 8,902, 197,281, 4,865,609 for N = 1 to 5). Stage 3C has no pruning: depth is the user's choice, and the report says the depth searched. Pruning or move ordering is a separate later decision, because it changes what can be found.

## 5c. Method: practice boards first (owner, 2026-10-25)

The loop is developed and tested on **practice boards** with few pieces before the full board, because the tree is small and every result can be checked by hand; pieces are then added back one at a time ("build-up ladder"). The math logic is read from how the measurements of Sections 4–5 change along a known strategy (Scholar's Mate), then checked on other strategies (Fool's Mate) and on sequences that do not end in mate, before it is trusted.

This needs one new ability before 3A: **position setup from text** (FEN), with its legality checks (exactly one king per side, the side not to move not in check, pawns not on the first or last rank, castling and en passant fields consistent).

## 6. Routes to a goal, and "width"

**Route length (one piece).** The smallest number of moves of that piece alone to reach goal G, the other side passing and the other pieces staying where they are. Reported per piece. This is the "one long piece" route.

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
| **3F** | Only after 3A–3E: comparing candidate moves, and the Side A / Side B experiment. No result is assumed | to be written with 3F |

**3A in detail.** A new command `R` (report) after a preview, or the report printed automatically under the preview (owner's choice). The block starts with a line `CALC <from><to>` and each line starts with a keyword (`ZERO`, `LOST`, `GAINED`, `DENIED`, `OPENED`, `PROTECT+`, `PROTECT-`, `ATTACK+`, `ATTACK-`, `CHECK`, `MATE1`), the side, and square names, so a program can read it without the board drawing. Squares are listed in one fixed order everywhere: rank 8 to 1, file a to h.

## 8. Worked examples (exact; computed by the programs in `tools/spec_examples/`)

**E1 — the start position.** Each side: 6 zero-move pieces (White a1, c1, d1, e1, f1, h1); 20 legal (piece, square) pairs; team reach 16 squares; 19 protection links; 2 unprotected non-king pieces (the rooks a1, h1).

**E2 — 1.e4.** White: 0 lost pairs, 11 gained (Qd1 → h5, g4, f3, e2; Ke1 → e2; Bf1 → a6, b5, c4, d3, e2; Ng1 → e2); 2 denied squares (e4, e3: the e-pawn's old moves are not counted as lost pairs, because the moved piece is left out, but the team can no longer reach them); 5 opened (a6, b5, e5, h5, e2). Black: no change. After the move White has 3 zero-move pieces (a1, c1, h1), 30 pairs, reach 19, 15 links (the moved pawn on e4 is unprotected).

**E3 — Scholar's Mate threat.** Position after 1.e4 e5 2.Bc4 Nc6 (FEN `r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/8/PPPP1PPP/RNBQK1NR w KQkq - 2 3`). White threatens no mate yet.
Candidate **3.Qh5**:
- White: 1 gained pair (Ke1 → d1), 1 denied square (h5), 7 opened (h7, g6, h6, e5, f5, g5, d1).
- Black: **3 lost pairs: f7 → f6, f7 → f5, h7 → h5.** The f7 pawn lost its moves because **Qh5 pins it** to the king on e8; h5 is now occupied. 2 denied squares (f5, h5).
- White now threatens mate in one: **exactly 1 move, Qh5xf7#**. f7 is attacked by 2 White pieces (Qh5, Bc4) and defended by 1 (Ke8).
- Black has **28 legal replies. 8 stop every mate-in-one: Qe7, Qf6, Qg5, Qh4, Ke7, Nh6, d5, g6. 20 leave one** (including the natural-looking Nf6, Be7, Nge7, a6, d6, h6).
- After 3...g6: White lost 1 pair (Qh5 → f7, because g6 blocks the line h5–g6–f7); Black gained 4 (Bf8 → g7, h6; and f7 → f6, f5 again: the pin is gone), 1 denied (g6), 3 opened (g7, f5, h5); Black's protection links rise from 19 to 20 (f7 → g6 and h7 → g6 formed).
- After 3...Nf6: Qxf7# is still available.

**E3b — the alternative 3.Nf3** from the same position: White lost 5 pairs (f2 → f4, f3; Qd1 → h5, g4, f3), gained 3 (Ke1 → g1 castling, Rh1 → f1, g1); no mate threat; protection links 14 → 17. Both candidates are reported in the same units; nothing says which is better.

**E4 — goals, from the E3 position, White, the other side passing.**

| Goal | Qd1 | Bc4 | Ng1 | Nb1 | White pieces attacking the goal now |
|---|---|---|---|---|---|
| f7 | 2 moves | 1 move | 3 | 4 | 1 (Bc4) |
| d5 | 3 moves | 1 move | 3 | 2 | 2 (Bc4, Pe4) |

This is the owner's "one piece versus several" in exact terms: **Bc4 reaches f7 in 1 move alone, unprotected** (it would be captured by the king), while the **coordinated route Qh5 then Qxf7 takes 2 moves and arrives protected by Bc4** — and that arrival is mate. By the vector rule of Section 6 neither route is "better" in every component: one is shorter, the other is supported.

**E5 — protection is not mobility** (to be added to the test set with 3A): a pawn protecting a friendly pawn diagonally, and a rook protecting the piece beside it, while neither can move to that square.

**E6 — the loop finds Fool's Mate** (`tools/spec_examples/loops.c`, output `loops_output.txt`). From the start position, sequences ending in checkmate: 0 within 1, 2 or 3 plies; **exactly 8 at 4 plies**, all Fool's Mate (1.f3 or 1.f4 or 1.g4, then ...e5 or ...e6, the other pawn move, ...Qh4#); **347 at 5 plies**. Both match the published counts for chess. The loop was not told to look for this pattern.

**E7 — the build-up ladder** (`ladder.c`, `ladder_output.txt`). White Ke1, Qd1, Bc4; Black Ke8 and Pf7, then Black's pieces added one at a time; White plays Qh5 and Qxf7 with Black passing. Black king's legal squares before Qxf7 / after Qxf7: Pf7 only 4 / 1 (not mate, escape d8); + Pd7 3 / 1 (not mate); + Qd8 2 / 0 (**checkmate**); + Bf8 1 / 0 (checkmate). Black's own queen on d8 is the blocker that makes the mate; Bf8 is not needed.

**E8 — practice boards** (`small.c`, `small_output.txt`). King and queen vs king (`7k/8/5K2/8/8/8/8/Q7 w`): no mate in 1, forced mate within 2 White moves (50 positions searched). King and rook vs king (`6k/8/6K1/8/8/8/8/R7 w`): mate in 1 (12 positions). The Scholar corner with only Ke1, Qd1, Bc4 vs Ke8, Pf7 (`4k3/5p2/8/8/2B5/8/8/3QK3 w`): **no forced mate within 3 White moves** (34,537 positions). From the full board after 1.e4 e5, 5 plies: 2,961 sequences end in mate (23,027,889 positions, about 2 minutes), Scholar's Mate among them.

## 9. Answers to ChatGPT's Section 9 questions

1. *Which counting unit does each sentence mean?* Section 4: U1 for "pieces that can't move", U2 (with U3) for "one move makes many pieces unable to move", and the defence test, not a count, for "enough pieces can move to block the queen and bishop". All three are kept.
2. *What is a "shorter route" when the opponent can reply?* Not one number: the vector of Section 6, compared without weights. The first route measurement (3D) assumes the opponent passes and says so; replies are added by the reply view (3B), the loop (3C) and the exchange analysis (3E).
3. *What does "coming up beside or protectively" mean?* A support follow-up: a later move after which another friendly piece protects the moved piece (Section 6). Mere adjacency is not used.
4. *Does Stage 3A match the owner's purpose without Gemini's weighted score?* Yes: 3A only reports sets and counts; no weights, no ranking, no move choice. The Gemini experiment stays in `experiments/`.

## 10. Open questions for the owner

1. Should the 3A report print automatically under every preview, or only when asked with `R`?
2. For goals: one square at a time, or a set of squares (for example the squares around the enemy king)?
3. In 3E, should piece values be shown at all, or only the exchange sequence?
4. For the loop, what depth should be the default (4 plies takes under a second, 5 plies about 20 seconds)?

## Revision history

| Version | Date | Change |
|---|---|---|
| 1.1 | 2026-10-25 | The owner's loop idea added (Section 5b, stage 3C, example E6); his method of practice boards first and the build-up ladder (Section 5c, stage 3S, examples E7, E8); goals, exchanges and comparison move to 3D, 3E, 3F; `docs/PLAN.md` written as the plain-words version of the plan. |
| 1.0 | 2026-10-25 | First draft, written by Claude from the owner's words and ChatGPT's plan; worked examples computed against version 5. |
