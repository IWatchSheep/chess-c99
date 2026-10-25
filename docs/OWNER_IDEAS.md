# The owner's calculator ideas, in his words

This is the owner's own list of his calculator ideas, in the order he wrote
them, recorded exactly as he gave it on 2026-10-25 (nothing corrected). It is
the authority for intent. The interpretations by Claude and ChatGPT are kept
apart, in `docs/reviews/` and in `docs/CALCULATOR_SPEC.md` (Sections 1 and 7);
where they built on an idea, the version is noted below the list.

```
calculator idea, draft:
{
then there can be a calculator that if this works then 
the shortest route to a goal 
what that translates to is the distance of two or more side by side or protective pieces is shorter than one piece or less pieces, this translates to strategizing/counting to stack pieces protectively or width wise instead of length wise to get to the goal counting on long pieces moves by the other side with less numbers to them which might mean less gain to the gain you get from them by losing pieces. The calculator measures the possible moves and arranges them like this so it says if you move here then the follow up piece with this width or value measure is going to be coming up beside or protectively with you.


How this might work is if there is a step where the sides lists after a move have pieces that can't move, now you balance that can't move list so that multiple versions of many pieces can't move isn't repeated and this goes for counting the other sides pieces that if moved = many pieces can't move, then the calculator balances many pieces can't move + another enemy move = another many pieces can't move to = many pieces can't move, to less pieces can't move + enemy move here is the calculation for queens scholar which means thereis enough pieces can move to block the queen and bishop. Then that counts the way the lists can't move after the enemy moves, that that's defensive, now count for offensive, repeat this but offensively to the enemy.

what goal is of course a question for possibly a ai developer debugging problems for the ai to solve, 
for instance, if from some square the goal square, 
then what would be the calculator to another second goal square.
}

addition calculator idea 2:
{
you can start with less pieces to make it easier then build up to all the pieces once you have a methodology. First the piece is selected, second the list of possible moves is displayed, third one of those possible pieces is chosen, fourth the list is made for the other pieces on the board showing a list of moves before the third step and the list of moves available to them after the third step. So far we have that. Then to build the calculator is the imaginary loop of doing steps 1 to 4 with the calculator counting the way the lists numbers were changed. then repeat another loop until you build the scholars maid just by these loops and see how the list before and after was affected then you have a math logic to build for all other chess strategies besides scholars maid, sound like a plan? if so and you need to add this to the github go ahead but does this agree with my previous calculator plan or is it a better strategy than my first calculator plan?
}

addition calculator idea 3:
{
you know how there was a initial piece and then that being selected presented multiple options. then is there a third list presented to choose from for a true or false and then you select one of those then this changes how the pieces lists move from unchanged because the piece hasn't moved to after the piece moved to a list of true or false options each of the moves available after the pieces moves listed.
}

 addition calculator idea 4, .
{
go ask chatgpt write up the idea that after the list is written with how possible moves changed the number of moves possible is listed in order with a value given to every move with the least number of squares moved = if condition, the increment the number of squares moved = else if condition, to the maximum number of squares moved = else.
}

 addition calculator idea 5, the new idea chatgpt hasn't seen yet
{
you mean if the if condition 1 square gets a pawn, but the else condition gets the queen, doesn't that illogical to go to else first? If yes then the motion to if is false and the motion to else is true.

so now to add to the if, else if, else, condition is a new order listing the if, else if, else, condition is the order the capture higher value pieces from greatest to least.
}

 addition calculator idea 6
{
it should be that given the list of if, else if, else, that second if, else if, else made up of piece trades with captures of good pieces worth more than captures of bad pieces. then together the if, else if, have to have some sorting to show without a capture then use this if and with a capture use this if. does that make sense?
}
```

Where each idea went (the interpretations, not the owner's words):

| Idea | Built in | Notes |
|---|---|---|
| draft | Versions 7, 11, 12 (stages 3A, 3D-1, 3D-2) | can't-move counting (U1/U2/U3), routes and protected "width", the second goal |
| 2 | Versions 6, 10 (stages 3S, 3C) | practice boards, the loop of steps 1 to 4 |
| 3 | Version 11 (stage 3D-1) | IF / ELSE marks in the lists |
| 4 | Version 11 (stage 3D-1) | the distance order IF / ELSE IF / ELSE (ChatGPT review 17) |
| 5 | Stage 3E, design | the capture-value order (`docs/reviews/24`) |
| 6 | Stage 3E, design | without a capture use the distance order, with a capture the value order (`docs/reviews/25`); the overall order of the two groups is the owner's to decide |
