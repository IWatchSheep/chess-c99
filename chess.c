/* Step 1: piece selection and legal destination highlighting.
 * Type two characters (for example d2), then press Enter to see legal moves.
 * Step 2 lists other pieces before and after a hypothetical destination.
 * Every move requires selecting, previewing, then F to finalize.
 * Stage 3S: "fen <FEN>" sets up a practice position; "fen" prints the
 * current position. See docs/CALCULATOR_SPEC.md.
 * Stage 3A: after a preview, "R" prints the calculator's fact report.
 * Stage 3B: after a preview, "replies" lists every reply of the other side
 * (does it stop the threat?), and "replies <move>" reports one reply.
 * Version 9: after a move that ends the game, R prints no MATE1 lines.
 * Stage 3C: "loop N" visits every sequence of up to N plies from the current
 * position (mates, forced or possible); "trace <moves>" reports a sequence.
 * Stage 3D-1: "goal f7 2" (or a set of squares, or "goal king 2") marks
 * every motion IF (the goal is reached within the limit) or ELSE, in the
 * lists while selecting and previewing; "routes" reports the routes.
 * PASS mode only: the other side passes (hypothetical). Results are
 * CONDITION IF / ELSE; route lengths and move distances get the owner's
 * ORDER chain IF / ELSE IF / ELSE ("distance on" shows move distances).
 * Stage 3D-2: "routes real N" (the other side answers: FORCED / POSSIBLE),
 * "support" after a preview, and "goal2" for a second goal.
 * Stage 3E: "exchange <square> N" (captures on one square, values logged),
 * "order N" (captures first, then distance: ideas 7 and 8), "risk on".
 */
/* Simplified two-player chess in C99.
 * Build: gcc -std=c99 -Wall -Wextra -O2 chess.c -o chess
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdarg.h>
#ifdef _WIN32
#include <io.h> /* _isatty */
#endif

#define RED   "\033[31m"
#define CYAN  "\033[36m"
#define RESET "\033[0m"

/* Wooden board colors: ANSI 256-color escape sequences. */
#define LIGHT_SQUARE_BG "\033[48;5;180m"  /* tan */
#define DARK_SQUARE_BG  "\033[48;5;94m"   /* brown */
#define LIGHT_TEXT_FG   "\033[38;5;231m"  /* ivory: readable on dark squares */
#define DARK_TEXT_FG    "\033[38;5;232m"  /* charcoal: readable on light squares */

typedef enum { EMPTY, WHITE, BLACK } Color;
typedef struct { char type; Color color; } Piece;
typedef struct { int sr, sc, er, ec; char promotion; } Move;
typedef struct { char key[80]; } Position;
typedef struct {
    Piece board[8][8];
    Color turn;
    bool white_king_moved, black_king_moved;
    bool white_rook_a_moved, white_rook_h_moved;
    bool black_rook_a_moved, black_rook_h_moved;
    int ep_row, ep_col, halfmove_clock;
    int fullmove_number; /* Stage 3S: the FEN move number, starting at 1 */
    Position *history;
    size_t history_size, history_capacity;
} Board;
typedef struct { Move *items; size_t count, capacity; } MoveList;
typedef struct { int row, col; } Direction;

static bool in_bounds(int row, int col) {
    bool row_is_valid = row >= 0 && row < 8;
    bool col_is_valid = col >= 0 && col < 8;
    return row_is_valid && col_is_valid;
}

static Piece empty_piece(void) {
    Piece piece = {'\0', EMPTY};
    return piece;
}

static char piece_symbol(Piece piece);

static Color opposite(Color color) {
    if (color == WHITE) {
        return BLACK;
    }
    return WHITE;
}

static const char *color_name(Color color) {
    if (color == WHITE) {
        return "White";
    }
    if (color == BLACK) {
        return "Black";
    }
    return "Empty";
}

static int home_row_for(Color side) {
    return side == WHITE ? 7 : 0;
}

static int rook_col_for(bool king_side) {
    return king_side ? 7 : 0;
}

static bool king_side_castle(Move move) {
    return move.ec > move.sc;
}

static void mark_rook_moved(Board *b, Color side, bool king_side) {
    if (side == WHITE) {
        if (king_side) {
            b->white_rook_h_moved = true;
        } else {
            b->white_rook_a_moved = true;
        }
    } else {
        if (king_side) {
            b->black_rook_h_moved = true;
        } else {
            b->black_rook_a_moved = true;
        }
    }
}

static bool path_clear(const Board *b, int sr, int sc, int er, int ec) {
    int dr = (er > sr) - (er < sr);
    int dc = (ec > sc) - (ec < sc);
    int r = sr + dr;
    int c = sc + dc;

    if (sr == er && sc == ec) {
        return false;
    }

    while (r != er || c != ec) {
        if (!in_bounds(r, c)) {
            return false;
        }
        if (b->board[r][c].color != EMPTY) {
            return false;
        }

        r += dr;
        c += dc;
    }

    return true;
}

static int pawn_direction(Color color) {
    return color == WHITE ? -1 : 1;
}

static bool is_knight_move(int row_change, int col_change) {
    int rows = abs(row_change);
    int cols = abs(col_change);
    return (rows == 2 && cols == 1) || (rows == 1 && cols == 2);
}

static bool is_diagonal_move(int row_change, int col_change) {
    return abs(row_change) == abs(col_change);
}

static bool is_straight_move(int row_change, int col_change) {
    return row_change == 0 || col_change == 0;
}

static bool is_queen_move(int row_change, int col_change) {
    return is_straight_move(row_change, col_change) ||
           is_diagonal_move(row_change, col_change);
}

static bool piece_attacks(const Board *b, int sr, int sc, int tr, int tc) {
    Piece p = b->board[sr][sc];
    int dr = tr - sr;
    int dc = tc - sc;

    if (p.color == EMPTY) {
        return false;
    }
    if (sr == tr && sc == tc) {
        return false;
    }

    switch (p.type) {
        case 'P':
            return abs(dc) == 1 && dr == pawn_direction(p.color);
        case 'N':
            return is_knight_move(dr, dc);
        case 'B':
            return is_diagonal_move(dr, dc) && path_clear(b, sr, sc, tr, tc);
        case 'R':
            return is_straight_move(dr, dc) && path_clear(b, sr, sc, tr, tc);
        case 'Q':
            return is_queen_move(dr, dc) && path_clear(b, sr, sc, tr, tc);
        case 'K':
            return abs(dr) <= 1 && abs(dc) <= 1;
        default:
            return false;
    }
}

static bool square_attacked(const Board *b, int r, int c, Color by) {
    for (int rr = 0; rr < 8; ++rr) {
        for (int cc = 0; cc < 8; ++cc) {
            if (b->board[rr][cc].color == by &&
                piece_attacks(b, rr, cc, r, c)) {
                return true;
            }
        }
    }
    return false;
}

static bool find_king(const Board *b, Color side, int *king_row, int *king_col) {
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            Piece piece = b->board[row][col];
            if (piece.type == 'K' && piece.color == side) {
                *king_row = row;
                *king_col = col;
                return true;
            }
        }
    }
    return false;
}

static bool has_king(const Board *b, Color side) {
    int row, col;
    return find_king(b, side, &row, &col);
}

static bool is_in_check(const Board *b, Color side) {
    int row, col;
    if (!find_king(b, side, &row, &col)) {
        return false;
    }
    return square_attacked(b, row, col, opposite(side));
}

static bool can_castle(const Board *b, Move move, Color side) {
    int home_row = home_row_for(side);
    bool king_moved = (side == WHITE) ? b->white_king_moved : b->black_king_moved;
    Color enemy = opposite(side);
    bool castle_kingside = king_side_castle(move);
    bool rook_moved = false;
    int rook_col = rook_col_for(castle_kingside);
    int transit_col = castle_kingside ? 5 : 3;
    int destination_col = castle_kingside ? 6 : 2;

    if (move.sr != home_row || move.sc != 4 || king_moved) {
        return false;
    }

    if (side == WHITE) {
        rook_moved = castle_kingside ? b->white_rook_h_moved : b->white_rook_a_moved;
    } else {
        rook_moved = castle_kingside ? b->black_rook_h_moved : b->black_rook_a_moved;
    }

    if (rook_moved ||
        b->board[home_row][rook_col].type != 'R' ||
        b->board[home_row][rook_col].color != side) {
        return false;
    }

    int first_empty_col = castle_kingside ? 5 : 1;
    int last_empty_col = castle_kingside ? 6 : 3;
    for (int col = first_empty_col; col <= last_empty_col; ++col) {
        if (b->board[home_row][col].color != EMPTY) {
            return false;
        }
    }

    return !(square_attacked(b, home_row, transit_col, enemy) ||
             square_attacked(b, home_row, destination_col, enemy));
}

static bool pawn_move_is_pseudo_legal(const Board *b, Move move, Piece pawn,
                                      Piece target) {
    int direction = pawn_direction(pawn.color);
    int starting_row = pawn.color == WHITE ? 6 : 1;
    int row_change = move.er - move.sr;
    int col_change = move.ec - move.sc;

    if (col_change == 0 && target.color == EMPTY) {
        if (row_change == direction) {
            return true;
        }
        if (row_change == 2 * direction &&
            move.sr == starting_row &&
            b->board[move.sr + direction][move.sc].color == EMPTY) {
            return true;
        }
    }

    if (abs(col_change) != 1 || row_change != direction) {
        return false;
    }
    if (target.color != EMPTY) {
        return true;
    }

    return move.er == b->ep_row &&
           move.ec == b->ep_col &&
           b->board[move.sr][move.ec].type == 'P' &&
           b->board[move.sr][move.ec].color == opposite(pawn.color);
}

static bool king_move_is_pseudo_legal(const Board *b, Move move) {
    if (piece_attacks(b, move.sr, move.sc, move.er, move.ec)) {
        return true;
    }
    if (move.sr != move.er || abs(move.ec - move.sc) != 2 ||
        is_in_check(b, b->turn)) {
        return false;
    }
    return can_castle(b, move, b->turn);
}

static bool promotion_is_valid(Move move, Piece piece) {
    if (move.promotion == '\0') {
        return true;
    }
    return piece.type == 'P' &&
           (move.er == 0 || move.er == 7) &&
           strchr("QRBN", move.promotion) != NULL;
}

static bool pseudo_legal(const Board *b, Move m) {
    if (!in_bounds(m.sr, m.sc) || !in_bounds(m.er, m.ec)) {
        return false;
    }

    Piece piece = b->board[m.sr][m.sc];
    Piece target = b->board[m.er][m.ec];
    if (piece.color != b->turn ||
        target.color == b->turn ||
        target.type == 'K' ||
        !promotion_is_valid(m, piece)) {
        return false;
    }

    if (piece.type == 'P') {
        return pawn_move_is_pseudo_legal(b, m, piece, target);
    }

    if (piece.type == 'K') {
        return king_move_is_pseudo_legal(b, m);
    }

    return piece_attacks(b, m.sr, m.sc, m.er, m.ec);
}

static bool move_is_en_passant(const Board *b, Move move, Piece piece) {
    return piece.type == 'P' &&
           move.er == b->ep_row &&
           move.ec == b->ep_col &&
           b->board[move.er][move.ec].color == EMPTY &&
           b->board[move.sr][move.ec].type == 'P' &&
           b->board[move.sr][move.ec].color == opposite(piece.color);
}

static void apply_move_to_board(Board *b, Move move) {
    Piece piece = b->board[move.sr][move.sc];

    if (move_is_en_passant(b, move, piece)) {
        b->board[move.sr][move.ec] = empty_piece();
    }

    if (piece.type == 'K' && abs(move.ec - move.sc) == 2) {
        bool king_side = move.ec > move.sc;
        int rook_from_col = rook_col_for(king_side);
        int rook_to_col = king_side ? 5 : 3;
        b->board[move.sr][rook_to_col] = b->board[move.sr][rook_from_col];
        b->board[move.sr][rook_from_col] = empty_piece();
    }

    b->board[move.sr][move.sc] = empty_piece();
    b->board[move.er][move.ec] = piece;

    if (piece.type == 'P' && (move.er == 0 || move.er == 7)) {
        b->board[move.er][move.ec].type =
            move.promotion == '\0' ? 'Q' : move.promotion;
    }
}

static bool legal_move(const Board *b, Move move) {
    if (!pseudo_legal(b, move)) {
        return false;
    }

    Board after_move = *b;
    apply_move_to_board(&after_move, move);
    return !is_in_check(&after_move, b->turn);
}

static char piece_key_char(Piece piece) {
    if (piece.color == EMPTY) {
        return '.';
    }
    if (piece.color == WHITE) {
        return (char)tolower((unsigned char)piece.type);
    }
    return piece.type;
}

static void position_key(const Board *b, char key[80]) {
    size_t n = 0;

    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            key[n] = piece_key_char(b->board[r][c]);
            ++n;
        }
    }

    key[n] = (b->turn == WHITE) ? 'W' : 'B';
    ++n;

    key[n] = (!b->white_king_moved && !b->white_rook_a_moved) ? '1' : '0';
    ++n;
    key[n] = (!b->white_king_moved && !b->white_rook_h_moved) ? '1' : '0';
    ++n;
    key[n] = (!b->black_king_moved && !b->black_rook_a_moved) ? '1' : '0';
    ++n;
    key[n] = (!b->black_king_moved && !b->black_rook_h_moved) ? '1' : '0';
    ++n;

    bool legal_ep = false;
    if (in_bounds(b->ep_row, b->ep_col)) {
        int direction = (b->turn == WHITE) ? 1 : -1;
        int starting_row = b->ep_row + direction;

        for (int dc = -1; dc <= 1; dc += 2) {
            int sc = b->ep_col + dc;
            if (!in_bounds(starting_row, sc)) {
                continue;
            }

            Move candidate = {starting_row, sc, b->ep_row, b->ep_col, '\0'};
            if (legal_move(b, candidate)) {
                legal_ep = true;
                break;
            }
        }
    }

    if (legal_ep) {
        key[n] = (char)('0' + b->ep_row);
        ++n;
        key[n] = (char)('0' + b->ep_col);
        ++n;
    } else {
        key[n] = '-';
        ++n;
    }

    key[n] = '\0';
}

static void record_position(Board *b) {
    char key[80];
    position_key(b, key);
    if (b->history_size == b->history_capacity) {
        size_t capacity;
        if (b->history_capacity == 0) {
            capacity = 32;
        } else {
            capacity = b->history_capacity * 2;
        }
        Position *history = realloc(b->history, capacity * sizeof(*history));
        if (!history) {
            fputs("Out of memory storing chess history.\n", stderr);
            exit(EXIT_FAILURE);
        }
        b->history = history;
        b->history_capacity = capacity;
    }
    strcpy(b->history[b->history_size++].key, key);
}

static unsigned repetition_count(const Board *b, const char *key) {
    unsigned count = 0;
    for (size_t i = 0; i < b->history_size; ++i) {
        if (strcmp(key, b->history[i].key) == 0) {
            ++count;
        }
    }
    return count;
}

static void append_move(MoveList *list, Move move) {
    if (list->count == list->capacity) {
        size_t next;
        if (list->capacity == 0) {
            next = 32;
        } else {
            next = list->capacity * 2;
        }
        Move *more = realloc(list->items, next * sizeof(*more));
        if (!more) {
            fputs("Out of memory generating moves.\n", stderr);
            exit(EXIT_FAILURE);
        }
        list->items = more;
        list->capacity = next;
    }
    list->items[list->count++] = move;
}

static void consider_move(const Board *b, MoveList *list, int sr, int sc, int er, int ec) {
    Move m = {sr, sc, er, ec, '\0'};
    if (!in_bounds(er, ec)) {
        return;
    }
    if (!legal_move(b, m)) {
        return;
    }

    if (b->board[sr][sc].type == 'P' && (er == 0 || er == 7)) {
        static const char promos[] = "QRBN";
        for (size_t i = 0; i < sizeof(promos) - 1; ++i) {
            m.promotion = promos[i];
            append_move(list, m);
        }
    } else {
        append_move(list, m);
    }
}

static void generate_sliding_moves(const Board *b, MoveList *list, int row, int col,
                                  const Direction *directions, size_t direction_count) {
    for (size_t i = 0; i < direction_count; ++i) {
        int step = 1;

        for (;;) {
            int end_row = row + directions[i].row * step;
            int end_col = col + directions[i].col * step;

            if (!in_bounds(end_row, end_col)) {
                break;
            }

            consider_move(b, list, row, col, end_row, end_col);
            if (b->board[end_row][end_col].color != EMPTY) {
                break;
            }

            ++step;
        }
    }
}

static void generate_pawn_moves(const Board *b, MoveList *list, int row, int col) {
    int direction = pawn_direction(b->board[row][col].color);

    consider_move(b, list, row, col, row + direction, col);
    consider_move(b, list, row, col, row + 2 * direction, col);
    consider_move(b, list, row, col, row + direction, col - 1);
    consider_move(b, list, row, col, row + direction, col + 1);
}

static void generate_knight_moves(const Board *b, MoveList *list, int row, int col) {
    static const Direction jumps[] = {
        {-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
        {1, -2}, {1, 2}, {2, -1}, {2, 1}
    };

    for (size_t i = 0; i < sizeof(jumps) / sizeof(jumps[0]); ++i) {
        consider_move(b, list, row, col,
                      row + jumps[i].row, col + jumps[i].col);
    }
}

static void generate_king_moves(const Board *b, MoveList *list, int row, int col) {
    for (int row_change = -1; row_change <= 1; ++row_change) {
        for (int col_change = -1; col_change <= 1; ++col_change) {
            if (row_change == 0 && col_change == 0) {
                continue;
            }
            consider_move(b, list, row, col,
                          row + row_change, col + col_change);
        }
    }

    consider_move(b, list, row, col, row, col + 2);
    consider_move(b, list, row, col, row, col - 2);
}

static void generate_piece_moves(const Board *b, MoveList *list, int row, int col) {
    static const Direction orthogonal[] = {
        {-1, 0}, {1, 0}, {0, -1}, {0, 1}
    };
    static const Direction diagonal[] = {
        {-1, -1}, {-1, 1}, {1, -1}, {1, 1}
    };
    static const Direction queen_directions[] = {
        {-1, 0}, {1, 0}, {0, -1}, {0, 1},
        {-1, -1}, {-1, 1}, {1, -1}, {1, 1}
    };

    switch (b->board[row][col].type) {
        case 'P':
            generate_pawn_moves(b, list, row, col);
            break;
        case 'N':
            generate_knight_moves(b, list, row, col);
            break;
        case 'B':
            generate_sliding_moves(b, list, row, col, diagonal,
                                   sizeof(diagonal) / sizeof(diagonal[0]));
            break;
        case 'R':
            generate_sliding_moves(b, list, row, col, orthogonal,
                                   sizeof(orthogonal) / sizeof(orthogonal[0]));
            break;
        case 'Q':
            generate_sliding_moves(b, list, row, col, queen_directions,
                                   sizeof(queen_directions) / sizeof(queen_directions[0]));
            break;
        case 'K':
            generate_king_moves(b, list, row, col);
            break;
        default:
            break;
    }
}

static MoveList generate_moves(const Board *b) {
    MoveList list = {NULL, 0, 0};

    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            if (b->board[row][col].color == b->turn) {
                generate_piece_moves(b, &list, row, col);
            }
        }
    }

    return list;
}

static void mark_rook_at_home_square(Board *b, int row, int col, Color side) {
    int home_row = home_row_for(side);
    if (row != home_row || (col != 0 && col != 7)) {
        return;
    }

    mark_rook_moved(b, side, col == 7);
}

static void update_castle_state(Board *b, Piece piece, Move m) {
    if (piece.type == 'K' && abs(m.ec - m.sc) == 2) {
        mark_rook_moved(b, piece.color, king_side_castle(m));
    }

    if (piece.type == 'K') {
        if (piece.color == WHITE) {
            b->white_king_moved = true;
        } else {
            b->black_king_moved = true;
        }
    }

    if (piece.type == 'R') {
        mark_rook_at_home_square(b, m.sr, m.sc, piece.color);
    }
}

static void make_move(Board *b, Move m) {
    Piece p = b->board[m.sr][m.sc];
    Piece target = b->board[m.er][m.ec];
    bool pawn_move = p.type == 'P';
    bool capture = target.color != EMPTY || move_is_en_passant(b, m, p);

    if (target.type == 'R') {
        mark_rook_at_home_square(b, m.er, m.ec, target.color);
    }

    update_castle_state(b, p, m);
    apply_move_to_board(b, m);

    if (pawn_move && abs(m.er - m.sr) == 2) {
        b->ep_row = (m.sr + m.er) / 2;
        b->ep_col = m.sc;
    } else {
        b->ep_row = -1;
        b->ep_col = -1;
    }

    if (pawn_move || capture) {
        b->halfmove_clock = 0;
        b->history_size = 0;
    } else {
        ++b->halfmove_clock;
    }
    if (b->turn == BLACK) {
        ++b->fullmove_number;
    }
    b->turn = opposite(b->turn);
}

static void reset_board(Board *b) {
    static const char back_row[] = "RNBQKBNR";
    int r, c;

    for (r = 0; r < 8; ++r) {
        for (c = 0; c < 8; ++c) {
            b->board[r][c] = empty_piece();
        }
    }

    for (c = 0; c < 8; ++c) {
        b->board[0][c] = (Piece){back_row[c], BLACK};
        b->board[1][c] = (Piece){'P', BLACK};
        b->board[6][c] = (Piece){'P', WHITE};
        b->board[7][c] = (Piece){back_row[c], WHITE};
    }

    b->turn = WHITE;
    b->white_king_moved = false;
    b->black_king_moved = false;
    b->white_rook_a_moved = false;
    b->white_rook_h_moved = false;
    b->black_rook_a_moved = false;
    b->black_rook_h_moved = false;
    b->ep_row = -1;
    b->ep_col = -1;
    b->halfmove_clock = 0;
    b->fullmove_number = 1;
    b->history_size = 0;
    record_position(b);
}

static bool parse_square(const char *s, int *row, int *col) {
    char file = (char)tolower((unsigned char)s[0]);
    char rank = s[1];
    if (file < 'a' || file > 'h') {
        return false;
    }
    if (rank < '1' || rank > '8') {
        return false;
    }
    *col = file - 'a';
    *row = 8 - (rank - '0');
    return true;
}

typedef enum {
    INPUT_READ_OK,
    INPUT_READ_TOO_LONG,
    INPUT_READ_EOF
} InputResult;

typedef enum {
    COMMAND_OTHER,
    COMMAND_SHORT_NEW,
    COMMAND_NEW,
    COMMAND_FINALIZE,
    COMMAND_PREVIEW,
    COMMAND_QUIT,
    COMMAND_RESTART,
    COMMAND_RESIGN,
    COMMAND_CLAIM
} Command;

static InputResult read_input_line(char *input, int capacity) {
    if (!fgets(input, capacity, stdin)) {
        return INPUT_READ_EOF;
    }

    size_t length = strlen(input);
    if (length > 0 && input[length - 1] == '\n') {
        input[--length] = '\0';
    } else if (length == (size_t)(capacity - 1)) {
        int ch = getchar();
        if (ch != '\n' && ch != EOF) {
            while ((ch = getchar()) != '\n' && ch != EOF) { }
            puts(RED "Input too long." RESET);
            return INPUT_READ_TOO_LONG;
        }
    }

    if (length > 0 && input[length - 1] == '\r')
        input[length - 1] = '\0';

    return INPUT_READ_OK;
}

static Command parse_command(const char *input) {
    if (!strcmp(input, "n") || !strcmp(input, "N")) {
        return COMMAND_SHORT_NEW;
    }
    if (!strcmp(input, "new") || !strcmp(input, "NEW") ||
        !strcmp(input, "select none")) {
        return COMMAND_NEW;
    }
    if (!strcmp(input, "f") || !strcmp(input, "F")) {
        return COMMAND_FINALIZE;
    }
    if (!strcmp(input, "p") || !strcmp(input, "P")) {
        return COMMAND_PREVIEW;
    }
    if (!strcmp(input, "quit") || !strcmp(input, "exit")) {
        return COMMAND_QUIT;
    }
    if (!strcmp(input, "restart")) {
        return COMMAND_RESTART;
    }
    if (!strcmp(input, "resign")) {
        return COMMAND_RESIGN;
    }
    if (!strcmp(input, "claim")) {
        return COMMAND_CLAIM;
    }
    return COMMAND_OTHER;
}

static bool is_game_command(Command command) {
    return command == COMMAND_QUIT || command == COMMAND_RESTART ||
           command == COMMAND_RESIGN || command == COMMAND_CLAIM;
}

/* Step 1: Inspect one piece without changing the position. */
static void legal_destination_map(const Board *position, int row, int col,
                                  Color side_to_move,
                                  bool destinations[8][8]) {
    memset(destinations, 0, sizeof(bool) * 8 * 8);
    if (!in_bounds(row, col)) {
        return;
    }
    if (position->board[row][col].color == EMPTY) {
        return;
    }

    Board view = *position;
    view.turn = side_to_move;
    MoveList moves = generate_moves(&view);
    for (size_t i = 0; i < moves.count; ++i) {
        Move move = moves.items[i];
        if (move.sr == row && move.sc == col) {
            destinations[move.er][move.ec] = true;
        }
    }
    free(moves.items);
}

static void selected_destinations(const Board *b, int row, int col,
                                  bool destinations[8][8]) {
    if (!in_bounds(row, col) || b->board[row][col].color != b->turn) {
        memset(destinations, 0, sizeof(bool) * 8 * 8);
        return;
    }

    legal_destination_map(b, row, col, b->turn, destinations);
}

/* Print available squares beside the next move prompt. */
static void print_selection_near_prompt(const Board *b, int row, int col) {
    bool destinations[8][8];
    unsigned count = 0;
    if (!in_bounds(row, col)) {
        return;
    }

    selected_destinations(b, row, col, destinations);
    printf("Selected %c%d ->", 'a' + col, 8 - row);

    for (int r = 7; r >= 0; --r) {
        for (int c = 0; c < 8; ++c) {
            if (destinations[r][c]) {
                printf(" %c%d", 'a' + c, 8 - r);
                ++count;
            }
        }
    }
    if (count == 0) {
        printf(" (no legal moves)");
    }
    puts("");
}

static void display(const Board *b, int selected_row, int selected_col) {
    bool destinations[8][8];
    selected_destinations(b, selected_row, selected_col, destinations);

    puts("\n    a   b   c   d   e   f   g   h");
    puts("  +---+---+---+---+---+---+---+---+");

    for (int row = 0; row < 8; ++row) {
        printf("%d |", 8 - row);

        for (int col = 0; col < 8; ++col) {
            Piece piece = b->board[row][col];
            bool light_square = ((row + col) % 2 == 0);
            const char *background;
            const char *foreground;
            char symbol = ' ';
            char left = ' ';
            char right = ' ';

            if (light_square) {
                background = LIGHT_SQUARE_BG;
                foreground = DARK_TEXT_FG;
            } else {
                background = DARK_SQUARE_BG;
                foreground = LIGHT_TEXT_FG;
            }

            symbol = piece_symbol(piece);
            if (row == selected_row && col == selected_col) {
                left = '[';
                right = ']';
            } else if (destinations[row][col]) {
                if (piece.color == EMPTY) {
                    symbol = '*';
                } else {
                    left = '(';
                    right = ')';
                }
            }

            /* Three visible characters per cell preserve the board layout. */
            printf("%s%s%c%c%c%s|", background, foreground,
                   left, symbol, right, RESET);
        }

        printf(" %d\n", 8 - row);
        puts("  +---+---+---+---+---+---+---+---+");
    }
    puts("    a   b   c   d   e   f   g   h\n");

    if (in_bounds(selected_row, selected_col)) {
        Piece piece = b->board[selected_row][selected_col];
        unsigned unique_squares = 0;
        printf("SELECTED: %s %c at %c%d\n", color_name(piece.color),
               piece.type, 'a' + selected_col, 8 - selected_row);
        printf("LEGAL DESTINATIONS:");
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                if (destinations[row][col]) {
                    printf(" %c%d", 'a' + col, 8 - row);
                    ++unique_squares;
                }
            }
        }
        if (unique_squares == 0) {
            printf(" (none)");
        }
        printf("\nTOTAL: %u square(s)\n", unique_squares);
        puts("LEGEND: [P]=selected piece, *=empty legal destination, (p)=legal capture");
        puts("Selection is informational: no piece has moved.\n");
    }
}

static char piece_symbol(Piece piece) {
    if (piece.color == EMPTY) {
        return ' ';
    }
    if (piece.color == WHITE) {
        return piece.type;
    }
    return (char)tolower((unsigned char)piece.type);
}

static bool dead_position(const Board *b) {
    int minors = 0, bishops = 0;
    int bishop_color = -1;
    bool bishops_same_color = true;

    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            Piece p = b->board[r][c];
            if (p.color == EMPTY || p.type == 'K') {
                continue;
            }
            if (p.type != 'B' && p.type != 'N') {
                return false;
            }

            ++minors;
            if (p.type == 'B') {
                ++bishops;
                if (bishop_color == -1) {
                    bishop_color = (r + c) % 2;
                } else if (bishop_color != (r + c) % 2) {
                    bishops_same_color = false;
                }
            }
        }
    }

    if (minors == 0 || minors == 1) {
        return true;
    }
    if (bishops == minors && bishops_same_color) {
        return true;
    }
    return false; /* Two knights versus a king is not a dead position. */
}

static bool automatic_draw(const Board *b, const char **reason) {
    if (b->halfmove_clock >= 150) {
        *reason = "75-move rule";
        return true;
    }

    char key[80];
    position_key(b, key);
    if (repetition_count(b, key) >= 5) {
        *reason = "fivefold repetition";
        return true;
    }

    if (dead_position(b)) {
        *reason = "dead position";
        return true;
    }

    return false;
}

static bool claimable_draw(const Board *b, Move intended, bool has_intended,
                           const char **reason) {
    const Board *position = b;
    Board after_move = {0};
    char key[80];

    if (has_intended) {
        if (!legal_move(b, intended)) {
            return false;
        }

        after_move = *b;
        make_move(&after_move, intended);
        position = &after_move;
    }

    if (position->halfmove_clock >= 100) {
        *reason = "50-move rule";
        return true;
    }

    position_key(position, key);
    unsigned repetitions_needed = has_intended ? 2u : 3u;
    if (repetition_count(position, key) >= repetitions_needed) {
        *reason = "threefold repetition";
        return true;
    }

    return false;
}

static bool game_over(const Board *b, bool *stalemate, const char **draw_reason) {
    MoveList moves = generate_moves(b);
    bool no_moves = (moves.count == 0);
    free(moves.items);

    *stalemate = false;
    if (no_moves) {
        *stalemate = !is_in_check(b, b->turn);
        return true;
    }
    return automatic_draw(b, draw_reason);
}


/* STEP 2: Preview analysis uses a temporary board. Only F commits a move. */
#define COMPARE_WIDTH 66

/* Return the destinations of a particular piece, even when it is the
 * other player's turn. Chess rules are checked with that side to move. */
static void list_piece_moves(const Board *position, int row, int col,
                             char *output, size_t capacity) {
    Board view = *position;
    MoveList moves;
    size_t used = 0;
    bool seen[8][8] = {{false}};
    Piece p = position->board[row][col];

    output[0] = '\0';
    if (p.color == EMPTY) {
        return;
    }

    view.turn = p.color;
    moves = generate_moves(&view);
    for (size_t i = 0; i < moves.count; ++i) {
        Move m = moves.items[i];
        bool is_this_piece = m.sr == row && m.sc == col;
        bool destination_was_listed = seen[m.er][m.ec];
        if (!is_this_piece || destination_was_listed) {
            continue;
        }

        seen[m.er][m.ec] = true;
        if (used < capacity) {
            const char *separator;
            if (used == 0) {
                separator = "";
            } else {
                separator = " ";
            }

            int n = snprintf(output + used, capacity - used,
                             "%s%c%d", separator, 'a' + m.ec, 8 - m.er);
            if (n < 0) {
                break;
            }
            if ((size_t)n >= capacity - used) {
                used = capacity - 1;
                break;
            }
            used += (size_t)n;
        }
    }

    free(moves.items);
    if (used == 0) {
        snprintf(output, capacity, "(none)");
    }
}

/* Print long destination lists side by side, wrapping at spaces. */
static size_t next_chunk(const char *source, size_t at, char *out) {
    size_t length = strlen(source);
    size_t copied = 0;

    while (at < length && source[at] == ' ') {
        ++at;
    }
    while (at < length) {
        size_t word_length = strcspn(source + at, " ");
        size_t separator_length = copied == 0 ? 0 : 1;

        if (copied + separator_length + word_length > COMPARE_WIDTH) {
            if (copied == 0) {
                memcpy(out, source + at, COMPARE_WIDTH);
                at += COMPARE_WIDTH;
                copied = COMPARE_WIDTH;
            }
            break;
        }

        if (separator_length != 0) {
            out[copied++] = ' ';
        }
        memcpy(out + copied, source + at, word_length);
        copied += word_length;
        at += word_length;

        while (at < length && source[at] == ' ') {
            ++at;
        }
    }

    out[copied] = '\0';
    return at;
}

static void print_side_by_side(const char *before, const char *after) {
    size_t left_offset = 0;
    size_t right_offset = 0;
    size_t before_length = strlen(before);
    size_t after_length = strlen(after);
    char left_chunk[COMPARE_WIDTH + 1];
    char right_chunk[COMPARE_WIDTH + 1];

    do {
        left_offset = next_chunk(before, left_offset, left_chunk);
        right_offset = next_chunk(after, right_offset, right_chunk);

        printf("%-*s | %s\n", COMPARE_WIDTH, left_chunk, right_chunk);
    } while (left_offset < before_length || right_offset < after_length);
}

static void make_piece_description(const Board *position, int row, int col,
                                   char *output, size_t capacity) {
    char destinations[256];
    Piece p = position->board[row][col];
    if (p.color == EMPTY) {
        snprintf(output, capacity, "(captured)");
        return;
    }

    list_piece_moves(position, row, col, destinations, sizeof(destinations));
    char displayed_piece = piece_symbol(p);

    snprintf(output, capacity, "%c%d %c: %s",
             'a' + col, 8 - row, displayed_piece, destinations);
}

/* STEP 2: Calculate a set of legal destination squares for ONE piece.
 * Temporarily let that piece's side move, for both BEFORE and AFTER.
 * A true entry at [row][col] means the piece can legally go there.
 */
static void piece_destination_map(const Board *position, int row, int col,
                                  bool destination[8][8]) {
    Color side_to_move = position->board[row][col].color;
    legal_destination_map(position, row, col, side_to_move, destination);
}

/* Append one destination to a bounded text buffer. */
static void append_destination(char *output, size_t capacity, int row, int col) {
    size_t used = strlen(output);
    if (used >= capacity) {
        return;
    }
    snprintf(output + used, capacity - used, "%s%c%d",
             used ? " " : "", 'a' + col, 8 - row);
}

/* Compare the same piece's destinations before and after a preview.
 * Both gained and lost squares may exist for the SAME piece.
 * A captured piece is marked CAPTURED rather than UNCHANGED.
 */
static void print_piece_changes(const Board *before, const Board *after,
                                int before_row, int before_col,
                                int after_row, int after_col, bool captured) {
    bool old_destinations[8][8];
    bool new_destinations[8][8];
    char gained[256] = "";
    char lost[256] = "";
    char unchanged[256] = "";

    if (captured) {
        puts("    CHANGE: CAPTURED - this piece has no AFTER moves.");
        return;
    }

    piece_destination_map(before, before_row, before_col, old_destinations);
    piece_destination_map(after, after_row, after_col, new_destinations);

    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            if (new_destinations[row][col] && !old_destinations[row][col]) {
                append_destination(gained, sizeof(gained), row, col);
            } else if (old_destinations[row][col] && !new_destinations[row][col]) {
                append_destination(lost, sizeof(lost), row, col);
            } else if (old_destinations[row][col] && new_destinations[row][col]) {
                append_destination(unchanged, sizeof(unchanged), row, col);
            }
        }
    }

    if (gained[0] == '\0' && lost[0] == '\0') {
        puts("    CHANGE: UNCHANGED - no destinations gained or lost.");
    } else {
        puts("    CHANGE: destinations changed.");
    }
    printf("    GAINED: %s\n", gained[0] ? gained : "(none)");
    printf("    LOST: %s\n", lost[0] ? lost : "(none)");
    printf("    UNCHANGED: %s\n", unchanged[0] ? unchanged : "(none)");
}

/* Lists every OTHER piece from the original board, aligned by identity.
 * The chosen piece is deliberately omitted. Castling rook relocation and
 * en-passant captures are handled so that before/after rows correspond. */
static bool is_en_passant_capture(const Board *before, Move proposed,
                                 int row, int col) {
    Piece moving_piece = before->board[proposed.sr][proposed.sc];
    return moving_piece.type == 'P' &&
           proposed.er == before->ep_row &&
           proposed.ec == before->ep_col &&
           row == proposed.sr &&
           col == proposed.ec;
}

static void resolve_piece_after_move(const Board *before, Move proposed,
                                   int row, int col,
                                   int *after_row, int *after_col,
                                   bool *captured) {
    *after_row = row;
    *after_col = col;
    *captured = false;

    if (row == proposed.er && col == proposed.ec) {
        *captured = true;
    }
    if (is_en_passant_capture(before, proposed, row, col)) {
        *captured = true;
    }

    Piece moving_piece = before->board[proposed.sr][proposed.sc];
    bool is_castling = moving_piece.type == 'K' &&
                       abs(proposed.ec - proposed.sc) == 2 &&
                       row == proposed.sr;
    if (is_castling) {
        if (proposed.ec == 6 && col == 7) {
            *after_col = 5;
        }
        if (proposed.ec == 2 && col == 0) {
            *after_col = 3;
        }
    }
}

static void compare_all_other_pieces(const Board *before, const Board *after,
                                     Move proposed) {
    for (Color side = WHITE; side <= BLACK; side = (Color)(side + 1)) {
        const char *side_name = (side == WHITE) ? "WHITE" : "BLACK";

        puts("");
        printf("%s PIECES (excluding selected piece)\n", side_name);
        print_side_by_side("BEFORE: square piece -> legal destinations",
                           "AFTER: square piece -> legal destinations");
        puts("-------------------------------------------------------------------+-------------------------------------------------------------------");

        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                Piece p = before->board[row][col];
                bool is_moving_side = p.color == side;
                bool is_selected_piece = row == proposed.sr && col == proposed.sc;
                if (!is_moving_side || is_selected_piece) {
                    continue;
                }

                int after_row = row;
                int after_col = col;
                char before_description[320];
                char after_description[320];
                bool captured = false;

                resolve_piece_after_move(before, proposed, row, col,
                                        &after_row, &after_col, &captured);

                make_piece_description(before, row, col,
                                       before_description, sizeof(before_description));
                if (captured) {
                    snprintf(after_description, sizeof(after_description),
                             "(captured at %c%d)", 'a' + col, 8 - row);
                } else {
                    make_piece_description(after, after_row, after_col,
                                           after_description, sizeof(after_description));
                }

                print_side_by_side(before_description, after_description);
                print_piece_changes(before, after, row, col,
                                    after_row, after_col, captured);
            }
        }
    }
}

static void show_step2_after(const Board *board, Move proposed) {
    Board simulated = *board; /* shallow copy safe: no history modification in output */
    printf("\nPREVIEW ONLY: %c%d -> %c%d (NOT PLAYED)\n",
           'a' + proposed.sc, 8 - proposed.sr, 'a' + proposed.ec, 8 - proposed.er);
    make_move(&simulated, proposed);
    puts("Widen your console to about 140 columns for the two-column report.");
    puts("CHANGE shows gained, lost, and unchanged destinations for each other piece.");
    puts("LEGAL MOVE LIST NOTE: Each side is evaluated as if it were that side's turn.");
    puts("Black and White possibilities are shown for comparison; only the actual");
    puts("side to move can play immediately. Check restrictions still apply.");
    compare_all_other_pieces(board, &simulated, proposed);
    puts("\nThe real board and player turn have NOT changed.");
    puts("Press F then Enter to FINALIZE this previewed move.");
    puts("Press P then Enter to preview another destination, or N for a new piece.");
}

/* STAGE 3S: practice boards.
 * A position can be set up from one line of standard chess notation (FEN),
 * for example the normal start:
 *   rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
 * Commands (not shown in the prompts, so version 5's screens are unchanged):
 *   fen <FEN>   set up that position, after checking that it is possible
 *   fen         print the current position as a FEN line
 * An impossible position is refused with a reason, and the game is left as
 * it was. A position where the side to move is in check is accepted. */

#define FEN_TEXT_SIZE 100

/* Splits text at single spaces into at most max_fields fields. Returns the
 * number of fields, or -1 when there are too many or a field is empty. */
static int split_fen_fields(char *text, char *fields[], int max_fields) {
    int count = 0;
    char *start = text;

    for (;;) {
        char *space = strchr(start, ' ');
        if (count == max_fields || *start == '\0' || *start == ' ') {
            return -1;
        }
        fields[count] = start;
        ++count;
        if (space == NULL) {
            return count;
        }
        *space = '\0';
        start = space + 1;
    }
}

static bool fen_piece_from_char(char symbol, Piece *piece) {
    char upper = (char)toupper((unsigned char)symbol);
    if (strchr("PNBRQK", upper) == NULL) {
        return false;
    }
    piece->type = upper;
    piece->color = isupper((unsigned char)symbol) ? WHITE : BLACK;
    return true;
}

/* Field 1: the eight ranks, from rank 8 down to rank 1. */
static const char *fen_read_placement(Board *b, const char *text) {
    int row = 0;
    int col = 0;

    for (const char *p = text; *p != '\0'; ++p) {
        if (*p == '/') {
            if (col != 8) {
                return "a rank does not have exactly 8 squares";
            }
            ++row;
            col = 0;
            if (row > 7) {
                return "more than 8 ranks";
            }
        } else if (*p >= '1' && *p <= '8') {
            col += *p - '0';
            if (col > 8) {
                return "a rank does not have exactly 8 squares";
            }
        } else if (isdigit((unsigned char)*p)) {
            return "a rank does not have exactly 8 squares"; /* 0 or 9 empty squares */
        } else {
            Piece piece;
            if (!fen_piece_from_char(*p, &piece)) {
                return "unknown piece letter";
            }
            if (col > 7) {
                return "a rank does not have exactly 8 squares";
            }
            b->board[row][col] = piece;
            ++col;
        }
    }
    if (row != 7 || col != 8) {
        return "the board must have exactly 8 ranks of 8 squares";
    }
    return NULL;
}

static bool piece_at(const Board *b, int row, int col, char type, Color color) {
    return b->board[row][col].type == type && b->board[row][col].color == color;
}

/* Field 3: castling rights, "-" or letters from KQkq in that order. Each
 * right needs the king and that rook on their starting squares. */
static const char *fen_read_castling(Board *b, const char *text) {
    bool wk = false, wq = false, bk = false, bq = false;

    if (strcmp(text, "-") != 0) {
        const char *order = "KQkq";
        const char *next = order;
        for (const char *p = text; *p != '\0'; ++p) {
            const char *found = strchr(next, *p);
            if (found == NULL) {
                return "castling field must be - or letters from KQkq in that order";
            }
            next = found + 1;
            wk = wk || *p == 'K';
            wq = wq || *p == 'Q';
            bk = bk || *p == 'k';
            bq = bq || *p == 'q';
        }
    }
    if ((wk || wq) && !piece_at(b, 7, 4, 'K', WHITE)) {
        return "White castling right without the king on e1";
    }
    if ((bk || bq) && !piece_at(b, 0, 4, 'K', BLACK)) {
        return "Black castling right without the king on e8";
    }
    if ((wk && !piece_at(b, 7, 7, 'R', WHITE)) || (wq && !piece_at(b, 7, 0, 'R', WHITE)) ||
        (bk && !piece_at(b, 0, 7, 'R', BLACK)) || (bq && !piece_at(b, 0, 0, 'R', BLACK))) {
        return "castling right without the rook on its starting square";
    }
    b->white_king_moved = !(wk || wq);
    b->black_king_moved = !(bk || bq);
    b->white_rook_h_moved = !wk;
    b->white_rook_a_moved = !wq;
    b->black_rook_h_moved = !bk;
    b->black_rook_a_moved = !bq;
    return NULL;
}

/* Field 4: the en passant square, "-" or the square a pawn just skipped. */
static const char *fen_read_en_passant(Board *b, const char *text) {
    int row, col;

    b->ep_row = -1;
    b->ep_col = -1;
    if (strcmp(text, "-") == 0) {
        return NULL;
    }
    if (strlen(text) != 2 || !parse_square(text, &row, &col)) {
        return "en passant field must be - or a square";
    }

    /* White to move: Black just moved a pawn two squares, so the square is
     * on rank 6, that pawn is on rank 5, and the square behind is empty. */
    int expected_row = (b->turn == WHITE) ? 2 : 5;
    int pawn_row = (b->turn == WHITE) ? 3 : 4;
    int start_row = (b->turn == WHITE) ? 1 : 6;
    Color mover = opposite(b->turn);
    if (row != expected_row) {
        return "en passant square is on the wrong rank for the side to move";
    }
    if (!piece_at(b, pawn_row, col, 'P', mover) ||
        b->board[row][col].color != EMPTY || b->board[start_row][col].color != EMPTY) {
        return "en passant square does not match a pawn that just moved two squares";
    }
    b->ep_row = row;
    b->ep_col = col;
    return NULL;
}

/* Fields 5 and 6: the halfmove clock and the move number. */
static bool fen_read_number(const char *text, int minimum, int *value) {
    long number = 0;

    if (*text == '\0' || strlen(text) > 5) {
        return false;
    }
    for (const char *p = text; *p != '\0'; ++p) {
        if (!isdigit((unsigned char)*p)) {
            return false;
        }
        number = number * 10 + (*p - '0');
    }
    if (number < minimum) {
        return false;
    }
    *value = (int)number;
    return true;
}

/* Piece counts that no legal game can reach. */
static const char *fen_check_material(const Board *b, Color side) {
    int count[128] = {0};
    int total = 0;

    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            Piece p = b->board[row][col];
            if (p.color != side) {
                continue;
            }
            ++count[(unsigned char)p.type];
            ++total;
            if (p.type == 'P' && (row == 0 || row == 7)) {
                return "a pawn is on the first or last rank";
            }
        }
    }
    if (count['K'] != 1) {
        return side == WHITE ? "White must have exactly one king" : "Black must have exactly one king";
    }
    int promoted = (count['Q'] > 1 ? count['Q'] - 1 : 0) + (count['R'] > 2 ? count['R'] - 2 : 0) +
                   (count['B'] > 2 ? count['B'] - 2 : 0) + (count['N'] > 2 ? count['N'] - 2 : 0);
    if (count['P'] > 8 || total > 16 || promoted > 8 - count['P']) {
        return side == WHITE ? "White has more pieces than a game allows" : "Black has more pieces than a game allows";
    }
    return NULL;
}

static int count_attackers(const Board *b, int row, int col, Color by) {
    int attackers = 0;
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            if (b->board[r][c].color == by && piece_attacks(b, r, c, row, col)) {
                ++attackers;
            }
        }
    }
    return attackers;
}

/* Builds the position in a scratch board and checks it; only a possible
 * position is copied into *b. On refusal, *reason says why and *b is
 * unchanged. The move history restarts from the new position. */
static bool load_fen(Board *b, const char *fen, const char **reason) {
    char text[FEN_TEXT_SIZE];
    char *fields[6];
    Board next = {0};
    int king_row = 0;
    int king_col = 0;

    if (strlen(fen) >= sizeof(text)) {
        *reason = "FEN text is too long";
        return false;
    }
    strcpy(text, fen);
    int field_count = split_fen_fields(text, fields, 6);
    if (field_count != 4 && field_count != 6) {
        *reason = "a FEN has 6 fields (or 4) separated by single spaces";
        return false;
    }

    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            next.board[row][col] = empty_piece();
        }
    }
    *reason = fen_read_placement(&next, fields[0]);
    if (*reason != NULL) {
        return false;
    }
    if (strcmp(fields[1], "w") != 0 && strcmp(fields[1], "b") != 0) {
        *reason = "side to move must be w or b";
        return false;
    }
    next.turn = (fields[1][0] == 'w') ? WHITE : BLACK;
    *reason = fen_check_material(&next, WHITE);
    if (*reason == NULL) {
        *reason = fen_check_material(&next, BLACK);
    }
    if (*reason == NULL) {
        *reason = fen_read_castling(&next, fields[2]);
    }
    if (*reason == NULL) {
        *reason = fen_read_en_passant(&next, fields[3]);
    }
    if (*reason != NULL) {
        return false;
    }

    next.halfmove_clock = 0;
    next.fullmove_number = 1;
    if (field_count == 6 &&
        (!fen_read_number(fields[4], 0, &next.halfmove_clock) ||
         !fen_read_number(fields[5], 1, &next.fullmove_number))) {
        *reason = "halfmove clock and move number must be whole numbers (move number at least 1)";
        return false;
    }

    if (is_in_check(&next, opposite(next.turn))) {
        *reason = "the side that is not to move is in check";
        return false;
    }
    if (!find_king(&next, next.turn, &king_row, &king_col)) {
        *reason = "the side to move has no king";
        return false;
    }
    if (count_attackers(&next, king_row, king_col, opposite(next.turn)) > 2) {
        *reason = "the king to move is attacked by more than two pieces";
        return false;
    }

    next.history = b->history;
    next.history_capacity = b->history_capacity;
    next.history_size = 0;
    *b = next;
    record_position(b);
    *reason = NULL;
    return true;
}

/* Writes the current position as a FEN line into out. */
static void board_to_fen(const Board *b, char out[FEN_TEXT_SIZE]) {
    size_t n = 0;

    for (int row = 0; row < 8; ++row) {
        int empty = 0;
        for (int col = 0; col < 8; ++col) {
            Piece p = b->board[row][col];
            if (p.color == EMPTY) {
                ++empty;
                continue;
            }
            if (empty > 0) {
                out[n++] = (char)('0' + empty);
                empty = 0;
            }
            out[n++] = piece_symbol(p);
        }
        if (empty > 0) {
            out[n++] = (char)('0' + empty);
        }
        if (row < 7) {
            out[n++] = '/';
        }
    }
    out[n++] = ' ';
    out[n++] = (b->turn == WHITE) ? 'w' : 'b';
    out[n++] = ' ';
    size_t castling_start = n;
    if (!b->white_king_moved && !b->white_rook_h_moved) {
        out[n++] = 'K';
    }
    if (!b->white_king_moved && !b->white_rook_a_moved) {
        out[n++] = 'Q';
    }
    if (!b->black_king_moved && !b->black_rook_h_moved) {
        out[n++] = 'k';
    }
    if (!b->black_king_moved && !b->black_rook_a_moved) {
        out[n++] = 'q';
    }
    if (n == castling_start) {
        out[n++] = '-';
    }
    out[n++] = ' ';
    if (in_bounds(b->ep_row, b->ep_col)) {
        out[n++] = (char)('a' + b->ep_col);
        out[n++] = (char)('0' + 8 - b->ep_row);
    } else {
        out[n++] = '-';
    }
    snprintf(out + n, FEN_TEXT_SIZE - n, " %d %d", b->halfmove_clock, b->fullmove_number);
}

/* STAGE 3S: following one piece across a move, for the calculator.
 * Every piece of the position before the move is in exactly one case:
 * it stays on its square, it is the moved piece, it is the castling rook,
 * it is captured (also en passant), or it is the pawn that promotes.
 * after_type is the piece's type after the move (it changes on promotion).
 * Not static: the tests use it now, and stage 3A's report will. */
typedef enum { FOLLOW_STAYS, FOLLOW_MOVED, FOLLOW_CASTLING_ROOK, FOLLOW_CAPTURED, FOLLOW_PROMOTED } FollowCase;

FollowCase follow_piece(const Board *before, Move move, int row, int col,
                        int *after_row, int *after_col, char *after_type);
FollowCase follow_piece(const Board *before, Move move, int row, int col,
                        int *after_row, int *after_col, char *after_type) {
    Piece piece = before->board[row][col];
    Piece mover = before->board[move.sr][move.sc];
    bool captured;

    *after_type = piece.type;
    if (row == move.sr && col == move.sc) {
        *after_row = move.er;
        *after_col = move.ec;
        if (mover.type == 'P' && (move.er == 0 || move.er == 7)) {
            *after_type = (move.promotion == '\0') ? 'Q' : move.promotion;
            return FOLLOW_PROMOTED;
        }
        return FOLLOW_MOVED;
    }

    resolve_piece_after_move(before, move, row, col, after_row, after_col, &captured);
    if (captured) {
        *after_row = -1;
        *after_col = -1;
        return FOLLOW_CAPTURED;
    }
    if (*after_col != col) {
        return FOLLOW_CASTLING_ROOK;
    }
    return FOLLOW_STAYS;
}

/* STAGE 3A: the calculator's fact report (type R after a preview).
 * It only counts and lists what a move changes; it never chooses a move.
 * The exact format is in docs/CALCULATOR_SPEC.md, Section 7.
 *
 * Piece identity: every piece gets a permanent number when tracking starts
 * (the normal start, restart, or a FEN set-up). The number follows the
 * piece wherever it goes (a promoted pawn keeps its number), so a move it
 * lost earlier is recognised when it comes back (RESTORED), and a move that
 * is still missing is never charged again. In the report a piece is named
 * by its type and square BEFORE the move, for example "Q d1" (spec 7). */

#define CALC_MAX_ID 33 /* at most 32 pieces, numbered from 1 */

/* The identity number of the piece on each square, 0 = empty. (A struct, so
 * it can be passed as const in C99.) */
typedef struct {
    int id[8][8];
} IdGrid;

typedef struct {
    IdGrid grid;                  /* identity number on each square */
    Color color[CALC_MAX_ID];
    bool lost[CALC_MAX_ID][64];   /* (piece, square) pairs lost and not yet back */
} Tracker;

/* Everything the report needs to know about one position. */
typedef struct {
    bool present[CALC_MAX_ID];
    int row[CALC_MAX_ID];
    int col[CALC_MAX_ID];
    bool legal[CALC_MAX_ID][64];  /* L: legal destinations, by square index */
    int legal_count[CALC_MAX_ID];
    bool reach[3][64];            /* team reach, indexed by Color */
} Snapshot;

/* A growing text, so the report can be printed or tested as one string. */
typedef struct {
    char *text;
    size_t length;
    size_t capacity;
} TextBuffer;

static void text_add(TextBuffer *t, const char *format, ...) {
    va_list args;
    char line[512];

    va_start(args, format);
    int n = vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    if (n < 0) {
        return;
    }
    size_t add = strlen(line);
    if (t->length + add + 1 > t->capacity) {
        size_t capacity = t->capacity == 0 ? 1024 : t->capacity;
        while (t->length + add + 1 > capacity) {
            capacity *= 2;
        }
        char *more = realloc(t->text, capacity);
        if (!more) {
            fputs("Out of memory building the calculator report.\n", stderr);
            exit(EXIT_FAILURE);
        }
        t->text = more;
        t->capacity = capacity;
    }
    memcpy(t->text + t->length, line, add + 1);
    t->length += add;
}

static int square_index(int row, int col) {
    return row * 8 + col; /* rank 8 to rank 1, file a to h */
}

static void square_text(int index, char out[3]) {
    out[0] = (char)('a' + index % 8);
    out[1] = (char)('0' + 8 - index / 8);
    out[2] = '\0';
}

/* Gives every piece its name, at the start of tracking. */
static void tracker_init(Tracker *t, const Board *b) {
    int next = 1;

    memset(t, 0, sizeof(*t));
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            Piece p = b->board[row][col];
            if (p.color == EMPTY || next >= CALC_MAX_ID) {
                continue;
            }
            t->grid.id[row][col] = next;
            t->color[next] = p.color;
            ++next;
        }
    }
}

/* Report names: each piece's type and square before the move. */
typedef struct {
    char name[CALC_MAX_ID][8];
} Names;

static void make_names(const Board *b, const Tracker *t, Names *names) {
    memset(names, 0, sizeof(*names));
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            int n = t->grid.id[row][col];
            if (n != 0) {
                char square[3];
                square_text(square_index(row, col), square);
                snprintf(names->name[n], 8, "%c %s", b->board[row][col].type, square);
            }
        }
    }
}

static void take_snapshot(const Board *b, const IdGrid *ids, Snapshot *s) {
    memset(s, 0, sizeof(*s));
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            int n = ids->id[row][col];
            if (n == 0) {
                continue;
            }
            bool destinations[8][8];
            Color side = b->board[row][col].color;
            legal_destination_map(b, row, col, side, destinations);
            s->present[n] = true;
            s->row[n] = row;
            s->col[n] = col;
            for (int i = 0; i < 64; ++i) {
                if (destinations[i / 8][i % 8]) {
                    s->legal[n][i] = true;
                    ++s->legal_count[n];
                    s->reach[side][i] = true;
                }
            }
        }
    }
}

/* Where every named piece is after the move (follow_piece, stage 3S). */
static void ids_after_move(const Board *before, Move m, const IdGrid *ids,
                           IdGrid *after, bool captured[CALC_MAX_ID]) {
    memset(after, 0, sizeof(*after));
    memset(captured, 0, sizeof(bool) * CALC_MAX_ID);
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            int n = ids->id[row][col];
            int to_row, to_col;
            char type_after;
            if (n == 0) {
                continue;
            }
            if (follow_piece(before, m, row, col, &to_row, &to_col, &type_after) == FOLLOW_CAPTURED) {
                captured[n] = true;
            } else {
                after->id[to_row][to_col] = n;
            }
        }
    }
}

/* Why a piece has no legal move: BLOCKED, CHECK or PIN (spec Section 3). */
static const char *zero_move_cause(const Board *b, int row, int col) {
    Board view = *b;
    int pseudo = 0;

    view.turn = b->board[row][col].color;
    for (int er = 0; er < 8; ++er) {
        for (int ec = 0; ec < 8; ++ec) {
            Move m = {row, col, er, ec, '\0'};
            if (pseudo_legal(&view, m)) {
                ++pseudo;
            }
        }
    }
    if (pseudo == 0) {
        return "BLOCKED";
    }
    return is_in_check(&view, view.turn) ? "CHECK" : "PIN";
}

/* Does the piece on (pr,pc) protect (same colour) or attack (other colour)
 * the piece on (qr,qc)? Kings are never protected or listed as attacked. */
static bool linked(const Board *b, int pr, int pc, int qr, int qc, bool protect) {
    Piece p = b->board[pr][pc];
    Piece q = b->board[qr][qc];
    if (p.color == EMPTY || q.color == EMPTY || q.type == 'K' || (pr == qr && pc == qc)) {
        return false;
    }
    if ((p.color == q.color) != protect) {
        return false;
    }
    return piece_attacks(b, pr, pc, qr, qc);
}

/* Lists links that exist in `has` and not in `lacks` (identity pairs). */
static void report_links(TextBuffer *out, const Names *names, const char *keyword, Color side, bool protect,
                         const Board *has, const IdGrid *has_id, const Board *lacks, const Snapshot *lacks_snap) {
    for (int pr = 0; pr < 8; ++pr) {
        for (int pc = 0; pc < 8; ++pc) {
            int p = has_id->id[pr][pc];
            if (p == 0 || has->board[pr][pc].color != side) {
                continue;
            }
            for (int qr = 0; qr < 8; ++qr) {
                for (int qc = 0; qc < 8; ++qc) {
                    int q = has_id->id[qr][qc];
                    if (q == 0 || !linked(has, pr, pc, qr, qc, protect)) {
                        continue;
                    }
                    bool in_other = lacks_snap->present[p] && lacks_snap->present[q] &&
                                    linked(lacks, lacks_snap->row[p], lacks_snap->col[p],
                                           lacks_snap->row[q], lacks_snap->col[q], protect);
                    if (!in_other) {
                        text_add(out, "%s %s %s %s\n", keyword, color_name(side), names->name[p], names->name[q]);
                    }
                }
            }
        }
    }
}

static void move_text(Move m, char out[6]) {
    out[0] = (char)('a' + m.sc);
    out[1] = (char)('0' + 8 - m.sr);
    out[2] = (char)('a' + m.ec);
    out[3] = (char)('0' + 8 - m.er);
    out[4] = m.promotion ? (char)tolower((unsigned char)m.promotion) : '\0';
    out[5] = '\0';
}

/* The moves of `side` (made the side to move) that give checkmate. */
static void report_mates(TextBuffer *out, const Board *b, Color side) {
    Board view = *b;
    MoveList moves;
    bool any = false;

    view.turn = side;
    moves = generate_moves(&view);
    for (size_t i = 0; i < moves.count; ++i) {
        Board next = view;
        make_move(&next, moves.items[i]);
        MoveList replies = generate_moves(&next);
        bool mate = replies.count == 0 && is_in_check(&next, next.turn);
        free(replies.items);
        if (mate) {
            char text[6];
            move_text(moves.items[i], text);
            if (!any) {
                text_add(out, "MATE1 %s", color_name(side));
                any = true;
            }
            text_add(out, " %s", text);
        }
    }
    free(moves.items);
    if (any) {
        text_add(out, "\n");
    }
}

static void report_header(TextBuffer *out, const Board *b, const Tracker *t, Move m,
                          const bool captured[CALC_MAX_ID]) {
    Piece mover = b->board[m.sr][m.sc];
    char text[6], from[3], to[3];

    move_text(m, text);
    text_add(out, "CALC BEGIN %s\n", text);
    square_text(square_index(m.sr, m.sc), from);
    square_text(square_index(m.er, m.ec), to);
    text_add(out, "MOVED %s %c %s %s\n", color_name(mover.color), mover.type, from, to);
    if (mover.type == 'K' && abs(m.ec - m.sc) == 2) {
        char rook_from[3], rook_to[3];
        square_text(square_index(m.sr, m.ec > m.sc ? 7 : 0), rook_from);
        square_text(square_index(m.sr, m.ec > m.sc ? 5 : 3), rook_to);
        text_add(out, "CASTLE %s %s %s\n", color_name(mover.color), rook_from, rook_to);
    }
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            int n = t->grid.id[row][col];
            if (n != 0 && captured[n]) {
                char square[3];
                square_text(square_index(row, col), square);
                text_add(out, "CAPTURED %s %c %s\n", color_name(t->color[n]), b->board[row][col].type, square);
            }
        }
    }
    if (mover.type == 'P' && (m.er == 0 || m.er == 7)) {
        text_add(out, "PROMOTED %s %s %s %c\n", color_name(mover.color), from, to,
                 m.promotion ? m.promotion : 'Q');
    }
}

static void report_zero(TextBuffer *out, const Board *b, const Board *a, const Tracker *t,
                        const Snapshot *s0, const Snapshot *s1, Color side) {
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            int n = t->grid.id[row][col];
            if (n == 0 || t->color[n] != side || !s1->present[n]) {
                continue;
            }
            bool before = s0->legal_count[n] == 0;
            bool after = s1->legal_count[n] == 0;
            if (!before && !after) {
                continue;
            }
            char name[8];
            char square[3];
            square_text(square_index(row, col), square);
            snprintf(name, sizeof(name), "%c %s", b->board[row][col].type, square);
            const char *cause = after ? zero_move_cause(a, s1->row[n], s1->col[n])
                                      : zero_move_cause(b, row, col);
            text_add(out, "ZERO %s %s %s %s\n", color_name(side), name, cause,
                     before && after ? "both" : (after ? "after" : "before"));
        }
    }
}

/* LOST, then GAINED, then RESTORED, for one side; returns the counts. */
static void report_pairs(TextBuffer *out, const Tracker *t, const Names *names, const Snapshot *s0,
                         const Snapshot *s1, Color side, const char *keyword, int *count) {
    *count = 0;
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            int n = t->grid.id[row][col];
            bool any = false;
            if (n == 0 || t->color[n] != side || !s1->present[n]) {
                continue;
            }
            for (int i = 0; i < 64; ++i) {
                bool lost = s0->legal[n][i] && !s1->legal[n][i];
                bool new_square = !s0->legal[n][i] && s1->legal[n][i];
                bool wanted = (keyword[0] == 'L' && lost) ||
                              (keyword[0] == 'G' && new_square && !t->lost[n][i]) ||
                              (keyword[0] == 'R' && new_square && t->lost[n][i]);
                if (!wanted) {
                    continue;
                }
                char square[3];
                square_text(i, square);
                if (!any) {
                    text_add(out, "%s %s %s", keyword, color_name(side), names->name[n]);
                    any = true;
                }
                text_add(out, " %s", square);
                ++*count;
            }
            if (any) {
                text_add(out, "\n");
            }
        }
    }
}

/* DENIED (with by-capture) or OPENED squares for one side. */
static int report_team_squares(TextBuffer *out, const Board *b, const Tracker *t, const Snapshot *s0,
                               const Snapshot *s1, const bool captured[CALC_MAX_ID], Color side, bool denied) {
    int count = 0;
    for (int i = 0; i < 64; ++i) {
        bool change = denied ? (s0->reach[side][i] && !s1->reach[side][i])
                             : (!s0->reach[side][i] && s1->reach[side][i]);
        if (!change) {
            continue;
        }
        bool only_captured = denied;
        for (int row = 0; row < 8 && only_captured; ++row) {
            for (int col = 0; col < 8; ++col) {
                int n = t->grid.id[row][col];
                if (n != 0 && b->board[row][col].color == side && s0->legal[n][i] && !captured[n]) {
                    only_captured = false;
                    break;
                }
            }
        }
        char square[3];
        square_text(i, square);
        text_add(out, "%s %s %s%s\n", denied ? "DENIED" : "OPENED", color_name(side), square,
                 only_captured ? " by-capture" : "");
        ++count;
    }
    return count;
}

static int count_zero_after(const Tracker *t, const Snapshot *s1, Color side) {
    int zero = 0;
    for (int n = 1; n < CALC_MAX_ID; ++n) {
        if (s1->present[n] && t->color[n] == side && s1->legal_count[n] == 0) {
            ++zero;
        }
    }
    return zero;
}

/* Builds the whole report for move m from position b, without changing b
 * or the tracker. */
static void calc_report(const Board *b, const Tracker *t, Move m, TextBuffer *out) {
    Board a = *b;
    IdGrid after_id;
    bool captured[CALC_MAX_ID];
    Snapshot s0, s1;
    int lost[3], gained[3], restored[3], denied[3], opened[3];
    static const Color sides[2] = {WHITE, BLACK};
    Names names;

    make_names(b, t, &names);
    make_move(&a, m);
    ids_after_move(b, m, &t->grid, &after_id, captured);
    take_snapshot(b, &t->grid, &s0);
    take_snapshot(&a, &after_id, &s1);

    report_header(out, b, t, m, captured);
    for (int i = 0; i < 2; ++i) {
        report_zero(out, b, &a, t, &s0, &s1, sides[i]);
    }
    for (int i = 0; i < 2; ++i) {
        report_pairs(out, t, &names, &s0, &s1, sides[i], "LOST", &lost[sides[i]]);
    }
    for (int i = 0; i < 2; ++i) {
        report_pairs(out, t, &names, &s0, &s1, sides[i], "GAINED", &gained[sides[i]]);
    }
    for (int i = 0; i < 2; ++i) {
        report_pairs(out, t, &names, &s0, &s1, sides[i], "RESTORED", &restored[sides[i]]);
    }
    for (int i = 0; i < 2; ++i) {
        denied[sides[i]] = report_team_squares(out, b, t, &s0, &s1, captured, sides[i], true);
    }
    for (int i = 0; i < 2; ++i) {
        opened[sides[i]] = report_team_squares(out, b, t, &s0, &s1, captured, sides[i], false);
    }
    for (int i = 0; i < 2; ++i) {
        report_links(out, &names, "PROTECT+", sides[i], true, &a, &after_id, b, &s0);
    }
    for (int i = 0; i < 2; ++i) {
        report_links(out, &names, "PROTECT-", sides[i], true, b, &t->grid, &a, &s1);
    }
    for (int i = 0; i < 2; ++i) {
        report_links(out, &names, "ATTACK+", sides[i], false, &a, &after_id, b, &s0);
    }
    for (int i = 0; i < 2; ++i) {
        report_links(out, &names, "ATTACK-", sides[i], false, b, &t->grid, &a, &s1);
    }
    for (int i = 0; i < 2; ++i) {
        if (is_in_check(&a, sides[i])) {
            text_add(out, "CHECK %s\n", color_name(sides[i]));
        }
    }
    /* Version 9 (spec v1.5): a move that ends the game (checkmate or
     * stalemate) leaves no further turn, so there are no MATE1 lines. */
    MoveList answers = generate_moves(&a);
    bool game_over = answers.count == 0;
    free(answers.items);
    for (int i = 0; !game_over && i < 2; ++i) {
        report_mates(out, &a, sides[i]);
    }
    for (int i = 0; i < 2; ++i) {
        Color s = sides[i];
        text_add(out, "COUNT %s U1 %d U2 %d U3 %d GAINED %d RESTORED %d OPENED %d\n", color_name(s),
                 count_zero_after(t, &s1, s), lost[s], denied[s], gained[s], restored[s], opened[s]);
    }
    text_add(out, "CALC END\n");
}

/* Called when a move is finalized with F: remembers which pairs were lost
 * (so a later return is RESTORED, and a pair still missing is never charged
 * again) and moves every name to its new square. */
static void tracker_commit(Tracker *t, const Board *b, Move m) {
    Board a = *b;
    IdGrid after_id;
    bool captured[CALC_MAX_ID];
    Snapshot s0, s1;

    make_move(&a, m);
    ids_after_move(b, m, &t->grid, &after_id, captured);
    take_snapshot(b, &t->grid, &s0);
    take_snapshot(&a, &after_id, &s1);
    for (int n = 1; n < CALC_MAX_ID; ++n) {
        if (captured[n]) {
            memset(t->lost[n], 0, sizeof(t->lost[n]));
            continue;
        }
        if (!s1.present[n]) {
            continue;
        }
        for (int i = 0; i < 64; ++i) {
            if (s0.legal[n][i] && !s1.legal[n][i]) {
                t->lost[n][i] = true;
            } else if (!s0.legal[n][i] && s1.legal[n][i]) {
                t->lost[n][i] = false;
            }
        }
    }
    t->grid = after_id;
}

/* STAGE 3B: the reply view (type "replies" after a preview).
 * For the previewed move m, every legal reply of the other side is listed,
 * in a fixed order, with:
 *   - what happens to the mover's mate-in-one threats:
 *       STOPS      m made a threat, and after this reply there is none
 *       LEAVES     m made a threat, and after this reply one remains
 *       NO-THREAT  m made no threat, and the reply gives none
 *       ALLOWS     m made no threat, but after this reply the mover has one
 *   - the reply's counts for both sides (as in the R report), measured with
 *     m treated as played, so a move m took away that the reply gives back
 *     is counted as RESTORED.
 * "replies <move>" prints the full R report of that one reply.
 * Nothing is chosen, scored or played. */

typedef struct {
    int zero_after;
    int lost;
    int denied;
    int gained;
    int restored;
    int opened;
} SideCounts;

/* The counts of the R report's COUNT line, for move m from b with tracker t. */
static void calc_counts(const Board *b, const Tracker *t, Move m, SideCounts counts[3]) {
    Board a = *b;
    IdGrid after_id;
    bool captured[CALC_MAX_ID];
    Snapshot s0, s1;

    make_move(&a, m);
    ids_after_move(b, m, &t->grid, &after_id, captured);
    take_snapshot(b, &t->grid, &s0);
    take_snapshot(&a, &after_id, &s1);
    memset(counts, 0, sizeof(SideCounts) * 3);
    for (int n = 1; n < CALC_MAX_ID; ++n) {
        if (!s1.present[n]) {
            continue;
        }
        SideCounts *c = &counts[t->color[n]];
        if (s1.legal_count[n] == 0) {
            ++c->zero_after;
        }
        for (int i = 0; i < 64; ++i) {
            if (s0.legal[n][i] && !s1.legal[n][i]) {
                ++c->lost;
            } else if (!s0.legal[n][i] && s1.legal[n][i]) {
                if (t->lost[n][i]) {
                    ++c->restored;
                } else {
                    ++c->gained;
                }
            }
        }
    }
    for (int side = WHITE; side <= BLACK; ++side) {
        for (int i = 0; i < 64; ++i) {
            if (s0.reach[side][i] && !s1.reach[side][i]) {
                ++counts[side].denied;
            } else if (!s0.reach[side][i] && s1.reach[side][i]) {
                ++counts[side].opened;
            }
        }
    }
}

static int count_mate_in_one(const Board *b, Color side) {
    Board view = *b;
    int mates = 0;

    view.turn = side;
    MoveList moves = generate_moves(&view);
    for (size_t i = 0; i < moves.count; ++i) {
        Board next = view;
        make_move(&next, moves.items[i]);
        MoveList replies = generate_moves(&next);
        if (replies.count == 0 && is_in_check(&next, next.turn)) {
            ++mates;
        }
        free(replies.items);
    }
    free(moves.items);
    return mates;
}

/* Fixed order of replies: by start square, then end square (rank 8 to 1,
 * file a to h), then promotion piece Q, R, B, N. */
static int promotion_rank(char promotion) {
    const char *order = "QRBN";
    const char *at = promotion ? strchr(order, promotion) : NULL;
    return at ? (int)(at - order) + 1 : 0;
}

static int compare_moves(const void *left, const void *right) {
    const Move *a = left;
    const Move *b = right;
    int ka = ((a->sr * 8 + a->sc) * 64 + a->er * 8 + a->ec) * 8 + promotion_rank(a->promotion);
    int kb = ((b->sr * 8 + b->sc) * 64 + b->er * 8 + b->ec) * 8 + promotion_rank(b->promotion);
    return (ka > kb) - (ka < kb);
}

static void counts_text(TextBuffer *out, const SideCounts counts[3]) {
    for (int side = WHITE; side <= BLACK; ++side) {
        const SideCounts *c = &counts[side];
        text_add(out, " %s U1 %d U2 %d U3 %d GAINED %d RESTORED %d OPENED %d", color_name((Color)side),
                 c->zero_after, c->lost, c->denied, c->gained, c->restored, c->opened);
    }
}

/* The position after m, and the tracker as if m had been finalized. */
static void position_after(const Board *b, const Tracker *t, Move m, Board *after, Tracker *after_tracker) {
    *after = *b;
    *after_tracker = *t;
    tracker_commit(after_tracker, b, m);
    make_move(after, m);
}

/* Builds the reply view for move m from position b, without changing b or t. */
static void replies_report(const Board *b, const Tracker *t, Move m, TextBuffer *out) {
    Board a;
    Tracker ta;
    char text[6];
    int stops = 0, leaves = 0, no_threat = 0, allows = 0;

    position_after(b, t, m, &a, &ta);
    Color mover = b->board[m.sr][m.sc].color;
    move_text(m, text);
    text_add(out, "REPLIES BEGIN %s\n", text);

    /* If m ends the game there is nothing to answer, so no THREAT line. */
    MoveList replies = generate_moves(&a);
    bool threat = false;
    if (replies.count == 0) {
        text_add(out, "NO REPLIES %s\n", is_in_check(&a, a.turn) ? "checkmate" : "stalemate");
    } else {
        TextBuffer threats = {NULL, 0, 0};
        report_mates(&threats, &a, mover);
        threat = threats.length > 0;
        if (threat) {
            text_add(out, "THREAT%s", threats.text + strlen("MATE1"));
        } else {
            text_add(out, "THREAT %s none\n", color_name(mover));
        }
        free(threats.text);
    }
    if (replies.count > 1) {
        qsort(replies.items, replies.count, sizeof(Move), compare_moves);
    }
    for (size_t i = 0; i < replies.count; ++i) {
        Board after_reply = a;
        SideCounts counts[3];
        make_move(&after_reply, replies.items[i]);
        bool still = count_mate_in_one(&after_reply, mover) > 0;
        const char *label;
        if (threat && still) {
            label = "LEAVES";
            ++leaves;
        } else if (threat) {
            label = "STOPS";
            ++stops;
        } else if (still) {
            label = "ALLOWS";
            ++allows;
        } else {
            label = "NO-THREAT";
            ++no_threat;
        }
        calc_counts(&a, &ta, replies.items[i], counts);
        move_text(replies.items[i], text);
        text_add(out, "REPLY %s %s", text, label);
        counts_text(out, counts);
        text_add(out, "\n");
    }
    text_add(out, "SUMMARY REPLIES %d STOPS %d LEAVES %d NO-THREAT %d ALLOWS %d\n", (int)replies.count,
             stops, leaves, no_threat, allows);
    text_add(out, "REPLIES END\n");
    free(replies.items);
}

/* The full R report of one reply r to move m (m treated as played). */
static bool reply_detail(const Board *b, const Tracker *t, Move m, Move r, TextBuffer *out) {
    Board a;
    Tracker ta;

    position_after(b, t, m, &a, &ta);
    if (!legal_move(&a, r)) {
        return false;
    }
    calc_report(&a, &ta, r, out);
    return true;
}

/* Reads a reply typed as e7e5 or e2e1q; false if it is not well formed. */
static bool parse_move_text(const char *text, Move *m) {
    size_t length = strlen(text);
    int sr, sc, er, ec;

    if ((length != 4 && length != 5) || !parse_square(text, &sr, &sc) || !parse_square(text + 2, &er, &ec)) {
        return false;
    }
    m->sr = sr;
    m->sc = sc;
    m->er = er;
    m->ec = ec;
    m->promotion = '\0';
    if (length == 5) {
        char p = (char)toupper((unsigned char)text[4]);
        if (strchr("QRBN", p) == NULL) {
            return false;
        }
        m->promotion = p;
    }
    return true;
}

/* STAGE 3C: the loop (type "loop N", or "loop N K").
 * From the current position, every legal sequence of 1 to N plies is
 * visited, with nothing skipped (no pruning). Reported, without choosing a
 * move: how many positions each ply has, every sequence that ends in
 * checkmate (with the mated side), stalemates, and for each first move of
 * the side to move (S) whether a mate of the other side is forced or only
 * possible within N plies (spec Section 5b). With K, also how many sequences
 * leave the side to move with at least K pieces that cannot move.
 * "trace <moves>" prints the R report of each move of one sequence, with
 * the piece memory carried from move to move as if each were finalized.
 * Neither command changes the game. */

#define LOOP_MAX_DEPTH 6
#define LOOP_MAX_FIRST 256

typedef struct {
    int depth;
    int zero_k;
    Color mover;
    long positions[LOOP_MAX_DEPTH + 1];
    long mated[LOOP_MAX_DEPTH + 1][3];
    long stalemates[LOOP_MAX_DEPTH + 1];
    long zero[LOOP_MAX_DEPTH + 1];
    long first_mated[LOOP_MAX_FIRST][3];
    int first_index;
    bool reply_reaches_mate; /* the current reply (ply 2) has a mate of the other side below it */
    int replies_with_mate[LOOP_MAX_FIRST];
    Move line[LOOP_MAX_DEPTH];
    TextBuffer *out;
} LoopState;

static MoveList sorted_moves(const Board *b) {
    MoveList moves = generate_moves(b);
    if (moves.count > 1) {
        qsort(moves.items, moves.count, sizeof(Move), compare_moves);
    }
    return moves;
}

/* Pieces of the side to move that have no legal move. */
static int zero_move_pieces(const Board *b, const MoveList *moves) {
    bool moves_from[64] = {false};
    int zero = 0;
    for (size_t i = 0; i < moves->count; ++i) {
        moves_from[moves->items[i].sr * 8 + moves->items[i].sc] = true;
    }
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            if (b->board[r][c].color == b->turn && !moves_from[r * 8 + c]) {
                ++zero;
            }
        }
    }
    return zero;
}

/* Visits position b, reached after `ply` plies (ply >= 1). */
static void loop_walk(LoopState *s, const Board *b, int ply) {
    MoveList moves = sorted_moves(b);
    ++s->positions[ply];
    if (s->zero_k > 0 && zero_move_pieces(b, &moves) >= s->zero_k) {
        ++s->zero[ply];
    }
    if (moves.count == 0) {
        if (is_in_check(b, b->turn)) {
            char text[6];
            ++s->mated[ply][b->turn];
            ++s->first_mated[s->first_index][b->turn];
            if (b->turn != s->mover && ply >= 3) {
                s->reply_reaches_mate = true;
            }
            text_add(s->out, "MATE %d", ply);
            for (int i = 0; i < ply; ++i) {
                move_text(s->line[i], text);
                text_add(s->out, " %s", text);
            }
            text_add(s->out, " MATED %s\n", color_name(b->turn));
        } else {
            ++s->stalemates[ply];
        }
    } else if (ply < s->depth) {
        for (size_t i = 0; i < moves.count; ++i) {
            Board next = *b;
            make_move(&next, moves.items[i]);
            s->line[ply] = moves.items[i];
            if (ply == 1) {
                s->reply_reaches_mate = false;
            }
            loop_walk(s, &next, ply + 1);
            if (ply == 1 && s->reply_reaches_mate) {
                ++s->replies_with_mate[s->first_index];
            }
        }
    }
    free(moves.items);
}

/* Forced within n plies for the side to move in b (spec Section 5b): a move
 * that gives checkmate, or (n >= 3) a move after which every reply leaves a
 * mate forced within n - 2 plies. */
static bool mate_forced(const Board *b, int n) {
    bool forced = false;
    MoveList moves;

    if (n < 1) {
        return false;
    }
    moves = generate_moves(b);
    for (size_t i = 0; i < moves.count && !forced; ++i) {
        Board next = *b;
        make_move(&next, moves.items[i]);
        MoveList replies = generate_moves(&next);
        if (replies.count == 0) {
            forced = is_in_check(&next, next.turn);
        } else if (n >= 3) {
            bool all = true;
            for (size_t j = 0; j < replies.count && all; ++j) {
                Board after = next;
                make_move(&after, replies.items[j]);
                all = mate_forced(&after, n - 2);
            }
            forced = all;
        }
        free(replies.items);
    }
    free(moves.items);
    return forced;
}

/* First move m (already played into `after`): forced within n plies? */
static bool first_move_forces(const Board *after, int n) {
    MoveList replies = generate_moves(after);
    bool forced;
    if (replies.count == 0) {
        forced = is_in_check(after, after->turn);
    } else {
        forced = n >= 3;
        for (size_t j = 0; j < replies.count && forced; ++j) {
            Board next = *after;
            make_move(&next, replies.items[j]);
            forced = mate_forced(&next, n - 2);
        }
    }
    free(replies.items);
    return forced;
}

/* Builds the loop report for position b, depth 1..LOOP_MAX_DEPTH. */
static void loop_report(const Board *b, int depth, int zero_k, TextBuffer *out) {
    static LoopState s;
    MoveList first = sorted_moves(b);
    bool any_forced = false, any_possible = false;
    char text[6];

    memset(&s, 0, sizeof(s));
    s.depth = depth;
    s.zero_k = zero_k;
    s.mover = b->turn;
    s.out = out;
    text_add(out, "LOOP BEGIN %s DEPTH %d", color_name(b->turn), depth);
    if (zero_k > 0) {
        text_add(out, " ZERO %d", zero_k);
    }
    text_add(out, "\n");
    if (first.count == 0) {
        text_add(out, "NO MOVES %s\n", is_in_check(b, b->turn) ? "checkmate" : "stalemate");
        text_add(out, "LOOP END\n");
        free(first.items);
        return;
    }

    /* 1. Every sequence: MATE lines in a fixed order. */
    for (size_t i = 0; i < first.count; ++i) {
        Board next = *b;
        make_move(&next, first.items[i]);
        s.first_index = (int)i;
        s.line[0] = first.items[i];
        loop_walk(&s, &next, 1);
    }

    /* 2. Totals per ply. */
    for (int p = 1; p <= depth; ++p) {
        text_add(out, "PLY %d POSITIONS %ld MATED White %ld Black %ld STALEMATES %ld", p, s.positions[p],
                 s.mated[p][WHITE], s.mated[p][BLACK], s.stalemates[p]);
        if (zero_k > 0) {
            text_add(out, " ZERO %ld", s.zero[p]);
        }
        text_add(out, "\n");
    }

    /* 3. Each first move: forced, possible, or not found (mate of the other side). */
    for (size_t i = 0; i < first.count; ++i) {
        Board next = *b;
        make_move(&next, first.items[i]);
        MoveList replies = generate_moves(&next);
        move_text(first.items[i], text);
        text_add(out, "FIRST %s ", text);
        if (first_move_forces(&next, depth)) {
            text_add(out, "FORCED");
            any_forced = true;
        } else if (s.replies_with_mate[i] > 0) {
            text_add(out, "POSSIBLE %d OF %d", s.replies_with_mate[i], (int)replies.count);
            any_possible = true;
        } else {
            text_add(out, "NOT-FOUND");
        }
        text_add(out, " MATED White %ld Black %ld\n", s.first_mated[i][WHITE], s.first_mated[i][BLACK]);
        free(replies.items);
    }
    text_add(out, "RESULT %s %s\n", color_name(b->turn),
             any_forced ? "FORCED" : any_possible ? "POSSIBLE" : "NOT-FOUND");
    if (!any_forced) {
        text_add(out, "LIMIT %d\n", depth);
    }
    text_add(out, "LOOP END\n");
    free(first.items);
}

/* Reads "N" or "N K" after "loop "; false if not well formed. */
static bool parse_loop_args(const char *text, int *depth, int *zero_k) {
    int n = 0, k = 0, used = 0;
    if (sscanf(text, "%d%n", &n, &used) != 1) {
        return false;
    }
    text += used;
    while (*text == ' ') {
        ++text;
    }
    if (*text != '\0') {
        if (sscanf(text, "%d%n", &k, &used) != 1 || text[used] != '\0' || k < 1 || k > 16) {
            return false;
        }
    }
    if (n < 1 || n > LOOP_MAX_DEPTH) {
        return false;
    }
    *depth = n;
    *zero_k = k;
    return true;
}

/* "trace e2e4 e7e5 ...": the R report of each move in turn, the memory
 * carried as if each move were finalized. False (and nothing printed) if a
 * move is badly written or illegal; *bad_ply tells which. */
static bool trace_report(const Board *b, const Tracker *t, const char *moves_text, TextBuffer *out, int *bad_ply) {
    static Tracker tracker;
    Board board = *b;
    Move moves[LOOP_MAX_FIRST];
    int count = 0;
    char word[16];
    const char *p = moves_text;

    *bad_ply = 0;
    tracker = *t;
    while (*p != '\0') {
        int length = 0;
        while (*p == ' ') {
            ++p;
        }
        if (*p == '\0') {
            break;
        }
        while (p[length] != '\0' && p[length] != ' ') {
            ++length;
        }
        if (count == LOOP_MAX_FIRST || length >= (int)sizeof(word)) {
            *bad_ply = count + 1;
            return false;
        }
        memcpy(word, p, (size_t)length);
        word[length] = '\0';
        p += length;
        if (!parse_move_text(word, &moves[count]) || !legal_move(&board, moves[count])) {
            *bad_ply = count + 1;
            return false;
        }
        make_move(&board, moves[count]);
        ++count;
    }
    if (count == 0) {
        *bad_ply = 1;
        return false;
    }

    board = *b;
    text_add(out, "TRACE BEGIN %d\n", count);
    for (int i = 0; i < count; ++i) {
        char text[6];
        move_text(moves[i], text);
        text_add(out, "TRACE PLY %d %s\n", i + 1, text);
        calc_report(&board, &tracker, moves[i], out);
        tracker_commit(&tracker, &board, moves[i]);
        make_move(&board, moves[i]);
    }
    text_add(out, "TRACE END\n");
    return true;
}

/* STAGE 3D-1: goals and routes, PASS mode only (spec Section 6, v1.7).
 * A goal is one square or an ordered set of squares ("goal f7 2",
 * "goal f7 g7 h7 3", "goal king 2" = the squares next to the enemy king),
 * with a limit of own moves (1 to 4). The condition C tested is "a piece of
 * the side to move (S) stands on the goal square" (REACH), and separately
 * "... and another S piece protects it" (PROTECTED). Every motion gets a
 * result, printed in the owner's words: IF (the condition holds within the
 * limit, "true") or ELSE (it does not, "false"); PIECE (this piece alone)
 * and SIDE (any S pieces).
 * PASS is hypothetical: only S moves, the other side passes; every own move
 * is a legal move (no jumping over pieces, the own king never left in
 * check, captures remove the captured piece), en passant is cleared after
 * each own move. It is a distance, never a real sequence of turns.
 * Nothing is chosen, scored or played. */

#define GOAL_MAX_SQUARES 16
#define GOAL_MAX_LIMIT 4

typedef struct {
    bool active;
    bool king;                    /* the squares next to the enemy king */
    int count;
    int square[GOAL_MAX_SQUARES]; /* square index, row * 8 + col */
    int limit;                    /* own moves, PASS */
} Goal;

typedef enum { COND_REACH, COND_PROTECTED } GoalCondition;

/* Appends `length` characters of text as they are (text_add takes at most
 * 511 characters per call). */
static void text_raw(TextBuffer *t, const char *text, size_t length) {
    char piece[256];
    while (length > 0) {
        size_t n = length < sizeof(piece) - 1 ? length : sizeof(piece) - 1;
        memcpy(piece, text, n);
        piece[n] = '\0';
        text_add(t, "%s", piece);
        text += n;
        length -= n;
    }
}

/* The goal's squares now, in order. For "king": next to the enemy king, in
 * board order (rank 8 to 1, file a to h). */
static int goal_squares(const Goal *g, const Board *b, Color side, int squares[GOAL_MAX_SQUARES]) {
    int count = 0;
    if (!g->king) {
        for (int i = 0; i < g->count; ++i) {
            squares[count++] = g->square[i];
        }
        return count;
    }
    int kr, kc;
    if (!find_king(b, side == WHITE ? BLACK : WHITE, &kr, &kc)) {
        return 0;
    }
    for (int r = kr - 1; r <= kr + 1; ++r) {
        for (int c = kc - 1; c <= kc + 1; ++c) {
            if (in_bounds(r, c) && (r != kr || c != kc)) {
                squares[count++] = r * 8 + c;
            }
        }
    }
    return count;
}

static void goal_text(const Goal *g, TextBuffer *out) {
    char sq[3];
    if (g->king) {
        text_add(out, "king");
    } else {
        for (int i = 0; i < g->count; ++i) {
            square_text(g->square[i], sq);
            text_add(out, "%s%s", i ? " " : "", sq);
        }
    }
}

/* Counts the pieces of `side` that attack or protect square sq, leaving out
 * the piece standing on sq itself. */
static int pieces_on_square(const Board *b, int sq, Color side) {
    int count = 0;
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            if (r * 8 + c != sq && b->board[r][c].color == side && piece_attacks(b, r, c, sq / 8, sq % 8)) {
                ++count;
            }
        }
    }
    return count;
}

static bool goal_holds(const Board *b, Color side, int sq, GoalCondition cond) {
    if (b->board[sq / 8][sq % 8].color != side) {
        return false;
    }
    return cond == COND_REACH || pieces_on_square(b, sq, side) > 0;
}

/* PASS: the legal moves of `side` in b, as if it were its turn. */
static MoveList pass_moves(const Board *b, Color side) {
    Board view = *b;
    view.turn = side;
    return sorted_moves(&view);
}

/* PASS: play m for `side`; the other side passes. */
static void pass_play(Board *b, Move m, Color side) {
    b->turn = side;
    make_move(b, m);
    b->turn = side;
    b->ep_row = -1;
    b->ep_col = -1;
}

/* Fewest own moves (0..max) for the piece on (r, c) alone to stand on sq;
 * -1 if more than max. */
static int piece_distance(const Board *b, int r, int c, Color side, int sq, int max) {
    if (r * 8 + c == sq) {
        return 0;
    }
    if (max <= 0) {
        return -1;
    }
    int best = -1;
    MoveList moves = pass_moves(b, side);
    for (size_t i = 0; i < moves.count; ++i) {
        Move m = moves.items[i];
        if (m.sr != r || m.sc != c) {
            continue;
        }
        if (m.er * 8 + m.ec == sq) {
            best = 1;
            break;
        }
    }
    for (size_t i = 0; best != 1 && i < moves.count; ++i) {
        Move m = moves.items[i];
        if (m.sr != r || m.sc != c) {
            continue;
        }
        Board next = *b;
        pass_play(&next, m, side);
        int d = piece_distance(&next, m.er, m.ec, side, sq, best == -1 ? max - 1 : best - 2);
        if (d >= 0 && (best == -1 || d + 1 < best)) {
            best = d + 1;
        }
    }
    free(moves.items);
    return best;
}

/* Fewest own moves (0..max) of any S pieces until the condition holds on
 * sq; -1 if more than max. */
static bool side_within(const Board *b, Color side, int sq, GoalCondition cond, int depth) {
    if (goal_holds(b, side, sq, cond)) {
        return true;
    }
    if (depth <= 0) {
        return false;
    }
    bool found = false;
    MoveList moves = pass_moves(b, side);
    for (size_t i = 0; i < moves.count && !found; ++i) {
        Board next = *b;
        pass_play(&next, moves.items[i], side);
        found = side_within(&next, side, sq, cond, depth - 1);
    }
    free(moves.items);
    return found;
}

static int side_distance(const Board *b, Color side, int sq, GoalCondition cond, int max) {
    for (int d = 0; d <= max; ++d) {
        if (side_within(b, side, sq, cond, d)) {
            return d;
        }
    }
    return -1;
}

/* ---- routes: the automatic report ---- */

typedef struct {
    int own;
    int protectors;
    int attackers;
    size_t line_start; /* where its ROUTE line starts in the text */
    size_t line_end;
    Move path[GOAL_MAX_LIMIT]; /* Stage 3D-2: the route itself, for the second goal */
    int length;
    bool dominated;
} RouteVector;

#define GOAL_MAX_ROUTES 512

static bool goal_show_risk = false; /* Stage 3E: "risk on" adds AT-RISK to ROUTE lines */
static int piece_value(char type);

typedef struct {
    TextBuffer *out;
    Color side;
    int sq;
    Move path[GOAL_MAX_LIMIT];
    RouteVector routes[GOAL_MAX_ROUTES];
    int route_count;
    int extra;           /* routes found beyond GOAL_MAX_ROUTES (counted, not listed) */
} RouteState;

static void route_line(RouteState *s, const Board *end, const char *kind, int length) {
    char text[6];
    if (s->route_count == GOAL_MAX_ROUTES) {
        ++s->extra;
        return;
    }
    RouteVector *v = &s->routes[s->route_count++];
    Color enemy = s->side == WHITE ? BLACK : WHITE;
    v->own = length;
    v->length = length;
    memcpy(v->path, s->path, sizeof(v->path));
    v->protectors = pieces_on_square(end, s->sq, s->side);
    v->attackers = pieces_on_square(end, s->sq, enemy);
    v->line_start = s->out->length;
    text_add(s->out, "ROUTE %s", kind);
    for (int i = 0; i < length; ++i) {
        move_text(s->path[i], text);
        text_add(s->out, " %s", text);
    }
    text_add(s->out, " OWN %d PROTECTORS %d ATTACKERS %d", v->own, v->protectors, v->attackers);
    if (goal_show_risk) {
        int value = v->attackers > 0 ? piece_value(end->board[s->sq / 8][s->sq % 8].type) : 0;
        text_add(s->out, " AT-RISK %d", value);
    }
    v->line_end = s->out->length;
    text_add(s->out, "\n");
}

/* Every route of exactly `length` moves of the piece on (r, c) ending on sq. */
static void piece_routes(RouteState *s, const Board *b, int r, int c, int ply, int length, const char *kind) {
    if (ply == length) {
        if (r * 8 + c == s->sq) {
            route_line(s, b, kind, length);
        }
        return;
    }
    if (r * 8 + c == s->sq) {
        return; /* arrived earlier: a shorter route */
    }
    MoveList moves = pass_moves(b, s->side);
    for (size_t i = 0; i < moves.count; ++i) {
        Move m = moves.items[i];
        if (m.sr != r || m.sc != c) {
            continue;
        }
        Board next = *b;
        pass_play(&next, m, s->side);
        s->path[ply] = m;
        piece_routes(s, &next, m.er, m.ec, ply + 1, length, kind);
    }
    free(moves.items);
}

/* Every sequence of exactly `length` own moves after which the condition
 * holds on sq (and not before). */
static void side_routes(RouteState *s, const Board *b, int ply, int length, GoalCondition cond, const char *kind) {
    bool holds = goal_holds(b, s->side, s->sq, cond);
    if (ply == length) {
        if (holds) {
            route_line(s, b, kind, length);
        }
        return;
    }
    if (holds) {
        return;
    }
    MoveList moves = pass_moves(b, s->side);
    for (size_t i = 0; i < moves.count; ++i) {
        Board next = *b;
        pass_play(&next, moves.items[i], s->side);
        s->path[ply] = moves.items[i];
        side_routes(s, &next, ply + 1, length, cond, kind);
    }
    free(moves.items);
}

/* Marks a route DOMINATED when another route of the same square is no worse
 * in every component (fewer or equal own moves, more or equal protectors,
 * fewer or equal attackers) and better in at least one. */
static void mark_dominated(RouteState *s) {
    TextBuffer marked = {NULL, 0, 0};
    size_t copied = s->routes[0].line_start;
    text_raw(&marked, s->out->text, copied);
    for (int i = 0; i < s->route_count; ++i) {
        RouteVector *a = &s->routes[i];
        bool dominated = false;
        for (int j = 0; j < s->route_count && !dominated; ++j) {
            const RouteVector *b = &s->routes[j];
            bool no_worse = b->own <= a->own && b->protectors >= a->protectors && b->attackers <= a->attackers;
            bool better = b->own < a->own || b->protectors > a->protectors || b->attackers < a->attackers;
            dominated = no_worse && better;
        }
        size_t next = i + 1 < s->route_count ? s->routes[i + 1].line_start : s->out->length;
        text_raw(&marked, s->out->text + a->line_start, a->line_end - a->line_start);
        a->dominated = dominated;
        if (dominated) {
            text_add(&marked, " DOMINATED");
        }
        text_raw(&marked, s->out->text + a->line_end, next - a->line_end);
    }
    free(s->out->text);
    *s->out = marked;
}

/* The owner's ordered chain (review 17): among the distinct values present,
 * the smallest is IF, each value between is ELSE IF, the largest is ELSE;
 * with one distinct value only IF. Equal values get the same label. */
static const char *order_label(const int *values, int count, int value) {
    int smaller = 0, larger = 0;
    bool seen_smaller[64] = {false}, seen_larger[64] = {false};
    for (int i = 0; i < count; ++i) {
        if (values[i] < 0 || values[i] >= 64) {
            continue;
        }
        if (values[i] < value && !seen_smaller[values[i]]) {
            seen_smaller[values[i]] = true;
            ++smaller;
        } else if (values[i] > value && !seen_larger[values[i]]) {
            seen_larger[values[i]] = true;
            ++larger;
        }
    }
    if (smaller == 0) {
        return "IF";
    }
    return larger == 0 ? "ELSE" : "ELSE IF";
}

static void second_goal_report(const Board *b, const Goal *second, const int *first_square,
                               const RouteVector *routes, int route_count, TextBuffer *out);

static void routes_report_with(const Board *b, const Goal *g, const Goal *second, TextBuffer *out) {
    static RouteState s;
    static RouteVector saved[GOAL_MAX_ROUTES];
    int saved_count = 0, first_reach = -1;
    int squares[GOAL_MAX_SQUARES];
    int count = goal_squares(g, b, b->turn, squares);
    Color side = b->turn, enemy = side == WHITE ? BLACK : WHITE;
    int reach[GOAL_MAX_SQUARES], prot[GOAL_MAX_SQUARES];
    char sq[3];

    text_add(out, "ROUTES BEGIN %s GOAL ", color_name(side));
    goal_text(g, out);
    text_add(out, " LIMIT %d PASS\n", g->limit);
    text_add(out, "NOTE PASS is hypothetical: only %s moves, %s passes; not a real sequence of turns\n",
             color_name(side), color_name(enemy));
    for (int k = 0; k < count; ++k) {
        TextBuffer part = {NULL, 0, 0};
        memset(&s, 0, sizeof(s));
        s.out = &part;
        s.side = side;
        s.sq = squares[k];
        square_text(squares[k], sq);
        text_add(&part, "GOAL %s NOW %s %d %s %d\n", sq, color_name(WHITE), pieces_on_square(b, squares[k], WHITE),
                 color_name(BLACK), pieces_on_square(b, squares[k], BLACK));
        /* PIECE: each S piece alone, in board order. */
        int distance[64];
        for (int i = 0; i < 64; ++i) {
            distance[i] = -2;
            if (b->board[i / 8][i % 8].color != side) {
                continue;
            }
            distance[i] = piece_distance(b, i / 8, i % 8, side, squares[k], g->limit);
        }
        for (int i = 0; i < 64; ++i) {
            char at[3];
            if (distance[i] == -2) {
                continue;
            }
            square_text(i, at);
            if (distance[i] >= 0) {
                text_add(&part, "PIECE %c %s CONDITION IF MOVES %d ORDER %s\n", b->board[i / 8][i % 8].type, at,
                         distance[i], order_label(distance, 64, distance[i]));
            } else {
                text_add(&part, "PIECE %c %s CONDITION ELSE ORDER NONE\n", b->board[i / 8][i % 8].type, at);
            }
        }
        for (int i = 0; i < 64; ++i) {
            if (distance[i] >= 1) {
                char kind[16], at[3];
                square_text(i, at);
                snprintf(kind, sizeof(kind), "PIECE %c %s", b->board[i / 8][i % 8].type, at);
                piece_routes(&s, b, i / 8, i % 8, 0, distance[i], kind);
            }
        }
        /* SIDE: any S pieces. */
        reach[k] = side_distance(b, side, squares[k], COND_REACH, g->limit);
        prot[k] = side_distance(b, side, squares[k], COND_PROTECTED, g->limit);
        if (reach[k] >= 0) {
            text_add(&part, "SIDE REACH CONDITION IF MOVES %d\n", reach[k]);
        } else {
            text_add(&part, "SIDE REACH CONDITION ELSE\n");
        }
        if (prot[k] >= 0) {
            text_add(&part, "SIDE PROTECTED CONDITION IF MOVES %d\n", prot[k]);
            if (prot[k] >= 1) {
                side_routes(&s, b, 0, prot[k], COND_PROTECTED, "SIDE PROTECTED");
            }
        } else {
            text_add(&part, "SIDE PROTECTED CONDITION ELSE\n");
        }
        if (s.route_count > 0) {
            mark_dominated(&s);
        }
        if (reach[k] >= 0 && first_reach < 0) {
            first_reach = squares[k];
            saved_count = s.route_count;
            memcpy(saved, s.routes, sizeof(RouteVector) * (size_t)s.route_count);
        }
        if (s.extra > 0) {
            text_add(&part, "MORE ROUTES %d not listed\n", s.extra);
        }
        text_raw(out, part.text, part.length);
        free(part.text);
    }
    if (count == 0) {
        text_add(out, "NO SQUARES\n");
    }
    for (int k = 0; k < count; ++k) {
        square_text(squares[k], sq);
        text_add(out, "SQUARE %s REACH CONDITION %s PROTECTED CONDITION %s\n", sq, reach[k] >= 0 ? "IF" : "ELSE",
                 prot[k] >= 0 ? "IF" : "ELSE");
    }
    /* The owner's if / else if / else order: the first square that is IF. */
    const char *names[2] = {"REACH", "PROTECTED"};
    for (int c = 0; c < 2; ++c) {
        int first = -1;
        for (int k = 0; k < count && first < 0; ++k) {
            if ((c == 0 ? reach[k] : prot[k]) >= 0) {
                first = k;
            }
        }
        if (first >= 0) {
            square_text(squares[first], sq);
            text_add(out, "FIRST-IF %s %s\n", names[c], sq);
        } else {
            text_add(out, "FIRST-IF %s none (ELSE)\n", names[c]);
        }
    }
    if (second != NULL && second->active) {
        second_goal_report(b, second, first_reach >= 0 ? &first_reach : NULL, saved, saved_count, out);
    }
    text_add(out, "ROUTES END\n");
}

/* The first goal only (version 11's report; used by the tests). */
void routes_report(const Board *b, const Goal *g, TextBuffer *out);
void routes_report(const Board *b, const Goal *g, TextBuffer *out) {
    routes_report_with(b, g, NULL, out);
}

/* ---- marks: the IF/ELSE results added to the lists while playing ---- */

/* One mark: after the motions already made (`used` own moves, the last one
 * ending on (r, c)), the PIECE and SIDE results for the goal's squares in
 * order (the first square that is IF is shown). */
static void mark_text(const Board *after, const Goal *g, const int *squares, int count, int r, int c, int used,
                      TextBuffer *out) {
    Color side = after->turn;
    char sq[3];
    int left = g->limit - used;
    bool done = false;
    for (int k = 0; k < count && !done && left >= 0; ++k) {
        int d = piece_distance(after, r, c, side, squares[k], left);
        if (d >= 0) {
            square_text(squares[k], sq);
            text_add(out, " PIECE CONDITION IF %s MOVES %d", sq, used + d);
            done = true;
        }
    }
    if (!done) {
        text_add(out, " PIECE CONDITION ELSE");
    }
    done = false;
    for (int k = 0; k < count && !done && left >= 0; ++k) {
        int d = side_distance(after, side, squares[k], COND_REACH, left);
        if (d >= 0) {
            square_text(squares[k], sq);
            text_add(out, " SIDE CONDITION IF %s MOVES %d", sq, used + d);
            done = true;
        }
    }
    if (!done) {
        text_add(out, " SIDE CONDITION ELSE");
    }
    text_add(out, "\n");
}

/* After selecting the piece on (r, c): a mark for each of its destinations. */
static void goal_marks_selection(const Board *b, const Goal *g, int r, int c, TextBuffer *out) {
    int squares[GOAL_MAX_SQUARES];
    int count = goal_squares(g, b, b->turn, squares);
    char at[3], text[6];
    square_text(r * 8 + c, at);
    text_add(out, "GOAL MARKS %c %s GOAL ", b->board[r][c].type, at);
    goal_text(g, out);
    text_add(out, " LIMIT %d PASS\n", g->limit);
    MoveList moves = sorted_moves(b);
    for (size_t i = 0; i < moves.count; ++i) {
        Move m = moves.items[i];
        if (m.sr != r || m.sc != c) {
            continue;
        }
        Board after = *b;
        Color side = b->turn;
        pass_play(&after, m, side);
        move_text(m, text);
        text_add(out, "MARK %s", text);
        mark_text(&after, g, squares, count, m.er, m.ec, 1, out);
    }
    free(moves.items);
    text_add(out, "GOAL MARKS END\n");
}

/* After previewing m: a mark for each move of each S piece in the AFTER
 * position (S moving again, PASS). */
static void goal_marks_after(const Board *b, const Goal *g, Move m, TextBuffer *out) {
    int squares[GOAL_MAX_SQUARES];
    Color side = b->turn;
    Board after = *b;
    char text[6], at[3];
    pass_play(&after, m, side);
    int count = goal_squares(g, &after, side, squares);
    move_text(m, text);
    text_add(out, "GOAL MARKS AFTER %s GOAL ", text);
    goal_text(g, out);
    text_add(out, " LIMIT %d PASS\n", g->limit);
    for (int k = 0; k < count; ++k) {
        if (goal_holds(&after, side, squares[k], COND_REACH)) {
            square_text(squares[k], at);
            text_add(out, "REACHED %s 1\n", at);
        }
    }
    MoveList moves = pass_moves(&after, side);
    for (size_t i = 0; i < moves.count; ++i) {
        Move n = moves.items[i];
        Board next = after;
        pass_play(&next, n, side);
        square_text(n.sr * 8 + n.sc, at);
        move_text(n, text);
        text_add(out, "MARK %c %s %s", after.board[n.sr][n.sc].type, at, text);
        mark_text(&next, g, squares, count, n.er, n.ec, 2, out);
    }
    free(moves.items);
    text_add(out, "GOAL MARKS END\n");
}

/* Reads "f7 2", "f7 g7 h7 3" or "king 2". False if not well formed. */
static bool parse_goal(const char *text, Goal *g) {
    Goal read;
    char word[16];
    int used = 0;
    memset(&read, 0, sizeof(read));
    read.limit = -1;
    while (sscanf(text, "%15s%n", word, &used) == 1) {
        text += used;
        int r, c, n;
        char extra;
        if (read.limit != -1) {
            return false; /* nothing after the limit */
        }
        if (!strcmp(word, "king") && read.count == 0 && !read.king) {
            read.king = true;
        } else if (strlen(word) == 2 && parse_square(word, &r, &c) && !read.king) {
            if (read.count == GOAL_MAX_SQUARES) {
                return false;
            }
            for (int i = 0; i < read.count; ++i) {
                if (read.square[i] == r * 8 + c) {
                    return false;
                }
            }
            read.square[read.count++] = r * 8 + c;
        } else if (sscanf(word, "%d%c", &n, &extra) == 1 && n >= 1 && n <= GOAL_MAX_LIMIT &&
                   (read.count > 0 || read.king)) {
            read.limit = n;
        } else {
            return false;
        }
    }
    if (read.limit == -1) {
        return false;
    }
    read.active = true;
    *g = read;
    return true;
}

/* ---- distance order: the owner's fourth idea (review 17, reading B) ----
 * "distance on" adds, after selecting a piece and after a preview, every
 * legal move with the number of squares it travels, max(|rows|, |files|),
 * ordered from the fewest squares (IF) through each larger distance
 * (ELSE IF) to the most (ELSE). A knight move is marked JUMP (one move,
 * displacement 2); castling is marked CASTLE with the rook's move (the
 * king's 2 squares are its distance). The moves of a piece whose side is
 * not to move are listed as if it were its turn. Nothing else changes. */

static int move_distance(Move m) {
    int dr = abs(m.er - m.sr), dc = abs(m.ec - m.sc);
    return dr > dc ? dr : dc;
}

/* The distance lines of the piece on (r, c) in b. `label` starts each line. */
static void distance_lines(const Board *b, int r, int c, const char *label, TextBuffer *out) {
    Board view = *b;
    int distances[256];
    Move moves[256];
    int count = 0;
    char text[6];

    view.turn = b->board[r][c].color;
    MoveList all = sorted_moves(&view);
    for (size_t i = 0; i < all.count && count < 256; ++i) {
        if (all.items[i].sr == r && all.items[i].sc == c) {
            moves[count] = all.items[i];
            distances[count] = move_distance(all.items[i]);
            ++count;
        }
    }
    free(all.items);
    if (count == 0) {
        text_add(out, "NO-MOVES%s\n", label + 4); /* label starts with "MOVE" */
        return;
    }
    for (int d = 1; d <= 7; ++d) {
        for (int i = 0; i < count; ++i) {
            if (distances[i] != d) {
                continue;
            }
            move_text(moves[i], text);
            text_add(out, "%s %s DISTANCE %d ORDER %s", label, text, d, order_label(distances, count, d));
            char type = b->board[r][c].type;
            if (type == 'N') {
                text_add(out, " JUMP");
            } else if (type == 'K' && d == 2) {
                bool king_side = moves[i].ec > moves[i].sc;
                char rook_from[3], rook_to[3];
                square_text(r * 8 + (king_side ? 7 : 0), rook_from);
                square_text(r * 8 + (king_side ? 5 : 3), rook_to);
                text_add(out, " CASTLE %s%s", rook_from, rook_to);
            }
            text_add(out, "\n");
        }
    }
}

/* After selecting the piece on (r, c). */
static void distance_selection(const Board *b, int r, int c, TextBuffer *out) {
    char at[3];
    square_text(r * 8 + c, at);
    text_add(out, "DISTANCE ORDER %c %s\n", b->board[r][c].type, at);
    distance_lines(b, r, c, "MOVE", out);
    text_add(out, "DISTANCE ORDER END\n");
}

/* After previewing m: every piece of both sides in the AFTER position,
 * White first, each in board order. */
static void distance_after(const Board *b, Move m, TextBuffer *out) {
    Board after = *b;
    char text[6];
    make_move(&after, m);
    move_text(m, text);
    text_add(out, "DISTANCE ORDER AFTER %s\n", text);
    for (int side = WHITE; side <= BLACK; ++side) {
        for (int i = 0; i < 64; ++i) {
            if (after.board[i / 8][i % 8].color != (Color)side) {
                continue;
            }
            char at[3], label[24];
            square_text(i, at);
            snprintf(label, sizeof(label), "MOVE %s %c %s", color_name((Color)side), after.board[i / 8][i % 8].type, at);
            distance_lines(&after, i / 8, i % 8, label, out);
        }
    }
    text_add(out, "DISTANCE ORDER END\n");
}

/* STAGE 3D-2 (spec v1.8): REAL goal searches, support follow-ups and the
 * second goal.
 * "routes real N": the goal squares tested with the other side answering
 * with every legal move, N plies (1 to 6). For each first move m of S:
 * CONDITION IF FORCED (m reaches the goal, or N >= 3 and after every reply
 * the goal is forced within N - 2), CONDITION IF POSSIBLE k OF r (after k
 * of the r replies the goal can still be reached within the depth), or
 * CONDITION ELSE. As in 3C, S may choose a different continuation after
 * each reply, and counting is never proof of forcing. A sequence ends at
 * checkmate or stalemate. The goal counts as reached right after an S
 * move.
 * "support": after a preview, every one-move follow-up of another S piece
 * that makes a new protection link to the moved piece (PASS).
 * "goal2 <squares> <L>": a second goal; "routes" then measures it from the
 * start position and from the end of every route to the first goal square
 * that is IF and not DOMINATED (PASS). */

static bool goal_forced_real(const Board *b, Color side, int sq, GoalCondition cond, int n) {
    bool forced = false;
    if (n < 1) {
        return false;
    }
    MoveList moves = generate_moves(b);
    for (size_t i = 0; i < moves.count && !forced; ++i) {
        Board next = *b;
        make_move(&next, moves.items[i]);
        if (goal_holds(&next, side, sq, cond)) {
            forced = true;
            break;
        }
        if (n < 3) {
            continue;
        }
        MoveList replies = generate_moves(&next);
        bool all = replies.count > 0;
        for (size_t j = 0; j < replies.count && all; ++j) {
            Board after = next;
            make_move(&after, replies.items[j]);
            all = goal_forced_real(&after, side, sq, cond, n - 2);
        }
        free(replies.items);
        forced = all;
    }
    free(moves.items);
    return forced;
}

static bool goal_possible_real(const Board *b, Color side, int sq, GoalCondition cond, int n) {
    bool possible = false;
    if (n < 1) {
        return false;
    }
    MoveList moves = generate_moves(b);
    for (size_t i = 0; i < moves.count && !possible; ++i) {
        Board next = *b;
        make_move(&next, moves.items[i]);
        if (goal_holds(&next, side, sq, cond)) {
            possible = true;
            break;
        }
        if (n < 3) {
            continue;
        }
        MoveList replies = generate_moves(&next);
        for (size_t j = 0; j < replies.count && !possible; ++j) {
            Board after = next;
            make_move(&after, replies.items[j]);
            possible = goal_possible_real(&after, side, sq, cond, n - 2);
        }
        free(replies.items);
    }
    free(moves.items);
    return possible;
}

typedef enum { REAL_ELSE, REAL_POSSIBLE, REAL_FORCED } RealResult;

static const char *real_text(RealResult r) {
    return r == REAL_FORCED ? "CONDITION IF FORCED" : r == REAL_POSSIBLE ? "CONDITION IF POSSIBLE" : "CONDITION ELSE";
}

/* The FIRST lines for one square and condition; returns the overall result. */
static RealResult real_first_lines(const Board *b, int sq, GoalCondition cond, int depth, TextBuffer *out) {
    Color side = b->turn;
    RealResult best = REAL_ELSE;
    char text[6];
    MoveList first = sorted_moves(b);
    for (size_t i = 0; i < first.count; ++i) {
        Board next = *b;
        make_move(&next, first.items[i]);
        move_text(first.items[i], text);
        text_add(out, "FIRST %s ", text);
        if (goal_holds(&next, side, sq, cond)) {
            text_add(out, "CONDITION IF FORCED MOVES 1\n");
            best = REAL_FORCED;
            continue;
        }
        MoveList replies = generate_moves(&next);
        bool forced = depth >= 3 && replies.count > 0;
        int k = 0;
        for (size_t j = 0; j < replies.count && depth >= 3; ++j) {
            Board after = next;
            make_move(&after, replies.items[j]);
            bool can = goal_possible_real(&after, side, sq, cond, depth - 2);
            if (can) {
                ++k;
            }
            if (forced && (!can || !goal_forced_real(&after, side, sq, cond, depth - 2))) {
                forced = false;
            }
        }
        if (forced) {
            text_add(out, "CONDITION IF FORCED\n");
            best = REAL_FORCED;
        } else if (k > 0) {
            text_add(out, "CONDITION IF POSSIBLE %d OF %d\n", k, (int)replies.count);
            if (best == REAL_ELSE) {
                best = REAL_POSSIBLE;
            }
        } else {
            text_add(out, "CONDITION ELSE\n");
        }
        free(replies.items);
    }
    free(first.items);
    return best;
}

static void routes_real_report(const Board *b, const Goal *g, int depth, TextBuffer *out) {
    int squares[GOAL_MAX_SQUARES];
    int count = goal_squares(g, b, b->turn, squares);
    Color side = b->turn, enemy = side == WHITE ? BLACK : WHITE;
    RealResult result[GOAL_MAX_SQUARES][2];
    const char *names[2] = {"REACH", "PROTECTED"};
    bool all_forced = true;
    char sq[3];

    text_add(out, "ROUTES BEGIN %s GOAL ", color_name(side));
    goal_text(g, out);
    text_add(out, " DEPTH %d REAL\n", depth);
    text_add(out, "NOTE REAL: %s answers with every legal move; FORCED = reached whatever %s does\n",
             color_name(enemy), color_name(enemy));
    MoveList moves = generate_moves(b);
    bool none = moves.count == 0;
    free(moves.items);
    if (none) {
        text_add(out, "NO MOVES %s\n", is_in_check(b, side) ? "checkmate" : "stalemate");
        text_add(out, "ROUTES END\n");
        return;
    }
    for (int k = 0; k < count; ++k) {
        square_text(squares[k], sq);
        for (int c = 0; c < 2; ++c) {
            text_add(out, "GOAL %s %s\n", sq, names[c]);
            result[k][c] = real_first_lines(b, squares[k], c == 0 ? COND_REACH : COND_PROTECTED, depth, out);
            text_add(out, "RESULT %s %s %s\n", sq, names[c], real_text(result[k][c]));
            if (result[k][c] != REAL_FORCED) {
                all_forced = false;
            }
        }
    }
    if (count == 0) {
        text_add(out, "NO SQUARES\n");
    }
    for (int k = 0; k < count; ++k) {
        square_text(squares[k], sq);
        text_add(out, "SQUARE %s REACH %s PROTECTED %s\n", sq, real_text(result[k][0]), real_text(result[k][1]));
    }
    for (int c = 0; c < 2; ++c) {
        int first = -1;
        for (int k = 0; k < count && first < 0; ++k) {
            if (result[k][c] != REAL_ELSE) {
                first = k;
            }
        }
        if (first >= 0) {
            square_text(squares[first], sq);
            text_add(out, "FIRST-IF %s %s %s\n", names[c], sq, result[first][c] == REAL_FORCED ? "FORCED" : "POSSIBLE");
        } else {
            text_add(out, "FIRST-IF %s none (ELSE)\n", names[c]);
        }
    }
    if (!all_forced || count == 0) {
        text_add(out, "LIMIT %d\n", depth);
    }
    text_add(out, "ROUTES END\n");
}

/* Reads "real N" after "routes "; false if not well formed. */
static bool parse_routes_real(const char *text, int *depth) {
    int n = 0, used = 0;
    if (strncmp(text, "real ", 5) != 0 || sscanf(text + 5, "%d%n", &n, &used) != 1 || text[5 + used] != '\0' ||
        n < 1 || n > LOOP_MAX_DEPTH) {
        return false;
    }
    *depth = n;
    return true;
}

/* "support" after previewing m (PASS: S moves again). */
static void support_report(const Board *b, Move m, TextBuffer *out) {
    Color side = b->turn;
    Board after = *b;
    char text[6], at[3], from[3];
    int follows = 0;
    pass_play(&after, m, side);
    int sq = m.er * 8 + m.ec;
    char type = after.board[m.er][m.ec].type;
    square_text(sq, at);
    move_text(m, text);
    text_add(out, "SUPPORT BEGIN %s PASS\n", text);
    text_add(out, "NOTE PASS is hypothetical: %s moves again, %s passes\n", color_name(side),
             color_name(side == WHITE ? BLACK : WHITE));
    text_add(out, "SUPPORT NOW %c %s PROTECTORS %d\n", type, at, pieces_on_square(&after, sq, side));
    MoveList moves = pass_moves(&after, side);
    for (size_t i = 0; i < moves.count; ++i) {
        Move f = moves.items[i];
        if (f.sr == m.er && f.sc == m.ec) {
            continue; /* another piece only */
        }
        Board next = after;
        pass_play(&next, f, side);
        if (next.board[m.er][m.ec].color != side) {
            continue;
        }
        bool before = piece_attacks(&after, f.sr, f.sc, m.er, m.ec);
        bool now = piece_attacks(&next, f.er, f.ec, m.er, m.ec);
        if (now && !before) {
            move_text(f, text);
            square_text(f.sr * 8 + f.sc, from);
            text_add(out, "FOLLOW %s %c %s PROTECTS %c %s PROTECTORS %d\n", text, after.board[f.sr][f.sc].type, from, type,
                     at, pieces_on_square(&next, sq, side));
            ++follows;
        }
    }
    free(moves.items);
    text_add(out, "SUMMARY FOLLOWS %d\n", follows);
    text_add(out, "SUPPORT END\n");
}

/* The second goal, measured from the start position and from the end of
 * every route to the first IF square of the first goal that is not
 * DOMINATED (PASS). */
static void second_from(const Board *from, const Goal *second, const char *label, TextBuffer *out) {
    int squares[GOAL_MAX_SQUARES];
    int count = goal_squares(second, from, from->turn, squares);
    char sq[3];
    for (int k = 0; k < count; ++k) {
        int reach = side_distance(from, from->turn, squares[k], COND_REACH, second->limit);
        int prot = side_distance(from, from->turn, squares[k], COND_PROTECTED, second->limit);
        square_text(squares[k], sq);
        text_add(out, "FROM %s SQUARE %s REACH ", label, sq);
        if (reach >= 0) {
            text_add(out, "CONDITION IF MOVES %d", reach);
        } else {
            text_add(out, "CONDITION ELSE");
        }
        text_add(out, " PROTECTED ");
        if (prot >= 0) {
            text_add(out, "CONDITION IF MOVES %d\n", prot);
        } else {
            text_add(out, "CONDITION ELSE\n");
        }
    }
}

static void second_goal_report(const Board *b, const Goal *second, const int *first_square,
                               const RouteVector *routes, int route_count, TextBuffer *out) {
    char sq[3], text[6];
    text_add(out, "SECOND GOAL ");
    goal_text(second, out);
    text_add(out, " LIMIT %d PASS AFTER ", second->limit);
    if (first_square != NULL) {
        square_text(*first_square, sq);
        text_add(out, "%s\n", sq);
    } else {
        text_add(out, "none (ELSE)\n");
    }
    second_from(b, second, "START", out);
    for (int i = 0; first_square != NULL && i < route_count; ++i) {
        bool repeated = false; /* the same moves listed as a PIECE and a SIDE route */
        for (int j = 0; j < i && !repeated; ++j) {
            repeated = !routes[j].dominated && routes[j].length == routes[i].length;
            for (int p = 0; repeated && p < routes[i].length; ++p) {
                const Move *x = &routes[j].path[p], *y = &routes[i].path[p];
                repeated = x->sr == y->sr && x->sc == y->sc && x->er == y->er && x->ec == y->ec &&
                           x->promotion == y->promotion;
            }
        }
        if (routes[i].dominated || repeated) {
            continue;
        }
        Board end = *b;
        TextBuffer label = {NULL, 0, 0};
        for (int p = 0; p < routes[i].length; ++p) {
            pass_play(&end, routes[i].path[p], b->turn);
            move_text(routes[i].path[p], text);
            text_add(&label, "%s%s", p ? " " : "", text);
        }
        second_from(&end, second, label.text, out);
        free(label.text);
    }
    text_add(out, "SECOND END\n");
}

/* STAGE 3E (spec v1.9): captures, piece values and exchanges.
 * Values are the conventional P 1, N 3, B 3, R 5, Q 9 (a king is never
 * captured), logged only: no weighted score, no move chosen.
 * "exchange <square> N": every sequence of legal captures on one square, the
 * side to move first, sides alternating, up to N captures (1 to 8). After
 * each capture the side to move may stop, so every prefix is a sequence.
 * For each first capture, the material of the side to move S, M = value S
 * took - value S lost, is classified in the bounded exchange model with the
 * structure of 3C: the side to move chooses (stop or a capture); MATERIAL
 * FORCED NONNEGATIVE = S has a choice at each of its turns so that M >= 0
 * whatever the other side does (stopping or recapturing); MATERIAL POSSIBLE
 * NONNEGATIVE = M >= 0 at the end of some line, not forced; MATERIAL ELSE =
 * never. A line cut by the limit is BOUNDED.
 * "order N": the owner's ideas 7 and 8, one combined list for the side to
 * move: group 1 = captures that are MATERIAL FORCED or POSSIBLE NONNEGATIVE
 * (FORCED first), by the value taken (greatest = IF), ties by fewer squares;
 * group 2 = every other move by the squares travelled (fewest = IF). The
 * list is rebuilt in every position; HEADS says which group is on top.
 * "risk on": ROUTE lines get AT-RISK = the arriving piece's value when an
 * enemy piece attacks the goal square on arrival (attacked, not lost). */

#define EXCHANGE_MAX 8

static int piece_value(char type) {
    switch (type) {
        case 'P': return 1;
        case 'N': return 3;
        case 'B': return 3;
        case 'R': return 5;
        case 'Q': return 9;
        default: return 0;
    }
}

/* The piece a move captures (its type), or 0; en passant included. */
static char captured_type(const Board *b, Move m) {
    Piece target = b->board[m.er][m.ec];
    if (target.color != EMPTY && target.color != b->board[m.sr][m.sc].color) {
        return target.type;
    }
    if (b->board[m.sr][m.sc].type == 'P' && m.sc != m.ec && target.color == EMPTY) {
        return 'P'; /* en passant */
    }
    return 0;
}

/* Legal captures on square sq for the side to move, in the fixed order. */
static MoveList captures_on(const Board *b, int sq) {
    MoveList all = sorted_moves(b);
    MoveList caps = {NULL, 0, 0};
    for (size_t i = 0; i < all.count; ++i) {
        if (all.items[i].er * 8 + all.items[i].ec == sq && captured_type(b, all.items[i])) {
            append_move(&caps, all.items[i]);
        }
    }
    free(all.items);
    return caps;
}

typedef struct {
    Color mover;     /* S */
    int sq;
    int limit;
    Move line[EXCHANGE_MAX];
    char taken[EXCHANGE_MAX];
    Color by[EXCHANGE_MAX];
    int sequences;
    bool cut;        /* some line was stopped by the limit */
    TextBuffer *out;
} ExchangeState;

static void exchange_line(ExchangeState *x, int length, bool cut) {
    char text[6];
    int took[3] = {0, 0, 0};
    text_add(x->out, "SEQUENCE");
    for (int i = 0; i < length; ++i) {
        move_text(x->line[i], text);
        text_add(x->out, " %s", text);
    }
    for (int side = WHITE; side <= BLACK; ++side) {
        text_add(x->out, " %s-TOOK", side == WHITE ? "WHITE" : "BLACK");
        bool any = false;
        for (int i = 0; i < length; ++i) {
            if (x->by[i] == (Color)side) {
                text_add(x->out, " %c", x->taken[i]);
                took[side] += piece_value(x->taken[i]);
                any = true;
            }
        }
        if (!any) {
            text_add(x->out, " -");
        }
    }
    text_add(x->out, " MATERIAL White %d Black %d%s\n", took[WHITE], took[BLACK], cut ? " LIMIT" : "");
    ++x->sequences;
}

/* Lists every capture sequence below this node (length captures so far). */
static void exchange_walk(ExchangeState *x, const Board *b, int length) {
    MoveList caps = captures_on(b, x->sq);
    bool cut = length == x->limit && caps.count > 0;
    if (length > 0) {
        exchange_line(x, length, cut);
    }
    if (cut) {
        x->cut = true;
    }
    for (size_t i = 0; length < x->limit && i < caps.count; ++i) {
        Board next = *b;
        x->line[length] = caps.items[i];
        x->taken[length] = captured_type(b, caps.items[i]);
        x->by[length] = b->turn;
        make_move(&next, caps.items[i]);
        exchange_walk(x, &next, length + 1);
    }
    free(caps.items);
}

/* Material classification below a node: M = S's gain - S's loss so far.
 * *forced: S can keep M >= 0 whatever the other side does;
 * *possible: some line ends with M >= 0; *bounded: the limit cut a line. */
static void exchange_value(const Board *b, Color mover, int sq, int m, int left, bool *forced, bool *possible,
                           bool *bounded) {
    MoveList caps = left > 0 ? captures_on(b, sq) : (MoveList){NULL, 0, 0};
    if (left == 0) {
        MoveList more = captures_on(b, sq);
        if (more.count > 0) {
            *bounded = true;
        }
        free(more.items);
    }
    bool stop_ok = m >= 0; /* the side to move stops here */
    bool any_forced = false, all_forced = true, any_possible = stop_ok;
    for (size_t i = 0; i < caps.count; ++i) {
        Board next = *b;
        int v = piece_value(captured_type(b, caps.items[i]));
        int child = b->turn == mover ? m + v : m - v;
        bool f = false, p = false;
        make_move(&next, caps.items[i]);
        exchange_value(&next, mover, sq, child, left - 1, &f, &p, bounded);
        any_forced = any_forced || f;
        all_forced = all_forced && f;
        any_possible = any_possible || p;
    }
    if (b->turn == mover) {
        *forced = stop_ok || any_forced;            /* S chooses */
    } else {
        *forced = stop_ok && all_forced;            /* the other side chooses */
    }
    *possible = any_possible;
    free(caps.items);
}

typedef enum { MATERIAL_ELSE, MATERIAL_POSSIBLE, MATERIAL_FORCED } MaterialResult;

static const char *material_text(MaterialResult r) {
    return r == MATERIAL_FORCED ? "MATERIAL FORCED NONNEGATIVE" : r == MATERIAL_POSSIBLE ? "MATERIAL POSSIBLE NONNEGATIVE"
                                                                                          : "MATERIAL ELSE";
}

/* The class of the first capture c (by S) on its square, limit captures. */
static MaterialResult first_capture_class(const Board *b, Move c, int limit, bool *bounded) {
    Board next = *b;
    bool forced = false, possible = false;
    int v = piece_value(captured_type(b, c));
    make_move(&next, c);
    *bounded = false;
    exchange_value(&next, b->turn, c.er * 8 + c.ec, v, limit - 1, &forced, &possible, bounded);
    return forced ? MATERIAL_FORCED : possible ? MATERIAL_POSSIBLE : MATERIAL_ELSE;
}

static void exchange_report(const Board *b, int sq, int limit, TextBuffer *out) {
    static ExchangeState x;
    char at[3], text[6];
    memset(&x, 0, sizeof(x));
    x.mover = b->turn;
    x.sq = sq;
    x.limit = limit;
    x.out = out;
    square_text(sq, at);
    text_add(out, "EXCHANGE BEGIN %s %s LIMIT %d\n", color_name(b->turn), at, limit);
    text_add(out, "NOTE values P 1, N 3, B 3, R 5, Q 9 are logged only, not a score; a nonnegative material result is not proof that a move is good\n");
    MoveList first = captures_on(b, sq);
    if (first.count == 0) {
        text_add(out, "NO CAPTURES\n");
        text_add(out, "EXCHANGE END\n");
        free(first.items);
        return;
    }
    exchange_walk(&x, b, 0);
    int values[256];
    for (size_t i = 0; i < first.count && i < 256; ++i) {
        values[i] = piece_value(captured_type(b, first.items[i]));
    }
    for (size_t i = 0; i < first.count; ++i) {
        bool bounded = false;
        MaterialResult r = first_capture_class(b, first.items[i], limit, &bounded);
        move_text(first.items[i], text);
        text_add(out, "FIRST %s TAKES %c VALUE %d %s ORDER %s%s\n", text, captured_type(b, first.items[i]), values[i],
                 material_text(r), order_label(values, (int)first.count, values[i]), bounded ? " BOUNDED" : "");
    }
    text_add(out, "SUMMARY SEQUENCES %d\n", x.sequences);
    if (x.cut) {
        text_add(out, "LIMIT %d\n", limit);
    }
    text_add(out, "EXCHANGE END\n");
    free(first.items);
}

/* Reads "<square> N" after "exchange ". */
static bool parse_exchange(const char *text, int *sq, int *limit) {
    char square[3];
    int n = 0, used = 0, r, c;
    if (sscanf(text, "%2s %d%n", square, &n, &used) != 2 || text[used] != '\0' || strlen(square) != 2 ||
        text[2] != ' ' || !parse_square(square, &r, &c) || n < 1 || n > EXCHANGE_MAX) {
        return false;
    }
    *sq = r * 8 + c;
    *limit = n;
    return true;
}

typedef struct {
    Move move;
    char taken;
    int value;
    int squares;
    MaterialResult material;
    bool bounded;
} OrderEntry;

/* The combined order of ideas 7 and 8, for the side to move. */
static void order_report(const Board *b, int limit, TextBuffer *out) {
    static OrderEntry e[256];
    int count = 0;
    char text[6];
    MoveList all = sorted_moves(b);
    for (size_t i = 0; i < all.count && count < 256; ++i) {
        OrderEntry *o = &e[count++];
        o->move = all.items[i];
        o->taken = captured_type(b, all.items[i]);
        o->value = piece_value(o->taken);
        o->squares = move_distance(all.items[i]);
        o->bounded = false;
        o->material = o->taken ? first_capture_class(b, all.items[i], limit, &o->bounded) : MATERIAL_ELSE;
    }
    free(all.items);
    text_add(out, "ORDER BEGIN %s EXCHANGE LIMIT %d\n", color_name(b->turn), limit);
    text_add(out, "NOTE an order of listed facts (ideas 7 and 8), rebuilt in every position; not a choice of move\n");
    if (count == 0) {
        text_add(out, "NO MOVES %s\n", is_in_check(b, b->turn) ? "checkmate" : "stalemate");
        text_add(out, "ORDER END\n");
        return;
    }
    bool group1 = false;
    for (int i = 0; i < count; ++i) {
        if (e[i].taken && e[i].material != MATERIAL_ELSE) {
            group1 = true;
        }
    }
    text_add(out, "HEADS %s\n", group1 ? "CAPTURE" : "DISTANCE");
    /* Group 1: FORCED, then POSSIBLE; each by value (greatest first), ties
     * by fewer squares, then the fixed move order. */
    for (int cls = MATERIAL_FORCED; cls >= MATERIAL_POSSIBLE; --cls) {
        int values[256], n = 0;
        for (int i = 0; i < count; ++i) {
            if (e[i].taken && (int)e[i].material == cls) {
                values[n++] = -e[i].value; /* greatest value = IF */
            }
        }
        for (int v = 9; v >= 1; --v) {
            for (int d = 1; d <= 7; ++d) {
                for (int i = 0; i < count; ++i) {
                    if (!e[i].taken || (int)e[i].material != cls || e[i].value != v || e[i].squares != d) {
                        continue;
                    }
                    move_text(e[i].move, text);
                    text_add(out, "GROUP 1 CAPTURE %s TAKES %c VALUE %d SQUARES %d %s ORDER %s%s\n", text, e[i].taken,
                             e[i].value, e[i].squares, material_text(e[i].material), order_label(values, n, -v),
                             e[i].bounded ? " BOUNDED" : "");
                }
            }
        }
    }
    /* Group 2: everything else, by squares travelled (fewest = IF). */
    int distances[256], n = 0;
    for (int i = 0; i < count; ++i) {
        if (!(e[i].taken && e[i].material != MATERIAL_ELSE)) {
            distances[n++] = e[i].squares;
        }
    }
    for (int d = 1; d <= 7; ++d) {
        for (int i = 0; i < count; ++i) {
            if ((e[i].taken && e[i].material != MATERIAL_ELSE) || e[i].squares != d) {
                continue;
            }
            move_text(e[i].move, text);
            text_add(out, "GROUP 2 DISTANCE %s SQUARES %d ORDER %s", text, d, order_label(distances, n, d));
            if (e[i].taken) {
                text_add(out, " TAKES %c VALUE %d %s%s", e[i].taken, e[i].value, material_text(e[i].material),
                         e[i].bounded ? " BOUNDED" : "");
            }
            text_add(out, "\n");
        }
    }
    text_add(out, "ORDER END\n");
}

static void clear_selection(int *row, int *col, bool *preview_shown,
                            bool *promotion_choice_pending) {
    *row = -1;
    *col = -1;
    *preview_shown = false;
    *promotion_choice_pending = false;
}

int main(void) {
    Board game = {0};
    char input[128];
    int selected_row = -1;
    int selected_col = -1;
    bool preview_shown = false;
    Move preview_move = {0, 0, 0, 0, '\0'};
    bool promotion_choice_pending = false;
    Move pending_promotion_move = {0, 0, 0, 0, '\0'};
    static Tracker tracker; /* Stage 3A: piece names and lost moves */
    static Goal goal;       /* Stage 3D-1: the active goal, if any */
    bool distance_view = false; /* Stage 3D-1: "distance on" */
    static Goal second_goal;    /* Stage 3D-2: "goal2" */

#ifdef _WIN32
    /* Only in a real console window. When the input comes from a file (the
     * transcript tests), the cmd.exe that system() starts used up that input
     * before the game could read it, so the game saw no moves at all. */
    if (_isatty(0)) {
        (void)system("chcp 65001 > nul");
    }
#endif

    reset_board(&game);
    tracker_init(&tracker, &game);
    puts("=== Simple 2-Player Chess ===");

    for (;;) {
        bool stalemate;
        const char *draw_reason = NULL;

        if (!has_king(&game, WHITE) || !has_king(&game, BLACK)) {
            puts("Invalid position: both kings must be on the board.");
            break;
        }

        display(&game, selected_row, selected_col);

        if (game_over(&game, &stalemate, &draw_reason)) {
            if (stalemate) {
                puts("Game Over! Stalemate.");
            } else if (draw_reason) {
                printf("Game Over! Draw by %s.\n", draw_reason);
            } else {
                const char *winner;
                if (game.turn == WHITE) {
                    winner = "Black";
                } else {
                    winner = "White";
                }
                printf("Checkmate! %s wins!\n", winner);
            }
            break;
        }

        if (is_in_check(&game, game.turn)) {
            printf(CYAN "Your king is in check!\n" RESET);
        }

        puts("STEP 2: Select, preview, then explicitly finalize a chess move.");
        if (promotion_choice_pending) {
            puts("PROMOTION:");
            puts("Q = Queen");
            puts("R = Rook");
            puts("B = Bishop");
            puts("N = Knight");
            puts("Type NEW to cancel and select another piece.");
            puts("Type your choice, then press Enter.");
        } else if (!in_bounds(selected_row, selected_col)) {
            puts("Characters 1-2: enter a piece square (e.g. d2), then Enter.");
        } else if (!preview_shown) {
            puts("Characters 3-4: enter a destination (e.g. d4), then Enter.");
            puts("The BEFORE and AFTER lists will appear. NO MOVE is made yet.");
            puts("Press N then Enter to choose a DIFFERENT piece.");
        } else {
            printf("PREVIEW READY: %c%d -> %c%d (NOT played).\n",
                   'a' + preview_move.sc, 8 - preview_move.sr,
                   'a' + preview_move.ec, 8 - preview_move.er);
            puts("F = Finalize move | P = Preview another destination | N = New piece");
            puts("Type one command, then press Enter.");
        }
        if (!preview_shown) {
            puts("Every move requires a preview followed by F + Enter.");
            puts("Other commands: restart, quit, claim, resign");
        }
        print_selection_near_prompt(&game, selected_row, selected_col);
        if (promotion_choice_pending) {
            printf("%s - choose promotion (Q/R/B/N): ", color_name(game.turn));
        } else if (preview_shown) {
            printf("%s - Enter command (F/P/N): ",
                   color_name(game.turn));
        } else if (in_bounds(selected_row, selected_col)) {
            printf("%s - enter destination (characters 3-4): ",
                   color_name(game.turn));
        } else {
            printf("%s - enter START square (characters 1-2): ",
                   color_name(game.turn));
        }
        fflush(stdout);

        InputResult input_result = read_input_line(input, sizeof(input));
        if (input_result == INPUT_READ_EOF) {
            break;
        }
        if (input_result == INPUT_READ_TOO_LONG) {
            continue;
        }
        Command command = parse_command(input);

        /* An explicit new-selection command prevents ambiguous two-character input. */
        if (command == COMMAND_NEW ||
            (command == COMMAND_SHORT_NEW && !promotion_choice_pending)) {
            clear_selection(&selected_row, &selected_col, &preview_shown,
                            &promotion_choice_pending);
            puts("NEW SELECTION: now enter characters 1-2 for a different piece.");
            continue;
        }

        /* Stage 3S: "fen" prints the position, "fen <FEN>" sets one up. */
        if (strcmp(input, "fen") == 0) {
            char fen_text[FEN_TEXT_SIZE];
            board_to_fen(&game, fen_text);
            printf("FEN: %s\n", fen_text);
            continue;
        }
        if (strncmp(input, "fen ", 4) == 0) {
            const char *reason = NULL;
            if (load_fen(&game, input + 4, &reason)) {
                clear_selection(&selected_row, &selected_col, &preview_shown,
                                &promotion_choice_pending);
                tracker_init(&tracker, &game);
                puts("FEN loaded: practice position set up.");
            } else {
                printf(RED "FEN rejected: %s. The position is unchanged." RESET "\n", reason);
            }
            continue;
        }

        /* Choose the promotion piece BEFORE previewing or finalizing. */
        if (promotion_choice_pending && strlen(input) == 1) {
            char choice = (char)toupper((unsigned char)input[0]);
            if (strchr("QRBN", choice) != NULL) {
                pending_promotion_move.promotion = choice;
                show_step2_after(&game, pending_promotion_move);
                if (goal.active) {
                    TextBuffer marks = {NULL, 0, 0};
                    goal_marks_after(&game, &goal, pending_promotion_move, &marks);
                    fputs(marks.text, stdout);
                    free(marks.text);
                }
                if (distance_view) {
                    TextBuffer view = {NULL, 0, 0};
                    distance_after(&game, pending_promotion_move, &view);
                    fputs(view.text, stdout);
                    free(view.text);
                }
                preview_move = pending_promotion_move;
                preview_shown = true;
                promotion_choice_pending = false;
                continue;
            }
        }
        if (promotion_choice_pending && !is_game_command(command)) {
            puts(RED "Choose Q, R, B or N; or type NEW to cancel." RESET);
            continue;
        }

        /* Stage 3A: R prints the calculator's report for the previewed move. */
        if (!strcmp(input, "r") || !strcmp(input, "R")) {
            if (!preview_shown) {
                puts(RED "No preview to report. Select a piece and preview a destination, then type R." RESET);
            } else {
                TextBuffer report = {NULL, 0, 0};
                calc_report(&game, &tracker, preview_move, &report);
                fputs(report.text, stdout);
                free(report.text);
            }
            continue;
        }

        /* Stage 3B: "replies" and "replies <move>" for the previewed move. */
        if (!strcmp(input, "replies") || !strncmp(input, "replies ", 8)) {
            TextBuffer view = {NULL, 0, 0};
            Move reply;
            if (!preview_shown) {
                puts(RED "No preview to answer. Select a piece and preview a destination, then type replies." RESET);
            } else if (input[7] == '\0') {
                replies_report(&game, &tracker, preview_move, &view);
            } else if (!parse_move_text(input + 8, &reply) ||
                       !reply_detail(&game, &tracker, preview_move, reply, &view)) {
                puts(RED "Not a legal reply. Type replies to list them, or replies e7e5 for one." RESET);
            }
            if (view.text != NULL) {
                fputs(view.text, stdout);
                free(view.text);
            }
            continue;
        }

        /* Stage 3C: "loop N" (or "loop N K") and "trace <moves>", from the
         * current position; nothing is played. */
        if (!strcmp(input, "loop") || !strncmp(input, "loop ", 5)) {
            int depth = 0, zero_k = 0;
            if (input[4] == '\0' || !parse_loop_args(input + 5, &depth, &zero_k)) {
                puts(RED "Type loop and a depth from 1 to 6, for example loop 3 (or loop 3 6 to also count pieces that can't move)." RESET);
            } else {
                TextBuffer view = {NULL, 0, 0};
                if (depth >= 5) {
                    printf("Searching %d plies. This can take minutes (5 plies: about half a minute to a few minutes; 6: much longer).\n", depth);
                    fflush(stdout);
                }
                loop_report(&game, depth, zero_k, &view);
                fputs(view.text, stdout);
                free(view.text);
            }
            continue;
        }
        /* Stage 3D-1: "goal ...", "goal", "goal off" and "routes". */
        if (!strcmp(input, "goal") || !strncmp(input, "goal ", 5)) {
            TextBuffer view = {NULL, 0, 0};
            if (input[4] == '\0') {
                if (goal.active) {
                    text_add(&view, "GOAL ACTIVE ");
                    goal_text(&goal, &view);
                    text_add(&view, " LIMIT %d PASS\n", goal.limit);
                } else {
                    text_add(&view, "No goal is active. Type goal f7 2 (squares, then a limit of own moves 1 to 4).\n");
                }
            } else if (!strcmp(input + 5, "off")) {
                goal.active = false;
                text_add(&view, "Goal cleared.\n");
            } else if (parse_goal(input + 5, &goal)) {
                text_add(&view, "GOAL ACTIVE ");
                goal_text(&goal, &view);
                text_add(&view, " LIMIT %d PASS\n", goal.limit);
            } else {
                puts(RED "Type goal, squares and a limit from 1 to 4, for example goal f7 2, goal f7 g7 h7 3 or goal king 2; goal off clears it." RESET);
            }
            if (view.text != NULL) {
                fputs(view.text, stdout);
                free(view.text);
            }
            continue;
        }
        if (!strcmp(input, "distance") || !strcmp(input, "distance on") || !strcmp(input, "distance off")) {
            if (input[8] == ' ') {
                distance_view = input[10] == 'n';
            }
            printf("DISTANCE ORDER %s\n", distance_view ? "ON" : "OFF");
            continue;
        }
        if (!strcmp(input, "goal2") || !strncmp(input, "goal2 ", 6)) {
            if (input[5] == '\0') {
                if (second_goal.active) {
                    TextBuffer view = {NULL, 0, 0};
                    text_add(&view, "SECOND GOAL ACTIVE ");
                    goal_text(&second_goal, &view);
                    text_add(&view, " LIMIT %d PASS\n", second_goal.limit);
                    fputs(view.text, stdout);
                    free(view.text);
                } else {
                    puts("No second goal is active. Type goal2 d5 2 (squares, then a limit of own moves 1 to 4).");
                }
            } else if (!strcmp(input + 6, "off")) {
                second_goal.active = false;
                puts("Second goal cleared.");
            } else if (parse_goal(input + 6, &second_goal)) {
                TextBuffer view = {NULL, 0, 0};
                text_add(&view, "SECOND GOAL ACTIVE ");
                goal_text(&second_goal, &view);
                text_add(&view, " LIMIT %d PASS\n", second_goal.limit);
                fputs(view.text, stdout);
                free(view.text);
            } else {
                puts(RED "Type goal2, squares and a limit from 1 to 4, for example goal2 d5 2; goal2 off clears it." RESET);
            }
            continue;
        }
        if (!strncmp(input, "routes ", 7)) {
            int depth = 0;
            if (!goal.active) {
                puts(RED "No goal is active. Type goal f7 2 first, then routes." RESET);
            } else if (!parse_routes_real(input + 7, &depth)) {
                puts(RED "Type routes (PASS), or routes real and a depth from 1 to 6, for example routes real 3." RESET);
            } else {
                TextBuffer view = {NULL, 0, 0};
                if (depth >= 5) {
                    printf("Searching %d plies. This can take minutes.\n", depth);
                    fflush(stdout);
                }
                routes_real_report(&game, &goal, depth, &view);
                fputs(view.text, stdout);
                free(view.text);
            }
            continue;
        }
        /* Stage 3E: "exchange <square> N", "order N", "risk on/off". */
        if (!strcmp(input, "exchange") || !strncmp(input, "exchange ", 9)) {
            int sq = 0, limit = 0;
            if (input[8] == '\0' || !parse_exchange(input + 9, &sq, &limit)) {
                puts(RED "Type exchange, a square and a limit of captures from 1 to 8, for example exchange e5 4." RESET);
            } else {
                TextBuffer view = {NULL, 0, 0};
                exchange_report(&game, sq, limit, &view);
                fputs(view.text, stdout);
                free(view.text);
            }
            continue;
        }
        if (!strcmp(input, "order") || !strncmp(input, "order ", 6)) {
            int limit = 0, used = 0;
            if (input[5] == '\0' || sscanf(input + 6, "%d%n", &limit, &used) != 1 || input[6 + used] != '\0' ||
                limit < 1 || limit > EXCHANGE_MAX) {
                puts(RED "Type order and an exchange limit from 1 to 8, for example order 4." RESET);
            } else {
                TextBuffer view = {NULL, 0, 0};
                order_report(&game, limit, &view);
                fputs(view.text, stdout);
                free(view.text);
            }
            continue;
        }
        if (!strcmp(input, "risk") || !strcmp(input, "risk on") || !strcmp(input, "risk off")) {
            if (input[4] == ' ') {
                goal_show_risk = input[6] == 'n';
            }
            printf("AT-RISK %s\n", goal_show_risk ? "ON" : "OFF");
            continue;
        }
        if (!strcmp(input, "support")) {
            if (!preview_shown) {
                puts(RED "No preview to support. Select a piece and preview a destination, then type support." RESET);
            } else {
                TextBuffer view = {NULL, 0, 0};
                support_report(&game, preview_move, &view);
                fputs(view.text, stdout);
                free(view.text);
            }
            continue;
        }
        if (!strcmp(input, "routes")) {
            if (!goal.active) {
                puts(RED "No goal is active. Type goal f7 2 first, then routes." RESET);
            } else {
                TextBuffer view = {NULL, 0, 0};
                routes_report_with(&game, &goal, &second_goal, &view);
                fputs(view.text, stdout);
                free(view.text);
            }
            continue;
        }
        if (!strcmp(input, "trace") || !strncmp(input, "trace ", 6)) {
            TextBuffer view = {NULL, 0, 0};
            int bad_ply = 0;
            if (!trace_report(&game, &tracker, input[5] == '\0' ? "" : input + 6, &view, &bad_ply)) {
                printf(RED "Not a legal sequence (move %d). Type trace and moves, for example trace e2e4 e7e5." RESET "\n",
                       bad_ply);
            }
            if (view.text != NULL) {
                fputs(view.text, stdout);
                free(view.text);
            }
            continue;
        }

        /* Explicitly finalize ONLY the last valid previewed move. */
        if (command == COMMAND_FINALIZE) {
            if (!preview_shown || !in_bounds(selected_row, selected_col)) {
                puts(RED "No preview to finalize. Select a piece and preview a destination first." RESET);
                continue;
            }
            if (!legal_move(&game, preview_move)) {
                puts(RED "Preview is no longer legal. Start a new selection." RESET);
                preview_shown = false;
                continue;
            }
            printf("FINALIZED: %c%d -> %c%d\n",
                   'a' + preview_move.sc, 8 - preview_move.sr,
                   'a' + preview_move.ec, 8 - preview_move.er);
            tracker_commit(&tracker, &game, preview_move);
            make_move(&game, preview_move);
            record_position(&game);
            clear_selection(&selected_row, &selected_col, &preview_shown,
                            &promotion_choice_pending);
            continue;
        }

        /* P discards the current preview, but retains the selected piece. */
        if (command == COMMAND_PREVIEW) {
            if (!in_bounds(selected_row, selected_col)) {
                puts(RED "Select a piece first." RESET);
            } else {
                preview_shown = false;
                puts("Preview cleared. Enter another destination for the selected piece.");
            }
            continue;
        }

        /* After preview, require F, P or N before another two-character entry. */
        if (preview_shown && strlen(input) == 2) {
            puts(RED "Preview already shown. Press F, P or N, then Enter." RESET);
            continue;
        }

        /* When no piece is selected, two characters mean the START square.
         * When a piece is selected, two characters mean a PREVIEW destination.
         * They NEVER automatically select another piece. */
        if (strlen(input) == 2) {
            int row, col;
            if (!parse_square(input, &row, &col)) {
                puts(RED "Enter a square such as d2." RESET);
                continue;
            }

            if (!in_bounds(selected_row, selected_col)) {
                if (game.board[row][col].color != game.turn) {
                    puts(RED "Choose a square containing your own piece." RESET);
                    continue;
                }
                selected_row = row;
                selected_col = col;
                preview_shown = false;
                print_selection_near_prompt(&game, row, col);
                puts("Piece selected. BEFORE/AFTER lists will appear together after the destination.");
                if (goal.active) {
                    TextBuffer marks = {NULL, 0, 0};
                    goal_marks_selection(&game, &goal, row, col, &marks);
                    fputs(marks.text, stdout);
                    free(marks.text);
                }
                if (distance_view) {
                    TextBuffer view = {NULL, 0, 0};
                    distance_selection(&game, row, col, &view);
                    fputs(view.text, stdout);
                    free(view.text);
                }
                continue;
            }

            Move chosen = {selected_row, selected_col, row, col, '\0'};
            if (!legal_move(&game, chosen)) {
                puts(RED "Not a legal destination. Press N then Enter to select a different piece." RESET);
                continue;
            }
            if (game.board[selected_row][selected_col].type == 'P' &&
                (row == 0 || row == 7)) {
                pending_promotion_move = chosen;
                promotion_choice_pending = true;
                puts("PROMOTION: choose Q (Queen), R (Rook), B (Bishop), or N (Knight).");
                puts("Type NEW to cancel and choose a different starting piece.");
                continue;
            }
            show_step2_after(&game, chosen);
            if (goal.active) {
                TextBuffer marks = {NULL, 0, 0};
                goal_marks_after(&game, &goal, chosen, &marks);
                fputs(marks.text, stdout);
                free(marks.text);
            }
            if (distance_view) {
                TextBuffer view = {NULL, 0, 0};
                distance_after(&game, chosen, &view);
                fputs(view.text, stdout);
                free(view.text);
            }
            preview_move = chosen;
            preview_shown = true;
            continue;
        }

        if (command == COMMAND_QUIT) {
            break;
        }
        if (command == COMMAND_RESTART) {
            reset_board(&game);
            tracker_init(&tracker, &game);
            clear_selection(&selected_row, &selected_col, &preview_shown,
                            &promotion_choice_pending);
            puts("Game restarted.");
            continue;
        }
        if (command == COMMAND_RESIGN) {
            Color winner = opposite(game.turn);
            printf("%s resigns. %s wins.\n",
                   color_name(game.turn),
                   color_name(winner));
            break;
        }

        /* Draw claims without an intended move do not play any chess move.
         * An intended move must be previewed and finalized normally first. */
        if (command == COMMAND_CLAIM) {
            if (claimable_draw(&game, (Move){0, 0, 0, 0, '\0'},
                               false, &draw_reason)) {
                printf("Game Over! Draw claimed by %s.\n", draw_reason);
                break;
            }
            puts(RED "No draw claim is currently available." RESET);
            continue;
        }

        /* Never permit a full move to bypass Step 2 and F. */
        if (strlen(input) == 4 || strlen(input) == 5 ||
            !strncmp(input, "offer ", 6) ||
            !strncmp(input, "claim ", 6)) {
            puts(RED "Direct moves are disabled. Enter the START square, then the"
                     " destination, review BEFORE/AFTER, and press F + Enter." RESET);
            continue;
        }

        puts(RED "Unknown command. Enter a square, or use F, P, N, restart, quit, claim or resign." RESET);
    }

    free(game.history);
    return 0;
}
