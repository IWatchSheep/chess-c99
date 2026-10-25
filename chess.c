/* Step 1: piece selection and legal destination highlighting.
 * Type two characters (for example d2), then press Enter to see legal moves.
 * Step 2 lists other pieces before and after a hypothetical destination.
 * Two-character destinations do not move pieces. Full moves still work.
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

static Color opposite(Color color) {
    if (color == WHITE) {
        return BLACK;
    }
    return WHITE;
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

static bool piece_attacks(const Board *b, int sr, int sc, int tr, int tc) {
    Piece p = b->board[sr][sc];
    int dr = tr - sr;
    int dc = tc - sc;
    int ar = abs(dr);
    int ac = abs(dc);

    if (p.color == EMPTY) {
        return false;
    }
    if (sr == tr && sc == tc) {
        return false;
    }

    switch (p.type) {
        case 'P': {
            int pawn_direction;
            if (p.color == WHITE) {
                pawn_direction = -1;
            } else {
                pawn_direction = 1;
            }
            return abs(dc) == 1 && dr == pawn_direction;
        }

        case 'N':
            if (ar == 2 && ac == 1) {
                return true;
            }
            if (ar == 1 && ac == 2) {
                return true;
            }
            return false;

        case 'B':
            if (ar != ac) {
                return false;
            }
            return path_clear(b, sr, sc, tr, tc);

        case 'R':
            if (dr != 0 && dc != 0) {
                return false;
            }
            return path_clear(b, sr, sc, tr, tc);

        case 'Q':
            if (dr != 0 && dc != 0 && ar != ac) {
                return false;
            }
            return path_clear(b, sr, sc, tr, tc);

        case 'K':
            return ar <= 1 && ac <= 1;

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
    int home_row;
    bool king_moved;
    Color enemy = opposite(side);
    bool king_side;
    bool rook_moved;
    int rook_col;
    int transit_col;
    int destination_col;

    if (side == WHITE) {
        home_row = 7;
        king_moved = b->white_king_moved;
    } else {
        home_row = 0;
        king_moved = b->black_king_moved;
    }
    king_side = move.ec > move.sc;

    if (move.sr != home_row || move.sc != 4 || king_moved) {
        return false;
    }

    if (king_side) {
        if (side == WHITE) {
            rook_moved = b->white_rook_h_moved;
        } else {
            rook_moved = b->black_rook_h_moved;
        }
        rook_col = 7;
        transit_col = 5;
        destination_col = 6;
    } else {
        if (side == WHITE) {
            rook_moved = b->white_rook_a_moved;
        } else {
            rook_moved = b->black_rook_a_moved;
        }
        rook_col = 0;
        transit_col = 3;
        destination_col = 2;
    }

    if (rook_moved ||
        b->board[home_row][rook_col].type != 'R' ||
        b->board[home_row][rook_col].color != side) {
        return false;
    }

    int first_empty_col;
    int last_empty_col;
    if (king_side) {
        first_empty_col = 5;
        last_empty_col = 6;
    } else {
        first_empty_col = 1;
        last_empty_col = 3;
    }
    for (int col = first_empty_col; col <= last_empty_col; ++col) {
        if (b->board[home_row][col].color != EMPTY) {
            return false;
        }
    }

    if (square_attacked(b, home_row, transit_col, enemy) ||
        square_attacked(b, home_row, destination_col, enemy)) {
        return false;
    }

    return true;
}

static bool pseudo_legal(const Board *b, Move m) {
    Piece p, target;
    int dr, dc, ar, ac;
    if (!in_bounds(m.sr, m.sc) || !in_bounds(m.er, m.ec)) {
        return false;
    }

    p = b->board[m.sr][m.sc];
    target = b->board[m.er][m.ec];
    if (p.color != b->turn) {
        return false;
    }
    if (target.color == b->turn) {
        return false;
    }
    if (target.type == 'K') {
        return false;
    }
    if (m.promotion != '\0') {
        if (p.type != 'P') {
            return false;
        }
        if (strchr("QRBN", m.promotion) == NULL) {
            return false;
        }
        if (m.er != 0 && m.er != 7) {
            return false;
        }
    }

    dr = m.er - m.sr;
    dc = m.ec - m.sc;
    ar = abs(dr);
    ac = abs(dc);

    switch (p.type) {
        case 'P': {
            int direction;
            int starting_row;
            if (p.color == WHITE) {
                direction = -1;
                starting_row = 6;
            } else {
                direction = 1;
                starting_row = 1;
            }

            if (dc == 0 && dr == direction && target.color == EMPTY) {
                return true;
            }

            if (dc == 0 &&
                dr == 2 * direction &&
                m.sr == starting_row &&
                target.color == EMPTY &&
                b->board[m.sr + direction][m.sc].color == EMPTY) {
                return true;
            }

            if (ac == 1 && dr == direction && target.color != EMPTY) {
                return true;
            }

            if (ac == 1 &&
                dr == direction &&
                target.color == EMPTY &&
                m.er == b->ep_row &&
                m.ec == b->ep_col &&
                b->board[m.sr][m.ec].type == 'P' &&
                b->board[m.sr][m.ec].color == opposite(p.color)) {
                return true;
            }

            return false;
        }

        case 'N':
            if (ar == 2 && ac == 1) {
                return true;
            }
            if (ar == 1 && ac == 2) {
                return true;
            }
            return false;

        case 'B':
            if (ar != ac) {
                return false;
            }
            return path_clear(b, m.sr, m.sc, m.er, m.ec);

        case 'R':
            if (dr != 0 && dc != 0) {
                return false;
            }
            return path_clear(b, m.sr, m.sc, m.er, m.ec);

        case 'Q':
            if (dr != 0 && dc != 0 && ar != ac) {
                return false;
            }
            return path_clear(b, m.sr, m.sc, m.er, m.ec);

        case 'K':
            if (ar <= 1 && ac <= 1) {
                return true;
            }
            if (dr != 0) {
                return false;
            }
            if (ac != 2) {
                return false;
            }
            if (is_in_check(b, b->turn)) {
                return false;
            }
            return can_castle(b, m, b->turn);

        default:
            return false;
    }
}

static bool legal_move(const Board *b, Move m) {
    Board temp;
    Piece p;
    if (!pseudo_legal(b, m)) {
        return false;
    }

    temp = *b;
    p = temp.board[m.sr][m.sc];

    if (p.type == 'P' &&
        m.er == b->ep_row &&
        m.ec == b->ep_col &&
        temp.board[m.er][m.ec].color == EMPTY) {
        temp.board[m.sr][m.ec] = empty_piece();
    }

    if (p.type == 'K' && abs(m.ec - m.sc) == 2) {
        if (m.ec == 6) {
            temp.board[m.sr][5] = temp.board[m.sr][7];
            temp.board[m.sr][7] = empty_piece();
        } else {
            temp.board[m.sr][3] = temp.board[m.sr][0];
            temp.board[m.sr][0] = empty_piece();
        }
    }

    temp.board[m.sr][m.sc] = empty_piece();
    temp.board[m.er][m.ec] = p;

    if (p.type == 'P' && (m.er == 0 || m.er == 7)) {
        if (m.promotion != '\0') {
            temp.board[m.er][m.ec].type = m.promotion;
        } else {
            temp.board[m.er][m.ec].type = 'Q';
        }
    }

    return !is_in_check(&temp, b->turn);
}

static void position_key(const Board *b, char key[80]) {
    size_t n = 0;
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            Piece p = b->board[r][c];
            key[n++] = p.color == EMPTY ? '.' :
                (p.color == WHITE ? (char)tolower((unsigned char)p.type) : p.type);
        }
    }
    key[n++] = b->turn == WHITE ? 'W' : 'B';
    key[n++] = !b->white_king_moved && !b->white_rook_a_moved ? '1' : '0';
    key[n++] = !b->white_king_moved && !b->white_rook_h_moved ? '1' : '0';
    key[n++] = !b->black_king_moved && !b->black_rook_a_moved ? '1' : '0';
    key[n++] = !b->black_king_moved && !b->black_rook_h_moved ? '1' : '0';

    bool legal_ep = false;
    if (in_bounds(b->ep_row, b->ep_col)) {
        for (int dc = -1; dc <= 1; dc += 2) {
            int sr = b->ep_row + (b->turn == WHITE ? 1 : -1);
            int sc = b->ep_col + dc;
            Move m = {sr, sc, b->ep_row, b->ep_col, '\0'};
            if (in_bounds(sr, sc) && legal_move(b, m)) {
                legal_ep = true;
                break;
            }
        }
    }
    if (legal_ep) {
        key[n++] = (char)('0' + b->ep_row);
        key[n++] = (char)('0' + b->ep_col);
    } else {
        key[n++] = '-';
    }
    key[n] = '\0';
}

static void record_position(Board *b) {
    char key[80];
    position_key(b, key);
    if (b->history_size == b->history_capacity) {
        size_t capacity = b->history_capacity ? b->history_capacity * 2 : 32;
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
    for (size_t i = 0; i < b->history_size; ++i)
        if (strcmp(key, b->history[i].key) == 0) ++count;
    return count;
}

static void append_move(MoveList *list, Move move) {
    if (list->count == list->capacity) {
        size_t next = list->capacity ? list->capacity * 2 : 32;
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

static MoveList generate_moves(const Board *b) {
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
    static const Direction knight_jumps[] = {
        {-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
        {1, -2}, {1, 2}, {2, -1}, {2, 1}
    };
    MoveList list = {NULL, 0, 0};

    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            Piece p = b->board[r][c];
            if (p.color != b->turn) {
                continue;
            }

            switch (p.type) {
                case 'P': {
                    int dir = (p.color == WHITE) ? -1 : 1;
                    consider_move(b, &list, r, c, r + dir, c);
                    consider_move(b, &list, r, c, r + 2 * dir, c);
                    for (int dc = -1; dc <= 1; dc += 2)
                        consider_move(b, &list, r, c, r + dir, c + dc);
                    break;
                }
                case 'N':
                    for (size_t i = 0; i < sizeof(knight_jumps) / sizeof(knight_jumps[0]); ++i)
                        consider_move(b, &list, r, c,
                                      r + knight_jumps[i].row, c + knight_jumps[i].col);
                    break;
                case 'B':
                    generate_sliding_moves(b, &list, r, c, diagonal,
                                           sizeof(diagonal) / sizeof(diagonal[0]));
                    break;
                case 'R':
                    generate_sliding_moves(b, &list, r, c, orthogonal,
                                           sizeof(orthogonal) / sizeof(orthogonal[0]));
                    break;
                case 'Q':
                    generate_sliding_moves(b, &list, r, c, queen_directions,
                                           sizeof(queen_directions) / sizeof(queen_directions[0]));
                    break;
                case 'K':
                    for (int dr = -1; dr <= 1; ++dr) {
                        for (int dc = -1; dc <= 1; ++dc) {
                            if (dr == 0 && dc == 0) {
                                continue;
                            }
                            consider_move(b, &list, r, c, r + dr, c + dc);
                        }
                    }
                    consider_move(b, &list, r, c, r, c + 2);
                    consider_move(b, &list, r, c, r, c - 2);
                    break;
                default:
                    break;
            }
        }
    }

    return list;
}

static void make_move(Board *b, Move m) {
    Piece p = b->board[m.sr][m.sc];
    Piece target = b->board[m.er][m.ec];
    bool pawn_move = p.type == 'P';
    bool capture = target.color != EMPTY;
    int prev_ep_row = b->ep_row;
    int prev_ep_col = b->ep_col;

    if (p.type == 'P' && abs(m.er - m.sr) == 2) {
        b->ep_row = (m.sr + m.er) / 2;
        b->ep_col = m.sc;
    } else {
        b->ep_row = -1;
        b->ep_col = -1;
    }

    bool is_en_passant_capture =
        p.type == 'P' &&
        m.er == prev_ep_row &&
        m.ec == prev_ep_col &&
        target.color == EMPTY;
    if (is_en_passant_capture) {
        b->board[m.sr][m.ec] = empty_piece();
        capture = true;
    }

    if (target.type == 'R') {
        if (m.er == 7 && m.ec == 0 && target.color == WHITE) {
            b->white_rook_a_moved = true;
        }
        if (m.er == 7 && m.ec == 7 && target.color == WHITE) {
            b->white_rook_h_moved = true;
        }
        if (m.er == 0 && m.ec == 0 && target.color == BLACK) {
            b->black_rook_a_moved = true;
        }
        if (m.er == 0 && m.ec == 7 && target.color == BLACK) {
            b->black_rook_h_moved = true;
        }
    }

    if (p.type == 'K' && abs(m.ec - m.sc) == 2) {
        if (m.ec == 6) {
            b->board[m.sr][5] = b->board[m.sr][7];
            b->board[m.sr][7] = empty_piece();
        } else {
            b->board[m.sr][3] = b->board[m.sr][0];
            b->board[m.sr][0] = empty_piece();
        }
    }

    if (p.type == 'K') {
        if (p.color == WHITE) {
            b->white_king_moved = true;
        } else {
            b->black_king_moved = true;
        }
    }

    if (p.type == 'R') {
        if (m.sr == 7 && m.sc == 0 && p.color == WHITE) {
            b->white_rook_a_moved = true;
        }
        if (m.sr == 7 && m.sc == 7 && p.color == WHITE) {
            b->white_rook_h_moved = true;
        }
        if (m.sr == 0 && m.sc == 0 && p.color == BLACK) {
            b->black_rook_a_moved = true;
        }
        if (m.sr == 0 && m.sc == 7 && p.color == BLACK) {
            b->black_rook_h_moved = true;
        }
    }

    b->board[m.er][m.ec] = p;
    b->board[m.sr][m.sc] = empty_piece();

    if (p.type == 'P' && (m.er == 0 || m.er == 7)) {
        if (m.promotion != '\0') {
            b->board[m.er][m.ec].type = m.promotion;
        } else {
            b->board[m.er][m.ec].type = 'Q';
        }
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
    MOVE_TEXT_VALID,
    MOVE_TEXT_BAD_LENGTH,
    MOVE_TEXT_BAD_SQUARE,
    MOVE_TEXT_BAD_PROMOTION
} MoveTextResult;

typedef enum {
    INPUT_READ_OK,
    INPUT_READ_TOO_LONG,
    INPUT_READ_EOF
} InputResult;

static MoveTextResult parse_move_text(const char *text, Move *move) {
    size_t length = strlen(text);

    if (length != 4 && length != 5) {
        return MOVE_TEXT_BAD_LENGTH;
    }
    if (!parse_square(text, &move->sr, &move->sc) ||
        !parse_square(text + 2, &move->er, &move->ec)) {
        return MOVE_TEXT_BAD_SQUARE;
    }

    move->promotion = '\0';
    if (length == 5) {
        move->promotion = (char)toupper((unsigned char)text[4]);
        if (strchr("QRBN", move->promotion) == NULL) {
            return MOVE_TEXT_BAD_PROMOTION;
        }
    }

    return MOVE_TEXT_VALID;
}

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
            const char *background = light_square ? LIGHT_SQUARE_BG : DARK_SQUARE_BG;
            const char *foreground = light_square ? DARK_TEXT_FG : LIGHT_TEXT_FG;
            char symbol = ' ';

            if (piece.color == WHITE) {
                symbol = piece.type;
            }
            if (piece.color == BLACK) {
                symbol = (char)tolower((unsigned char)piece.type);
            }

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
        printf("SELECTED: %s %c at %c%d\n", b->turn == WHITE ? "White" : "Black",
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
    Board after_move;
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
    unsigned repetitions_needed;
    if (has_intended) {
        repetitions_needed = 2;
    } else {
        repetitions_needed = 3;
    }
    if (repetition_count(b, key) >= repetitions_needed) {
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


/* STEP 2: The analysis uses temporary boards. It never commits a move. */
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
    if (p.color == EMPTY) return;
    view.turn = p.color;
    moves = generate_moves(&view);
    for (size_t i = 0; i < moves.count; ++i) {
        Move m = moves.items[i];
        if (m.sr != row || m.sc != col || seen[m.er][m.ec]) continue;
        seen[m.er][m.ec] = true;
        if (used < capacity) {
            int n = snprintf(output + used, capacity - used, "%s%c%d",
                             used ? " " : "", 'a' + m.ec, 8 - m.er);
            if (n < 0) break;
            if ((size_t)n >= capacity - used) { used = capacity - 1; break; }
            used += (size_t)n;
        }
    }
    free(moves.items);
    if (used == 0) snprintf(output, capacity, "(none)");
}

/* Print long destination lists side by side, wrapping at spaces. */
static size_t next_chunk(const char *source, size_t at, char *out) {
    size_t length = strlen(source), n = 0;
    if (at >= length) { out[0] = '\0'; return at; }
    while (at < length && source[at] == ' ') ++at;
    while (at < length && n < COMPARE_WIDTH) out[n++] = source[at++];
    if (at < length && source[at] != ' ' && n == COMPARE_WIDTH) {
        while (n > 0 && out[n - 1] != ' ') --n;
        if (n > 0) at -= (COMPARE_WIDTH - n);
        else n = COMPARE_WIDTH;
    }
    while (n > 0 && out[n - 1] == ' ') --n;
    out[n] = '\0';
    return at;
}

static void print_side_by_side(const char *before, const char *after) {
    size_t left = 0, right = 0;
    char l[COMPARE_WIDTH + 1], r[COMPARE_WIDTH + 1];
    do {
        left = next_chunk(before, left, l);
        right = next_chunk(after, right, r);
        printf("%-*s | %s\n", COMPARE_WIDTH, l, r);
    } while (left < strlen(before) || right < strlen(after));
}

static void make_piece_description(const Board *position, int row, int col,
                                   char *output, size_t capacity) {
    char destinations[256];
    Piece p = position->board[row][col];
    if (p.color == EMPTY) { snprintf(output, capacity, "(captured)"); return; }
    list_piece_moves(position, row, col, destinations, sizeof(destinations));
    snprintf(output, capacity, "%c%d %c: %s", 'a' + col, 8 - row,
             p.color == WHITE ? p.type : (char)tolower((unsigned char)p.type),
             destinations);
}

/* Lists every OTHER piece from the original board, aligned by identity.
 * The chosen piece is deliberately omitted. Castling rook relocation and
 * en-passant captures are handled so that before/after rows correspond. */
static void compare_all_other_pieces(const Board *before, const Board *after,
                                     Move proposed) {
    for (Color side = WHITE; side <= BLACK; side = (Color)(side + 1)) {
        puts("");
        printf("%s PIECES (excluding selected piece)\n", side == WHITE ? "WHITE" : "BLACK");
        print_side_by_side("BEFORE: square piece -> legal destinations",
                           "AFTER: square piece -> legal destinations");
        puts("-------------------------------------------------------------------+-------------------------------------------------------------------");
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                Piece p = before->board[row][col];
                if (p.color != side || (row == proposed.sr && col == proposed.sc)) continue;
                int after_row = row, after_col = col;
                char lhs[320], rhs[320];
                bool captured = (row == proposed.er && col == proposed.ec);
                /* En passant removes a pawn alongside the moving pawn. */
                if (before->board[proposed.sr][proposed.sc].type == 'P' &&
                    proposed.er == before->ep_row && proposed.ec == before->ep_col &&
                    row == proposed.sr && col == proposed.ec) captured = true;
                /* Castling moves the rook as well as the selected king. */
                if (before->board[proposed.sr][proposed.sc].type == 'K' &&
                    abs(proposed.ec - proposed.sc) == 2 && row == proposed.sr) {
                    if (proposed.ec == 6 && col == 7) after_col = 5;
                    if (proposed.ec == 2 && col == 0) after_col = 3;
                }
                make_piece_description(before, row, col, lhs, sizeof(lhs));
                if (captured) {
                    snprintf(rhs, sizeof(rhs), "(captured at %c%d)", 'a' + col, 8 - row);
                } else {
                    make_piece_description(after, after_row, after_col, rhs, sizeof(rhs));
                }
                print_side_by_side(lhs, rhs);
            }
        }
    }
}

static void show_step2_before(const Board *board, int origin_row, int origin_col) {
    char before[320];
    printf("\nSTEP 2 BEFORE: OTHER PIECES (board unchanged)\n");
    for (Color side = WHITE; side <= BLACK; side = (Color)(side + 1)) {
        printf("\n%s PIECES\n", side == WHITE ? "WHITE" : "BLACK");
        for (int r = 0; r < 8; ++r) for (int c = 0; c < 8; ++c) {
            if (r == origin_row && c == origin_col) continue;
            if (board->board[r][c].color != side) continue;
            make_piece_description(board, r, c, before, sizeof(before));
            printf("  %s\n", before);
        }
    }
}

static void show_step2_after(const Board *board, Move proposed) {
    Board simulated = *board; /* shallow copy safe: no history modification in output */
    printf("\nPREVIEW ONLY: %c%d -> %c%d (NOT PLAYED)\n",
           'a' + proposed.sc, 8 - proposed.sr, 'a' + proposed.ec, 8 - proposed.er);
    make_move(&simulated, proposed);
    puts("Widen your console to about 140 columns for the two-column report.");
    compare_all_other_pieces(board, &simulated, proposed);
    puts("\nThe real board and player turn have NOT changed.");
    puts("Choose another destination to preview, or enter a full four-character move to play.");
}

int main(void) {
    Board game = {0};
    char input[128];
    bool draw_offered = false;
    int selected_row = -1;
    int selected_col = -1;

#ifdef _WIN32
    (void)system("chcp 65001 > nul");
#endif

    reset_board(&game);
    puts("=== Simple 2-Player Chess ===");

    for (;;) {
        bool stalemate;
        const char *draw_reason = NULL;
        Move m;

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

        puts("STEP 1: Enter the piece square (2 characters, e.g. d2), then press Enter.");
        puts("The game will list its legal destination squares without moving the piece.");
        puts("Then enter a destination (e.g. d4) to PREVIEW before/after lists; no move occurs.");
        puts("Use a full move (e.g. d2d4) when you want to actually play.");
        puts("You may also enter a complete move (e.g. d2d4) at once.");
        puts("Clear selection: select none");
        puts("Commands: claim [move], offer move, accept, resign, restart, quit");
        if (draw_offered) {
            puts("A draw has been offered. Type 'accept' to agree.");
        }
        print_selection_near_prompt(&game, selected_row, selected_col);
        if (in_bounds(selected_row, selected_col)) {
            printf("%s - enter destination to PREVIEW (or full move to PLAY): ",
                   game.turn == WHITE ? "White" : "Black");
        } else {
            printf("%s - enter piece square, then press Enter: ",
                   game.turn == WHITE ? "White" : "Black");
        }
        fflush(stdout);

        InputResult input_result = read_input_line(input, sizeof(input));
        if (input_result == INPUT_READ_EOF) {
            break;
        }
        if (input_result == INPUT_READ_TOO_LONG) {
            continue;
        }

        /* Selection does not modify the board or consume a turn. */
        if (!strcmp(input, "select none")) {
            selected_row = -1;
            selected_col = -1;
            puts("Selection cleared.");
            continue;
        }
        /* Two separate square entries: origin, then destination. */
        if (strlen(input) == 2 || !strncmp(input, "select ", 7)) {
            bool explicit_select = !strncmp(input, "select ", 7);
            const char *square = explicit_select ? input + 7 : input;
            int row, col;

            if (strlen(square) != 2 || !parse_square(square, &row, &col)) {
                puts(RED "Enter a square such as d2." RESET);
                continue;
            }

            /* If a piece is selected, a legal destination completes its move. */
            if (!explicit_select && in_bounds(selected_row, selected_col)) {
                Move chosen = {selected_row, selected_col, row, col, '\0'};
                if (legal_move(&game, chosen)) {
                    if (game.board[selected_row][selected_col].type == 'P' &&
                        (row == 0 || row == 7)) {
                        puts("Preview assumes promotion to Queen. For another piece, use a full move e7e8n/r/b.");
                    }
                    show_step2_after(&game, chosen);
                    continue;
                }
            }

            /* Selecting another friendly piece changes the selection. */
            if (game.board[row][col].color == game.turn) {
                selected_row = row;
                selected_col = col;
                print_selection_near_prompt(&game, row, col);
                show_step2_before(&game, row, col);
            } else if (in_bounds(selected_row, selected_col) && !explicit_select) {
                puts(RED "That is not a legal destination for the selected piece." RESET);
            } else {
                puts(RED "Choose a square containing your own piece." RESET);
            }
            continue;
        }

        if (!strcmp(input, "quit") || !strcmp(input, "exit")) {
            break;
        }
        if (!strcmp(input, "restart")) {
            reset_board(&game);
            draw_offered = false;
            selected_row = -1;
            selected_col = -1;
            puts("Game restarted.");
            continue;
        }
        if (!strcmp(input, "accept")) {
            if (draw_offered) {
                puts("Draw agreed. Game over.");
                break;
            }
            puts(RED "There is no draw offer to accept." RESET);
            continue;
        }
        if (!strcmp(input, "resign")) {
            printf("%s resigns. %s wins.\n",
                   game.turn == WHITE ? "White" : "Black",
                   game.turn == WHITE ? "Black" : "White");
            break;
        }

        bool starts_with_claim = strncmp(input, "claim", 5) == 0;
        bool claim = false;
        if (starts_with_claim) {
            bool claim_has_valid_separator =
                input[5] == '\0' || input[5] == ' ';
            if (claim_has_valid_separator) {
                claim = true;
            }
        }
        bool offer = !strncmp(input, "offer ", 6);
        const char *move_text = input;
        if (claim) {
            if (input[5] == ' ') {
                move_text = input + 6;
            } else {
                move_text = input + 5;
            }
        }
        if (offer) {
            move_text = input + 6;
        }
        size_t move_len = strlen(move_text);

        if (claim && move_len == 0) {
            if (claimable_draw(&game, (Move){0, 0, 0, 0, '\0'}, false, &draw_reason)) {
                printf("Game Over! Draw claimed by %s.\n", draw_reason);
                break;
            }
            puts(RED "No draw claim is currently available." RESET);
            continue;
        }

        MoveTextResult parse_result = parse_move_text(move_text, &m);
        if (parse_result == MOVE_TEXT_BAD_LENGTH) {
            puts(RED "Invalid move. Example: e2e4 or e7e8q" RESET);
            continue;
        }
        if (parse_result == MOVE_TEXT_BAD_SQUARE) {
            puts(RED "Invalid square format. Use a-h and 1-8." RESET);
            continue;
        }
        if (parse_result == MOVE_TEXT_BAD_PROMOTION) {
            puts(RED "Invalid promotion piece. Use Q, R, B, or N." RESET);
            continue;
        }

        if (claim) {
            if (claimable_draw(&game, m, true, &draw_reason)) {
                printf("Game Over! Draw claimed by %s before the move.\n", draw_reason);
                break;
            }
            puts(RED "That move does not support a draw claim." RESET);
            continue;
        }

        if (legal_move(&game, m)) {
            draw_offered = offer;
            make_move(&game, m);
            record_position(&game);
            selected_row = -1;
            selected_col = -1;
            if (draw_offered) {
                puts("Draw offer sent.");
            }
        } else {
            puts(RED "Illegal move. Try again." RESET);
        }
    }

    free(game.history);
    return 0;
}
