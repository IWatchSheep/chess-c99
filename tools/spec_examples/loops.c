/* Loop example of docs/PLAN.md step 3 and CALCULATOR_SPEC.md E6: every move
 * sequence of 1 to 5 moves (plies) from the start that ends in checkmate,
 * using version 5's own rules. Build from the repository root:
 *   gcc -std=c99 -O2 tools/spec_examples/loops.c -o loops
 *   ./loops > tools/spec_examples/loops_output.txt   (about 20 s)
 */
#define main chess_main
#include "../../chess.c"
#undef main
static char line[16][6]; static long mates;
static void walk(const Board *b, int ply, int depth, bool print) {
    MoveList ms = generate_moves(b);
    if (ms.count == 0) { if (is_in_check(b, b->turn) && ply == depth) { ++mates; if (print) { for (int i = 0; i < ply; ++i) printf("%s ", line[i]); printf("#\n"); } } free(ms.items); return; }
    if (ply == depth) { free(ms.items); return; }
    for (size_t i = 0; i < ms.count; ++i) {
        Move m = ms.items[i]; Board c = *b; make_move(&c, m);
        line[ply][0] = (char)('a' + m.sc);
        line[ply][1] = (char)('8' - m.sr);
        line[ply][2] = (char)('a' + m.ec);
        line[ply][3] = (char)('8' - m.er);
        line[ply][4] = '\0';
        walk(&c, ply + 1, depth, print);
    }
    free(ms.items);
}
int main(void) {
    Board b = {0}; reset_board(&b);
    for (int d = 1; d <= 5; ++d) { mates = 0; walk(&b, 0, d, d == 4); printf("from the start, loops of %d moves (plies): %ld sequences end in checkmate\n", d, mates); }
    return 0;
}
