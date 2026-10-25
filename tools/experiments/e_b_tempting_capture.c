/* Experiment E-B (docs/FLOWCHART_CROSS_REFERENCE.md, row 12; approved in
 * ChatGPT Review 35): a tempting capture and its legal consequences.
 * Position (White to move): 6k1/8/4p3/3r4/4P3/2N5/8/3Q2K1 w - - 0 1
 * Candidate: the queen takes the rook, Qxd5 (d1d5). For comparison the two
 * other captures of the same rook (e4d5, c3d5) are shown in the exchange.
 * Only the existing reports of chess.c are called (included unchanged):
 * R (calc_report), replies (replies_report), exchange (exchange_report),
 * loop (loop_report). This program prints OBSERVATIONS only; candidate
 * "warning facts" are named in docs/FLOWCHART_CROSS_REFERENCE.md, kept apart
 * from any mechanism (no warning delivery, cancellation or decision).
 * Build and run from the repository root:
 *   gcc -std=c99 -O2 tools/experiments/e_b_tempting_capture.c -o e_b
 *   ./e_b > tools/experiments/e_b_output.txt
 */
#define main chess_main
#include "../../chess.c"
#undef main

static void show(const char *title, TextBuffer *t) {
    printf("=== %s ===\n%s\n", title, t->text ? t->text : "(nothing)\n");
    free(t->text);
    t->text = NULL;
    t->length = t->capacity = 0;
}

int main(void) {
    static const char *const fen = "6k1/8/4p3/3r4/4P3/2N5/8/3Q2K1 w - - 0 1";
    static Board b, after;
    static Tracker t;
    TextBuffer out = {NULL, 0, 0};
    const char *reason = NULL;
    Move qxd5 = {0, 0, 0, 0, '\0'};

    memset(&b, 0, sizeof(b));
    reset_board(&b);
    if (!load_fen(&b, fen, &reason)) {
        printf("FEN refused: %s\n", reason);
        return 1;
    }
    tracker_init(&t, &b);
    parse_move_text("d1d5", &qxd5);
    printf("EXPERIMENT E-B a tempting capture and its legal consequences (observations only)\n");
    printf("POSITION %s\nCANDIDATE d1d5 (Qxd5: the queen takes the rook)\n\n", fen);

    calc_report(&b, &t, qxd5, &out);
    show("1. R report of Qxd5 (what changes for every piece; attack and protection links)", &out);
    replies_report(&b, &t, qxd5, &out);
    show("2. replies to Qxd5 (every legal answer of Black, with its counts)", &out);
    exchange_report(&b, 3 * 8 + 3, 4, &out);
    show("3. exchange on d5, limit 4 (every capture order, both totals; Qxd5 next to exd5 and Nxd5)", &out);
    loop_report(&b, 3, 0, &out);
    show("4. loop 3 from the position (bounded mates for either side; the first move's status)", &out);
    after = b;
    make_move(&after, qxd5);
    loop_report(&after, 2, 0, &out);
    show("5. loop 2 after Qxd5 (Black to move)", &out);
    exchange_report(&after, 3 * 8 + 3, 4, &out);
    show("6. exchange on d5 after Qxd5 (Black to move)", &out);
    free(b.history);
    return 0;
}
