/* The build-up ladder of docs/PLAN.md: Black's pieces added around the king one at a time, and when Qxf7 becomes checkmate.
 * Uses the game's own rules and FEN loader (chess.c included unchanged,
 * its main renamed; needs version 6 or later).
 * Build from the repository root:
 *   gcc -std=c99 -O2 tools/spec_examples/ladder.c -o ladder
 *   ./ladder > tools/spec_examples/ladder_output.txt
 */
#define main chess_main
#include "../../chess.c"
#undef main
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
static int king_moves(const Board *b) { /* legal squares of the black king */
    for (int r = 0; r < 8; ++r) for (int c = 0; c < 8; ++c) if (b->board[r][c].type == 'K' && b->board[r][c].color == BLACK) {
        bool d[8][8]; legal_destination_map(b, r, c, BLACK, d); int n = 0; for (int i = 0; i < 64; ++i) n += d[i/8][i%8]; return n; }
    return -1;
}
int main(void) {
    struct { const char *added, *fen; } t[] = {
        {"K + Pf7 only",            "4k3/5p2/8/8/2B5/8/8/3QK3 w - - 0 1"},
        {"+ Pd7",                   "4k3/3p1p2/8/8/2B5/8/8/3QK3 w - - 0 1"},
        {"+ Pd7, Qd8",              "3qk3/3p1p2/8/8/2B5/8/8/3QK3 w - - 0 1"},
        {"+ Pd7, Qd8, Bf8",         "3qkb2/3p1p2/8/8/2B5/8/8/3QK3 w - - 0 1"}};
    for (int i = 0; i < 4; ++i) {
        Board b; setup(&b, t[i].fen);
        Move qh5 = {7, 3, 3, 7, 0}, qf7 = {3, 7, 1, 5, 0};
        Board a = b; make_move(&a, qh5); a.turn = WHITE;          /* Qh5, Black passes */
        Board z = a; make_move(&z, qf7);                          /* Qxf7+ */
        MoveList rs = generate_moves(&z); bool mate = rs.count == 0 && is_in_check(&z, BLACK); free(rs.items);
        Board k = z; k.turn = BLACK;
        printf("%-18s black king's squares before Qxf7: %d; after Qxf7: %d -> %s\n", t[i].added, king_moves(&a), king_moves(&k), mate ? "CHECKMATE" : "not mate");
    }
    return 0;
}
