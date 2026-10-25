/* Experiment E-A (docs/FLOWCHART_CROSS_REFERENCE.md, row 1; approved in
 * ChatGPT Review 31): "seeing" versus "entering".
 * For chosen pieces on the E5, E3, start, pinned-knight and castling
 * positions it lists
 *   A = the squares the piece attacks or protects (chess.c piece_attacks,
 *       every square, occupied or not),
 *   L = the squares it may legally move to (chess.c legal_destination_map),
 * and the set differences A - L and L - A, each square with the chess
 * reason. Only existing chess.c functions are used (included unchanged);
 * no new chess rule. The result can SUPPORT, CONTRADICT or NOT RESOLVE only
 * the narrow mapping "the chess system measures observation and legal
 * movement separately"; it says nothing about the flowchart's rock itself.
 * Build and run from the repository root:
 *   gcc -std=c99 -O2 tools/experiments/e_a_seeing_vs_entering.c -o e_a
 *   ./e_a > tools/experiments/e_a_output.txt
 */
#define main chess_main
#include "../../chess.c"
#undef main

static int pieces_with_a_minus_l = 0, pieces_with_l_minus_a = 0, pieces = 0;

static void square_name(int r, int c, char out[3]) {
    out[0] = (char)('a' + c);
    out[1] = (char)('8' - r);
    out[2] = '\0';
}

/* Why a watched square is not a legal destination. */
static const char *a_not_l_reason(const Board *b, int r, int c, int tr, int tc) {
    Piece p = b->board[r][c], t = b->board[tr][tc];
    if (t.color == p.color) {
        return "OWN-PIECE (protected, cannot be entered)";
    }
    if (p.type == 'P' && t.color == EMPTY) {
        return "PAWN-DIAGONAL-EMPTY (a pawn captures diagonally, moves straight)";
    }
    return "KING-SAFETY (the move would leave the own king in check: pin or check)";
}

/* Why a legal destination is not a watched square. */
static const char *l_not_a_reason(const Board *b, int r, int c, int tr, int tc) {
    Piece p = b->board[r][c];
    if (p.type == 'P' && tc == c) {
        return "PAWN-FORWARD (a pawn moves straight, attacks diagonally)";
    }
    if (p.type == 'K' && abs(tc - c) == 2) {
        return "CASTLING";
    }
    (void)tr;
    return "OTHER";
}

static void examine(const Board *b, const char *square) {
    int r = 0, c = 0;
    bool legal[8][8];
    char name[3];
    int a_count = 0, l_count = 0, a_minus_l = 0, l_minus_a = 0;

    parse_square(square, &r, &c);
    legal_destination_map(b, r, c, b->board[r][c].color, legal);
    printf("PIECE %s %c %s\n", color_name(b->board[r][c].color), b->board[r][c].type, square);
    printf("  A (attacks or protects):");
    for (int tr = 0; tr < 8; ++tr) {
        for (int tc = 0; tc < 8; ++tc) {
            if ((tr != r || tc != c) && piece_attacks(b, r, c, tr, tc)) {
                square_name(tr, tc, name);
                printf(" %s", name);
                ++a_count;
            }
        }
    }
    printf("%s\n", a_count ? "" : " none");
    printf("  L (legal destinations):");
    for (int tr = 0; tr < 8; ++tr) {
        for (int tc = 0; tc < 8; ++tc) {
            if (legal[tr][tc]) {
                square_name(tr, tc, name);
                printf(" %s", name);
                ++l_count;
            }
        }
    }
    printf("%s\n", l_count ? "" : " none");
    for (int tr = 0; tr < 8; ++tr) {
        for (int tc = 0; tc < 8; ++tc) {
            if ((tr != r || tc != c) && piece_attacks(b, r, c, tr, tc) && !legal[tr][tc]) {
                square_name(tr, tc, name);
                printf("  A-L %s %s\n", name, a_not_l_reason(b, r, c, tr, tc));
                ++a_minus_l;
            }
        }
    }
    for (int tr = 0; tr < 8; ++tr) {
        for (int tc = 0; tc < 8; ++tc) {
            if (legal[tr][tc] && !piece_attacks(b, r, c, tr, tc)) {
                square_name(tr, tc, name);
                printf("  L-A %s %s\n", name, l_not_a_reason(b, r, c, tr, tc));
                ++l_minus_a;
            }
        }
    }
    printf("  COUNT A %d L %d A-L %d L-A %d\n", a_count, l_count, a_minus_l, l_minus_a);
    ++pieces;
    pieces_with_a_minus_l += a_minus_l > 0;
    pieces_with_l_minus_a += l_minus_a > 0;
}

static void position(const char *title, const char *fen, const char *const *squares, int count) {
    static Board b;
    const char *reason = NULL;
    free(b.history);
    memset(&b, 0, sizeof(b));
    reset_board(&b);
    if (fen != NULL && !load_fen(&b, fen, &reason)) {
        printf("FEN refused: %s\n", reason);
        return;
    }
    printf("POSITION %s\n  FEN %s\n", title, fen ? fen : "start");
    for (int i = 0; i < count; ++i) {
        examine(&b, squares[i]);
    }
    printf("\n");
}

int main(void) {
    static const char *const e5[] = {"c2", "d3", "a1", "b1"};
    static const char *const e3[] = {"f7", "h5", "c4", "e8"};
    static const char *const start[] = {"g1", "a1", "d1", "e2"};
    printf("EXPERIMENT E-A seeing versus entering (A = attacks or protects, L = legal destinations)\n");
    printf("STATEMENT (Draft 12, Section [2], owner-accepted): \"Seeing the rock is a dimensional perspective and remains strictly separate from removing the rock.\"\n\n");
    position("E5 protection is not mobility", "4k3/8/8/8/8/3P4/2P5/RN2K3 w - - 0 1", e5, 4);
    position("E3 after 3.Qh5 (the f7 pawn is pinned)", "r1bqkbnr/pppp1ppp/2n5/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR b KQkq - 3 3", e3, 4);
    position("start", NULL, start, 4);
    position("a pinned knight (the rook on e8 pins it to the king)", "4r1k1/8/8/8/8/8/4N3/4K3 w - - 0 1",
             (const char *const[]){"e2"}, 1);
    position("castling (A and L of the king)", "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", (const char *const[]){"e1"}, 1);
    printf("SUMMARY pieces %d with A-L non-empty %d with L-A non-empty %d\n", pieces, pieces_with_a_minus_l,
           pieces_with_l_minus_a);
    printf("RESULT %s for the narrow mapping: the chess program measures what a piece watches (A) and where it may move (L) separately, and the two sets differ in both directions.\n",
           pieces_with_a_minus_l > 0 && pieces_with_l_minus_a > 0 ? "SUPPORTS" : "DOES NOT RESOLVE");
    printf("LIMIT this does not show that the flowchart's rock is a chess obstacle, nor that seeing and removing the rock are modelled (Review 31, correction A).\n");
    return 0;
}
