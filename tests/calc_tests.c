/* Stage 3A tests: the calculator's report (R) against the worked examples of
 * docs/CALCULATOR_SPEC.md, the special moves, the zero-move causes, the
 * count-once rule over a sequence, and the preview invariants. chess.c is
 * included unchanged, with its main renamed. Run by CTest. */
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

static Move mv(const char *text) {
    int sr = 0, sc = 0, er = 0, ec = 0;
    Move m = {0, 0, 0, 0, '\0'};
    if (parse_square(text, &sr, &sc) && parse_square(text + 2, &er, &ec)) {
        m.sr = sr;
        m.sc = sc;
        m.er = er;
        m.ec = ec;
        if (text[4] != '\0') {
            m.promotion = (char)toupper((unsigned char)text[4]);
        }
    }
    return m;
}

/* A game position with its tracker: set up from a FEN (or the start when
 * fen is NULL), then the moves in `played` finalized as F would do. */
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
        Move m = mv(played[i]);
        if (!legal_move(&g->board, m)) {
            printf("  (illegal move in test: %s)\n", played[i]);
            return false;
        }
        tracker_commit(&g->tracker, &g->board, m);
        make_move(&g->board, m);
        record_position(&g->board);
    }
    return true;
}

/* The report for `move` in game g; the caller frees the text. */
static char *report(const Game *g, const char *move) {
    TextBuffer out = {NULL, 0, 0};
    Move m = mv(move);
    if (!legal_move(&g->board, m)) {
        printf("  (illegal report move in test: %s)\n", move);
        return NULL;
    }
    calc_report(&g->board, &g->tracker, m, &out);
    return out.text;
}

/* Does the report contain this exact line? */
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

static void expect_lines(const char *text, const char *const *lines, int count, const char *what) {
    bool ok = text != NULL;
    for (int i = 0; ok && i < count; ++i) {
        if (!has_line(text, lines[i])) {
            printf("  (missing line: %s)\n", lines[i]);
            ok = false;
        }
    }
    check(ok, what);
}

static void expect_absent(const char *text, const char *line, const char *what) {
    check(text != NULL && !has_line(text, line), what);
}

/* Does some line of the report start with this text? */
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

/* E2: the whole report for 1.e4, line by line. */
static void test_e2(void) {
    static const char expected[] =
        "CALC BEGIN e2e4\n"
        "MOVED White P e2 e4\n"
        "ZERO White R a1 BLOCKED both\n"
        "ZERO White B c1 BLOCKED both\n"
        "ZERO White Q d1 BLOCKED before\n"
        "ZERO White K e1 BLOCKED before\n"
        "ZERO White B f1 BLOCKED before\n"
        "ZERO White R h1 BLOCKED both\n"
        "ZERO Black R a8 BLOCKED both\n"
        "ZERO Black B c8 BLOCKED both\n"
        "ZERO Black Q d8 BLOCKED both\n"
        "ZERO Black K e8 BLOCKED both\n"
        "ZERO Black B f8 BLOCKED both\n"
        "ZERO Black R h8 BLOCKED both\n"
        "LOST White P e2 e4 e3\n"
        "GAINED White P e2 e5\n"
        "GAINED White Q d1 h5 g4 f3 e2\n"
        "GAINED White K e1 e2\n"
        "GAINED White B f1 a6 b5 c4 d3 e2\n"
        "GAINED White N g1 e2\n"
        "DENIED White e4\n"
        "DENIED White e3\n"
        "OPENED White a6\n"
        "OPENED White b5\n"
        "OPENED White e5\n"
        "OPENED White h5\n"
        "OPENED White e2\n"
        "PROTECT- White Q d1 P e2\n"
        "PROTECT- White K e1 P e2\n"
        "PROTECT- White B f1 P e2\n"
        "PROTECT- White N g1 P e2\n"
        "COUNT White U1 3 U2 2 U3 2 GAINED 12 RESTORED 0 OPENED 5\n"
        "COUNT Black U1 6 U2 0 U3 0 GAINED 0 RESTORED 0 OPENED 0\n"
        "CALC END\n";
    Game g;
    char *text = game_setup(&g, NULL, NULL, 0) ? report(&g, "e2e4") : NULL;
    bool same = text != NULL && strcmp(text, expected) == 0;
    if (!same && text != NULL) {
        printf("  (actual report:)\n%s", text);
    }
    check(same, "E2 the whole report for 1.e4");
    free(text);
    free(g.board.history);
}

/* E3: 3.Qh5 in the Scholar's Mate position. */
static void test_e3(void) {
    static const char *const lines[] = {
        "MOVED White Q d1 h5",
        "ZERO Black P f7 PIN after",
        "LOST White Q d1 h5",
        "LOST Black P f7 f6 f5",
        "LOST Black P h7 h5",
        "GAINED White Q d1 f7 h7 g6 h6 e5 f5 g5 h4 h3 d1",
        "GAINED White K e1 d1",
        "DENIED White h5",
        "DENIED Black f5",
        "DENIED Black h5",
        "ATTACK+ White Q d1 P f7",
        "MATE1 White h5f7",
        "COUNT White U1 4 U2 1 U3 1 GAINED 11 RESTORED 0 OPENED 7",
        "COUNT Black U1 5 U2 3 U3 2 GAINED 0 RESTORED 0 OPENED 0"};
    Game g;
    char *text = game_setup(&g, SCHOLAR, NULL, 0) ? report(&g, "d1h5") : NULL;
    expect_lines(text, lines, (int)(sizeof(lines) / sizeof(lines[0])), "E3 3.Qh5: pin, lost moves, the one mate threat, counts");
    free(text);
    free(g.board.history);
}

/* E3 after 3.Qh5: 3...g6, with and without the game's history. */
static void test_e3_g6(void) {
    static const char *const played[] = {"d1h5"};
    static const char *const with_history[] = {
        "LOST White Q h5 f7",
        "LOST Black P g7 g6",
        "GAINED Black B f8 g7 h6",
        "GAINED Black P g7 h5",
        "RESTORED Black P f7 f6 f5",
        "DENIED Black g6",
        "PROTECT+ Black P f7 P g7",
        "PROTECT+ Black P h7 P g7",
        "PROTECT- Black B f8 P g7",
        "ATTACK+ Black P g7 Q h5",
        "ATTACK- White Q h5 P f7",
        "COUNT White U1 4 U2 1 U3 0 GAINED 0 RESTORED 0 OPENED 0",
        "COUNT Black U1 4 U2 1 U3 1 GAINED 3 RESTORED 2 OPENED 3"};
    Game g;
    char *text = game_setup(&g, SCHOLAR, played, 1) ? report(&g, "g7g6") : NULL;
    expect_lines(text, with_history, (int)(sizeof(with_history) / sizeof(with_history[0])),
                 "E3 3...g6 after 3.Qh5 was played: f7's moves come back as RESTORED");
    expect_absent(text, "MATE1 White h5f7", "E3 3...g6 stops the mate threat");
    free(text);
    free(g.board.history);

    /* The same position set up directly: no history, so they are GAINED. */
    Game fresh;
    text = game_setup(&fresh, "r1bqkbnr/pppp1ppp/2n5/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR b KQkq - 3 3", NULL, 0)
               ? report(&fresh, "g7g6") : NULL;
    static const char *const without_history[] = {
        "GAINED Black P f7 f6 f5",
        "COUNT Black U1 4 U2 1 U3 1 GAINED 5 RESTORED 0 OPENED 3"};
    expect_lines(text, without_history, 2, "E3 3...g6 from a FEN (no history): the same moves count as GAINED");
    free(text);
    free(fresh.board.history);
}

/* E3b: 3.Nf3 from the same position. */
static void test_e3b(void) {
    static const char *const lines[] = {
        "LOST White N g1 f3 h3 e2",
        "LOST White P f2 f4 f3",
        "LOST White Q d1 h5 g4 f3",
        "GAINED White N g1 e5 g5 d4 h4 g1",
        "GAINED White K e1 g1",
        "GAINED White R h1 f1 g1",
        "ATTACK+ White N g1 P e5",
        "COUNT White U1 4 U2 8 U3 3 GAINED 8 RESTORED 0 OPENED 3"};
    Game g;
    char *text = game_setup(&g, SCHOLAR, NULL, 0) ? report(&g, "g1f3") : NULL;
    expect_lines(text, lines, (int)(sizeof(lines) / sizeof(lines[0])), "E3b 3.Nf3: lost and gained moves, no threat");
    expect_absent(text, "MATE1 White h5f7", "E3b 3.Nf3 makes no mate threat");
    free(text);
    free(g.board.history);
}

/* E5: protection is not mobility. The c2 pawn protects d3 but cannot move
 * there; the rook a1 protects the knight b1 but cannot move there. */
static void test_e5(void) {
    Game g;
    bool ok = game_setup(&g, "4k3/8/8/8/8/3P4/2P5/RN2K3 w - - 0 1", NULL, 0);
    int c2r, c2c, d3r, d3c, a1r, a1c, b1r, b1c;
    parse_square("c2", &c2r, &c2c);
    parse_square("d3", &d3r, &d3c);
    parse_square("a1", &a1r, &a1c);
    parse_square("b1", &b1r, &b1c);
    bool d[8][8];
    legal_destination_map(&g.board, c2r, c2c, WHITE, d);
    check(ok && linked(&g.board, c2r, c2c, d3r, d3c, true) && !d[d3r][d3c], "E5 the c2 pawn protects d3 but cannot move there");
    legal_destination_map(&g.board, a1r, a1c, WHITE, d);
    check(ok && linked(&g.board, a1r, a1c, b1r, b1c, true) && !d[b1r][b1c], "E5 the rook protects the knight beside it but cannot move there");
    free(g.board.history);
}

static void test_special_moves(void) {
    static const char *const after_e4_d5[] = {"e2e4", "d7d5"};
    static const char *const capture[] = {
        "MOVED White P e4 d5", "CAPTURED Black P d5", "DENIED Black d4 by-capture", "DENIED Black e4 by-capture"};
    Game g;
    char *text = game_setup(&g, NULL, after_e4_d5, 2) ? report(&g, "e4d5") : NULL;
    expect_lines(text, capture, 4, "S1 capture: CAPTURED line, squares only the captured pawn reached are DENIED by-capture");
    free(text);
    free(g.board.history);

    static const char *const ep[] = {"MOVED White P e5 d6", "CAPTURED Black P d5"};
    text = game_setup(&g, "rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 3", NULL, 0) ? report(&g, "e5d6") : NULL;
    expect_lines(text, ep, 2, "S2 en passant: the pawn beside is CAPTURED");
    free(text);
    free(g.board.history);

    static const char *const castle[] = {"MOVED White K e1 g1", "CASTLE White h1 f1"};
    text = game_setup(&g, "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", NULL, 0) ? report(&g, "e1g1") : NULL;
    expect_lines(text, castle, 2, "S3 castling: CASTLE line with the rook's squares");
    free(text);
    free(g.board.history);

    static const char *const promo[] = {"MOVED White P a7 b8", "CAPTURED Black N b8", "PROMOTED White a7 b8 Q"};
    text = game_setup(&g, "1n2k3/P7/8/8/8/8/8/4K3 w - - 0 1", NULL, 0) ? report(&g, "a7b8q") : NULL;
    expect_lines(text, promo, 3, "S4 promotion with capture: PROMOTED and CAPTURED lines");
    free(text);
    free(g.board.history);
}

static void test_zero_causes(void) {
    Game g;
    char *text = game_setup(&g, "4k3/8/8/4p3/4P3/8/8/4K3 w - - 0 1", NULL, 0) ? report(&g, "e1d1") : NULL;
    check(text != NULL && has_line(text, "ZERO White P e4 BLOCKED both"), "Z1 a pawn facing a pawn is BLOCKED");
    free(text);
    free(g.board.history);

    text = game_setup(&g, "4k3/8/8/8/1b6/8/3N3P/4K3 w - - 0 1", NULL, 0) ? report(&g, "h2h3") : NULL;
    check(text != NULL && has_line(text, "ZERO White N d2 PIN both"), "Z2 a knight between its king and a bishop is PIN");
    free(text);
    free(g.board.history);

    text = game_setup(&g, "4k3/8/8/4q3/8/8/8/R3K3 w - - 0 1", NULL, 0) ? report(&g, "e1f1") : NULL;
    check(text != NULL && has_line(text, "ZERO White R a1 CHECK before"), "Z3 a rook that cannot help its checked king is CHECK");
    free(text);
    free(g.board.history);
}

/* The count-once rule over a game: the rook a1 loses a8, a7 when the bishop
 * blocks the a-file, gets them back when it leaves, and loses them again
 * when it returns. Two separate LOST events, one RESTORED, never a repeated
 * charge while they stay missing. */
static void test_charge_once(void) {
    static const char *const moves[] = {"b7a6", "e1f2", "a6b7", "f2e1", "b7a6"};
    static const char *const expected[] = {
        "LOST White R a1 a8 a7",    /* 1...Ba6: lost */
        "GAINED White R a1 e1 f1 g1 h1", /* 2.Kf2: new (never lost) */
        "RESTORED White R a1 a8 a7", /* 2...Bb7: back */
        "LOST White R a1 e1 f1 g1 h1",   /* 3.Ke1: first loss of these */
        "LOST White R a1 a8 a7"};   /* 3...Ba6: lost again, a new event */
    char what[96];
    for (int i = 0; i < 5; ++i) {
        Game g;
        char *text = game_setup(&g, "7k/1b6/8/8/8/8/8/R3K3 b - - 0 1", moves, i) ? report(&g, moves[i]) : NULL;
        snprintf(what, sizeof(what), "C%d move %s: %s", i + 1, moves[i], expected[i]);
        check(text != NULL && has_line(text, expected[i]), what);
        if (i == 1 || i == 3) {
            snprintf(what, sizeof(what), "C%d the still-missing a8, a7 are not charged again", i + 1);
            check(text != NULL && strstr(text, "a8 a7") == NULL, what);
        }
        free(text);
        free(g.board.history);
    }
}

static void test_invariants(void) {
    Game g;
    static const char *const played[] = {"e2e4", "e7e5"};
    bool ok = game_setup(&g, NULL, played, 2);
    char fen_before[FEN_TEXT_SIZE], fen_after[FEN_TEXT_SIZE];
    Tracker tracker_before = g.tracker;
    size_t history_before = g.board.history_size;
    board_to_fen(&g.board, fen_before);
    char *first = report(&g, "g1f3");
    char *second = report(&g, "g1f3");
    board_to_fen(&g.board, fen_after);
    check(ok && strcmp(fen_before, fen_after) == 0 && g.board.history_size == history_before,
          "I1 R leaves the real board, the turn and the history unchanged");
    check(memcmp(&tracker_before, &g.tracker, sizeof(Tracker)) == 0, "I2 R leaves the tracker unchanged");
    check(first != NULL && second != NULL && strcmp(first, second) == 0, "I3 the report is deterministic");
    check(first != NULL && strncmp(first, "CALC BEGIN g1f3\n", 16) == 0 &&
          strcmp(first + strlen(first) - 9, "CALC END\n") == 0, "I4 the report starts with CALC BEGIN and ends with CALC END");
    free(first);
    free(second);
    free(g.board.history);
}

/* Version 9 (spec v1.5): no MATE1 lines after a move that ends the game. */
static void test_game_over(void) {
    Game g;
    static const char *const played[] = {"d1h5", "g8f6"};
    char *text = game_setup(&g, SCHOLAR, played, 2) ? report(&g, "h5f7") : NULL;
    check(text != NULL && has_line(text, "CHECK Black") && !has_prefix(text, "MATE1 "),
          "C1 4.Qxf7# is checkmate: CHECK Black, no MATE1 lines");
    check(text != NULL && has_line(text, "CALC END") && has_prefix(text, "COUNT White ") &&
              has_prefix(text, "COUNT Black "),
          "C2 the rest of the checkmate report is still printed");
    free(text);
    free(g.board.history);

    text = game_setup(&g, "7k/5K2/8/8/8/8/8/6Q1 w - - 0 1", NULL, 0) ? report(&g, "g1g6") : NULL;
    check(text != NULL && !has_prefix(text, "MATE1 ") && !has_prefix(text, "CHECK "),
          "C3 Qg6 is stalemate: no MATE1 and no CHECK lines");
    free(text);
    free(g.board.history);

    text = game_setup(&g, SCHOLAR, NULL, 0) ? report(&g, "d1h5") : NULL;
    check(text != NULL && has_line(text, "MATE1 White h5f7"), "C4 a game that goes on keeps its MATE1 line (3.Qh5)");
    free(text);
    free(g.board.history);
}

int main(void) {
    test_e2();
    test_e3();
    test_e3_g6();
    test_e3b();
    test_e5();
    test_special_moves();
    test_zero_causes();
    test_charge_once();
    test_invariants();
    test_game_over();
    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
