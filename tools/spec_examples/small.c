/* Practice boards of docs/PLAN.md (fewer pieces first): forced mates on small boards, and the cost of rebuilding Scholar's Mate from 1.e4 e5 (about 2 minutes).
 * Uses the game's own rules and FEN loader (chess.c included unchanged,
 * its main renamed; needs version 6 or later).
 * Build from the repository root:
 *   gcc -std=c99 -O2 tools/spec_examples/small.c -o small
 *   ./small > tools/spec_examples/small_output.txt
 */
#define main chess_main
#include "../../chess.c"
#undef main
#include <time.h>
/* Sets up a position with chess.c's own FEN loader (version 6, stage 3S),
 * which also checks that the position is possible. */
static void setup(Board *b, const char *fen) {
    const char *reason = NULL;
    memset(b, 0, sizeof *b);
    reset_board(b);
    if (!load_fen(b, fen, &reason)) {
        printf("FEN refused (%s): %s\n", reason, fen);
        exit(1);
    }
}
static long nodes;
/* forced mate for side to move within n of its own moves */
static bool forced(const Board *b, int n) {
    MoveList ms = generate_moves(b); bool ok = false;
    for (size_t i = 0; i < ms.count && !ok; ++i) {
        Board c = *b; make_move(&c, ms.items[i]); ++nodes;
        MoveList rs = generate_moves(&c);
        if (rs.count == 0) { ok = is_in_check(&c, c.turn); free(rs.items); continue; }
        if (n > 1) { bool all = true;
            for (size_t j = 0; j < rs.count && all; ++j) { Board d = c; make_move(&d, rs.items[j]); ++nodes; all = forced(&d, n - 1); }
            ok = all; }
        free(rs.items);
    }
    free(ms.items); return ok;
}
static long mates_at; static void walk(const Board *b, int ply, int depth) {
    MoveList ms = generate_moves(b); ++nodes;
    if (ms.count == 0) { if (ply == depth && is_in_check(b, b->turn)) ++mates_at; free(ms.items); return; }
    if (ply < depth) for (size_t i = 0; i < ms.count; ++i) { Board c = *b; make_move(&c, ms.items[i]); walk(&c, ply + 1, depth); }
    free(ms.items);
}
int main(void) {
    struct { const char *name, *fen; } t[] = {
        {"K+Q vs K", "7k/8/5K2/8/8/8/8/Q7 w - - 0 1"},
        {"K+R vs K", "6k1/8/6K1/8/8/8/8/R7 w - - 0 1"},
        {"Scholar corner: K+Q+B vs K+P", "4k3/5p2/8/8/2B5/8/8/3QK3 w - - 0 1"}};
    for (int i = 0; i < 3; ++i) { Board b; setup(&b, t[i].fen);
        for (int n = 1; n <= 3; ++n) { nodes = 0; clock_t c0 = clock(); bool f = forced(&b, n);
            printf("%-30s forced mate within %d White moves: %s (%ld positions, %.2f s)\n", t[i].name, n, f ? "YES" : "no", nodes, (double)(clock() - c0) / CLOCKS_PER_SEC);
            if (f) break; } }
    Board b = {0}; reset_board(&b); Move e4 = {6,4,4,4,0}, e5 = {1,4,3,4,0}; make_move(&b, e4); make_move(&b, e5);
    nodes = 0; mates_at = 0; clock_t c0 = clock(); walk(&b, 0, 5);
    printf("after 1.e4 e5, loops of 5 plies: %ld sequences end in mate (%ld positions, %.1f s)\n", mates_at, nodes, (double)(clock() - c0) / CLOCKS_PER_SEC);
    return 0;
}
