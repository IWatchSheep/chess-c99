/* Stage 3C tests: the loop ("loop N", "loop N K") and "trace", against the
 * worked examples of docs/CALCULATOR_SPEC.md: the perft counts, Fool's Mate
 * (E6), the practice boards (E8), Scholar's Mate checked against the reply
 * view of 3B, forced versus possible, stalemates, pieces that can't move,
 * trace with the piece memory, refused input, and that nothing changes.
 * chess.c is included unchanged. Run by CTest. */
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

static void done(Game *g) {
    free(g->board.history);
}

static char *loop_for(const Game *g, int depth, int zero_k) {
    TextBuffer out = {NULL, 0, 0};
    loop_report(&g->board, depth, zero_k, &out);
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

#define COUNT_OF(a) ((int)(sizeof(a) / sizeof((a)[0])))

static const char *const SCHOLAR = "r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/8/PPPP1PPP/RNBQK1NR w KQkq - 2 3";

/* E6: from the start, 4 plies. Positions per ply are the published perft
 * counts; the only mates are the 8 Fool's Mates. */
static void test_start(void) {
    static const char *const lines[] = {
        "LOOP BEGIN White DEPTH 4",
        "PLY 1 POSITIONS 20 MATED White 0 Black 0 STALEMATES 0",
        "PLY 2 POSITIONS 400 MATED White 0 Black 0 STALEMATES 0",
        "PLY 3 POSITIONS 8902 MATED White 0 Black 0 STALEMATES 0",
        "PLY 4 POSITIONS 197281 MATED White 8 Black 0 STALEMATES 0",
        "MATE 4 f2f3 e7e5 g2g4 d8h4 MATED White",
        "MATE 4 f2f3 e7e6 g2g4 d8h4 MATED White",
        "MATE 4 f2f4 e7e5 g2g4 d8h4 MATED White",
        "MATE 4 f2f4 e7e6 g2g4 d8h4 MATED White",
        "MATE 4 g2g4 e7e5 f2f3 d8h4 MATED White",
        "MATE 4 g2g4 e7e5 f2f4 d8h4 MATED White",
        "MATE 4 g2g4 e7e6 f2f3 d8h4 MATED White",
        "MATE 4 g2g4 e7e6 f2f4 d8h4 MATED White",
        "FIRST f2f3 NOT-FOUND MATED White 2 Black 0",
        "FIRST f2f4 NOT-FOUND MATED White 2 Black 0",
        "FIRST g2g4 NOT-FOUND MATED White 4 Black 0",
        "FIRST e2e4 NOT-FOUND MATED White 0 Black 0",
        "RESULT White NOT-FOUND",
        "LIMIT 4",
        "LOOP END"};
    Game g;
    char *text = game_setup(&g, NULL, NULL, 0) ? loop_for(&g, 4, 0) : NULL;
    check(all_lines(text, lines, COUNT_OF(lines)), "L1 start, 4 plies: perft counts, the 8 Fool's Mates (E6), not found, LIMIT 4");
    check(text && count_prefix(text, "MATE ") == 8 && count_prefix(text, "FIRST ") == 20,
          "L2 exactly 8 MATE lines and one FIRST line per first move (20)");
    free(text);
    done(&g);
}

/* Kiwipete: castling, en passant and promotions; perft 48, 2039, 97862. */
static void test_kiwipete(void) {
    Game g;
    char *text = game_setup(&g, "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", NULL, 0)
                     ? loop_for(&g, 3, 0)
                     : NULL;
    check(text && strstr(text, "PLY 1 POSITIONS 48 ") && strstr(text, "PLY 2 POSITIONS 2039 ") &&
              strstr(text, "PLY 3 POSITIONS 97862 "),
          "L3 Kiwipete, 3 plies: 48, 2039, 97862 positions (published perft)");
    free(text);
    done(&g);
}

/* E3 / 3B: from the Scholar position, 3 plies. A first move's "possible in k
 * of r replies" must equal the reply view's LEAVES count for that move. */
static void test_scholar(void) {
    Game g;
    char *text = game_setup(&g, SCHOLAR, NULL, 0) ? loop_for(&g, 3, 0) : NULL;
    check(text && has_line(text, "FIRST d1h5 POSSIBLE 20 OF 28 MATED White 0 Black 22") &&
              has_line(text, "FIRST d1f3 POSSIBLE 21 OF 31 MATED White 0 Black 23") &&
              has_line(text, "RESULT White POSSIBLE") && has_line(text, "LIMIT 3"),
          "L4 Scholar, 3 plies: Qh5 possible in 20 of 28 replies, Qf3 in 21 of 31, not forced");

    bool agree = text != NULL;
    MoveList first = generate_moves(&g.board);
    for (size_t i = 0; agree && i < first.count; ++i) {
        TextBuffer view = {NULL, 0, 0};
        char move[6], want[64];
        int stops = 0, leaves = 0, no_threat = 0, allows = 0, replies = 0;
        replies_report(&g.board, &g.tracker, first.items[i], &view);
        const char *summary = view.text ? strstr(view.text, "SUMMARY REPLIES ") : NULL;
        if (summary == NULL || sscanf(summary, "SUMMARY REPLIES %d STOPS %d LEAVES %d NO-THREAT %d ALLOWS %d",
                                      &replies, &stops, &leaves, &no_threat, &allows) != 5) {
            agree = false;
        } else {
            int k = leaves + allows;
            move_text(first.items[i], move);
            if (k > 0) {
                sprintf(want, "FIRST %s POSSIBLE %d OF %d ", move, k, replies);
            } else {
                sprintf(want, "FIRST %s NOT-FOUND ", move);
            }
            if (strstr(text, want) == NULL) {
                printf("  (expected %s)\n", want);
                agree = false;
            }
        }
        free(view.text);
    }
    free(first.items);
    check(agree, "L5 for all 33 first moves, the loop agrees with the reply view (k = LEAVES + ALLOWS)");
    free(text);
    done(&g);

    static const char *const played[] = {"d1h5", "g8f6"};
    text = game_setup(&g, SCHOLAR, played, 2) ? loop_for(&g, 1, 0) : NULL;
    check(text && has_line(text, "FIRST h5f7 FORCED MATED White 0 Black 1") && has_line(text, "RESULT White FORCED") &&
              count_prefix(text, "LIMIT ") == 0,
          "L6 after 3.Qh5 Nf6, 1 ply: Qxf7 FORCED, no LIMIT line");
    free(text);
    done(&g);
}

/* E8: the practice boards. */
static void test_practice_boards(void) {
    Game g;
    char *text = game_setup(&g, "7k/8/5K2/8/8/8/8/Q7 w - - 0 1", NULL, 0) ? loop_for(&g, 1, 0) : NULL;
    check(text && has_line(text, "RESULT White NOT-FOUND") && has_line(text, "LIMIT 1"),
          "L7 K+Q vs K, 1 ply: no mate (E8)");
    free(text);
    text = loop_for(&g, 3, 0);
    check(text && has_line(text, "RESULT White FORCED") && has_line(text, "FIRST f6g6 FORCED MATED White 0 Black 2") &&
              count_prefix(text, "LIMIT ") == 0,
          "L8 K+Q vs K, 3 plies: mate forced (E8)");
    free(text);
    done(&g);

    text = game_setup(&g, "6k1/8/6K1/8/8/8/8/R7 w - - 0 1", NULL, 0) ? loop_for(&g, 1, 0) : NULL;
    check(text && has_line(text, "FIRST a1a8 FORCED MATED White 0 Black 1") &&
              has_line(text, "MATE 1 a1a8 MATED Black") && has_line(text, "RESULT White FORCED"),
          "L9 K+R vs K, 1 ply: Ra8 mate (E8)");
    free(text);
    done(&g);

    text = game_setup(&g, "4k3/5p2/8/8/2B5/8/8/3QK3 w - - 0 1", NULL, 0) ? loop_for(&g, 5, 0) : NULL;
    check(text && has_line(text, "RESULT White POSSIBLE") && has_line(text, "LIMIT 5") &&
              strstr(text, " FORCED ") == NULL,
          "L10 Scholar corner, 5 plies: mates possible, none forced (E8)");
    free(text);
    done(&g);
}

/* Forced is checked over every reply; a mate in some branches is only possible. */
static void test_forced_rule(void) {
    Game g;
    char *text = game_setup(&g, SCHOLAR, NULL, 0) ? loop_for(&g, 3, 0) : NULL;
    check(text && strstr(text, "FIRST d1h5 POSSIBLE") && strstr(text, " FORCED ") == NULL,
          "L11 3.Qh5 leads to 22 mates within 3 plies, yet it is POSSIBLE, not FORCED (8 replies stop it)");
    free(text);
    done(&g);

    /* Every first move marked FORCED really is: an independent check that
     * each reply has a mate in one. */
    text = game_setup(&g, "7k/8/5K2/8/8/8/8/Q7 w - - 0 1", NULL, 0) ? loop_for(&g, 3, 0) : NULL;
    bool ok = text != NULL;
    MoveList first = generate_moves(&g.board);
    for (size_t i = 0; ok && i < first.count; ++i) {
        char move[6], line[32];
        Board after = g.board;
        bool forced = true;
        make_move(&after, first.items[i]);
        MoveList replies = generate_moves(&after);
        if (replies.count == 0) {
            forced = is_in_check(&after, after.turn);
        }
        for (size_t j = 0; j < replies.count && forced; ++j) {
            Board next = after;
            make_move(&next, replies.items[j]);
            forced = count_mate_in_one(&next, WHITE) > 0;
        }
        free(replies.items);
        move_text(first.items[i], move);
        sprintf(line, "FIRST %s FORCED ", move);
        ok = (strstr(text, line) != NULL) == forced;
        if (!ok) {
            printf("  (disagree on %s)\n", move);
        }
    }
    free(first.items);
    check(ok, "L12 K+Q vs K, 3 plies: FORCED exactly for the moves after which every reply allows mate in one");
    free(text);
    done(&g);
}

static void test_stalemate_and_zero(void) {
    Game g;
    char *text = game_setup(&g, "7k/5K2/8/8/8/8/8/6Q1 w - - 0 1", NULL, 0) ? loop_for(&g, 1, 0) : NULL;
    check(text && has_line(text, "MATE 1 g1g7 MATED Black") && strstr(text, "STALEMATES 0") == NULL &&
              has_line(text, "FIRST g1g6 NOT-FOUND MATED White 0 Black 0"),
          "L13 K+Q vs K: Qg7 is mate, Qg6 stalemate (counted, NOT-FOUND)");
    free(text);
    done(&g);

    text = game_setup(&g, NULL, NULL, 0) ? loop_for(&g, 1, 6) : NULL;
    check(text && has_line(text, "LOOP BEGIN White DEPTH 1 ZERO 6") &&
              has_line(text, "PLY 1 POSITIONS 20 MATED White 0 Black 0 STALEMATES 0 ZERO 20"),
          "L14 after any first move Black has 6 pieces that can't move: ZERO 20 with K = 6");
    free(text);
    text = loop_for(&g, 1, 7);
    check(text && has_line(text, "PLY 1 POSITIONS 20 MATED White 0 Black 0 STALEMATES 0 ZERO 0"),
          "L15 ... and never 7: ZERO 0 with K = 7");
    free(text);
    done(&g);

    text = game_setup(&g, "r1bqkb1r/pppp1Qpp/2n2n2/4p3/2B1P3/8/PPPP1PPP/RNB1K1NR b KQkq - 0 4", NULL, 0)
               ? loop_for(&g, 2, 0)
               : NULL;
    check(text && has_line(text, "NO MOVES checkmate") && count_prefix(text, "PLY ") == 0,
          "L16 from a checkmated position: NO MOVES checkmate");
    free(text);
    done(&g);
    text = game_setup(&g, "7k/5K2/6Q1/8/8/8/8/8 b - - 0 1", NULL, 0) ? loop_for(&g, 2, 0) : NULL;
    check(text && has_line(text, "NO MOVES stalemate"), "L17 from a stalemate: NO MOVES stalemate");
    free(text);
    done(&g);
}

/* trace: each step equals the R report of that move in a game where the
 * earlier moves were finalized (so the memory is carried). */
static void test_trace(void) {
    static const char *const moves[] = {"e2e4", "e7e5", "f1c4", "b8c6", "d1h5", "g7g6"};
    Game g, step;
    TextBuffer out = {NULL, 0, 0};
    int bad = -1;
    bool ok = game_setup(&g, NULL, NULL, 0) &&
              trace_report(&g.board, &g.tracker, "e2e4 e7e5 f1c4 b8c6 d1h5 g7g6", &out, &bad) && bad == 0;
    check(ok && strncmp(out.text, "TRACE BEGIN 6\n", 14) == 0 && has_line(out.text, "TRACE END") &&
              count_prefix(out.text, "CALC BEGIN ") == 6,
          "L18 trace of 6 moves: framed, six reports");
    check(ok && has_line(out.text, "RESTORED Black P f7 f6 f5") &&
              has_line(out.text, "COUNT Black U1 4 U2 1 U3 1 GAINED 3 RESTORED 2 OPENED 3"),
          "L19 trace keeps the memory: 3...g6 gives f7 back its moves as RESTORED (E3)");

    bool same = ok;
    const char *at = ok ? out.text : NULL;
    for (int i = 0; same && i < COUNT_OF(moves); ++i) {
        TextBuffer one = {NULL, 0, 0};
        Move m;
        same = game_setup(&step, NULL, moves, i) && parse_move_text(moves[i], &m);
        if (same) {
            calc_report(&step.board, &step.tracker, m, &one);
            at = strstr(at, "TRACE PLY ");
            const char *report = at ? strchr(at, '\n') + 1 : NULL;
            same = report != NULL && strncmp(report, one.text, strlen(one.text)) == 0;
            at = report;
        }
        free(one.text);
        done(&step);
    }
    check(same, "L20 every trace step equals R in a game with the earlier moves finalized");
    free(out.text);

    out.text = NULL;
    out.length = out.capacity = 0;
    ok = trace_report(&g.board, &g.tracker, "e2e4 e7e5 f1c4 b8c6 d1h5 g8f6 h5f7", &out, &bad);
    const char *last = ok ? strstr(out.text, "TRACE PLY 7 h5f7") : NULL;
    check(last && strstr(last, "\nCHECK Black\n") && strstr(last, "\nMATE1 ") == NULL,
          "L21 trace of Scholar's Mate: the last step is check, with no MATE1 lines (version 9)");
    free(out.text);

    out.text = NULL;
    out.length = out.capacity = 0;
    check(!trace_report(&g.board, &g.tracker, "e2e4 e2e4", &out, &bad) && bad == 2 && out.text == NULL,
          "L22 an illegal second move is refused (move 2), nothing printed");
    check(!trace_report(&g.board, &g.tracker, "e2x4", &out, &bad) && bad == 1 && out.text == NULL &&
              !trace_report(&g.board, &g.tracker, "", &out, &bad) && bad == 1,
          "L23 a badly written or empty sequence is refused");
    done(&g);
}

static void test_input_and_invariants(void) {
    int n = 0, k = 0;
    check(parse_loop_args("3", &n, &k) && n == 3 && k == 0 && parse_loop_args("3 6", &n, &k) && n == 3 && k == 6 &&
              parse_loop_args("6", &n, &k),
          "L24 loop N and loop N K are read");
    check(!parse_loop_args("0", &n, &k) && !parse_loop_args("7", &n, &k) && !parse_loop_args("x", &n, &k) &&
              !parse_loop_args("3 0", &n, &k) && !parse_loop_args("3 x", &n, &k) &&
              !parse_loop_args("3 6 1", &n, &k) && !parse_loop_args("", &n, &k),
          "L25 bad depths and extra words are refused");

    Game g;
    static Board board_before;
    static Tracker tracker_before;
    static const char *const played[] = {"e2e4", "e7e5"};
    bool ok = game_setup(&g, NULL, played, 2);
    board_before = g.board;
    tracker_before = g.tracker;
    char *first = ok ? loop_for(&g, 3, 2) : NULL;
    char *second = ok ? loop_for(&g, 3, 2) : NULL;
    TextBuffer out = {NULL, 0, 0};
    int bad = 0;
    ok = ok && trace_report(&g.board, &g.tracker, "f1c4 b8c6", &out, &bad);
    check(ok && memcmp(&board_before, &g.board, sizeof(Board)) == 0 &&
              memcmp(&tracker_before, &g.tracker, sizeof(Tracker)) == 0,
          "L26 loop and trace leave the board, the turn, the history and the memory unchanged");
    check(first && second && strcmp(first, second) == 0, "L27 the loop is deterministic");
    check(first && strncmp(first, "LOOP BEGIN White DEPTH 3 ZERO 2\n", 32) == 0 &&
              strcmp(first + strlen(first) - 9, "LOOP END\n") == 0,
          "L28 framed by LOOP BEGIN and LOOP END");
    free(first);
    free(second);
    free(out.text);
    done(&g);
}

int main(void) {
    test_start();
    test_kiwipete();
    test_scholar();
    test_practice_boards();
    test_forced_rule();
    test_stalemate_and_zero();
    test_trace();
    test_input_and_invariants();
    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
