/* Stage 3E tests: piece values (logged only), exchanges on one square
 * ("exchange <square> N"), the bounded material classes MATERIAL FORCED /
 * POSSIBLE NONNEGATIVE / ELSE, the combined order of the owner's ideas 7
 * and 8 ("order N"), and AT-RISK ("risk on"), against hand-checked
 * positions and an independent minimax; stopping, limits, en passant,
 * refused input, and that nothing changes. chess.c is included unchanged.
 * Run by CTest. */
#define main chess_main
#include "../chess.c"
#undef main

static int checks = 0;
static int failures = 0;

static void check(bool ok, const char *what) {
    ++checks;
    if (!ok) {
        ++failures;
        printf("FAIL: %s\n", what);
    }
}

/* b must be a static Board (zero at first); its old history is freed. */
static bool setup(Board *b, const char *fen) {
    const char *reason = NULL;
    free(b->history);
    memset(b, 0, sizeof(*b));
    reset_board(b);
    if (fen != NULL && !load_fen(b, fen, &reason)) {
        printf("  (FEN refused: %s)\n", reason);
        return false;
    }
    return true;
}

static bool has_line(const char *text, const char *line) {
    size_t n = strlen(line);
    for (const char *p = text; p != NULL && *p != '\0';) {
        const char *end = strchr(p, '\n');
        size_t len = end ? (size_t)(end - p) : strlen(p);
        if (len == n && strncmp(p, line, n) == 0) {
            return true;
        }
        p = end ? end + 1 : NULL;
    }
    return false;
}

static bool all_lines(const char *text, const char *const *lines, int count) {
    bool ok = text != NULL;
    for (int i = 0; ok && i < count; ++i) {
        if (!has_line(text, lines[i])) {
            printf("  (missing line: %s)\n", lines[i]);
            ok = false;
        }
    }
    return ok;
}

static int count_prefix(const char *text, const char *prefix) {
    size_t n = strlen(prefix);
    int count = 0;
    for (const char *p = text; p != NULL && *p != '\0';) {
        if (strncmp(p, prefix, n) == 0) {
            ++count;
        }
        const char *end = strchr(p, '\n');
        p = end ? end + 1 : NULL;
    }
    return count;
}

#define COUNT_OF(a) ((int)(sizeof(a) / sizeof((a)[0])))

static char *exchange_for(const Board *b, const char *square, int limit) {
    TextBuffer out = {NULL, 0, 0};
    int r, c;
    parse_square(square, &r, &c);
    exchange_report(b, r * 8 + c, limit, &out);
    return out.text;
}

static char *order_for(const Board *b, int limit) {
    TextBuffer out = {NULL, 0, 0};
    order_report(b, limit, &out);
    return out.text;
}

/* White Qd1, Nc3, Pe4 against a black rook on d5 protected by a pawn on e6. */
static const char *const ROOK = "6k1/8/4p3/3r4/4P3/2N5/8/3Q2K1 w - - 0 1";
/* White Pe4, Nc3 against a black pawn on d5 protected by the queen on d8. */
static const char *const QUEEN = "3q2k1/8/8/3p4/4P3/2N5/8/6K1 w - - 0 1";
static const char *const SCHOLAR = "r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/8/PPPP1PPP/RNBQK1NR w KQkq - 2 3";

static void test_values(void) {
    check(piece_value('P') == 1 && piece_value('N') == 3 && piece_value('B') == 3 && piece_value('R') == 5 &&
              piece_value('Q') == 9 && piece_value('K') == 0,
          "X1 the conventional values P 1, N 3, B 3, R 5, Q 9; a king has none");
}

static void test_rook(void) {
    static Board b;
    static const char *const lines[] = {
        "EXCHANGE BEGIN White d5 LIMIT 4",
        "SEQUENCE e4d5 WHITE-TOOK R BLACK-TOOK - MATERIAL White 5 Black 0",
        "SEQUENCE e4d5 e6d5 WHITE-TOOK R BLACK-TOOK P MATERIAL White 5 Black 1",
        "SEQUENCE e4d5 e6d5 c3d5 WHITE-TOOK R P BLACK-TOOK P MATERIAL White 6 Black 1",
        "SEQUENCE d1d5 e6d5 WHITE-TOOK R BLACK-TOOK Q MATERIAL White 5 Black 9",
        "SEQUENCE d1d5 e6d5 c3d5 WHITE-TOOK R P BLACK-TOOK Q MATERIAL White 6 Black 9",
        "FIRST e4d5 TAKES R VALUE 5 MATERIAL FORCED NONNEGATIVE ORDER IF",
        "FIRST c3d5 TAKES R VALUE 5 MATERIAL FORCED NONNEGATIVE ORDER IF",
        "FIRST d1d5 TAKES R VALUE 5 MATERIAL POSSIBLE NONNEGATIVE ORDER IF",
        "SUMMARY SEQUENCES 12",
        "EXCHANGE END"};
    char *text = setup(&b, ROOK) ? exchange_for(&b, "d5", 4) : NULL;
    check(all_lines(text, lines, COUNT_OF(lines)) && count_prefix(text, "SEQUENCE ") == 12 && strstr(text, "\nLIMIT") == NULL,
          "X2 every capture order on d5, with both sides' totals (a summation over the moves)");
    check(text && has_line(text, "FIRST d1d5 TAKES R VALUE 5 MATERIAL POSSIBLE NONNEGATIVE ORDER IF"),
          "X3 a valuable first capture can turn bad: Qxd5 exd5 leaves White 5, Black 9 (only POSSIBLE)");
    check(text && has_line(text, "FIRST e4d5 TAKES R VALUE 5 MATERIAL FORCED NONNEGATIVE ORDER IF"),
          "X4 exd5 is FORCED: after ...exd5 White may stop (5 - 1); the choice to stop belongs to the side to move");
    free(text);
    text = exchange_for(&b, "d5", 1);
    check(text && has_line(text, "FIRST d1d5 TAKES R VALUE 5 MATERIAL FORCED NONNEGATIVE ORDER IF BOUNDED") &&
              has_line(text, "LIMIT 1") && count_prefix(text, "SEQUENCE ") == 3,
          "X5 with limit 1 the recapture is not seen: FORCED but BOUNDED, LIMIT 1");
    free(text);
}

static void test_low_first_capture(void) {
    static Board b;
    char *text = setup(&b, QUEEN) ? exchange_for(&b, "d5", 4) : NULL;
    check(text && has_line(text, "SEQUENCE e4d5 d8d5 c3d5 WHITE-TOOK P Q BLACK-TOOK P MATERIAL White 10 Black 1") &&
              has_line(text, "FIRST e4d5 TAKES P VALUE 1 MATERIAL FORCED NONNEGATIVE ORDER IF"),
          "X6 a low-value first capture (a pawn) ends 10 to 1 over the sequence (idea 8)");
    free(text);
}

static void test_edges(void) {
    static Board b;
    char *text = setup(&b, SCHOLAR) ? exchange_for(&b, "e5", 4) : NULL;
    check(text && has_line(text, "NO CAPTURES") && count_prefix(text, "FIRST ") == 0 && count_prefix(text, "SEQUENCE ") == 0,
          "X7 no capture on the square: NO CAPTURES; a line without a capture is never a trade");
    free(text);
    text = exchange_for(&b, "f7", 4);
    check(text && has_line(text, "FIRST c4f7 TAKES P VALUE 1 MATERIAL POSSIBLE NONNEGATIVE ORDER IF") &&
              has_line(text, "SEQUENCE c4f7 e8f7 WHITE-TOOK P BLACK-TOOK B MATERIAL White 1 Black 3"),
          "X8 Bxf7+ Kxf7: a bishop for a pawn, so only POSSIBLE");
    free(text);
    text = setup(&b, "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1") ? exchange_for(&b, "d6", 2) : NULL;
    check(text && has_line(text, "FIRST e5d6 TAKES P VALUE 1 MATERIAL FORCED NONNEGATIVE ORDER IF"),
          "X9 en passant: the pawn taken on d5 counts as a capture on d6");
    free(text);
}

/* An independent minimax with stopping: the side to move takes the best of
 * stopping and each capture for itself. */
static int guarantee(const Board *b, Color mover, int sq, int m, int left) {
    int best = m;
    if (left == 0) {
        return m;
    }
    MoveList all = generate_moves(b);
    for (size_t i = 0; i < all.count; ++i) {
        Move c = all.items[i];
        if (c.er * 8 + c.ec != sq || !captured_type(b, c)) {
            continue;
        }
        Board next = *b;
        int v = piece_value(captured_type(b, c));
        make_move(&next, c);
        int r = guarantee(&next, mover, sq, b->turn == mover ? m + v : m - v, left - 1);
        if (b->turn == mover ? r > best : r < best) {
            best = r;
        }
    }
    free(all.items);
    return best;
}

static void test_independent(void) {
    static Board b;
    static const char *const fens[] = {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", ROOK, QUEEN,
                                       SCHOLAR, "r1b1k2r/ppppnppp/2n2q2/2b5/3NP3/2P1B3/PP3PPP/RN1QKB1R w KQkq - 0 1"};
    bool ok = true;
    int tested = 0;
    for (int f = 0; ok && f < COUNT_OF(fens); ++f) {
        setup(&b, fens[f]);
        MoveList all = generate_moves(&b);
        for (size_t i = 0; ok && i < all.count; ++i) {
            Move c = all.items[i];
            if (!captured_type(&b, c)) {
                continue;
            }
            bool bounded = false;
            MaterialResult r = first_capture_class(&b, c, 4, &bounded);
            Board next = b;
            int v = piece_value(captured_type(&b, c));
            make_move(&next, c);
            bool forced = guarantee(&next, b.turn, c.er * 8 + c.ec, v, 3) >= 0;
            ok = (r == MATERIAL_FORCED) == forced && r != MATERIAL_ELSE;
            ++tested;
            if (!ok) {
                char t[6];
                move_text(c, t);
                printf("  (disagree on %s)\n", t);
            }
        }
        free(all.items);
    }
    check(ok && tested > 10,
          "X10 FORCED agrees with an independent minimax for every capture of five positions (incl. Kiwipete)");
    check(ok, "X11 no first capture is MATERIAL ELSE: in this model the other side may simply not recapture");
}

static void test_order(void) {
    static Board b;
    static const char *const lines[] = {
        "ORDER BEGIN White EXCHANGE LIMIT 4",
        "HEADS CAPTURE",
        "GROUP 1 CAPTURE e4d5 TAKES R VALUE 5 SQUARES 1 MATERIAL FORCED NONNEGATIVE ORDER IF",
        "GROUP 1 CAPTURE c3d5 TAKES R VALUE 5 SQUARES 2 MATERIAL FORCED NONNEGATIVE ORDER IF",
        "GROUP 1 CAPTURE d1d5 TAKES R VALUE 5 SQUARES 4 MATERIAL POSSIBLE NONNEGATIVE ORDER IF",
        "GROUP 2 DISTANCE e4e5 SQUARES 1 ORDER IF",
        "ORDER END"};
    char *text = setup(&b, ROOK) ? order_for(&b, 4) : NULL;
    check(all_lines(text, lines, COUNT_OF(lines)) &&
              strstr(text, "CAPTURE e4d5") < strstr(text, "CAPTURE c3d5") &&
              strstr(text, "CAPTURE c3d5") < strstr(text, "CAPTURE d1d5") &&
              strstr(text, "CAPTURE d1d5") < strstr(text, "GROUP 2"),
          "X12 captures first (FORCED, then POSSIBLE; equal value: fewer squares first), then the other moves by distance");
    MoveList all = generate_moves(&b);
    check(text && count_prefix(text, "GROUP ") == (int)all.count, "X13 the combined list holds every legal move once");
    free(all.items);
    free(text);

    text = setup(&b, NULL) ? order_for(&b, 4) : NULL;
    check(text && has_line(text, "HEADS DISTANCE") && count_prefix(text, "GROUP 1 ") == 0 &&
              has_line(text, "GROUP 2 DISTANCE a2a3 SQUARES 1 ORDER IF") && has_line(text, "GROUP 2 DISTANCE a2a4 SQUARES 2 ORDER ELSE"),
          "X14 no capture available: back to the distance list (HEADS DISTANCE)");
    free(text);

    text = setup(&b, QUEEN) ? order_for(&b, 4) : NULL;
    check(text && has_line(text, "HEADS CAPTURE") && has_line(text, "GROUP 1 CAPTURE e4d5 TAKES P VALUE 1 SQUARES 1 MATERIAL FORCED NONNEGATIVE ORDER IF"),
          "X15 when a capture exists the capture list heads the order");
    free(text);
    text = setup(&b, "r1bqkb1r/pppp1Qpp/2n2n2/4p3/2B1P3/8/PPPP1PPP/RNB1K1NR b KQkq - 0 4") ? order_for(&b, 2) : NULL;
    check(text && has_line(text, "NO MOVES checkmate"), "X16 checkmated: NO MOVES");
    free(text);
}

/* Review 27: the transition between the two lists over successive positions. */
static void test_transition(void) {
    static Board b;
    static const char *const moves[] = {"e2e4", "d7d5", "e4e5", "f7f5"};
    static const char *const heads[] = {"HEADS DISTANCE", "HEADS DISTANCE", "HEADS CAPTURE", "HEADS DISTANCE",
                                        "HEADS CAPTURE"};
    bool ok = setup(&b, NULL);
    for (int i = 0; ok && i <= 4; ++i) {
        char *text = order_for(&b, 4);
        ok = text && has_line(text, heads[i]);
        if (ok && i == 4) {
            ok = has_line(text, "GROUP 1 CAPTURE e5f6 TAKES P VALUE 1 SQUARES 1 MATERIAL FORCED NONNEGATIVE ORDER IF");
        }
        if (!ok) {
            printf("  (position %d: expected %s)\n", i, heads[i]);
        }
        free(text);
        if (ok && i < 4) {
            Move m;
            parse_move_text(moves[i], &m);
            make_move(&b, m);
        }
    }
    check(ok, "X21 the list switches: start DISTANCE, after 1.e4 d5 CAPTURE (exd5), after 2.e5 DISTANCE (Black), after 2...f5 CAPTURE again (exf6 e.p.)");
}

static void test_risk(void) {
    static Board b;
    Goal g;
    TextBuffer out = {NULL, 0, 0};
    setup(&b, SCHOLAR);
    parse_goal("f7 2", &g);
    goal_show_risk = true;
    routes_report(&b, &g, &out);
    goal_show_risk = false;
    check(out.text && has_line(out.text, "ROUTE PIECE B c4 c4f7 OWN 1 PROTECTORS 0 ATTACKERS 1 AT-RISK 3") &&
              has_line(out.text, "ROUTE PIECE Q d1 d1h5 h5f7 OWN 2 PROTECTORS 1 ATTACKERS 1 AT-RISK 9"),
          "X17 risk on: the arriving piece's value when the goal square is attacked (the king attacks f7)");
    free(out.text);
    out.text = NULL;
    out.length = out.capacity = 0;
    routes_report(&b, &g, &out);
    check(out.text && has_line(out.text, "ROUTE PIECE B c4 c4f7 OWN 1 PROTECTORS 0 ATTACKERS 1") &&
              strstr(out.text, "AT-RISK") == NULL,
          "X18 risk off: the version 11 report is unchanged");
    free(out.text);
}

static void test_input_and_invariants(void) {
    int sq = 0, n = 0;
    check(parse_exchange("e5 4", &sq, &n) && sq == 3 * 8 + 4 && n == 4 && parse_exchange("a1 8", &sq, &n) &&
              !parse_exchange("e5", &sq, &n) && !parse_exchange("e5 0", &sq, &n) && !parse_exchange("e5 9", &sq, &n) &&
              !parse_exchange("x5 2", &sq, &n) && !parse_exchange("e5 2 3", &sq, &n) && !parse_exchange("e55 2", &sq, &n),
          "X19 exchange <square> N is read for N = 1..8; anything else is refused");
    static Board b, before;
    setup(&b, ROOK);
    before = b;
    char *a = exchange_for(&b, "d5", 4), *c = exchange_for(&b, "d5", 4), *o = order_for(&b, 4);
    check(memcmp(&before, &b, sizeof(Board)) == 0 && a && c && strcmp(a, c) == 0 && o,
          "X20 exchange and order are deterministic and leave the board unchanged");
    free(a);
    free(c);
    free(o);
}

int main(void) {
    test_values();
    test_rook();
    test_low_first_capture();
    test_edges();
    test_independent();
    test_order();
    test_transition();
    test_risk();
    test_input_and_invariants();
    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
