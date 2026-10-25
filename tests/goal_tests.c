/* Stage 3D-1 tests: goals, IF / ELSE results, PASS routes and marks,
 * against the worked example E4 of docs/CALCULATOR_SPEC.md and the points of
 * ChatGPT's review 16: IF and ELSE, ELSE then IF on a later motion, PIECE
 * versus SIDE, one square and ordered sets with the first IF, shortest versus
 * protected arrival, blocked and pinned pieces, the own king's safety,
 * capture and promotion, PASS labels and limits, refused input, and that
 * nothing changes; and review 17: CONDITION and ORDER fields, the distance
 * order (equal distances, one distance, no moves, knight, pawn, castling,
 * promotion, nothing added or removed). chess.c is included unchanged. Run by CTest. */
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

static char *routes_for(const Board *b, const char *goal_text_in) {
    Goal g;
    TextBuffer out = {NULL, 0, 0};
    if (!parse_goal(goal_text_in, &g)) {
        printf("  (goal refused: %s)\n", goal_text_in);
        return NULL;
    }
    routes_report(b, &g, &out);
    return out.text;
}

static Move mv(const char *text) {
    Move m = {0, 0, 0, 0, '\0'};
    parse_move_text(text, &m);
    return m;
}

static const char *const SCHOLAR = "r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/8/PPPP1PPP/RNBQK1NR w KQkq - 2 3";

/* E4: route lengths from the E3 position, PASS. */
static void test_e4(void) {
    static const char *const lines[] = {
        "ROUTES BEGIN White GOAL f7 d5 LIMIT 4 PASS",
        "NOTE PASS is hypothetical: only White moves, Black passes; not a real sequence of turns",
        "GOAL f7 NOW White 1 Black 1",
        "PIECE Q d1 CONDITION IF MOVES 2 ORDER ELSE IF",
        "PIECE B c4 CONDITION IF MOVES 1 ORDER IF",
        "PIECE N g1 CONDITION IF MOVES 3 ORDER ELSE IF",
        "PIECE N b1 CONDITION IF MOVES 4 ORDER ELSE",
        "GOAL d5 NOW White 2 Black 0",
        "PIECE B c4 CONDITION IF MOVES 1 ORDER IF",
        "PIECE N b1 CONDITION IF MOVES 2 ORDER ELSE IF",
        "PIECE Q d1 CONDITION IF MOVES 3 ORDER ELSE",
        "PIECE N g1 CONDITION IF MOVES 3 ORDER ELSE",
        "SQUARE f7 REACH CONDITION IF PROTECTED CONDITION IF",
        "SQUARE d5 REACH CONDITION IF PROTECTED CONDITION IF",
        "ROUTES END"};
    static Board b;
    char *text = setup(&b, SCHOLAR) ? routes_for(&b, "f7 d5 4") : NULL;
    check(all_lines(text, lines, COUNT_OF(lines)), "G1 E4: Qd1 2, Bc4 1, Ng1 3, Nb1 4 to f7; Bc4 1, Nb1 2, Qd1 3, Ng1 3 to d5; the ORDER chain by moves");

    /* Shortest versus protected arrival (E4): neither dominates. */
    check(text && has_line(text, "ROUTE PIECE B c4 c4f7 OWN 1 PROTECTORS 0 ATTACKERS 1") &&
              has_line(text, "ROUTE PIECE Q d1 d1h5 h5f7 OWN 2 PROTECTORS 1 ATTACKERS 1") &&
              has_line(text, "ROUTE PIECE Q d1 d1f3 f3f7 OWN 2 PROTECTORS 1 ATTACKERS 1"),
          "G2 Bxf7 alone arrives in 1 unprotected; Qh5-f7 and Qf3-f7 arrive in 2 protected; both kept");
    check(text && strstr(text, "ROUTE PIECE N g1 g1f3 f3g5 g5f7 OWN 3 PROTECTORS 1 ATTACKERS 1 DOMINATED"),
          "G3 the knight's 3-move route is DOMINATED by the queen's (2 moves, same support)");
    check(text && has_line(text, "SIDE REACH CONDITION IF MOVES 1") && has_line(text, "SIDE PROTECTED CONDITION IF MOVES 2") &&
              has_line(text, "ROUTE SIDE PROTECTED d1h5 h5f7 OWN 2 PROTECTORS 1 ATTACKERS 1") &&
              has_line(text, "ROUTE SIDE PROTECTED d1h5 c4f7 OWN 2 PROTECTORS 1 ATTACKERS 1"),
          "G4 SIDE: reached in 1, reached protected in 2 (the queen and bishop together: width)");
    check(text && has_line(text, "PIECE R a1 CONDITION ELSE ORDER NONE") && has_line(text, "PIECE B c1 CONDITION ELSE ORDER NONE"),
          "G5 blocked pieces (Ra1, Bc1) are ELSE: no jumping over pieces");
    free(text);
}

/* IF / ELSE, the limit, and ELSE followed by IF on a later motion. */
static void test_if_else(void) {
    static Board b;
    char *text = setup(&b, SCHOLAR) ? routes_for(&b, "f7 1") : NULL;
    check(text && has_line(text, "PIECE Q d1 CONDITION ELSE ORDER NONE") &&
              has_line(text, "PIECE B c4 CONDITION IF MOVES 1 ORDER IF") && has_line(text, "SIDE PROTECTED CONDITION ELSE"),
          "G6 limit 1: the queen is ELSE, the bishop IF; no protected arrival in 1 move");
    free(text);

    Board after = b;
    make_move(&after, mv("d1h5"));
    after.turn = WHITE; /* PASS: Black passes */
    after.ep_row = after.ep_col = -1;
    text = routes_for(&after, "f7 1");
    check(text && has_line(text, "PIECE Q h5 CONDITION IF MOVES 1 ORDER IF") && has_line(text, "SIDE PROTECTED CONDITION IF MOVES 1"),
          "G7 after the motion Qh5 the same test (limit 1) is IF for the queen, and protected");
    free(text);
}

/* PIECE versus SIDE in the marks of the lists (selection and preview). */
static void test_marks(void) {
    static Board b;
    Goal g;
    TextBuffer out = {NULL, 0, 0};
    static const char *const selection[] = {
        "GOAL MARKS Q d1 GOAL f7 LIMIT 2 PASS",
        "MARK d1h5 PIECE CONDITION IF f7 MOVES 2 SIDE CONDITION IF f7 MOVES 2",
        "MARK d1g4 PIECE CONDITION ELSE SIDE CONDITION IF f7 MOVES 2",
        "MARK d1f3 PIECE CONDITION IF f7 MOVES 2 SIDE CONDITION IF f7 MOVES 2",
        "MARK d1e2 PIECE CONDITION ELSE SIDE CONDITION IF f7 MOVES 2",
        "GOAL MARKS END"};
    bool ok = setup(&b, SCHOLAR) && parse_goal("f7 2", &g);
    if (ok) {
        goal_marks_selection(&b, &g, 7, 3, &out);
    }
    check(ok && all_lines(out.text, selection, COUNT_OF(selection)) && count_prefix(out.text, "MARK ") == 4,
          "G8 selecting the queen: each destination marked; Qe2 is PIECE ELSE but SIDE IF (Bxf7)");
    free(out.text);

    out.text = NULL;
    out.length = out.capacity = 0;
    goal_marks_after(&b, &g, mv("d1h5"), &out);
    check(out.text && has_line(out.text, "MARK Q h5 h5f7 PIECE CONDITION IF f7 MOVES 2 SIDE CONDITION IF f7 MOVES 2") &&
              has_line(out.text, "MARK B c4 c4f7 PIECE CONDITION IF f7 MOVES 2 SIDE CONDITION IF f7 MOVES 2") &&
              has_line(out.text, "MARK Q h5 h5g6 PIECE CONDITION ELSE SIDE CONDITION ELSE") &&
              count_prefix(out.text, "REACHED ") == 0,
          "G9 previewing Qh5: every White move in the AFTER lists is marked (h5f7 and c4f7 IF, h5g6 ELSE)");
    free(out.text);

    out.text = NULL;
    out.length = out.capacity = 0;
    goal_marks_after(&b, &g, mv("c4f7"), &out);
    check(out.text && has_line(out.text, "REACHED f7 1"), "G10 previewing Bxf7: the goal is REACHED by the previewed move");
    free(out.text);
}

/* One square and ordered sets: every square's result, and the first IF. */
static void test_sets(void) {
    static Board b;
    char *text = setup(&b, SCHOLAR) ? routes_for(&b, "d5 f7 4") : NULL;
    check(text && has_line(text, "FIRST-IF REACH d5") && has_line(text, "SQUARE f7 REACH CONDITION IF PROTECTED CONDITION IF"),
          "G11 two IF squares: the first in the owner's order is d5; f7 stays IF");
    free(text);
    text = routes_for(&b, "h8 d5 1");
    check(text && has_line(text, "SQUARE h8 REACH CONDITION ELSE PROTECTED CONDITION ELSE") && has_line(text, "FIRST-IF REACH d5") &&
              has_line(text, "FIRST-IF PROTECTED d5"),
          "G12 if h8 (ELSE) else if d5 (IF): the first IF is d5");
    free(text);
    text = routes_for(&b, "h8 a8 1");
    check(text && has_line(text, "FIRST-IF REACH none (ELSE)") && has_line(text, "FIRST-IF PROTECTED none (ELSE)"),
          "G13 no square IF: the whole set is ELSE");
    free(text);

    setup(&b, NULL);
    text = routes_for(&b, "king 1");
    check(text && has_line(text, "ROUTES BEGIN White GOAL king LIMIT 1 PASS") && strstr(text, "GOAL d8 ") &&
              strstr(text, "GOAL f8 ") && strstr(text, "GOAL d7 ") && strstr(text, "GOAL e7 ") &&
              strstr(text, "GOAL f7 ") && count_prefix(text, "GOAL ") == 5 &&
              strstr(text, "GOAL d8 ") < strstr(text, "GOAL f8 ") && strstr(text, "GOAL f8 ") < strstr(text, "GOAL d7 "),
          "G14 goal king: the 5 squares next to the black king, in board order (rank 8 to 1, file a to h)");
    free(text);
}

/* Legality in PASS: pins, the own king, capture, promotion. */
static void test_legality(void) {
    static Board b;
    char *text = setup(&b, "4r1k1/8/8/8/8/8/4N3/4K3 w - - 0 1") ? routes_for(&b, "c3 1") : NULL;
    check(text && has_line(text, "PIECE N e2 CONDITION ELSE ORDER NONE"), "G15 a pinned knight cannot go to c3 (own king safety)");
    free(text);

    text = setup(&b, "4k3/8/8/8/8/8/8/3rK3 w - - 0 1") ? routes_for(&b, "d1 d2 f2 1") : NULL;
    check(text && has_line(text, "SQUARE d1 REACH CONDITION IF PROTECTED CONDITION ELSE") && has_line(text, "SQUARE d2 REACH CONDITION ELSE PROTECTED CONDITION ELSE") &&
              has_line(text, "SQUARE f2 REACH CONDITION IF PROTECTED CONDITION ELSE"),
          "G16 in check from the rook on d1: the king can take it (d1) or go to f2, not to d2 (attacked)");
    free(text);
    text = setup(&b, "4k3/8/8/8/8/8/3r4/4K3 w - - 0 1") ? routes_for(&b, "e2 1") : NULL;
    check(text && has_line(text, "PIECE K e1 CONDITION ELSE ORDER NONE"), "G17 the king never moves into check (e2 attacked by the rook)");
    free(text);

    text = setup(&b, SCHOLAR) ? routes_for(&b, "e5 2") : NULL;
    check(text && has_line(text, "PIECE N g1 CONDITION IF MOVES 2 ORDER IF") && has_line(text, "ROUTE PIECE N g1 g1f3 f3e5 OWN 2 PROTECTORS 0 ATTACKERS 1"),
          "G18 capture: the knight reaches e5 by capturing the pawn (Nf3xe5)");
    free(text);

    text = setup(&b, "8/P7/8/8/8/8/8/K6k w - - 0 1") ? routes_for(&b, "a8 1") : NULL;
    check(text && has_line(text, "PIECE P a7 CONDITION IF MOVES 1 ORDER IF") && count_prefix(text, "ROUTE PIECE P a7 a7a8") == 4,
          "G19 promotion: the pawn reaches a8, as queen, rook, bishop or knight");
    free(text);
}

static void test_input_and_invariants(void) {
    Goal g;
    check(parse_goal("f7 2", &g) && g.count == 1 && g.limit == 2 && !g.king && parse_goal("f7 g7 h7 4", &g) &&
              g.count == 3 && parse_goal("king 1", &g) && g.king,
          "G20 goal f7 2, goal f7 g7 h7 4 and goal king 1 are read");
    check(!parse_goal("f7", &g) && !parse_goal("f7 0", &g) && !parse_goal("f7 5", &g) && !parse_goal("x9 2", &g) &&
              !parse_goal("f7 2 3", &g) && !parse_goal("f7 f7 2", &g) && !parse_goal("2", &g) &&
              !parse_goal("king f7 2", &g) && !parse_goal("f7 king 2", &g) && !parse_goal("", &g),
          "G21 a missing or bad limit, bad or repeated squares and extra words are refused");

    static Board before, b;
    TextBuffer out = {NULL, 0, 0};
    bool ok = setup(&b, SCHOLAR) && parse_goal("f7 d5 3", &g);
    before = b;
    char *first = ok ? routes_for(&b, "f7 d5 3") : NULL;
    char *second = ok ? routes_for(&b, "f7 d5 3") : NULL;
    if (ok) {
        goal_marks_selection(&b, &g, 7, 3, &out);
        goal_marks_after(&b, &g, mv("d1h5"), &out);
    }
    check(ok && memcmp(&before, &b, sizeof(Board)) == 0, "G22 routes and marks leave the board, the turn and the history unchanged");
    check(first && second && strcmp(first, second) == 0, "G23 routes are deterministic");
    check(first && strncmp(first, "ROUTES BEGIN ", 13) == 0 && strcmp(first + strlen(first) - 11, "ROUTES END\n") == 0 &&
              strstr(first, " PASS\nNOTE PASS is hypothetical") != NULL,
          "G24 framed by ROUTES BEGIN and ROUTES END, labelled PASS (hypothetical)");
    free(first);
    free(second);
    free(out.text);
}

/* The distance order (review 17): squares travelled, IF / ELSE IF / ELSE. */
static char *distance_for_selection(const Board *b, const char *square) {
    TextBuffer out = {NULL, 0, 0};
    int r, c;
    parse_square(square, &r, &c);
    distance_selection(b, r, c, &out);
    return out.text;
}

static void test_distance(void) {
    static Board b;
    static const char *const queen[] = {
        "DISTANCE ORDER Q d1",
        "MOVE d1e2 DISTANCE 1 ORDER IF",
        "MOVE d1f3 DISTANCE 2 ORDER ELSE IF",
        "MOVE d1g4 DISTANCE 3 ORDER ELSE IF",
        "MOVE d1h5 DISTANCE 4 ORDER ELSE",
        "DISTANCE ORDER END"};
    char *text = setup(&b, SCHOLAR) ? distance_for_selection(&b, "d1") : NULL;
    check(all_lines(text, queen, COUNT_OF(queen)) && count_prefix(text, "MOVE ") == 4 &&
              strstr(text, "d1e2") < strstr(text, "d1f3") && strstr(text, "d1g4") < strstr(text, "d1h5"),
          "G25 the queen's moves of 1, 2, 3, 4 squares: IF, ELSE IF, ELSE IF, ELSE, in that order");
    free(text);

    static const char *const bishop[] = {
        "MOVE c4b5 DISTANCE 1 ORDER IF", "MOVE c4d5 DISTANCE 1 ORDER IF", "MOVE c4b3 DISTANCE 1 ORDER IF",
        "MOVE c4d3 DISTANCE 1 ORDER IF", "MOVE c4a6 DISTANCE 2 ORDER ELSE IF", "MOVE c4e6 DISTANCE 2 ORDER ELSE IF",
        "MOVE c4e2 DISTANCE 2 ORDER ELSE IF", "MOVE c4f7 DISTANCE 3 ORDER ELSE", "MOVE c4f1 DISTANCE 3 ORDER ELSE"};
    text = distance_for_selection(&b, "c4");
    check(all_lines(text, bishop, COUNT_OF(bishop)) && count_prefix(text, "MOVE ") == 9,
          "G26 equal distances get the same label (the bishop: four IF, three ELSE IF, two ELSE)");
    free(text);

    text = distance_for_selection(&b, "g1");
    check(text && has_line(text, "MOVE g1f3 DISTANCE 2 ORDER IF JUMP") && has_line(text, "MOVE g1h3 DISTANCE 2 ORDER IF JUMP") &&
              has_line(text, "MOVE g1e2 DISTANCE 2 ORDER IF JUMP") && strstr(text, "ELSE") == NULL,
          "G27 one distinct distance: IF only (the knight; each move marked JUMP)");
    free(text);
    text = distance_for_selection(&b, "a2");
    check(text && has_line(text, "MOVE a2a3 DISTANCE 1 ORDER IF") && has_line(text, "MOVE a2a4 DISTANCE 2 ORDER ELSE"),
          "G28 a pawn: one square IF, two squares ELSE");
    free(text);
    text = distance_for_selection(&b, "a1");
    check(text && has_line(text, "NO-MOVES") && count_prefix(text, "MOVE ") == 0,
          "G29 a piece with no legal move has no distance order (NO-MOVES)");
    free(text);

    text = setup(&b, "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1") ? distance_for_selection(&b, "e1") : NULL;
    check(text && has_line(text, "MOVE e1g1 DISTANCE 2 ORDER ELSE CASTLE h1f1") &&
              has_line(text, "MOVE e1c1 DISTANCE 2 ORDER ELSE CASTLE a1d1") && has_line(text, "MOVE e1d1 DISTANCE 1 ORDER IF"),
          "G30 castling: the king's 2 squares, with the rook's move named");
    free(text);
    text = setup(&b, "1n2k3/P7/8/8/8/8/8/4K3 w - - 0 1") ? distance_for_selection(&b, "a7") : NULL;
    check(text && has_line(text, "MOVE a7a8q DISTANCE 1 ORDER IF") && has_line(text, "MOVE a7b8n DISTANCE 1 ORDER IF") &&
              count_prefix(text, "MOVE ") == 8 && strstr(text, "ELSE") == NULL,
          "G31 promotion, with and without capture: all 8 moves 1 square, the same label");
    free(text);

    /* After a preview: every piece of both sides; nothing else changes. */
    TextBuffer out = {NULL, 0, 0};
    setup(&b, SCHOLAR);
    static Board before;
    before = b;
    distance_after(&b, mv("d1h5"), &out);
    check(out.text && has_line(out.text, "DISTANCE ORDER AFTER d1h5") &&
              has_line(out.text, "MOVE White Q h5 h5f7 DISTANCE 2 ORDER ELSE IF") &&
              has_line(out.text, "MOVE White Q h5 h5d1 DISTANCE 4 ORDER ELSE") && has_line(out.text, "NO-MOVES Black P f7") &&
              memcmp(&before, &b, sizeof(Board)) == 0,
          "G32 after Qh5: every piece's moves with their order (the pinned f7 pawn has none); the board unchanged");
    int listed = count_prefix(out.text, "MOVE White ") + count_prefix(out.text, "MOVE Black ");
    Board after = b;
    make_move(&after, mv("d1h5"));
    int legal = 0;
    for (int side = WHITE; side <= BLACK; ++side) {
        Board view = after;
        view.turn = (Color)side;
        MoveList moves = generate_moves(&view);
        legal += (int)moves.count;
        free(moves.items);
    }
    check(listed == legal, "G33 the distance view lists exactly the legal moves (sorting adds and removes nothing)");
    free(out.text);

    /* CONDITION and ORDER are separate fields. */
    text = routes_for(&b, "f7 4");
    check(text && has_line(text, "PIECE N b1 CONDITION IF MOVES 4 ORDER ELSE") &&
              has_line(text, "PIECE R a1 CONDITION ELSE ORDER NONE"),
          "G34 CONDITION (reached or not) and ORDER (the chain by moves) are separate: Nb1 IF and ELSE, Ra1 ELSE and NONE");
    free(text);
}

int main(void) {
    test_e4();
    test_if_else();
    test_marks();
    test_sets();
    test_legality();
    test_input_and_invariants();
    test_distance();
    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
