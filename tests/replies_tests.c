/* Stage 3B tests: the reply view ("replies") against the worked examples of
 * docs/CALCULATOR_SPEC.md: Scholar's Mate (28 replies, 8 stop the threat),
 * 3.Nf3 (no threat), a reply that allows a mate, a check, checkmate and
 * stalemate (no THREAT line once the game is over),
 * promotion replies, the fixed order, counts that agree with the R report,
 * and that nothing changes. chess.c is included unchanged. Run by CTest. */
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

typedef struct {
    Board board;
    Tracker tracker;
} Game;

static bool game_setup(Game *g, const char *fen, const char *const *played, int count) {
    const char *reason = NULL;
    memset(g, 0, sizeof(*g));
    reset_board(&g->board);
    if (fen != NULL && !load_fen(&g->board, fen, &reason)) {
        printf("  (FEN refused: %s)\n", reason);
        return false;
    }
    tracker_init(&g->tracker, &g->board);
    for (int i = 0; i < count; ++i) {
        Move m;
        if (!parse_move_text(played[i], &m) || !legal_move(&g->board, m)) {
            printf("  (illegal move in test: %s)\n", played[i]);
            return false;
        }
        tracker_commit(&g->tracker, &g->board, m);
        make_move(&g->board, m);
        record_position(&g->board);
    }
    return true;
}

static char *replies_for(const Game *g, const char *move) {
    TextBuffer out = {NULL, 0, 0};
    Move m;
    if (!parse_move_text(move, &m) || !legal_move(&g->board, m)) {
        printf("  (illegal preview move in test: %s)\n", move);
        return NULL;
    }
    replies_report(&g->board, &g->tracker, m, &out);
    return out.text;
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

/* Does some line start with this text? */
static bool has_prefix(const char *text, const char *prefix) {
    size_t n = strlen(prefix);
    for (const char *p = text; p != NULL && *p != '\0';) {
        if (strncmp(p, prefix, n) == 0) {
            return true;
        }
        const char *end = strchr(p, '\n');
        p = end ? end + 1 : NULL;
    }
    return false;
}

static const char *const SCHOLAR = "r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/8/PPPP1PPP/RNBQK1NR w KQkq - 2 3";

static void test_scholar(void) {
    Game g;
    char *text = game_setup(&g, SCHOLAR, NULL, 0) ? replies_for(&g, "d1h5") : NULL;
    check(text && has_line(text, "THREAT White h5f7"), "B1 3.Qh5 threatens exactly Qxf7 mate");
    check(text && has_line(text, "SUMMARY REPLIES 28 STOPS 8 LEAVES 20 NO-THREAT 0 ALLOWS 0"),
          "B2 28 replies: 8 stop the threat, 20 leave it");
    static const char *const stops[] = {"d8e7", "d8f6", "d8g5", "d8h4", "e8e7", "g8h6", "d7d5", "g7g6"};
    bool all = text != NULL;
    char prefix[32];
    for (int i = 0; all && i < 8; ++i) {
        snprintf(prefix, sizeof(prefix), "REPLY %s STOPS ", stops[i]);
        all = has_prefix(text, prefix);
    }
    check(all, "B3 the 8 stopping replies are Qe7, Qf6, Qg5, Qh4, Ke7, Nh6, d5, g6");
    check(text && has_prefix(text, "REPLY g8f6 LEAVES ") && has_prefix(text, "REPLY f8e7 LEAVES "),
          "B4 natural-looking Nf6 and Be7 leave the mate");
    check(text && has_line(text, "REPLY g7g6 STOPS White U1 4 U2 1 U3 0 GAINED 0 RESTORED 0 OPENED 0 "
                                 "Black U1 4 U2 1 U3 1 GAINED 3 RESTORED 2 OPENED 3"),
          "B5 3...g6: f7's two moves taken by 3.Qh5 come back as RESTORED");
    free(text);
    free(g.board.history);
}

static void test_no_threat_and_allows(void) {
    Game g;
    char *text = game_setup(&g, SCHOLAR, NULL, 0) ? replies_for(&g, "g1f3") : NULL;
    check(text && has_line(text, "THREAT White none") && has_prefix(text, "SUMMARY REPLIES ") &&
          strstr(text, " STOPS 0 LEAVES 0 ") != NULL, "B6 3.Nf3 makes no threat: no reply stops or leaves one");
    free(text);
    free(g.board.history);

    static const char *const played[] = {"e2e4", "f7f6"};
    text = game_setup(&g, NULL, played, 2) ? replies_for(&g, "d2d4") : NULL;
    check(text && has_line(text, "THREAT White none") && has_prefix(text, "REPLY g7g5 ALLOWS "),
          "B7 after 1.e4 f6 2.d4, the reply 2...g5 ALLOWS a mate in one (Qh5)");
    free(text);
    free(g.board.history);
}

static void test_game_end(void) {
    Game g;
    static const char *const played[] = {"d1h5", "g8f6"};
    char *text = game_setup(&g, SCHOLAR, played, 2) ? replies_for(&g, "h5f7") : NULL;
    check(text && has_line(text, "NO REPLIES checkmate") &&
          has_line(text, "SUMMARY REPLIES 0 STOPS 0 LEAVES 0 NO-THREAT 0 ALLOWS 0"), "B8 Qxf7 is mate: no replies");
    free(text);
    free(g.board.history);

    text = game_setup(&g, "7k/5K2/8/8/8/8/8/6Q1 w - - 0 1", NULL, 0) ? replies_for(&g, "g1g6") : NULL;
    check(text && has_line(text, "NO REPLIES stalemate"), "B9 Qg6 is stalemate: no replies");
    check(text && !has_prefix(text, "THREAT "), "B20 the game is over: no THREAT line (stalemate)");
    free(text);
    free(g.board.history);

    text = game_setup(&g, SCHOLAR, played, 2) ? replies_for(&g, "h5f7") : NULL;
    check(text && !has_prefix(text, "THREAT "), "B21 the game is over: no THREAT line (checkmate)");
    free(text);
    free(g.board.history);

    /* 1.e4 f6 2.Qh5+ : a check with exactly one answer. */
    text = game_setup(&g, "rnbqkbnr/ppppp1pp/5p2/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2", NULL, 0)
               ? replies_for(&g, "d1h5")
               : NULL;
    check(text && has_line(text, "THREAT White none") && has_prefix(text, "REPLY g7g6 NO-THREAT ") &&
              has_line(text, "SUMMARY REPLIES 1 STOPS 0 LEAVES 0 NO-THREAT 1 ALLOWS 0"),
          "B22 2.Qh5+ is check: the only reply is g7g6");
    free(text);
    free(g.board.history);
}

static void test_promotion_order(void) {
    Game g;
    char *text = game_setup(&g, "4k3/8/8/8/8/8/p7/4K2R w - - 0 1", NULL, 0) ? replies_for(&g, "h1h2") : NULL;
    const char *q = text ? strstr(text, "REPLY a2a1q ") : NULL;
    const char *r = text ? strstr(text, "REPLY a2a1r ") : NULL;
    const char *b = text ? strstr(text, "REPLY a2a1b ") : NULL;
    const char *n = text ? strstr(text, "REPLY a2a1n ") : NULL;
    check(q && r && b && n && q < r && r < b && b < n, "B10 the four promotion replies are listed Q, R, B, N");
    free(text);
    free(g.board.history);
}

/* Every REPLY line must be in the fixed order, and its counts must equal the
 * COUNT lines of that reply's full report (replies <move>). */
static void test_order_and_consistency(void) {
    Game g;
    Move m;
    bool ok = game_setup(&g, SCHOLAR, NULL, 0) && parse_move_text("d1h5", &m);
    char *text = ok ? replies_for(&g, "d1h5") : NULL;
    bool ordered = text != NULL;
    bool consistent = text != NULL;
    int lines = 0;
    Move previous = {0, 0, 0, 0, '\0'};
    for (const char *p = text; p && *p; p = strchr(p, '\n') ? strchr(p, '\n') + 1 : NULL) {
        if (strncmp(p, "REPLY ", 6) != 0) {
            continue;
        }
        char move[6] = {0};
        sscanf(p + 6, "%5s", move);
        Move r;
        if (!parse_move_text(move, &r)) {
            consistent = false;
            break;
        }
        if (lines > 0 && compare_moves(&previous, &r) >= 0) {
            ordered = false;
        }
        previous = r;
        ++lines;
        TextBuffer detail = {NULL, 0, 0};
        if (!reply_detail(&g.board, &g.tracker, m, r, &detail)) {
            consistent = false;
            break;
        }
        int w[6], k[6];
        const char *cw = strstr(detail.text, "COUNT White ");
        const char *cb = strstr(detail.text, "COUNT Black ");
        const char *rw = strstr(p, " White U1 ");
        bool parsed = cw && cb && rw &&
                      sscanf(cw, "COUNT White U1 %d U2 %d U3 %d GAINED %d RESTORED %d OPENED %d",
                             &w[0], &w[1], &w[2], &w[3], &w[4], &w[5]) == 6 &&
                      sscanf(cb, "COUNT Black U1 %d U2 %d U3 %d GAINED %d RESTORED %d OPENED %d",
                             &k[0], &k[1], &k[2], &k[3], &k[4], &k[5]) == 6;
        char expected[200];
        if (parsed) {
            snprintf(expected, sizeof(expected),
                     " White U1 %d U2 %d U3 %d GAINED %d RESTORED %d OPENED %d Black U1 %d U2 %d U3 %d GAINED %d RESTORED %d OPENED %d\n",
                     w[0], w[1], w[2], w[3], w[4], w[5], k[0], k[1], k[2], k[3], k[4], k[5]);
        }
        if (!parsed || strncmp(rw, expected, strlen(expected)) != 0) {
            printf("  (counts differ for reply %s)\n", move);
            consistent = false;
        }
        free(detail.text);
    }
    check(ordered && lines == 28, "B11 the 28 replies are in the fixed order (start square, end square, promotion)");
    check(consistent, "B12 every reply's counts equal the COUNT lines of its full report");
    free(text);
    free(g.board.history);
}

/* "replies g7g6" (3.Qh5 previewed) gives exactly the R report of 3...g6 in a
 * game where 3.Qh5 was played. */
static void test_detail(void) {
    Game previewed, played_game;
    static const char *const played[] = {"d1h5"};
    Move m, r;
    TextBuffer detail = {NULL, 0, 0}, direct = {NULL, 0, 0};
    bool ok = game_setup(&previewed, SCHOLAR, NULL, 0) && game_setup(&played_game, SCHOLAR, played, 1) &&
              parse_move_text("d1h5", &m) && parse_move_text("g7g6", &r) &&
              reply_detail(&previewed.board, &previewed.tracker, m, r, &detail);
    if (ok) {
        calc_report(&played_game.board, &played_game.tracker, r, &direct);
    }
    check(ok && direct.text && strcmp(detail.text, direct.text) == 0,
          "B13 replies g7g6 = the R report of 3...g6 after 3.Qh5 was played");
    Move bad;
    check(parse_move_text("g7g4", &bad) && !reply_detail(&previewed.board, &previewed.tracker, m, bad, &detail),
          "B14 an illegal reply is refused");
    check(!parse_move_text("g7", &bad) && !parse_move_text("g7g6x", &bad), "B15 badly written replies are refused");
    free(detail.text);
    free(direct.text);
    free(previewed.board.history);
    free(played_game.board.history);
}

static void test_invariants(void) {
    Game g;
    static const char *const played[] = {"e2e4", "e7e5"};
    bool ok = game_setup(&g, NULL, played, 2);
    char before[FEN_TEXT_SIZE], after[FEN_TEXT_SIZE];
    Tracker tracker_before = g.tracker;
    size_t history_before = g.board.history_size;
    board_to_fen(&g.board, before);
    char *first = replies_for(&g, "g1f3");
    char *second = replies_for(&g, "g1f3");
    board_to_fen(&g.board, after);
    check(ok && strcmp(before, after) == 0 && g.board.history_size == history_before,
          "B16 replies leaves the real board, the turn and the history unchanged");
    check(memcmp(&tracker_before, &g.tracker, sizeof(Tracker)) == 0, "B17 replies leaves the tracker unchanged");
    check(first && second && strcmp(first, second) == 0, "B18 the reply view is deterministic");
    check(first && strncmp(first, "REPLIES BEGIN g1f3\n", 19) == 0 &&
          strcmp(first + strlen(first) - 12, "REPLIES END\n") == 0, "B19 framed by REPLIES BEGIN and REPLIES END");
    free(first);
    free(second);
    free(g.board.history);
}

int main(void) {
    test_scholar();
    test_no_threat_and_allows();
    test_game_end();
    test_promotion_order();
    test_order_and_consistency();
    test_detail();
    test_invariants();
    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
