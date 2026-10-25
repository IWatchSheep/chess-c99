# The plan, in plain words

This is the owner's plan for `chess-c99`, written as steps anyone can follow. His original words are at the end (Appendix A); they are the authority. The exact technical details for programmers are in `CALCULATOR_SPEC.md`.

## Where we are

| Step | What | Status |
|---|---|---|
| 1 | The chess game | **Done** (versions 1–5) |
| 2 | The calculator: one move at a time ("snapshot") | Planned |
| 3 | The calculator: many moves ahead ("loop"), starting with practice boards of few pieces | Planned |
| 4 | Goals: routes to a chosen square, then a second square | Planned |
| Later | Lines drawn on the board, rule explanations | Ideas only |

## Words used in this plan

- **Square list** — the squares a piece can legally move to right now.
- **Protects** — a piece protects a friendly piece if it could capture on that piece's square should an enemy land there. (Protecting is not the same as moving there: a pawn protects diagonally but moves straight.)
- **Can't move** — three different things, always counted separately:
  1. a **piece** with an empty square list;
  2. a **move** a piece had before and has lost;
  3. a **square** no piece of that side can reach any more.
- **Threat** — a move the other side could play next that wins something, for example checkmate.
- **Move (ply)** — one turn by one side: White's move is one, Black's reply is another. In this plan "4 moves ahead" means 4 of these single turns (chess programmers call each one a *ply*). "2 of White's moves" means 3 single turns: White, Black, White.
- **Forced** — a mate is forced if, **whatever the other side replies, you always have an answer** that keeps the mate coming, and you may choose a different answer for each reply. Finding a mate in *some* lines is not enough: that is only **possible**.
- **Imagined pass** — in some practice experiments one side is imagined to skip its turn. That is **not legal chess**; such results are always marked *(imagined pass)* and are only used to measure the board, never to claim how a real game goes.
- **Report** — after a preview, typing `R` shows the calculator's facts, one per line. The normal preview stays exactly as it is now. (Agreed by the owner, 2026-10-25.)
- **Goal** — a square someone chooses for the calculator to aim at.

## The method: start small, build up, learn from the lists

The owner's method (2026-10-25), step by step:

1. **A piece is selected.**
2. **Its list of possible moves is shown.**
3. **One of those moves is chosen** (only imagined, not played).
4. **For every other piece on the board, two lists are made:** its moves *before* step 3 and its moves *after* step 3.

Steps 1–4 already exist (versions 2–5). **The calculator is an imaginary loop of steps 1–4**, counting how the numbers in the lists changed. Then the loop is repeated, move after move, until Scholar's Mate is built just by these loops. Watching how the before and after lists changed along the way gives a **math logic**, which can then be tried on other chess strategies.

**Start with fewer pieces.** A board with few pieces has few moves, so the loop is fast and every result can be checked by hand. Once the method works there, pieces are added back until it works on the full board.

Why this matters, in numbers:

| Board | What the loop does | Positions looked at | Time |
|---|---|---|---|
| King and queen against king | proves a forced mate in 2 of White's moves (3 single moves) | 50 | instant |
| King and rook against king (practice position) | finds mate in 1 | 12 | instant |
| Full board, from after 1.e4 e5 | 5 single moves ahead: 2,961 mating sequences, Scholar's Mate among them | 23,027,889 | about 2 minutes |

**What the build-up shows (the "ladder").** Take only what Scholar's Mate uses: White's king, queen on d1 and bishop on c4, against Black's king and f7 pawn. Add Black's other pieces around the king one at a time, and play Qh5 then Qxf7 (Black not moving — an *imagined pass*, a board experiment, not a real game):

| Black pieces around the king | Black king's squares before Qxf7 | After Qxf7 | Result |
|---|---|---|---|
| f7 pawn only | 4 | 1 | not mate: the king escapes to d8 |
| + pawn d7 | 3 | 1 | not mate |
| + queen d8 | 2 | 0 | **checkmate** |
| + bishop f8 | 1 | 0 | checkmate |

The count of the king's squares falling to 0 is the mate. The piece that makes it work is **Black's own queen on d8**, which blocks her king's last escape; the bishop on f8 is not needed. With only the king and the f7 pawn, there is **no forced mate within 3 of White's moves at all** (Black replying with real moves): the king runs. So Scholar's Mate is a fact about Black's own pieces that *can't move out of the way* — the owner's "pieces that can't move", seen in the numbers.

**One caution, so the math logic is real.** A pattern seen in Scholar's Mate must also be checked on *other* mates (for example Fool's Mate) and on sequences that do *not* end in mate. If it only appears in one game, it may be a coincidence.

**What the program needs first.** Version 5 always starts from the normal starting position. To use practice boards, it needs one new ability: **set up a board from a line of text** (the standard chess notation, "FEN"). This is the first small coding step.

## Step 1 — The chess game (done)

1. Type a square (for example `e2`). The game shows that piece and its square list. Nothing moves.
2. Type a destination (for example `e4`). Nothing moves. The game shows, for **every other piece on both sides**, its square list before and after, and what it gained, lost and kept.
3. Press `F` to make the move for real, `P` to try another destination, `N` to pick another piece.
4. Everything is also printed as plain text lines, so a machine can read it without the board picture.
5. It is written in C99, and its chess rules are checked against the published move counts.

## Step 2 — The calculator, one move at a time ("snapshot")

After you preview a move, type `R` and the calculator shows its report. It only **counts and lists**; it never chooses a move for you. The report also says **why** a piece can't move: it is **blocked**, **pinned** to its king, or its king is **in check**. A captured piece is listed on its own line, so it never changes the counts silently.

1. **For both sides, after your move:**
   - which pieces can't move at all;
   - which moves were lost, and which were gained, piece by piece;
   - which squares the side can no longer reach, and which it now can;
   - who protects whom: which protections were made and which were broken;
   - which pieces attack which, and whether a king is in check;
   - whether your move makes a threat (for example, mate next move).
2. **Then every possible enemy reply**, one by one:
   - does the reply stop your threat, or leave it?
   - what does the reply take away from you, and what does it give back?
3. **Count each loss only once.** If a move is lost, it is counted at the moment it is lost. If it is still lost after the next move, it is not counted again. If a reply gives it back, it is reported as **given back**. This is the owner's "balance the can't-move list so it isn't repeated".
4. **Defence, then offence.** The same report is made from your side (what the enemy can do to you) and from the enemy's side (what you can do to them).

**Example (Scholar's Mate).** After 1.e4 e5 2.Bc4 Nc6, if White plays 3.Qh5:
- Black's f7 pawn can't move any more (the queen pins it to the king), and Black loses the moves f6, f5 and h5;
- White threatens exactly one checkmate: Qxf7;
- Black has 28 replies. **8 stop the mate** (Qe7, Qf6, Qg5, Qh4, Ke7, Nh6, d5, g6) and **20 do not**. This is the owner's "there are enough pieces that can move to block the queen and bishop": 8 ways exist.

**Done when** every number in the examples of `CALCULATOR_SPEC.md` comes out exactly in the game's report.

## Step 3 — The calculator, many moves ahead ("loop")

The snapshot of step 2 looks one move and one reply ahead. The **loop** repeats it: your move, their reply, your move, their reply, and so on, for as many moves as you choose.

1. **You choose how many loops** (how many moves ahead).
2. **At every loop the same counts are made** as in step 2, with the same rule: each loss is counted once, when it happens.
3. **The loop reports what it finds**, for example:
   - sequences that end in checkmate, and for which side;
   - whether a mate is **forced** (an answer to every reply) or only **possible** (it happens only if the other side makes a mistake);
   - when the chosen number of moves runs out without an answer, it says so, instead of pretending nothing exists;
   - sequences where many enemy pieces end up unable to move.
4. **This is how other strategies than Scholar's Mate can be found.** The loop is not told what to look for; it lists what happens.

**Example (found by the loop, from the starting position).** With 4 moves of loop, the loop finds exactly **8 sequences that end in checkmate**. All of them are **Fool's Mate**: White weakens the king with f3 or f4 and g4, and Black mates with Qh4. With 5 moves it finds 347. These numbers match the published values for chess.

**What the loop costs.** Each extra loop multiplies the work by about 30 (the number of moves in a typical position):

| Loops (moves ahead) | Positions to look at (from the start) | Time on Claude's test computer |
|---|---|---|
| 1 | 20 | instant |
| 2 | 400 | instant |
| 3 | 8,902 | instant |
| 4 | 197,281 | under a second |
| 5 | 4,865,609 | about 20 seconds |

So the loop is useful for a few moves ahead. Going much deeper would need a rule for which moves to skip, and choosing that rule is itself a decision for later (step 3 does not skip anything).

**The loop is run in the order of the method above:** first on practice boards (king and queen against king; the Scholar's Mate corner with Black's pieces added one at a time), then on the full board from positions close to the strategy (for example after 1.e4 e5), and only then from the start.

**Done when** the loop reproduces the practice-board results (forced mate in 2 of White's moves with king and queen; the ladder table), the Fool's Mate numbers (8 at 4 moves, 347 at 5) and the Scholar's Mate reply counts.

## Step 4 — Goals: routes to a chosen square

1. **Choose a goal square** (for example f7).
2. **For each of your pieces**, the calculator shows how many moves it needs to get there alone (the other side not moving: an *imagined pass*, used only to measure distance), and whether it would arrive **protected** or **unprotected**.
3. **It also shows team routes**: two or more pieces moving so that one arrives on the goal **protected by another**. This is the owner's "two or more side by side or protective pieces" against "one long piece".
4. **Routes are compared honestly.** A route is only called better if it is at least as good in every way (fewer moves, more protectors, fewer attackers, less material at risk). If one route is shorter and the other is safer, both are shown; the calculator does not pretend one number decides.
5. **Then a second goal**, measured from where the first route ended, so you can follow the plan from goal 1 to goal 2.

**Example.** From the Scholar's Mate position, with goal f7: the bishop gets there in **1 move, unprotected** (the king would capture it). The queen gets there in **2 moves (Qh5, then Qxf7), protected by the bishop**, and that arrival is checkmate. One is shorter, the other is supported.

## Later ideas (from the owner's notes, not planned yet)

- Thin lines drawn from each piece to its squares, in a large console window, to make the game easier to follow.
- Showing the chess rule behind each move next to the piece's squares.

## How the work is done

- One step at a time, each a new version of `chess.c`, committed in order. Earlier versions stay in the history.
- The first coding step is the board setup from text (for practice boards); then the snapshot report (step 2); then the loop (step 3); then goals (step 4).
- Before any code: Claude and ChatGPT agree on the step, and the owner agrees.
- Every example number is computed by a small program that uses the game's own rules (`tools/spec_examples/`), so anyone can check it.

## Appendix A — The owner's plan, in his words

The chess game:

> make the chess game show the piece selected, and the squares available to it.
>
> if you select a square you don't go there, but you see how it affects all the other pieces squares they can go to.
>
> Then you repeat this for the other side if you go to a square you don't go there but you see the squares all the other sides pieces can go to as a result.
>
> then you go to the square you want by confirming it.
>
> the graphical version can be listed by a text list to the side of the game would work for a machine to read because a machine doesn't need to see fancy gui to see the squares.
>
> and I want the chess game in c99.

The calculator:

> then there can be a calculator that if this works then
> the shortest route to a goal
> what that translates to is the distance of two or more side by side or protective pieces is shorter than one piece or less pieces, this translates to strategizing/counting to stack pieces protectively or width wise instead of length wise to get to the goal counting on long pieces moves by the other side with less numbers to them which might mean less gain to the gain you get from them by losing pieces. The calculator measures the possible moves and arranges them like this so it says if you move here then the follow up piece with this width or value measure is going to be coming up beside or protectively with you.
>
> How this might work is if there is a step where the sides lists after a move have pieces that can't move, now you balance that can't move list so that multiple versions of many pieces can't move isn't repeated and this goes for counting the other sides pieces that if moved = many pieces can't move, then the calculator balances many pieces can't move + another enemy move = another many pieces can't move to = many pieces can't move, to less pieces can't move + enemy move here is the calculation for queens scholar which means thereis enough pieces can move to block the queen and bishop. Then that counts the way the lists can't move after the enemy moves, that that's defensive, now count for offensive, repeat this but offensively to the enemy.
>
> what goal is of course a question for possibly a ai developer debugging problems for the ai to solve,
> for instance, if from some square the goal square,
> then what would be the calculator to another second goal square.

The loop (2026-10-25):

> step 3 can be a snapshot or loop, now however many loops then you can see other strategies besides scholars mate

The method (2026-10-25):

> you can start with less pieces to make it easier then build up to all the pieces once you have a methodology. First the piece is selected, second the list of possible moves is displayed, third one of those possible pieces is chosen, fourth the list is made for the other pieces on the board showing a list of moves before the third step and the list of moves available to them after the third step. So far we have that. Then to build the calculator is the imaginary loop of doing steps 1 to 4 with the calculator counting the way the lists numbers were changed. then repeat another loop until you build the scholars maid just by these loops and see how the list before and after was affected then you have a math logic to build for all other chess strategies besides scholars maid

Notes (ideas for later):

> since this will probably be used by other having a large console window with lines from the pieces to the squares detailed above small thin lines would help people understand the game better.

> And the moves can also show chess rules along with the pieces possible squares and the resulting squares the other pieces can go to.
