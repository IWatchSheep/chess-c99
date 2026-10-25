/* Computes the worked examples of docs/CALCULATOR_SPEC.md, using version 5's
 * own rules (chess.c is included unchanged, its main renamed). Build from
 * the repository root:
 *   gcc -std=c99 -O2 tools/spec_examples/examples.c -o examples
 *   ./examples > tools/spec_examples/examples_output.txt
 */
#define main chess_main
#include "../../chess.c"
#undef main

static void sq(int r, int c, char out[3]) { out[0] = (char)('a' + c); out[1] = (char)('0' + 8 - r); out[2] = 0; }

static Board from_moves(const char *const *moves, int n) {
    Board b = {0};
    reset_board(&b);
    for (int i = 0; i < n; ++i) {
        int sr = 0, sc = 0, er = 0, ec = 0;
        if (!parse_square(moves[i], &sr, &sc) || !parse_square(moves[i] + 2, &er, &ec)) {
            printf("BAD SQUARE %s\n", moves[i]);
            exit(1);
        }
        Move m = {sr, sc, er, ec, '\0'};
        if (!legal_move(&b, m)) { printf("ILLEGAL %s\n", moves[i]); exit(1); }
        make_move(&b, m);
    }
    return b;
}

/* L: legal destinations of the piece on (r,c), its side to move. */
static int legal_set(const Board *b, int r, int c, bool out[8][8]) {
    legal_destination_map(b, r, c, b->board[r][c].color, out);
    int n = 0;
    for (int i = 0; i < 64; ++i) n += out[i / 8][i % 8];
    return n;
}

/* A: squares the piece attacks or protects (friendly-occupied included). */
static bool attacks(const Board *b, int r, int c, int tr, int tc) { return piece_attacks(b, r, c, tr, tc); }

static void side_report(const Board *b, Color side, const char *title) {
    bool reach[8][8] = {{false}};
    int zero = 0, pairs = 0, links = 0, unprotected = 0, team = 0;
    char zero_list[200] = "", link_list[1200] = "", unprot_list[200] = "";
    for (int r = 0; r < 8; ++r) for (int c = 0; c < 8; ++c) {
        if (b->board[r][c].color != side) continue;
        bool L[8][8]; int n = legal_set(b, r, c, L);
        pairs += n;
        char s[3]; sq(r, c, s);
        if (n == 0) { ++zero; strcat(zero_list, " "); strcat(zero_list, s); }
        for (int i = 0; i < 64; ++i) if (L[i / 8][i % 8]) reach[i / 8][i % 8] = true;
        bool protected_by_someone = false;
        for (int rr = 0; rr < 8; ++rr) for (int cc = 0; cc < 8; ++cc) {
            if (b->board[rr][cc].color != side || (rr == r && cc == c)) continue;
            if (b->board[r][c].type == 'K') continue; /* a king is never protected: it cannot be captured */
            if (attacks(b, rr, cc, r, c)) {
                ++links; protected_by_someone = true;
                char t[3]; sq(rr, cc, t);
                char e[16]; snprintf(e, sizeof e, " %c%s>%c%s", b->board[rr][cc].type, t, b->board[r][c].type, s);
                strcat(link_list, e);
            }
        }
        if (!protected_by_someone && b->board[r][c].type != 'K') { ++unprotected; strcat(unprot_list, " "); strcat(unprot_list, s); }
    }
    for (int i = 0; i < 64; ++i) team += reach[i / 8][i % 8];
    printf("%s %s: zero-move pieces %d [%s ], legal (piece,square) pairs %d, team reach %d squares, protection links %d, unprotected non-king pieces %d [%s ]\n",
           title, color_name(side), zero, zero_list, pairs, team, links, unprotected, unprot_list);
    printf("   links:%s\n", link_list);
}

/* Lost/gained between B and B' for one side, matching identities with
 * version 5's resolve_piece_after_move; the moved piece is included,
 * compared between its start and end squares (spec v1.2). */
static void change_report(const Board *b, const Board *a, Move m, Color side) {
    int lost_pairs = 0, gained_pairs = 0, denied = 0, opened = 0;
    char lost[600] = "", gained[600] = "", den[200] = "", opn[200] = "";
    bool before_reach[8][8] = {{false}}, after_reach[8][8] = {{false}};
    for (int r = 0; r < 8; ++r) for (int c = 0; c < 8; ++c) {
        if (b->board[r][c].color != side) continue;
        bool L0[8][8]; legal_set(b, r, c, L0);
        for (int i = 0; i < 64; ++i) if (L0[i / 8][i % 8]) before_reach[i / 8][i % 8] = true;
        int ar, ac; bool captured;
        resolve_piece_after_move(b, m, r, c, &ar, &ac, &captured);
        if (r == m.sr && c == m.sc) { ar = m.er; ac = m.ec; captured = false; } /* the moved piece is included (spec v1.2) */
        if (captured) continue;
        bool L1[8][8]; legal_set(a, ar, ac, L1);
        char s[3]; sq(r, c, s);
        for (int i = 0; i < 64; ++i) {
            char t[3]; sq(i / 8, i % 8, t); char e[12];
            if (L0[i / 8][i % 8] && !L1[i / 8][i % 8]) { ++lost_pairs; snprintf(e, sizeof e, " %s>%s", s, t); strcat(lost, e); }
            if (!L0[i / 8][i % 8] && L1[i / 8][i % 8]) { ++gained_pairs; snprintf(e, sizeof e, " %s>%s", s, t); strcat(gained, e); }
        }
    }
    for (int r = 0; r < 8; ++r) for (int c = 0; c < 8; ++c) {
        if (a->board[r][c].color != side) continue;
        bool L1[8][8]; legal_set(a, r, c, L1);
        for (int i = 0; i < 64; ++i) if (L1[i / 8][i % 8]) after_reach[i / 8][i % 8] = true;
    }
    for (int i = 0; i < 64; ++i) {
        char t[3]; sq(i / 8, i % 8, t);
        if (before_reach[i / 8][i % 8] && !after_reach[i / 8][i % 8]) { ++denied; strcat(den, " "); strcat(den, t); }
        if (!before_reach[i / 8][i % 8] && after_reach[i / 8][i % 8]) { ++opened; strcat(opn, " "); strcat(opn, t); }
    }
    printf("   %s: lost pairs %d [%s ], gained pairs %d [%s ], team squares denied %d [%s ], team squares opened %d [%s ]\n",
           color_name(side), lost_pairs, lost, gained_pairs, gained, denied, den, opened, opn);
}

/* Does `side` (made the side to move) have a move that checkmates? */
static int mating_moves(const Board *b, Color side, char *list) {
    Board v = *b; v.turn = side;
    MoveList ms = generate_moves(&v); int n = 0; list[0] = 0;
    for (size_t i = 0; i < ms.count; ++i) {
        Board w = v; make_move(&w, ms.items[i]);
        MoveList reply = generate_moves(&w);
        bool mate = reply.count == 0 && is_in_check(&w, w.turn);
        free(reply.items);
        if (mate) {
            char a[3], z[3]; sq(ms.items[i].sr, ms.items[i].sc, a); sq(ms.items[i].er, ms.items[i].ec, z);
            ++n; strcat(list, " "); strcat(list, a); strcat(list, z);
        }
    }
    free(ms.items); return n;
}

/* Shortest number of own moves for the piece on (r,c) to reach the goal,
 * the other side passing (no replies). Breadth-first over positions. */
static int route_length(const Board *b, int r, int c, int gr, int gc, int limit) {
    typedef struct { Board pos; int r, c, d; } Node;
    static Node q[200000]; int head = 0, tail = 0;
    static bool seen[64];
    memset(seen, 0, sizeof seen);
    q[tail++] = (Node){*b, r, c, 0}; seen[r * 8 + c] = true;
    q[0].pos.turn = b->board[r][c].color;
    while (head < tail) {
        Node n = q[head++];
        if (n.r == gr && n.c == gc) return n.d;
        if (n.d == limit) continue;
        bool L[8][8]; legal_set(&n.pos, n.r, n.c, L);
        for (int i = 0; i < 64; ++i) {
            if (!L[i / 8][i % 8] || seen[i]) continue;
            seen[i] = true;
            Node m = n; Move mv = {n.r, n.c, i / 8, i % 8, '\0'};
            m.pos.turn = n.pos.board[n.r][n.c].color;
            make_move(&m.pos, mv); m.pos.turn = n.pos.board[n.r][n.c].color;
            m.r = i / 8; m.c = i % 8; m.d = n.d + 1; q[tail++] = m;
        }
    }
    return -1;
}

static int attackers_of(const Board *b, int tr, int tc, Color side, char *list) {
    int n = 0; list[0] = 0;
    for (int r = 0; r < 8; ++r) for (int c = 0; c < 8; ++c) {
        if (b->board[r][c].color == side && attacks(b, r, c, tr, tc)) {
            char s[3]; sq(r, c, s); char e[8]; snprintf(e, sizeof e, " %c%s", b->board[r][c].type, s);
            strcat(list, e); ++n;
        }
    }
    return n;
}

int main(void) {
    char buf[2000];
    puts("== E1 start position");
    Board s = from_moves(NULL, 0);
    side_report(&s, WHITE, "E1"); side_report(&s, BLACK, "E1");

    puts("== E2 1.e4 from the start");
    Board a = s; Move e4 = {6, 4, 4, 4, '\0'}; make_move(&a, e4);
    change_report(&s, &a, e4, WHITE); change_report(&s, &a, e4, BLACK);
    side_report(&a, WHITE, "E2 after");

    puts("== E3 Scholar: 1.e4 e5 2.Bc4 Nc6, candidate 3.Qh5");
    const char *ms[] = {"e2e4", "e7e5", "f1c4", "b8c6"};
    Board p = from_moves(ms, 4);
    printf("   White mating moves now (if White to move): %d [%s ]\n", mating_moves(&p, WHITE, buf), buf);
    side_report(&p, WHITE, "E3 before"); side_report(&p, BLACK, "E3 before");
    Move qh5 = {7, 3, 3, 7, '\0'}; Board q = p; make_move(&q, qh5);
    change_report(&p, &q, qh5, WHITE); change_report(&p, &q, qh5, BLACK);
    printf("   after 3.Qh5, White mating moves (threat): %d [%s ]\n", mating_moves(&q, WHITE, buf), buf);
    printf("   attackers of f7: White %d", attackers_of(&q, 1, 5, WHITE, buf)); printf(" [%s ]", buf);
    printf(", Black defenders %d", attackers_of(&q, 1, 5, BLACK, buf)); printf(" [%s ]\n", buf);
    MoveList replies = generate_moves(&q);
    int stop = 0, allow = 0; char stops[600] = "", allows[600] = "";
    for (size_t i = 0; i < replies.count; ++i) {
        Board r = q; make_move(&r, replies.items[i]);
        char a1[3], z1[3]; sq(replies.items[i].sr, replies.items[i].sc, a1); sq(replies.items[i].er, replies.items[i].ec, z1);
        char e[8]; snprintf(e, sizeof e, " %s%s", a1, z1);
        if (mating_moves(&r, WHITE, buf) > 0) { ++allow; strcat(allows, e); } else { ++stop; strcat(stops, e); }
    }
    printf("   Black replies %zu: stop every mate-in-one %d [%s ], leave a mate-in-one %d [%s ]\n", replies.count, stop, stops, allow, allows);
    free(replies.items);
    const char *g6[] = {"e2e4", "e7e5", "f1c4", "b8c6", "d1h5", "g7g6"};
    Board r6 = from_moves(g6, 6);
    puts("   after 3...g6:");
    { Move m = {1, 6, 2, 6, '\0'}; change_report(&q, &r6, m, BLACK); change_report(&q, &r6, m, WHITE); }
    side_report(&r6, BLACK, "E3 after g6");
    const char *nf6[] = {"e2e4", "e7e5", "f1c4", "b8c6", "d1h5", "g8f6"};
    Board rn = from_moves(nf6, 6);
    printf("   after 3...Nf6: White mating moves %d [%s ]\n", mating_moves(&rn, WHITE, buf), buf);

    puts("== E3b the alternative 3.Nf3 from the same position");
    Move nf3 = {7, 6, 5, 5, '\0'}; Board n3 = p; make_move(&n3, nf3);
    change_report(&p, &n3, nf3, WHITE); change_report(&p, &n3, nf3, BLACK);
    printf("   after 3.Nf3, White mating moves: %d\n", mating_moves(&n3, WHITE, buf));
    side_report(&n3, WHITE, "E3b after Nf3");

    puts("== E4 goal squares from the E3 position (White, the other side passing)");
    printf("   goal f7: queen d1 %d moves, bishop c4 %d, knight g1 %d, knight b1 %d\n",
           route_length(&p, 7, 3, 1, 5, 6), route_length(&p, 4, 2, 1, 5, 6),
           route_length(&p, 7, 6, 1, 5, 6), route_length(&p, 7, 1, 1, 5, 6));
    printf("   goal d5: queen d1 %d moves, bishop c4 %d, knight g1 %d, knight b1 %d\n",
           route_length(&p, 7, 3, 3, 3, 6), route_length(&p, 4, 2, 3, 3, 6),
           route_length(&p, 7, 6, 3, 3, 6), route_length(&p, 7, 1, 3, 3, 6));
    printf("   attackers of f7 now: White %d", attackers_of(&p, 1, 5, WHITE, buf)); printf(" [%s ]", buf);
    printf("; of d5: White %d", attackers_of(&p, 3, 3, WHITE, buf)); printf(" [%s ]", buf);
    printf(", Black %d", attackers_of(&p, 3, 3, BLACK, buf)); printf(" [%s ]\n", buf);
    return 0;
}
