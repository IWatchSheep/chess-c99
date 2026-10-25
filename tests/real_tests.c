/* Stage 3D-2 tests: REAL goal searches ("routes real N": the other side
 * answers with every legal move; FORCED / POSSIBLE k OF r / ELSE), support
 * follow-ups ("support") and the second goal ("goal2"), against the E3
 * position, hand-checked facts and an independent count; the separate PASS
 * and REAL labels; refused input; and that nothing changes. chess.c is
 * included unchanged. Run by CTest. */
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

static Move mv(const char *text) {
    Move m = {0, 0, 0, 0, '\0'};
    parse_move_text(text, &m);
    return m;
}

static char *real_for(const Board *b, const char *goal, int depth) {
    Goal g;
    TextBuffer out = {NULL, 0, 0};
    if (!parse_goal(goal, &g)) {
        return NULL;
    }
    routes_real_report(b, &g, depth, &out);
    return out.text;
}

static const char *const SCHOLAR = "r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/8/PPPP1PPP/RNBQK1NR w KQkq - 2 3";

static void test_real_scholar(void) {
    static Board b;
    static const char *const one[] = {
        "ROUTES BEGIN White GOAL f7 DEPTH 1 REAL",
        "NOTE REAL: Black answers with every legal move; FORCED = reached whatever Black does",
        "GOAL f7 REACH",
        "FIRST c4f7 CONDITION IF FORCED MOVES 1",
        "FIRST d1h5 CONDITION ELSE",
        "RESULT f7 REACH CONDITION IF FORCED",
        "RESULT f7 PROTECTED CONDITION ELSE",
        "SQUARE f7 REACH CONDITION IF FORCED PROTECTED CONDITION ELSE",
        "FIRST-IF REACH f7 FORCED",
        "FIRST-IF PROTECTED none (ELSE)",
        "LIMIT 1",
        "ROUTES END"};
    char *text = setup(&b, SCHOLAR) ? real_for(&b, "f7 2", 1) : NULL;
    check(all_lines(text, one, COUNT_OF(one)), "R1 depth 1: Bxf7 reaches f7 (FORCED), nothing arrives protected");
    free(text);

    static const char *const three[] = {
        "FIRST d1h5 CONDITION IF FORCED",
        "FIRST d1f3 CONDITION IF FORCED",
        "FIRST a2a3 CONDITION IF POSSIBLE 30 OF 31",
        "FIRST c4f7 CONDITION IF POSSIBLE 1 OF 2",
        "FIRST d1h5 CONDITION IF POSSIBLE 26 OF 28",
        "FIRST d1f3 CONDITION IF POSSIBLE 26 OF 31",
        "RESULT f7 PROTECTED CONDITION IF POSSIBLE",
        "FIRST-IF PROTECTED f7 POSSIBLE",
        "LIMIT 3"};
    text = real_for(&b, "f7 2", 3);
    check(all_lines(text, three, COUNT_OF(three)),
          "R2 depth 3: after Qh5 reaching f7 is FORCED, arriving protected only POSSIBLE (26 of 28: ...g6 and ...d5 block)");
    const char *protected_part = text ? strstr(text, "GOAL f7 PROTECTED") : NULL;
    check(protected_part && strstr(protected_part, "FIRST d1h5 CONDITION IF POSSIBLE 26 OF 28") &&
              strstr(text, "FIRST d1h5 CONDITION IF FORCED") < protected_part,
          "R3 the same move is FORCED for REACH and POSSIBLE for PROTECTED: the two conditions stay separate");

    /* Independent count at depth 3: for each first move that does not
     * reach at once, k = replies after which one White move satisfies the
     * condition; FORCED when that holds for every reply. */
    bool agree = text != NULL;
    MoveList first = generate_moves(&b);
    for (int c = 0; agree && c < 2; ++c) {
        const char *part = strstr(text, c == 0 ? "GOAL f7 REACH" : "GOAL f7 PROTECTED");
        for (size_t i = 0; agree && i < first.count; ++i) {
            Board next = b;
            char move[6], want[64];
            make_move(&next, first.items[i]);
            move_text(first.items[i], move);
            GoalCondition cond = c == 0 ? COND_REACH : COND_PROTECTED;
            if (goal_holds(&next, WHITE, 5 + 1 * 8, cond)) {
                sprintf(want, "FIRST %s CONDITION IF FORCED MOVES 1\n", move);
            } else {
                MoveList replies = generate_moves(&next);
                int k = 0;
                for (size_t j = 0; j < replies.count; ++j) {
                    Board after = next;
                    make_move(&after, replies.items[j]);
                    MoveList own = generate_moves(&after);
                    bool can = false;
                    for (size_t x = 0; x < own.count && !can; ++x) {
                        Board end = after;
                        make_move(&end, own.items[x]);
                        can = goal_holds(&end, WHITE, 5 + 1 * 8, cond);
                    }
                    free(own.items);
                    k += can;
                }
                if (k > 0 && k == (int)replies.count) {
                    sprintf(want, "FIRST %s CONDITION IF FORCED\n", move);
                } else if (k > 0) {
                    sprintf(want, "FIRST %s CONDITION IF POSSIBLE %d OF %d\n", move, k, (int)replies.count);
                } else {
                    sprintf(want, "FIRST %s CONDITION ELSE\n", move);
                }
                free(replies.items);
            }
            const char *found = strstr(part, want);
            const char *next_goal = strstr(part + 1, "RESULT f7");
            agree = found != NULL && found < next_goal;
            if (!agree) {
                printf("  (expected %s)", want);
            }
        }
    }
    free(first.items);
    check(agree, "R4 for all 33 first moves and both conditions, REAL depth 3 agrees with an independent count");
    free(text);
}

static void test_real_rules(void) {
    static Board b;
    char *text = setup(&b, "6k1/8/6K1/8/8/8/8/R7 w - - 0 1") ? real_for(&b, "b8 2", 3) : NULL;
    check(text && has_line(text, "FIRST a1a8 CONDITION ELSE") && has_line(text, "FIRST a1b1 CONDITION IF FORCED"),
          "R5 Ra8 is checkmate on a8, not b8: the game ends, so it is ELSE (PASS would continue); Rb1 forces Rb8");
    free(text);
    text = setup(&b, "r1bqkb1r/pppp1Qpp/2n2n2/4p3/2B1P3/8/PPPP1PPP/RNB1K1NR b KQkq - 0 4") ? real_for(&b, "e7 1", 2) : NULL;
    check(text && has_line(text, "NO MOVES checkmate"), "R6 from a checkmated position: NO MOVES checkmate");
    free(text);

    int n = 0;
    check(parse_routes_real("real 3", &n) && n == 3 && parse_routes_real("real 6", &n) && !parse_routes_real("real 0", &n) &&
              !parse_routes_real("real 7", &n) && !parse_routes_real("real", &n) && !parse_routes_real("real 3 x", &n) &&
              !parse_routes_real("x", &n) && !parse_routes_real("real x", &n),
          "R7 routes real N is read for N = 1..6; anything else is refused");
}

static void test_support(void) {
    static Board b;
    TextBuffer out = {NULL, 0, 0};
    static const char *const e4[] = {
        "SUPPORT BEGIN e2e4 PASS",
        "NOTE PASS is hypothetical: White moves again, Black passes",
        "SUPPORT NOW P e4 PROTECTORS 0",
        "FOLLOW d2d3 P d2 PROTECTS P e4 PROTECTORS 1",
        "FOLLOW f2f3 P f2 PROTECTS P e4 PROTECTORS 1",
        "FOLLOW b1c3 N b1 PROTECTS P e4 PROTECTORS 1",
        "FOLLOW d1g4 Q d1 PROTECTS P e4 PROTECTORS 1",
        "FOLLOW d1f3 Q d1 PROTECTS P e4 PROTECTORS 1",
        "FOLLOW d1e2 Q d1 PROTECTS P e4 PROTECTORS 1",
        "FOLLOW f1d3 B f1 PROTECTS P e4 PROTECTORS 1",
        "SUMMARY FOLLOWS 7",
        "SUPPORT END"};
    setup(&b, NULL);
    support_report(&b, mv("e2e4"), &out);
    check(all_lines(out.text, e4, COUNT_OF(e4)) && count_prefix(out.text, "FOLLOW ") == 7,
          "R8 after 1.e4 (the pawn unprotected): exactly the 7 moves that protect it next");
    free(out.text);

    out.text = NULL;
    out.length = out.capacity = 0;
    support_report(&b, mv("d2d4"), &out);
    check(out.text && has_line(out.text, "SUPPORT NOW P d4 PROTECTORS 1") && strstr(out.text, "FOLLOW d1d2") == NULL &&
              strstr(out.text, "FOLLOW d1d3") == NULL && has_line(out.text, "FOLLOW c2c3 P c2 PROTECTS P d4 PROTECTORS 2"),
          "R9 after 1.d4 the queen already protects d4: Qd2 or Qd3 add no new link and are not listed; c3 is");
    free(out.text);

    out.text = NULL;
    out.length = out.capacity = 0;
    setup(&b, SCHOLAR);
    support_report(&b, mv("d1h5"), &out);
    check(out.text && has_line(out.text, "FOLLOW c4f7 B c4 PROTECTS Q h5 PROTECTORS 1") &&
              has_line(out.text, "FOLLOW c4e2 B c4 PROTECTS Q h5 PROTECTORS 1") &&
              has_line(out.text, "FOLLOW g2g4 P g2 PROTECTS Q h5 PROTECTORS 1") && has_line(out.text, "SUMMARY FOLLOWS 3"),
          "R10 after 3.Qh5: Bxf7, Be2 and g4 protect the queen");
    free(out.text);
}

static char *routes_with_second(const Board *b, const char *first, const char *second) {
    Goal g1, g2;
    TextBuffer out = {NULL, 0, 0};
    if (!parse_goal(first, &g1) || !parse_goal(second, &g2)) {
        return NULL;
    }
    routes_report_with(b, &g1, &g2, &out);
    return out.text;
}

static void test_second_goal(void) {
    static Board b;
    static const char *const lines[] = {
        "SECOND GOAL c4 LIMIT 2 PASS AFTER f7",
        "FROM START SQUARE c4 REACH CONDITION IF MOVES 0 PROTECTED CONDITION IF MOVES 1",
        "FROM c4f7 SQUARE c4 REACH CONDITION IF MOVES 1 PROTECTED CONDITION IF MOVES 1",
        "FROM d1h5 h5f7 SQUARE c4 REACH CONDITION IF MOVES 0 PROTECTED CONDITION IF MOVES 0",
        "FROM d1f3 f3f7 SQUARE c4 REACH CONDITION IF MOVES 0 PROTECTED CONDITION IF MOVES 0",
        "SECOND END"};
    char *text = setup(&b, SCHOLAR) ? routes_with_second(&b, "f7 4", "c4 2") : NULL;
    check(all_lines(text, lines, COUNT_OF(lines)),
          "R11 second goal c4: 0 moves from the start; 1 after Bxf7 (the bishop left); 0 and protected after Qxf7");
    check(text && count_prefix(text, "FROM ") == 8 && count_prefix(text, "FROM d1h5 h5f7 ") == 1,
          "R12 every non-dominated route to f7 once (no repeats), plus the start");
    check(text && strstr(text, "SECOND GOAL") > strstr(text, "FIRST-IF PROTECTED") &&
              strcmp(text + strlen(text) - 11, "ROUTES END\n") == 0,
          "R13 the second goal comes after the first goal's report, inside ROUTES");
    free(text);
    text = routes_with_second(&b, "h8 1", "c4 2");
    check(text && has_line(text, "SECOND GOAL c4 LIMIT 2 PASS AFTER none (ELSE)") && count_prefix(text, "FROM ") == 1 &&
              count_prefix(text, "FROM START ") == 1,
          "R14 the first goal not reached: the second goal is measured from the start only");
    free(text);
}

static void test_invariants(void) {
    static Board b, before;
    Goal g, g2;
    setup(&b, SCHOLAR);
    before = b;
    parse_goal("f7 d5 3", &g);
    parse_goal("c4 2", &g2);
    TextBuffer a = {NULL, 0, 0}, c = {NULL, 0, 0}, s = {NULL, 0, 0};
    routes_real_report(&b, &g, 3, &a);
    routes_real_report(&b, &g, 3, &c);
    support_report(&b, mv("d1h5"), &s);
    routes_report_with(&b, &g, &g2, &s);
    check(memcmp(&before, &b, sizeof(Board)) == 0, "R15 REAL routes, support and the second goal leave the board unchanged");
    check(a.text && c.text && strcmp(a.text, c.text) == 0 && strncmp(a.text, "ROUTES BEGIN White GOAL f7 d5 DEPTH 3 REAL\n", 43) == 0,
          "R16 REAL routes are deterministic and labelled REAL (PASS reports stay labelled PASS)");
    free(a.text);
    free(c.text);
    free(s.text);
}

int main(void) {
    test_real_scholar();
    test_real_rules();
    test_support();
    test_second_goal();
    test_invariants();
    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
