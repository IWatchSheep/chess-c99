/* Stage 3F tests: "compare N m1 m2 ..." (comparing candidate moves),
 * against hand-checked positions: raw facts, STRONGER-CONDITION / TIE /
 * NOT-COMPARABLE, the owner's IF chains (EARLIER-IN-ORDER /
 * SAME-ORDER-LEVEL), AGREEMENT (ALL-AHEAD, MIXED, ALL-TIE), the LOOP-1 /
 * LOOP-2 views, NEURON-STATE UNDEFINED, a goal, promotion, castling, en
 * passant, a candidate that ends the game, missing values ("-", never
 * zero), refused input, and that nothing changes. chess.c is included
 * unchanged. Run by CTest. */
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

static Board board;
static Tracker tracker;

static bool setup(const char *fen) {
    const char *reason = NULL;
    free(board.history);
    memset(&board, 0, sizeof(board));
    reset_board(&board);
    if (fen != NULL && !load_fen(&board, fen, &reason)) {
        printf("  (FEN refused: %s)\n", reason);
        return false;
    }
    tracker_init(&tracker, &board);
    return true;
}

/* compare with the text "N m1 m2 ..." and an optional goal ("f7 2"). */
static char *compare_for(const char *args, const char *goal) {
    Move moves[COMPARE_MAX];
    int depth = 0, count = 0;
    Goal g;
    TextBuffer out = {NULL, 0, 0};
    memset(&g, 0, sizeof(g));
    if (goal != NULL && !parse_goal(goal, &g)) {
        return NULL;
    }
    if (!parse_compare(&board, args, &depth, moves, &count)) {
        printf("  (compare refused: %s)\n", args);
        return NULL;
    }
    compare_report(&board, &tracker, &g, depth, moves, count, &out);
    return out.text;
}

/* A rook on d5 (protected by e6) and a loose pawn on b3. */
static const char *const ROOK_PAWN = "6k1/8/4p3/3r4/4P3/1pN5/8/3Q2K1 w - - 0 1";
static const char *const SCHOLAR = "r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/8/PPPP1PPP/RNBQK1NR w KQkq - 2 3";

static void test_values_and_views(void) {
    static const char *const lines[] = {
        "COMPARE BEGIN White DEPTH 3 CANDIDATES d1d5 d1b3 e4d5",
        "CANDIDATE d1d5",
        "FACT CAPTURE R VALUE 5",
        "FACT MATERIAL POSSIBLE NONNEGATIVE",
        "FACT VALUE-ORDER IF",
        "FACT SQUARES-ORDER 4 ELSE",
        "VIEW LOOP-1 SQUARES 4 ORDER ELSE STOP d5 CAPTURE",
        "VIEW LOOP-2 VALUE 5 ORDER IF MATERIAL POSSIBLE NONNEGATIVE",
        "CANDIDATE d1b3",
        "FACT CAPTURE P VALUE 1",
        "FACT MATERIAL FORCED NONNEGATIVE",
        "FACT VALUE-ORDER ELSE",
        "FACT GROUP 1 CAPTURE FORCED ELSE",
        "COMPARE d1d5 d1b3 MATERIAL POSSIBLE NONNEGATIVE | FORCED NONNEGATIVE STRONGER-CONDITION d1b3",
        "COMPARE d1d5 d1b3 VALUE-ORDER IF | ELSE EARLIER-IN-ORDER d1d5",
        "COMPARE d1d5 d1b3 SQUARES-ORDER 4 ELSE | 2 ELSE IF EARLIER-IN-ORDER d1b3",
        "AGREEMENT d1d5 d1b3 ALL-AHEAD d1b3",
        "VIEWS d1d5 d1b3 LOOP-1 d1b3 LOOP-2 d1d5 DIFFERENT-VIEW",
        "COMPARE d1d5 e4d5 VALUE-ORDER IF | IF SAME-ORDER-LEVEL",
        "VIEWS d1d5 e4d5 LOOP-1 e4d5 LOOP-2 e4d5 SAME-VIEW",
        "AGREEMENT d1b3 e4d5 ALL-TIE",
        "COMPARE END"};
    char *text = setup(ROOK_PAWN) ? compare_for("3 d1d5 d1b3 e4d5", NULL) : NULL;
    check(all_lines(text, lines, COUNT_OF(lines)),
          "F1 a higher capture value (the rook, 5) with a weaker material condition (POSSIBLE) than the pawn capture (FORCED)");
    check(text && count_prefix(text, "PAIR ") == 3 && count_prefix(text, "NEURON-STATE UNDEFINED") == 3 &&
              count_prefix(text, "CANDIDATE ") == 3,
          "F2 one block per candidate, every pair once, NEURON-STATE UNDEFINED for each");
    free(text);
}

static void test_mixed_and_ahead(void) {
    char *text = setup("6k1/5ppp/4p3/3r4/4P3/1pN5/5PPP/3Q2K1 w - - 0 1") ? compare_for("3 e4d5 d1d5", NULL) : NULL;
    check(text && has_line(text, "COMPARE e4d5 d1d5 LOOP NOT-FOUND | POSSIBLE 4 OF 11 STRONGER-CONDITION d1d5") &&
              has_line(text, "COMPARE e4d5 d1d5 MATERIAL FORCED NONNEGATIVE | POSSIBLE NONNEGATIVE STRONGER-CONDITION e4d5") &&
              has_line(text, "AGREEMENT e4d5 d1d5 MIXED"),
          "F3 MIXED: Qxd5 makes a mate possible (4 of 11), exd5 is forced nonnegative in material - kept incomparable");
    free(text);
    text = setup("r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 4 4") ? compare_for("1 h5h7 h5f7", NULL)
                                                                                     : NULL;
    check(text && has_line(text, "COMPARE h5h7 h5f7 LOOP NOT-FOUND | FORCED STRONGER-CONDITION h5f7") &&
              has_line(text, "AGREEMENT h5h7 h5f7 ALL-AHEAD h5f7") && has_line(text, "FACT THREAT GAME-OVER checkmate") &&
              has_line(text, "FACT REPLIES -"),
          "F4 Qxf7# is FORCED (checkmate): ALL-AHEAD; a candidate that ends the game has no replies (\"-\", not 0)");
    free(text);
}

static void test_scholar_and_goal(void) {
    char *text = setup(SCHOLAR) ? compare_for("3 d1h5 d1f3 g1f3", NULL) : NULL;
    check(text && has_line(text, "COMPARE d1h5 d1f3 LOOP POSSIBLE 20 OF 28 | POSSIBLE 21 OF 31 TIE") &&
              has_line(text, "COMPARE d1h5 d1f3 MATERIAL - | - NOT-COMPARABLE") &&
              has_line(text, "AGREEMENT d1h5 d1f3 ALL-TIE") &&
              has_line(text, "COMPARE d1h5 g1f3 LOOP POSSIBLE 20 OF 28 | NOT-FOUND STRONGER-CONDITION d1h5") &&
              has_line(text, "COMPARE d1h5 g1f3 THREAT White h5f7 | White none DIFFERENT") &&
              has_line(text, "COMPARE d1h5 g1f3 VALUE-ORDER none | none NOT-COMPARABLE") &&
              has_line(text, "FACT VALUE-ORDER none") && has_line(text, "FACT MATERIAL -"),
          "F5 E3: both queen moves POSSIBLE (k of r kept, a TIE in strength); no capture = missing, NOT-COMPARABLE");
    free(text);
    text = compare_for("3 d1h5 g1f3", "f7 2");
    check(text && has_line(text, "GOAL f7 REAL DEPTH 3") && has_line(text, "FACT GOAL f7 REACH CONDITION IF FORCED") &&
              has_line(text, "COMPARE d1h5 g1f3 GOAL-f7-REACH CONDITION IF FORCED | CONDITION IF POSSIBLE 30 OF 31 STRONGER-CONDITION d1h5"),
          "F6 with a goal: the REAL goal results are compared as condition strengths");
    free(text);
}

static void test_special_moves(void) {
    char *text = setup("1n2k3/P7/8/8/8/8/8/4K3 w - - 0 1") ? compare_for("1 a7a8q a7a8n a7b8q", NULL) : NULL;
    check(text && has_line(text, "COMPARE BEGIN White DEPTH 1 CANDIDATES a7a8q a7a8n a7b8q") &&
              has_line(text, "FACT CAPTURE N VALUE 3") && count_prefix(text, "FACT CAPTURE none") == 2 &&
              has_line(text, "COMPARE a7a8q a7a8n SQUARES-ORDER 1 IF | 1 IF SAME-ORDER-LEVEL"),
          "F7 promotion: three different candidates (to queen, to knight, capturing to queen); equal distance = same level");
    free(text);
    text = setup("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1") ? compare_for("1 e1g1 e1f1", NULL) : NULL;
    check(text && has_line(text, "VIEW LOOP-1 SQUARES 2 ORDER ELSE IF STOP g1 EMPTY") &&
              has_line(text, "COMPARE e1g1 e1f1 SQUARES-ORDER 2 ELSE IF | 1 IF EARLIER-IN-ORDER e1f1"),
          "F8 castling: the king's 2 squares (ELSE IF: the rooks can travel up to 7, so 2 is a middle level)");
    free(text);
    text = setup("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1") ? compare_for("1 e5d6 e5e6", NULL) : NULL;
    check(text && has_line(text, "VIEW LOOP-1 SQUARES 1 ORDER IF STOP d6 CAPTURE") && has_line(text, "FACT CAPTURE P VALUE 1"),
          "F9 en passant: a capture of value 1 on d6");
    free(text);
}

static void test_input_and_invariants(void) {
    Move m[COMPARE_MAX];
    int depth = 0, count = 0;
    setup(NULL);
    check(parse_compare(&board, "2 e2e4 d2d4", &depth, m, &count) && depth == 2 && count == 2 &&
              parse_compare(&board, "1 a2a3 b2b3 c2c3 d2d3 e2e3 f2f3", &depth, m, &count) && count == 6,
          "F10 compare N with 2 to 6 moves is read");
    check(!parse_compare(&board, "0 e2e4 d2d4", &depth, m, &count) && !parse_compare(&board, "6 e2e4 d2d4", &depth, m, &count) &&
              !parse_compare(&board, "2 e2e4", &depth, m, &count) &&
              !parse_compare(&board, "1 a2a3 b2b3 c2c3 d2d3 e2e3 f2f3 g2g3", &depth, m, &count) &&
              !parse_compare(&board, "2 e2e4 e2e4", &depth, m, &count) && !parse_compare(&board, "2 e2e5 d2d4", &depth, m, &count) &&
              !parse_compare(&board, "2 e7e5 d2d4", &depth, m, &count) && !parse_compare(&board, "x e2e4 d2d4", &depth, m, &count),
          "F11 refused: bad depth, fewer than 2 or more than 6 moves, a repeated move, an illegal move, the other side's move");
    static Board before;
    static Tracker before_t;
    setup(SCHOLAR);
    before = board;
    before_t = tracker;
    char *a = compare_for("2 d1h5 c4f7", "f7 2"), *b = compare_for("2 d1h5 c4f7", "f7 2");
    check(a && b && strcmp(a, b) == 0 && memcmp(&before, &board, sizeof(Board)) == 0 &&
              memcmp(&before_t, &tracker, sizeof(Tracker)) == 0,
          "F12 deterministic; the board, turn, history and memory unchanged");
    check(a && strncmp(a, "COMPARE BEGIN ", 14) == 0 && strcmp(a + strlen(a) - 12, "COMPARE END\n") == 0 &&
              strstr(a, "not the quality of a move") != NULL && strstr(a, "AHEAD d") == NULL,
          "F13 framed; the NOTE says a stronger condition is not move quality; no plain AHEAD lines");
    free(a);
    free(b);
}

int main(void) {
    test_values_and_views();
    test_mixed_and_ahead();
    test_scholar_and_goal();
    test_special_moves();
    test_input_and_invariants();
    free(board.history);
    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
