/* Step 1: piece selection and legal destination highlighting.
 * Type two characters (for example d2), then press Enter to see legal moves.
 * Step 2 lists other pieces before and after a hypothetical destination.
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

static bool has_king(const Board *b, Color side) {
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            if (b->board[r][c].type == 'K' && b->board[r][c].color == side) {
                return true;
            }
        }
    }
    return false;
}

static bool is_in_check(const Board *b, Color side) {
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            if (b->board[r][c].type == 'K' && b->board[r][c].color == side) {
                return square_attacked(b, r, c, opposite(side));
            }
        }
    }
    return false;
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
    int row_change = move.er - move.sr;
    int col_change = move.ec - move.sc;
    if (abs(row_change) <= 1 && abs(col_change) <= 1) {
        return true;
    }
    if (row_change != 0 || abs(col_change) != 2 || is_in_check(b, b->turn)) {
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

    int row_change = m.er - m.sr;
    int col_change = m.ec - m.sc;

    switch (piece.type) {
        case 'P':
            return pawn_move_is_pseudo_legal(b, m, piece, target);
        case 'N':
            return is_knight_move(row_change, col_change);
        case 'B':
            return is_diagonal_move(row_change, col_change) &&
                   path_clear(b, m.sr, m.sc, m.er, m.ec);
        case 'R':
            return is_straight_move(row_change, col_change) &&
                   path_clear(b, m.sr, m.sc, m.er, m.ec);
        case 'Q':
            return is_queen_move(row_change, col_change) &&
                   path_clear(b, m.sr, m.sc, m.er, m.ec);
        case 'K':
            return king_move_is_pseudo_legal(b, m);
        default:
            return false;
    }
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

static void mark_rook_captured(Board *b, int row, int col, Color color) {
    if (row == 7 && col == 0 && color == WHITE) {
        b->white_rook_a_moved = true;
    }
    if (row == 7 && col == 7 && color == WHITE) {
        b->white_rook_h_moved = true;
    }
    if (row == 0 && col == 0 && color == BLACK) {
        b->black_rook_a_moved = true;
    }
    if (row == 0 && col == 7 && color == BLACK) {
        b->black_rook_h_moved = true;
    }
}

static void mark_rook_moved_from(Board *b, Piece piece, int row, int col) {
    if (piece.color == WHITE) {
        if (row == 7 && col == 0) {
            b->white_rook_a_moved = true;
        }
        if (row == 7 && col == 7) {
            b->white_rook_h_moved = true;
        }
    } else {
        if (row == 0 && col == 0) {
            b->black_rook_a_moved = true;
        }
        if (row == 0 && col == 7) {
            b->black_rook_h_moved = true;
        }
    }
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
        mark_rook_moved_from(b, piece, m.sr, m.sc);
    }
}

static void make_move(Board *b, Move m) {
    Piece p = b->board[m.sr][m.sc];
    Piece target = b->board[m.er][m.ec];
    bool pawn_move = p.type == 'P';
    bool capture = target.color != EMPTY || move_is_en_passant(b, m, p);

    if (target.type == 'R') {
        mark_rook_captured(b, m.er, m.ec, target.color);
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

/* Step 1: Inspect one piece without changing the position. */
static void selected_destinations(const Board *b, int row, int col,
                                  bool destinations[8][8]) {
    memset(destinations, 0, sizeof(bool) * 8 * 8);
    if (!in_bounds(row, col)) {
        return;
    }

    /* Only the player whose turn it is can select a piece. */
    if (b->board[row][col].color != b->turn) {
        return;
    }

    MoveList moves = generate_moves(b);
    for (size_t i = 0; i < moves.count; ++i) {
        Move move = moves.items[i];
        if (move.sr == row && move.sc == col) {
            destinations[move.er][move.ec] = true;
        }
    }
    free(moves.items);
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

            if (light_square) {
                background = LIGHT_SQUARE_BG;
                foreground = DARK_TEXT_FG;
            } else {
                background = DARK_SQUARE_BG;
                foreground = LIGHT_TEXT_FG;
            }

            symbol = piece_symbol(piece);

            /* Three visible characters per cell preserve the board layout. */
            if (row == selected_row && col == selected_col) {
                printf("%s%s[%c]%s|", background, foreground, symbol, RESET);
            } else if (destinations[row][col]) {
                if (piece.color == EMPTY)
                    printf("%s%s * %s|", background, foreground, RESET);
                else
                    printf("%s%s(%c)%s|", background, foreground, symbol, RESET);
            } else {
                printf("%s%s %c %s|", background, foreground, symbol, RESET);
            }
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

    if (at >= length) {
        out[0] = '\0';
        return at;
    }

    while (at < length && source[at] == ' ') {
        ++at;
    }
    while (at < length && copied < COMPARE_WIDTH) {
        out[copied] = source[at];
        ++copied;
        ++at;
    }

    if (at < length && source[at] != ' ' && copied == COMPARE_WIDTH) {
        while (copied > 0 && out[copied - 1] != ' ') {
            --copied;
        }
        if (copied > 0) {
            at -= (COMPARE_WIDTH - copied);
        } else {
            copied = COMPARE_WIDTH;
        }
    }

    while (copied > 0 && out[copied - 1] == ' ') {
        --copied;
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
    puts("LEGAL MOVE LIST NOTE: Each side is evaluated as if it were that side's turn.");
    puts("Black and White possibilities are shown for comparison; only the actual");
    puts("side to move can play immediately. Check restrictions still apply.");
    compare_all_other_pieces(board, &simulated, proposed);
    puts("\nThe real board and player turn have NOT changed.");
    puts("Press F then Enter to FINALIZE this previewed move.");
    puts("Press P then Enter to preview another destination, or N for a new piece.");
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

        /* An explicit new-selection command prevents ambiguous two-character input. */
        if ((!promotion_choice_pending && (!strcmp(input, "n") || !strcmp(input, "N"))) ||
            !strcmp(input, "new") || !strcmp(input, "NEW") ||
            !strcmp(input, "select none")) {
            selected_row = -1;
            selected_col = -1;
            preview_shown = false;
            promotion_choice_pending = false;
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
        if (promotion_choice_pending) {
            puts(RED "Choose Q, R, B or N; or type NEW to cancel." RESET);
            continue;
        }

        /* Explicitly finalize ONLY the last valid previewed move. */
        if (!strcmp(input, "f") || !strcmp(input, "F")) {
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
            selected_row = -1;
            selected_col = -1;
            preview_shown = false;
            continue;
        }

        /* P discards the current preview, but retains the selected piece. */
        if (!strcmp(input, "p") || !strcmp(input, "P")) {
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
            preview_move = chosen;
            preview_shown = true;
            continue;
        }

        if (!strcmp(input, "quit") || !strcmp(input, "exit")) {
            break;
        }
        if (!strcmp(input, "restart")) {
            reset_board(&game);
            selected_row = -1;
            selected_col = -1;
            preview_shown = false;
            promotion_choice_pending = false;
            puts("Game restarted.");
            continue;
        }
        if (!strcmp(input, "resign")) {
            Color winner = opposite(game.turn);
            printf("%s resigns. %s wins.\n",
                   color_name(game.turn),
                   color_name(winner));
            break;
        }

        /* Draw claims without an intended move do not play any chess move.
         * An intended move must be previewed and finalized normally first. */
        if (!strcmp(input, "claim")) {
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
