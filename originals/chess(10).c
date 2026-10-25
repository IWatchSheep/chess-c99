/* Step 1: piece selection and legal destination highlighting.
 * Type two characters (for example d2), then press Enter to see legal moves.
 * Step 2 lists other pieces before and after a hypothetical destination.
 * Step 3 calculates whole-board strategic differentials, comparing:
 *   - Side A (Forgetting / Opportunistic: dives at goal threats)
 *   - Side B (Remembering / Coordinated: preserves team protection, safe width,
 *     and verifies opponent replies & value exchanges).
 * Every move requires selecting, previewing, then F to finalize.
 */
/* Simplified two-player chess in C99.
 * Build: gcc -std=c99 -Wall -Wextra -O2 chess.c -o chess
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define RED   "\033[31m"
#define CYAN  "\033[36m"
#define RESET "\033[0m"

/* Wooden board colors: ANSI 256-color escape sequences. */
#define LIGHT_SQUARE_BG "\033[48;5;180m"  /* tan */
#define DARK_SQUARE_BG  "\033[48;5;94m"   /* brown */
#define LIGHT_TEXT_FG   "\033[38;5;231m"  /* ivory: readable on dark squares */
#define DARK_TEXT_FG    "\033[38;5;232m"  /* charcoal: readable on light squares */

typedef enum {
    EMPTY,
    WHITE,
    BLACK
} Color;

typedef struct {
    char type;
    Color color;
} Piece;

typedef struct {
    int sr;
    int sc;
    int er;
    int ec;
    char promotion;
} Move;

typedef struct {
    char key[80];
} Position;

typedef struct {
    Piece board[8][8];
    Color turn;
    bool white_king_moved;
    bool black_king_moved;
    bool white_rook_a_moved;
    bool white_rook_h_moved;
    bool black_rook_a_moved;
    bool black_rook_h_moved;
    int ep_row;
    int ep_col;
    int halfmove_clock;
    Position *history;
    size_t history_size;
    size_t history_capacity;
} Board;

typedef struct {
    Move *items;
    size_t count;
    size_t capacity;
} MoveList;

typedef struct {
    int row;
    int col;
} Direction;

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

static void apply_move_to_board(Board *b, Move move);

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
    bool castle_kingside = king_side_castle(move);
    bool rook_moved = false;
    int rook_col = rook_col_for(castle_kingside);
    int transit_col = castle_kingside ? 5 : 3;

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

    Board transit = *b;
    transit.board[home_row][4] = empty_piece();
    transit.board[home_row][transit_col] = (Piece){'K', side};
    if (is_in_check(&transit, side)) {
        return false;
    }

    Board destination = *b;
    apply_move_to_board(&destination, move);
    return !is_in_check(&destination, side);
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

static bool apply_move(Board *b, Move move) {
    Piece moving_piece = b->board[move.sr][move.sc];
    Piece target_piece = b->board[move.er][move.ec];
    bool is_pawn_move = moving_piece.type == 'P';
    bool is_capture = target_piece.color != EMPTY ||
                      move_is_en_passant(b, move, moving_piece);

    if (target_piece.type == 'R') {
        mark_rook_at_home_square(b, move.er, move.ec, target_piece.color);
    }

    update_castle_state(b, moving_piece, move);
    apply_move_to_board(b, move);

    if (is_pawn_move && abs(move.er - move.sr) == 2) {
        b->ep_row = (move.sr + move.er) / 2;
        b->ep_col = move.sc;
    } else {
        b->ep_row = -1;
        b->ep_col = -1;
    }

    if (is_pawn_move || is_capture) {
        b->halfmove_clock = 0;
    } else {
        ++b->halfmove_clock;
    }
    b->turn = opposite(b->turn);

    return is_pawn_move || is_capture;
}

static void make_move(Board *b, Move move) {
    bool resets_repetition_history = apply_move(b, move);
    if (resets_repetition_history) {
        b->history_size = 0;
    }
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
    COMMAND_CALC,
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
    if (!strcmp(input, "c") || !strcmp(input, "C") ||
        !strcmp(input, "calc") || !strcmp(input, "CALC")) {
        return COMMAND_CALC;
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

/* ============================================================================
 * STEP 3: STRATEGY CALCULATOR (SIDE A FORGETTING VS. SIDE B REMEMBERING)
 * Uses chess.c's exact Board, piece_attacks, and legal_move primitives.
 * Shallow board copies never mutate game.history.
 * ============================================================================ */

static int step3_piece_value(char type) {
    switch (type) {
        case 'P': return 100;
        case 'N': return 300;
        case 'B': return 300;
        case 'R': return 500;
        case 'Q': return 900;
        default:  return 0; /* Kings cannot be captured */
    }
}

/* Simulate the shared position transition without recording game history. */
static Board step3_sim_move(const Board *b, Move m) {
    Board next = *b;
    (void)apply_move(&next, m);
    return next;
}

/*
 * Stage 3D Exact Single-Square Capture Minimax:
 * Evaluates legal captures restricted to (target_r, target_c) for 'side'.
 * Dynamically updates the board so X-rays, batteries, and pins are exact.
 * Each side may stand pat (0) if continuing the exchange loses material.
 */
static int step3_square_exchange(const Board *b, int target_r, int target_c, Color side) {
    int best = 0; /* Stand pat */
    Board view = *b;
    view.turn = side;

    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            if (view.board[r][c].color != side) {
                continue;
            }
            Move m = {r, c, target_r, target_c, '\0'};
            if (view.board[r][c].type == 'P' && (target_r == 0 || target_r == 7)) {
                m.promotion = 'Q';
            }
            if (!legal_move(&view, m)) {
                continue;
            }

            int victim_val = step3_piece_value(view.board[target_r][target_c].type);
            if (move_is_en_passant(&view, m, view.board[r][c])) {
                victim_val = 100;
            }
            int promo_gain = (m.promotion == 'Q') ? (900 - 100) : 0;

            Board next = step3_sim_move(&view, m);
            int reply = step3_square_exchange(&next, target_r, target_c, opposite(side));
            int net = victim_val + promo_gain - reply;
            if (net > best) {
                best = net;
            }
        }
    }
    return best;
}

/* Evaluates the material result on m.er, m.ec immediately after playing m */
static int step3_move_dest_exchange(const Board *b, Move m) {
    Color mover = b->board[m.sr][m.sc].color;
    int gained = step3_piece_value(b->board[m.er][m.ec].type);
    if (move_is_en_passant(b, m, b->board[m.sr][m.sc])) {
        gained = 100;
    }
    if (b->board[m.sr][m.sc].type == 'P' && (m.er == 0 || m.er == 7)) {
        char promo = (m.promotion == '\0') ? 'Q' : m.promotion;
        gained += step3_piece_value(promo) - 100;
    }

    Board after = step3_sim_move(b, m);
    int enemy_recap = step3_square_exchange(&after, m.er, m.ec, opposite(mover));
    return gained - enemy_recap;
}

/* Defines the opponent's King Zone (King square + 8 adjacent squares, e.g., f7) */
static void step3_build_goal_zone(const Board *b, Color us, bool goal[8][8]) {
    memset(goal, 0, sizeof(bool) * 8 * 8);
    Color enemy = opposite(us);
    int kr = -1, kc = -1;
    if (!find_king(b, enemy, &kr, &kc)) {
        return;
    }
    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            if (in_bounds(kr + dr, kc + dc)) {
                goal[kr + dr][kc + dc] = true;
            }
        }
    }
}

/* Checks if 'side' has an immediate legal checkmate in 1 move on board 'b' */
static bool step3_has_mate_in_one(const Board *b, Color side) {
    Board view = *b;
    view.turn = side;
    MoveList moves = generate_moves(&view);
    bool found_mate = false;

    for (size_t i = 0; i < moves.count; ++i) {
        Board next = step3_sim_move(&view, moves.items[i]);
        if (is_in_check(&next, opposite(side))) {
            MoveList replies = generate_moves(&next);
            if (replies.count == 0) {
                found_mate = true;
            }
            free(replies.items);
            if (found_mate) {
                break;
            }
        }
    }
    free(moves.items);
    return found_mate;
}

typedef struct {
    int total_legal_dests;
    int safe_useful_dests;
    int protection_links;
    int team_exposure_cp;
    int goal_cooperation;
    int goal_attackers;
} Step3Snapshot;

typedef struct {
    Move move;

    /* Step 2 Team Destination Totals (excluding the moving piece) */
    int gained_team_dests;
    int lost_team_dests;
    int unchanged_team_dests;

    /* Step 3 Relational & Width Deltas */
    int delta_safe_width;
    int formed_prot_links;
    int broken_prot_links;
    int delta_exposure_cp;
    int delta_goal_coop;
    int delta_goal_attacks;

    /* Immediate Offense (Unverified / Null-Move) */
    int  dest_exchange_cp;
    bool creates_mate_threat;
    int  immediate_goal_gain_cp;

    /* Adversarial 1-Ply Opponent Reply Verification */
    int  total_enemy_replies;
    int  refutation_count;
    char sample_refutations[64];
    bool is_immediate_mate;
    int  worst_case_net_cp;
    int  surviving_goal_threat_cp;
} Step3Report;

static void step3_take_snapshot(const Board *b, Color us, const bool *goal,
                                Step3Snapshot *snap) {
    memset(snap, 0, sizeof(*snap));
    Color enemy = opposite(us);
    Board view = *b;
    view.turn = us;

    MoveList moves = generate_moves(&view);
    for (size_t i = 0; i < moves.count; ++i) {
        Move m = moves.items[i];
        /* Count unique destination squares (avoiding 4x promotion duplicates) */
        if (m.promotion != '\0' && m.promotion != 'Q') {
            continue;
        }
        snap->total_legal_dests++;
        if (step3_move_dest_exchange(&view, m) >= 0) {
            snap->safe_useful_dests++;
        }
    }
    free(moves.items);

    /* Friendly Protection Matrix & Team Exposure */
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            if (b->board[r][c].color != us) {
                continue;
            }
            /* Count other friendly pieces protected by this piece */
            for (int rr = 0; rr < 8; ++rr) {
                for (int cc = 0; cc < 8; ++cc) {
                    if ((r != rr || c != cc) &&
                        b->board[rr][cc].color == us &&
                        b->board[rr][cc].type != 'K' &&
                        piece_attacks(b, r, c, rr, cc)) {
                        snap->protection_links++;
                    }
                }
            }
            /* Check if this friendly piece is hanging / under-defended */
            if (b->board[r][c].type != 'K') {
                int loss = step3_square_exchange(b, r, c, enemy);
                if (loss > 0) {
                    snap->team_exposure_cp += loss;
                }
            }
        }
    }

    /* Goal Cooperation & Convergence */
    for (int gr = 0; gr < 8; ++gr) {
        for (int gc = 0; gc < 8; ++gc) {
            if (!goal[gr * 8 + gc]) {
                continue;
            }
            int attackers = 0;
            for (int r = 0; r < 8; ++r) {
                for (int c = 0; c < 8; ++c) {
                    if (b->board[r][c].color == us &&
                        piece_attacks(b, r, c, gr, gc)) {
                        attackers++;
                    }
                }
            }
            snap->goal_attackers += attackers;
            if (attackers >= 2) {
                snap->goal_cooperation += (attackers - 1);
            }
        }
    }
}

static void step3_evaluate_move(const Board *before, Move proposed,
                                const bool *goal,
                                const Step3Snapshot *snap_before,
                                Step3Report *rep) {
    memset(rep, 0, sizeof(*rep));
    rep->move = proposed;

    Color us = before->board[proposed.sr][proposed.sc].color;
    Color enemy = opposite(us);

    int captured_val = step3_piece_value(before->board[proposed.er][proposed.ec].type);
    if (move_is_en_passant(before, proposed, before->board[proposed.sr][proposed.sc])) {
        captured_val = 100;
    }
    if (before->board[proposed.sr][proposed.sc].type == 'P' &&
        (proposed.er == 0 || proposed.er == 7)) {
        char promo = (proposed.promotion == '\0') ? 'Q' : proposed.promotion;
        captured_val += step3_piece_value(promo) - 100;
    }

    Board after = step3_sim_move(before, proposed);
    Step3Snapshot snap_after;
    step3_take_snapshot(&after, us, goal, &snap_after);

    /* 1. Step 2 Team Gained / Lost / Unchanged Totals across all other friendly pieces */
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            if (before->board[r][c].color != us) {
                continue;
            }
            if (r == proposed.sr && c == proposed.sc) {
                continue;
            }
            int ar = r, ac = c;
            bool cap = false;
            resolve_piece_after_move(before, proposed, r, c, &ar, &ac, &cap);
            if (cap) {
                continue;
            }

            bool old_d[8][8], new_d[8][8];
            piece_destination_map(before, r, c, old_d);
            piece_destination_map(&after, ar, ac, new_d);

            for (int dr = 0; dr < 8; ++dr) {
                for (int dc = 0; dc < 8; ++dc) {
                    if (new_d[dr][dc] && !old_d[dr][dc]) {
                        rep->gained_team_dests++;
                    } else if (old_d[dr][dc] && !new_d[dr][dc]) {
                        rep->lost_team_dests++;
                    } else if (old_d[dr][dc] && new_d[dr][dc]) {
                        rep->unchanged_team_dests++;
                    }
                }
            }
        }
    }

    /* 2. Pairwise Friendly Protection Links (Formed vs. Broken) */
    for (int r1 = 0; r1 < 8; ++r1) {
        for (int c1 = 0; c1 < 8; ++c1) {
            if (before->board[r1][c1].color != us) {
                continue;
            }
            int ar1 = r1, ac1 = c1;
            bool cap1 = false;
            if (r1 == proposed.sr && c1 == proposed.sc) {
                ar1 = proposed.er;
                ac1 = proposed.ec;
            } else {
                resolve_piece_after_move(before, proposed, r1, c1, &ar1, &ac1, &cap1);
            }
            if (cap1) {
                continue;
            }

            for (int r2 = 0; r2 < 8; ++r2) {
                for (int c2 = 0; c2 < 8; ++c2) {
                    if (r1 == r2 && c1 == c2) {
                        continue;
                    }
                    if (before->board[r2][c2].color != us ||
                        before->board[r2][c2].type == 'K') {
                        continue;
                    }
                    int ar2 = r2, ac2 = c2;
                    bool cap2 = false;
                    if (r2 == proposed.sr && c2 == proposed.sc) {
                        ar2 = proposed.er;
                        ac2 = proposed.ec;
                    } else {
                        resolve_piece_after_move(before, proposed, r2, c2, &ar2, &ac2, &cap2);
                    }
                    if (cap2) {
                        continue;
                    }

                    bool old_p = piece_attacks(before, r1, c1, r2, c2);
                    bool new_p = piece_attacks(&after, ar1, ac1, ar2, ac2);
                    if (!old_p && new_p) {
                        rep->formed_prot_links++;
                    } else if (old_p && !new_p) {
                        rep->broken_prot_links++;
                    }
                }
            }
        }
    }

    /* 3. Whole-Board Relational Deltas */
    rep->delta_safe_width   = snap_after.safe_useful_dests - snap_before->safe_useful_dests;
    rep->delta_exposure_cp  = snap_after.team_exposure_cp  - snap_before->team_exposure_cp;
    rep->delta_goal_coop    = snap_after.goal_cooperation  - snap_before->goal_cooperation;
    rep->delta_goal_attacks = snap_after.goal_attackers    - snap_before->goal_attackers;

    /* 4. Immediate Offense on Board After Move */
    rep->dest_exchange_cp    = step3_move_dest_exchange(before, proposed);
    rep->creates_mate_threat = step3_has_mate_in_one(&after, us);

    int best_goal_gain = 0;
    for (int gr = 0; gr < 8; ++gr) {
        for (int gc = 0; gc < 8; ++gc) {
                        if (goal[gr * 8 + gc]) {
                int ex = step3_square_exchange(&after, gr, gc, us);
                if (ex > best_goal_gain) {
                    best_goal_gain = ex;
                }
            }
        }
    }
    rep->immediate_goal_gain_cp = best_goal_gain;

    /* 5. Adversarial 1-Ply Reply Verification across ALL legal opponent replies */
    MoveList replies = generate_moves(&after);
    rep->total_enemy_replies = (int)replies.count;

    if (replies.count == 0 && is_in_check(&after, enemy)) {
        rep->is_immediate_mate        = true;
        rep->worst_case_net_cp        = 10000;
        rep->surviving_goal_threat_cp = 10000;
        free(replies.items);
        return;
    }

    int max_enemy_gain = 0;
    int min_surviving_threat = 10000;
    size_t ref_str_used = 0;

    for (size_t i = 0; i < replies.count; ++i) {
        Move r_move = replies.items[i];

        int enemy_gain = step3_move_dest_exchange(&after, r_move);
        if (enemy_gain > max_enemy_gain) {
            max_enemy_gain = enemy_gain;
        }

        /* Only run deep mate/goal-survival checks when our move created a goal threat */
        int threat_after_r = 0;
        if (rep->creates_mate_threat || rep->immediate_goal_gain_cp > 0) {
            Board after_reply = step3_sim_move(&after, r_move);
            if (step3_has_mate_in_one(&after_reply, us)) {
                threat_after_r = 10000;
            } else {
                bool reply_goal[8][8];
                step3_build_goal_zone(&after_reply, us, reply_goal);
                for (int gr = 0; gr < 8; ++gr) {
                    for (int gc = 0; gc < 8; ++gc) {
                        if (reply_goal[gr][gc]) {
                            int ex = step3_square_exchange(&after_reply, gr, gc, us);
                            if (ex > threat_after_r) {
                                threat_after_r = ex;
                            }
                        }
                    }
                }
            }

            if (threat_after_r == 0) {
                rep->refutation_count++;
                if (rep->refutation_count <= 5 &&
                    ref_str_used + 8 < sizeof(rep->sample_refutations)) {
                    int n = snprintf(rep->sample_refutations + ref_str_used,
                                     sizeof(rep->sample_refutations) - ref_str_used,
                                     "%s%c%d->%c%d",
                                     ref_str_used ? ", " : "",
                                     'a' + r_move.sc, 8 - r_move.sr,
                                     'a' + r_move.ec, 8 - r_move.er);
                    if (n > 0) {
                        ref_str_used += (size_t)n;
                    }
                }
            }
        }

        if (threat_after_r < min_surviving_threat) {
            min_surviving_threat = threat_after_r;
        }
    }

    free(replies.items);
    rep->worst_case_net_cp = captured_val - max_enemy_gain;
    rep->surviving_goal_threat_cp =
        (rep->total_enemy_replies > 0 &&
         (rep->creates_mate_threat || rep->immediate_goal_gain_cp > 0))
            ? min_surviving_threat
            : 0;
}

/*
 * SIDE A (Forgets & Dives Toward Opportunity):
 * Maximizes direct offensive threats on the Goal Zone (such as Scholar's Mate
 * Bc4 + Qh5/Qf3), while forgetting broken team protection links, collateral
 * team exposure, and opponent non-capturing refutations.
 */
static bool step3_side_a_prefers(const Step3Report *a, const Step3Report *b) {
    if (a->is_immediate_mate != b->is_immediate_mate) {
        return a->is_immediate_mate;
    }
    bool a_dest_safe = (a->dest_exchange_cp >= 0);
    bool b_dest_safe = (b->dest_exchange_cp >= 0);
    if (a_dest_safe != b_dest_safe) {
        return a_dest_safe;
    }
    if (a->creates_mate_threat != b->creates_mate_threat) {
        return a->creates_mate_threat;
    }
    if (a->immediate_goal_gain_cp != b->immediate_goal_gain_cp) {
        return a->immediate_goal_gain_cp > b->immediate_goal_gain_cp;
    }
    if (a->delta_goal_attacks != b->delta_goal_attacks) {
        return a->delta_goal_attacks > b->delta_goal_attacks;
    }
    if (a->delta_goal_coop != b->delta_goal_coop) {
        return a->delta_goal_coop > b->delta_goal_coop;
    }
    return a->gained_team_dests > b->gained_team_dests;
}

/*
 * SIDE B (Remembers & Coordinates):
 * Remembers friendly piece protection, team exposure, and opponent replies.
 * Rejects moves that leave teammates hanging or rely on easily refuted threats,
 * preferring favorable value trades and coordinated safe width expansion.
 */
static bool step3_side_b_prefers(const Step3Report *a, const Step3Report *b) {
    if (a->is_immediate_mate != b->is_immediate_mate) {
        return a->is_immediate_mate;
    }

    bool a_sound = (a->worst_case_net_cp >= 0) && (a->delta_exposure_cp <= 0);
    bool b_sound = (b->worst_case_net_cp >= 0) && (b->delta_exposure_cp <= 0);
    if (a_sound != b_sound) {
        return a_sound;
    }
    if (!a_sound && !b_sound) {
        if (a->worst_case_net_cp != b->worst_case_net_cp) {
            return a->worst_case_net_cp > b->worst_case_net_cp;
        }
    }

    if (a->worst_case_net_cp != b->worst_case_net_cp) {
        return a->worst_case_net_cp > b->worst_case_net_cp;
    }
    if (a->surviving_goal_threat_cp != b->surviving_goal_threat_cp) {
        return a->surviving_goal_threat_cp > b->surviving_goal_threat_cp;
    }

    int a_width = a->delta_safe_width +
                  (a->formed_prot_links - a->broken_prot_links) +
                  a->delta_goal_coop;
    int b_width = b->delta_safe_width +
                  (b->formed_prot_links - b->broken_prot_links) +
                  b->delta_goal_coop;
    if (a_width != b_width) {
        return a_width > b_width;
    }
    return a->gained_team_dests > b->gained_team_dests;
}

/* Prints the Step 3 Calculator table comparing all legal moves (command: C or CALC) */
static void show_step3_full_comparison(const Board *board) {
    bool goal[8][8];
    step3_build_goal_zone(board, board->turn, goal);

    Step3Snapshot snap_before;
        step3_take_snapshot(board, board->turn, &goal[0][0], &snap_before);

    MoveList moves = generate_moves(board);
    if (moves.count == 0) {
        free(moves.items);
        return;
    }

    Step3Report *reports = malloc(moves.count * sizeof(*reports));
    if (!reports) {
        free(moves.items);
        fputs("Out of memory comparing legal moves.\n", stderr);
        return;
    }

    size_t count = 0;
    size_t best_a = 0, best_b = 0;

    for (size_t i = 0; i < moves.count; ++i) {
        Move m = moves.items[i];
        if (m.promotion != '\0' && m.promotion != 'Q') {
            continue;
        }
        step3_evaluate_move(board, m, &goal[0][0], &snap_before, &reports[count]);
        if (count == 0 || step3_side_a_prefers(&reports[count], &reports[best_a])) {
            best_a = count;
        }
        if (count == 0 || step3_side_b_prefers(&reports[count], &reports[best_b])) {
            best_b = count;
        }
        count++;
    }

    puts("\n==========================================================================================================");
    printf("STEP 3 STRATEGY CALCULATOR — %s TO MOVE (All Legal Moves Compared)\n",
           color_name(board->turn));
    puts("----------------------------------------------------------------------------------------------------------");
    puts("Move      | TeamDests (G/L) | SafeWidth | Prot (+/-) | Exposure | GoalHit | MateThr (Refuted) | WorstNet");
    puts("----------+-----------------+-----------+------------+----------+---------+-------------------+-----------");

    for (size_t i = 0; i < count; ++i) {
        const Step3Report *r = &reports[i];
        Piece p = board->board[r->move.sr][r->move.sc];
        const char *tag = "";
        if (i == best_a && i == best_b) {
            tag = " <-- [BOTH A & B]";
        } else if (i == best_a) {
            tag = " <-- [SIDE A: Opportunistic]";
        } else if (i == best_b) {
            tag = " <-- [SIDE B: Coordinated]";
        }

        printf("%c %c%d->%c%d |   +%2d / -%2d     |   %+4d    |  +%2d / -%2d |  %+5d   |   %+3d   |  %-3s (%2d/%2d)      | %+5d%s\n",
               piece_symbol(p),
               'a' + r->move.sc, 8 - r->move.sr,
               'a' + r->move.ec, 8 - r->move.er,
               r->gained_team_dests, r->lost_team_dests,
               r->delta_safe_width,
               r->formed_prot_links, r->broken_prot_links,
               r->delta_exposure_cp,
               r->delta_goal_attacks,
               r->creates_mate_threat ? "YES" : "NO",
               r->refutation_count, r->total_enemy_replies,
               r->worst_case_net_cp,
               tag);
    }
    puts("==========================================================================================================");
    printf("  * SIDE A (Forgetting / Direct Dive) Top Choice : %c %c%d -> %c%d\n",
           piece_symbol(board->board[reports[best_a].move.sr][reports[best_a].move.sc]),
           'a' + reports[best_a].move.sc, 8 - reports[best_a].move.sr,
           'a' + reports[best_a].move.ec, 8 - reports[best_a].move.er);
    printf("  * SIDE B (Remembering / Coordinated) Top Choice: %c %c%d -> %c%d\n\n",
           piece_symbol(board->board[reports[best_b].move.sr][reports[best_b].move.sc]),
           'a' + reports[best_b].move.sc, 8 - reports[best_b].move.sr,
           'a' + reports[best_b].move.ec, 8 - reports[best_b].move.er);

    free(reports);
    free(moves.items);
}

/* Prints the detailed Step 3 Calculator analysis for the single previewed move */
static void show_step3_preview_report(const Board *board, Move proposed) {
    bool goal[8][8];
    step3_build_goal_zone(board, board->turn, goal);

    Step3Snapshot snap_before;
    step3_take_snapshot(board, board->turn, &goal[0][0], &snap_before);

    Step3Report rep;
    step3_evaluate_move(board, proposed, &goal[0][0], &snap_before, &rep);

    /* Also identify Side A and Side B top recommendations across the board for context */
    MoveList all = generate_moves(board);
    Step3Report best_a_rep = rep, best_b_rep = rep;
    bool initialized = false;
    for (size_t i = 0; i < all.count; ++i) {
        Move m = all.items[i];
        if (m.promotion != '\0' && m.promotion != 'Q') {
            continue;
        }
        Step3Report cur;
        step3_evaluate_move(board, m, &goal[0][0], &snap_before, &cur);
        if (!initialized || step3_side_a_prefers(&cur, &best_a_rep)) {
            best_a_rep = cur;
        }
        if (!initialized || step3_side_b_prefers(&cur, &best_b_rep)) {
            best_b_rep = cur;
        }
        initialized = true;
    }
    free(all.items);

    puts("\n====================================================================================================");
    printf("STEP 3 STRATEGY CALCULATOR REPORT FOR PREVIEW: %c%d -> %c%d\n",
           'a' + proposed.sc, 8 - proposed.sr, 'a' + proposed.ec, 8 - proposed.er);
    puts("----------------------------------------------------------------------------------------------------");
    printf("  1. TEAM MOVEMENT (Other Friendly Pieces) : +%d GAINED, -%d LOST, %d UNCHANGED\n",
           rep.gained_team_dests, rep.lost_team_dests, rep.unchanged_team_dests);
    printf("  2. EFFECTIVE WIDTH & PROTECTION MEMORY   : Safe Useful Moves %+d | Protection Links +%d Formed, -%d Broken\n",
           rep.delta_safe_width, rep.formed_prot_links, rep.broken_prot_links);
    printf("  3. COLLATERAL TEAM EXPOSURE              : %+d centipawns (%s)\n",
           rep.delta_exposure_cp,
           rep.delta_exposure_cp > 0 ? "WARNING: Leaves friendly piece under-defended!" :
           rep.delta_exposure_cp < 0 ? "Improves friendly team safety" : "Team protection intact");
    printf("  4. OFFENSE TOWARD KING ZONE (Goal)       : %+d Goal Attacks | %+d Multi-Piece Cooperation\n",
           rep.delta_goal_attacks, rep.delta_goal_coop);
    printf("  5. EXCHANGE & ADVERSARIAL REPLY CHECK    : Dest Square Exchange %+d cp | Worst-Case Net After Reply %+d cp\n",
           rep.dest_exchange_cp, rep.worst_case_net_cp);

    if (rep.is_immediate_mate) {
        puts("     ROUTE STATUS                          : FORCED CHECKMATE NOW!");
    } else if (rep.creates_mate_threat || rep.immediate_goal_gain_cp > 0) {
        if (rep.refutation_count == 0) {
            puts("     ROUTE STATUS                          : FORCED ROUTE (Threat survives all legal opponent replies)");
        } else {
            printf("     ROUTE STATUS                          : REFUTABLE ROUTE (%d/%d replies neutralize threat, e.g., %s)\n",
                   rep.refutation_count, rep.total_enemy_replies,
                   rep.sample_refutations[0] ? rep.sample_refutations : "defensive reply");
        }
    } else {
        puts("     ROUTE STATUS                          : DEVELOPING / POSITIONAL MOVE (No immediate goal capture threat)");
    }

    puts("----------------------------------------------------------------------------------------------------");
    printf("  * SIDE A (Forgetting / Opportunistic) Evaluation : %s (Side A Top Pick: %c%d->%c%d)\n",
           (rep.creates_mate_threat || rep.immediate_goal_gain_cp > 0 || rep.delta_goal_attacks > 0)
               ? "FAVORS direct offensive push toward goal"
               : "Low direct goal threat",
           'a' + best_a_rep.move.sc, 8 - best_a_rep.move.sr,
           'a' + best_a_rep.move.ec, 8 - best_a_rep.move.er);
    printf("  * SIDE B (Remembering / Coordinated) Evaluation  : %s (Side B Top Pick: %c%d->%c%d)\n",
           (rep.worst_case_net_cp < 0 || rep.delta_exposure_cp > 0)
               ? "REJECTS move (breaks protection or loses material to enemy reply)"
               : (rep.creates_mate_threat && rep.refutation_count > 0)
                   ? "CAUTIONS: One-turn threat is refutable; prefers coordinated width"
                   : "APPROVES: Preserves team protection and safe exchanges",
           'a' + best_b_rep.move.sc, 8 - best_b_rep.move.sr,
           'a' + best_b_rep.move.ec, 8 - best_b_rep.move.er);
    puts("====================================================================================================");
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

    /* STEP 3: Automatically display the Strategy Calculator report below Step 2 */
    show_step3_preview_report(board, proposed);

    puts("\nThe real board and player turn have NOT changed.");
    puts("Press F then Enter to FINALIZE this previewed move.");
    puts("Press P then Enter to preview another destination, N for a new piece, or C for full board calculator.");
}

static void clear_selection(int *row, int *col, bool *preview_shown,
                            bool *promotion_choice_pending) {
    *row = -1;
    *col = -1;
    *preview_shown = false;
    *promotion_choice_pending = false;
}

static bool is_promotion_move(const Board *board, Move move) {
    Piece piece = board->board[move.sr][move.sc];
    return piece.type == 'P' && (move.er == 0 || move.er == 7);
}

static bool handle_square_input(Board *game, const char *input,
                                int *selected_row, int *selected_col,
                                Move *preview_move, bool *preview_shown,
                                bool *promotion_choice_pending,
                                Move *pending_promotion_move) {
    if (strlen(input) != 2) {
        return false;
    }

    int row;
    int col;
    if (!parse_square(input, &row, &col)) {
        puts(RED "Enter a square such as d2." RESET);
        return true;
    }

    if (!in_bounds(*selected_row, *selected_col)) {
        if (game->board[row][col].color != game->turn) {
            puts(RED "Choose a square containing your own piece." RESET);
            return true;
        }

        *selected_row = row;
        *selected_col = col;
        *preview_shown = false;
        print_selection_near_prompt(game, row, col);
        puts("Piece selected. BEFORE/AFTER lists will appear together after the destination.");
        return true;
    }

    Move chosen = {*selected_row, *selected_col, row, col, '\0'};
    if (!legal_move(game, chosen)) {
        puts(RED "Not a legal destination. Press N then Enter to select a different piece." RESET);
        return true;
    }

    if (is_promotion_move(game, chosen)) {
        *pending_promotion_move = chosen;
        *promotion_choice_pending = true;
        puts("PROMOTION: choose Q (Queen), R (Rook), B (Bishop), or N (Knight).");
        puts("Type NEW to cancel and choose a different starting piece.");
        return true;
    }

    show_step2_after(game, chosen);
    *preview_move = chosen;
    *preview_shown = true;
    return true;
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

#ifdef _WIN32
    (void)system("chcp 65001 > nul");
#endif

    reset_board(&game);
    puts("=== Simple 2-Player Chess (Steps 1, 2 & Step 3 Strategy Calculator) ===");

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

        puts("STEP 2 & 3: Select, preview (with Side A vs Side B calculator), then F to finalize.");
        if (promotion_choice_pending) {
            puts("PROMOTION:");
            puts("Q = Queen");
            puts("R = Rook");
            puts("B = Bishop");
            puts("N = Knight");
            puts("Type NEW to cancel and select another piece.");
            puts("Type your choice, then press Enter.");
        } else if (!in_bounds(selected_row, selected_col)) {
            puts("Characters 1-2: enter a piece square (e.g. d2), or C to run Step 3 Calculator on all moves.");
        } else if (!preview_shown) {
            puts("Characters 3-4: enter a destination (e.g. d4), then Enter.");
            puts("The BEFORE/AFTER lists and Step 3 Strategy Report will appear. NO MOVE is made yet.");
            puts("Press N then Enter to choose a DIFFERENT piece, or C for full board calculator.");
        } else {
            printf("PREVIEW READY: %c%d -> %c%d (NOT played).\n",
                   'a' + preview_move.sc, 8 - preview_move.sr,
                   'a' + preview_move.ec, 8 - preview_move.er);
            puts("F = Finalize move | P = Preview another destination | N = New piece | C = All-Move Calculator");
            puts("Type one command, then press Enter.");
        }
        if (!preview_shown) {
            puts("Every move requires a preview followed by F + Enter.");
            puts("Other commands: calc (or c), restart, quit, claim, resign");
        }
        print_selection_near_prompt(&game, selected_row, selected_col);
        if (promotion_choice_pending) {
            printf("%s - choose promotion (Q/R/B/N): ", color_name(game.turn));
        } else if (preview_shown) {
            printf("%s - Enter command (F/P/N/C): ",
                   color_name(game.turn));
        } else if (in_bounds(selected_row, selected_col)) {
            printf("%s - enter destination (characters 3-4): ",
                   color_name(game.turn));
        } else {
            printf("%s - enter START square (characters 1-2, or C): ",
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

        /* Step 3 Full-Board Strategy Comparison on demand ('c' or 'calc') */
        if (command == COMMAND_CALC && !promotion_choice_pending) {
            show_step3_full_comparison(&game);
            continue;
        }

        /* An explicit new-selection command prevents ambiguous two-character input. */
        if (command == COMMAND_NEW ||
            (command == COMMAND_SHORT_NEW && !promotion_choice_pending)) {
            clear_selection(&selected_row, &selected_col, &preview_shown,
                            &promotion_choice_pending);
            puts("NEW SELECTION: now enter characters 1-2 for a different piece.");
            continue;
        }

        /* Choose the promotion piece BEFORE previewing or finalizing. */
        if (promotion_choice_pending && strlen(input) == 1) {
            char choice = (char)toupper((unsigned char)input[0]);
            if (strchr("QRBN", choice) != NULL) {
                pending_promotion_move.promotion = choice;
                show_step2_after(&game, pending_promotion_move);
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

        /* A square selects a piece or previews a destination, depending on state. */
        if (handle_square_input(&game, input,
                                &selected_row, &selected_col,
                                &preview_move, &preview_shown,
                                &promotion_choice_pending,
                                &pending_promotion_move)) {
            continue;
        }

        if (command == COMMAND_QUIT) {
            break;
        }
        if (command == COMMAND_RESTART) {
            reset_board(&game);
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

        puts(RED "Unknown command. Enter a square, or use F, P, N, C, restart, quit, claim or resign." RESET);
    }

    free(game.history);
    return 0;
}