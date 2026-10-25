/* Stage 3S tests: FEN setup (accepted, refused, round trip, perft from
 * loaded positions) and following pieces across a move (moved, castling
 * rook, captured, en passant, promotion). chess.c is included unchanged,
 * with its main renamed. Run by CTest; prints one line per failure and a
 * total, and returns non-zero if anything fails. */
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

static Board start_board(void) {
    Board b = {0};
    reset_board(&b);
    return b;
}

static bool fen_ok(const char *fen) {
    Board b = start_board();
    const char *reason = NULL;
    bool ok = load_fen(&b, fen, &reason);
    free(b.history);
    return ok;
}

/* The FEN must be refused, with exactly this reason. */
static void refused(const char *fen, const char *expected_reason, const char *what) {
    Board b = start_board();
    char before[FEN_TEXT_SIZE], after[FEN_TEXT_SIZE];
    const char *reason = NULL;
    board_to_fen(&b, before);
    bool ok = load_fen(&b, fen, &reason);
    board_to_fen(&b, after);
    bool right = !ok && reason != NULL && strcmp(reason, expected_reason) == 0 && strcmp(before, after) == 0;
    if (!right) {
        printf("  (got %s, reason \"%s\")\n", ok ? "accepted" : "refused", reason ? reason : "");
    }
    check(right, what);
    free(b.history);
}

static bool round_trip(const char *fen) {
    Board b = start_board();
    const char *reason = NULL;
    char out[FEN_TEXT_SIZE];
    bool ok = load_fen(&b, fen, &reason);
    board_to_fen(&b, out);
    free(b.history);
    if (!ok || strcmp(out, fen) != 0) {
        printf("  (round trip of %s gave %s)\n", fen, ok ? out : reason);
        return false;
    }
    return true;
}

static unsigned long long perft(const Board *b, int depth) {
    MoveList m = generate_moves(b);
    unsigned long long n = 0;
    if (depth == 1) {
        n = m.count;
    } else {
        for (size_t i = 0; i < m.count; ++i) {
            Board c = *b;
            make_move(&c, m.items[i]);
            n += perft(&c, depth - 1);
        }
    }
    free(m.items);
    return n;
}

static Move mv(const char *text) {
    int sr, sc, er, ec;
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

static void test_fen_accepted(void) {
    check(fen_ok("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"), "FEN-A1 start position accepted");
    check(fen_ok("7k/8/5K2/8/8/8/8/Q7 w - - 0 1"), "FEN-A2 king and queen vs king accepted");
    check(fen_ok("7k/8/5K2/8/8/8/8/Q7 w - -"), "FEN-A3 four fields accepted");
    check(fen_ok("4k3/8/8/8/8/8/4q3/4K3 w - - 0 1"), "FEN-A4 side to move in check accepted");
    check(fen_ok("rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3"),
          "FEN-A5 side to move checkmated (Fool's Mate) accepted");
    check(fen_ok("4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1"), "FEN-A6 castling rights with king and rooks home accepted");
    check(fen_ok("rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 3"), "FEN-A7 en passant square accepted");
    check(fen_ok("QQQQQQQQ/Q7/8/8/8/8/8/k1K5 b - - 0 1"),
          "FEN-A8 nine queens and no pawns (eight promotions), side to move in double check, accepted");
}

static void test_fen_refused(void) {
    refused("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP w KQkq - 0 1", "the board must have exactly 8 ranks of 8 squares", "FEN-R1 seven ranks");
    refused("rnbqkbnr/pppppppp/9/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "a rank does not have exactly 8 squares", "FEN-R2 a rank of nine");
    refused("rnbqkbnr/ppppxppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "unknown piece letter", "FEN-R3 unknown letter");
    refused("rnbq1bnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQ - 0 1", "Black must have exactly one king", "FEN-R4 no black king");
    refused("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKKNR w KQkq - 0 1", "White must have exactly one king", "FEN-R5 two white kings");
    refused("Pnbqkbnr/pppppppp/8/8/8/8/1PPPPPPP/RNBQKBNR w KQk - 0 1", "a pawn is on the first or last rank", "FEN-R6 pawn on the last rank");
    refused("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR x KQkq - 0 1", "side to move must be w or b", "FEN-R7 side x");
    refused("4k3/8/8/8/8/8/4Q3/4K3 w - - 0 1", "the side that is not to move is in check", "FEN-R8 side not to move in check");
    refused("4k3/8/8/8/8/8/8/4K3 w KQ - 0 1", "castling right without the rook on its starting square", "FEN-R9 castling without rooks");
    refused("4k3/8/8/8/8/8/8/R4K1R w KQ - 0 1", "White castling right without the king on e1", "FEN-R10 castling without the king home");
    refused("4k3/8/8/8/8/8/8/R3K2R w QK - 0 1", "castling field must be - or letters from KQkq in that order", "FEN-R11 castling letters out of order");
    refused("rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d3 0 3",
            "en passant square is on the wrong rank for the side to move", "FEN-R12 en passant on the wrong rank");
    refused("rnbqkbnr/pppppppp/8/4P3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 3",
            "en passant square does not match a pawn that just moved two squares", "FEN-R13 en passant without the pawn");
    refused("4k3/8/8/8/8/P7/PPPPPPPP/4K3 w - - 0 1", "White has more pieces than a game allows", "FEN-R14 nine white pawns");
    refused("4k3/8/8/8/8/8/PPPPPPPP/QQQQK3 w - - 0 1", "White has more pieces than a game allows", "FEN-R15 extra queens with eight pawns");
    refused("4k3/8/8/8/8/8/8/4K3 w - - x 1", "halfmove clock and move number must be whole numbers (move number at least 1)", "FEN-R16 bad halfmove clock");
    refused("4k3/8/8/8/8/8/8/4K3 w - - 0 0", "halfmove clock and move number must be whole numbers (move number at least 1)", "FEN-R17 move number 0");
    refused("4k3/8/8/8/8/8/8/4K3 w -", "a FEN has 6 fields (or 4) separated by single spaces", "FEN-R18 three fields");
    refused("4k3/8/8/8/8/8/8/4K3  w - - 0 1", "a FEN has 6 fields (or 4) separated by single spaces", "FEN-R19 double space");
    refused("k7/8/8/4r3/1b6/3n4/8/4K3 w - - 0 1", "the king to move is attacked by more than two pieces", "FEN-R20 triple check");
    refused("QQQQQQQQ/QQ6/8/8/8/8/8/k1K5 b - - 0 1", "White has more pieces than a game allows", "FEN-R21 ten queens (more than eight promotions)");
}

static void test_fen_round_trip(void) {
    static const char *const fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/8/PPPP1PPP/RNBQK1NR w KQkq - 2 3",
        "7k/8/5K2/8/8/8/8/Q7 w - - 0 1",
        "6k1/8/6K1/8/8/8/8/R7 w - - 0 1",
        "4k3/5p2/8/8/2B5/8/8/3QK3 w - - 0 1",
        "3qk3/3p1p2/8/8/2B5/8/8/3QK3 w - - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
        "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
        "rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 3"};
    char what[64];
    for (size_t i = 0; i < sizeof(fens) / sizeof(fens[0]); ++i) {
        snprintf(what, sizeof(what), "FEN-T%u round trip", (unsigned)(i + 1));
        check(round_trip(fens[i]), what);
    }

    /* The normal start, and FENs written after real moves. */
    Board b = start_board();
    char out[FEN_TEXT_SIZE];
    board_to_fen(&b, out);
    check(strcmp(out, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") == 0, "FEN-T11 the default start is the normal position");
    make_move(&b, mv("e2e4"));
    board_to_fen(&b, out);
    check(strcmp(out, "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1") == 0, "FEN-T12 after 1.e4");
    make_move(&b, mv("e7e5"));
    make_move(&b, mv("g1f3"));
    board_to_fen(&b, out);
    check(strcmp(out, "rnbqkbnr/pppp1ppp/8/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2") == 0, "FEN-T13 after 1.e4 e5 2.Nf3");
    free(b.history);
}

static void test_fen_perft(void) {
    static const struct { const char *fen; int depth; unsigned long long nodes; } t[] = {
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 3, 8902ULL},
        {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 3, 97862ULL},
        {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 4, 43238ULL},
        {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 3, 9467ULL},
        {"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 3, 62379ULL},
        {"r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 3, 89890ULL}};
    char what[64];
    for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); ++i) {
        Board b = start_board();
        const char *reason = NULL;
        bool ok = load_fen(&b, t[i].fen, &reason);
        snprintf(what, sizeof(what), "FEN-P%u perft from a loaded FEN", (unsigned)(i + 1));
        check(ok && perft(&b, t[i].depth) == t[i].nodes, what);
        free(b.history);
    }
}

/* Follows the piece on `square` across `move` from the position `fen`. */
static void follow(const char *fen, const char *move, const char *square, FollowCase expected_case,
                   const char *expected_square, char expected_type, const char *what) {
    Board b = start_board();
    const char *reason = NULL;
    int row, col, after_row, after_col;
    char after_type;
    bool ok = load_fen(&b, fen, &reason) && parse_square(square, &row, &col);
    Move m = mv(move);
    ok = ok && legal_move(&b, m);
    FollowCase got = ok ? follow_piece(&b, m, row, col, &after_row, &after_col, &after_type) : FOLLOW_STAYS;
    if (ok && expected_square == NULL) {
        ok = got == expected_case && after_row == -1 && after_col == -1;
    } else if (ok) {
        int er, ec;
        parse_square(expected_square, &er, &ec);
        ok = got == expected_case && after_row == er && after_col == ec && after_type == expected_type;
    }
    check(ok, what);
    free(b.history);
}

static void test_follow_piece(void) {
    const char *castle = "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1";
    const char *castle_black = "r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1";
    follow(castle, "e1g1", "e1", FOLLOW_MOVED, "g1", 'K', "ID1 castling king is the moved piece");
    follow(castle, "e1g1", "h1", FOLLOW_CASTLING_ROOK, "f1", 'R', "ID2 rook h1 goes to f1");
    follow(castle, "e1c1", "a1", FOLLOW_CASTLING_ROOK, "d1", 'R', "ID3 rook a1 goes to d1");
    follow(castle, "e1g1", "a1", FOLLOW_STAYS, "a1", 'R', "ID4 the other rook stays");
    follow(castle_black, "e8g8", "h8", FOLLOW_CASTLING_ROOK, "f8", 'R', "ID5 rook h8 goes to f8");
    follow(castle_black, "e8c8", "a8", FOLLOW_CASTLING_ROOK, "d8", 'R', "ID6 rook a8 goes to d8");
    const char *capture = "4k3/8/8/3p4/4P3/8/8/4K3 w - - 0 1";
    follow(capture, "e4d5", "d5", FOLLOW_CAPTURED, NULL, 'P', "ID7 captured pawn has no square after");
    follow(capture, "e4d5", "e4", FOLLOW_MOVED, "d5", 'P', "ID8 capturing pawn is the moved piece");
    const char *ep = "rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 3";
    follow(ep, "e5d6", "d5", FOLLOW_CAPTURED, NULL, 'P', "ID9 en passant: the pawn beside is captured");
    follow(ep, "e5d6", "e5", FOLLOW_MOVED, "d6", 'P', "ID10 en passant: the capturing pawn moves to d6");
    const char *promo = "1n2k3/P7/8/8/8/8/8/4K3 w - - 0 1";
    follow(promo, "a7a8n", "a7", FOLLOW_PROMOTED, "a8", 'N', "ID11 promotion to a knight keeps the pawn's identity");
    follow(promo, "a7b8q", "a7", FOLLOW_PROMOTED, "b8", 'Q', "ID12 promotion with capture: pawn becomes a queen on b8");
    follow(promo, "a7b8q", "b8", FOLLOW_CAPTURED, NULL, 'N', "ID13 the knight captured by the promotion");
    follow(promo, "a7b8r", "e8", FOLLOW_STAYS, "e8", 'K', "ID14 an unaffected piece stays");
}

static void test_load_replaces_history(void) {
    Board b = start_board();
    const char *reason = NULL;
    make_move(&b, mv("g1f3"));
    record_position(&b);
    bool ok = load_fen(&b, "7k/8/5K2/8/8/8/8/Q7 w - - 0 1", &reason);
    check(ok && b.history_size == 1, "FEN-H1 the move history restarts at the loaded position");
    free(b.history);
}

int main(void) {
    test_fen_accepted();
    test_fen_refused();
    test_fen_round_trip();
    test_fen_perft();
    test_follow_piece();
    test_load_replaces_history();
    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
