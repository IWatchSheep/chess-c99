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
    return row >= 0 && row < 8 && col >= 0 && col < 8;
}

static Piece empty_piece(void) {
    Piece piece = {'\0', EMPTY};
    return piece;
}

static Color opposite(Color color) {
    return color == WHITE ? BLACK : WHITE;
}

static bool path_clear(const Board *b, int sr, int sc, int er, int ec) {
    int dr = (er > sr) - (er < sr);
    int dc = (ec > sc) - (ec < sc);
    int r = sr + dr, c = sc + dc;
    if (sr == er && sc == ec) return false;
    while (r != er || c != ec) {
        if (!in_bounds(r, c) || b->board[r][c].color != EMPTY) return false;
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

    if (p.color == EMPTY || (sr == tr && sc == tc)) return false;

    switch (p.type) {
        case 'P':
            return abs(dc) == 1 && dr == (p.color == WHITE ? -1 : 1);

        case 'N':
            return (ar == 2 && ac == 1) || (ar == 1 && ac == 2);

        case 'B':
            return ar == ac && path_clear(b, sr, sc, tr, tc);

        case 'R':
            return (dr == 0 || dc == 0) && path_clear(b, sr, sc, tr, tc);

        case 'Q':
            return (dr == 0 || dc == 0 || ar == ac) && path_clear(b, sr, sc, tr, tc);

        case 'K':
            return ar <= 1 && ac <= 1;

        default:
            return false;
    }
}

static bool square_attacked(const Board *b, int r, int c, Color by) {
    for (int rr = 0; rr < 8; ++rr) {
        for (int cc = 0; cc < 8; ++cc) {
            if (b->board[rr][cc].color == by && piece_attacks(b, rr, cc, r, c))
                return true;
        }
    }
    return false;
}

static bool is_in_check(const Board *b, Color side) {
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            if (b->board[r][c].type == 'K' && b->board[r][c].color == side)
                return square_attacked(b, r, c, opposite(side));
        }
    }
    return true;
}

static bool can_castle(const Board *b, Move move, Color side) {
    int home_row = (side == WHITE) ? 7 : 0;
    Color enemy = opposite(side);
    bool king_moved = (side == WHITE) ? b->white_king_moved : b->black_king_moved;
    bool king_side = move.ec > move.sc;
    bool rook_moved;
    int rook_col;
    int transit_col;
    int destination_col;

    if (move.sr != home_row || move.sc != 4 || king_moved) return false;

    if (king_side) {
        rook_moved = (side == WHITE) ? b->white_rook_h_moved : b->black_rook_h_moved;
        rook_col = 7;
        transit_col = 5;
        destination_col = 6;
    } else {
        rook_moved = (side == WHITE) ? b->white_rook_a_moved : b->black_rook_a_moved;
        rook_col = 0;
        transit_col = 3;
        destination_col = 2;
    }

    if (rook_moved ||
        b->board[home_row][rook_col].type != 'R' ||
        b->board[home_row][rook_col].color != side)
        return false;

    int first_empty_col = king_side ? 5 : 1;
    int last_empty_col = king_side ? 6 : 3;
    for (int col = first_empty_col; col <= last_empty_col; ++col) {
        if (b->board[home_row][col].color != EMPTY) return false;
    }

    if (square_attacked(b, home_row, transit_col, enemy) ||
        square_attacked(b, home_row, destination_col, enemy))
        return false;

    return true;
}

static bool pseudo_legal(const Board *b, Move m) {
    Piece p, target;
    int dr, dc, ar, ac;
    if (!in_bounds(m.sr, m.sc) || !in_bounds(m.er, m.ec)) return false;

    p = b->board[m.sr][m.sc];
    target = b->board[m.er][m.ec];
    if (p.color != b->turn) return false;
    if (target.color == b->turn || target.type == 'K') return false;
    if (m.promotion && (p.type != 'P' || !strchr("QRBN", m.promotion))) return false;
    if (m.promotion && m.er != 0 && m.er != 7) return false;

    dr = m.er - m.sr;
    dc = m.ec - m.sc;
    ar = abs(dr);
    ac = abs(dc);

    switch (p.type) {
        case 'P': {
            int dir = (p.color == WHITE) ? -1 : 1;
            int start_row = (p.color == WHITE) ? 6 : 1;

            if (dc == 0 && dr == dir && target.color == EMPTY) return true;
            if (dc == 0 && dr == 2 * dir && m.sr == start_row &&
                target.color == EMPTY && b->board[m.sr + dir][m.sc].color == EMPTY)
                return true;
            if (ac == 1 && dr == dir && target.color != EMPTY) return true;
            if (ac == 1 && dr == dir && target.color == EMPTY &&
                m.er == b->ep_row && m.ec == b->ep_col &&
                b->board[m.sr][m.ec].type == 'P' &&
                b->board[m.sr][m.ec].color == opposite(p.color))
                return true;
            return false;
        }

        case 'N':
            return (ar == 2 && ac == 1) || (ar == 1 && ac == 2);

        case 'B':
            return ar == ac && path_clear(b, m.sr, m.sc, m.er, m.ec);

        case 'R':
            return (dr == 0 || dc == 0) && path_clear(b, m.sr, m.sc, m.er, m.ec);

        case 'Q':
            return (dr == 0 || dc == 0 || ar == ac) && path_clear(b, m.sr, m.sc, m.er, m.ec);

        case 'K':
            if (ar <= 1 && ac <= 1) return true;
            if (dr != 0 || ac != 2 || is_in_check(b, b->turn)) return false;
            return can_castle(b, m, b->turn);

        default:
            return false;
    }
}

static bool legal_move(const Board *b, Move m) {
    Board temp;
    Piece p;
    if (!pseudo_legal(b, m)) return false;

    temp = *b;
    p = temp.board[m.sr][m.sc];

    if (p.type == 'P' && m.er == b->ep_row && m.ec == b->ep_col && temp.board[m.er][m.ec].color == EMPTY)
        temp.board[m.sr][m.ec] = empty_piece();

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

    if (p.type == 'P' && (m.er == 0 || m.er == 7))
        temp.board[m.er][m.ec].type = m.promotion ? m.promotion : 'Q';

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
    if (!in_bounds(er, ec) || !legal_move(b, m)) return;

    if (b->board[sr][sc].type == 'P' && (er == 0 || er == 7)) {
        static const char promos[] = "QRBN";
        for (size_t i = 0; i < strlen(promos); ++i) {
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

            if (!in_bounds(end_row, end_col)) break;

            consider_move(b, list, row, col, end_row, end_col);
            if (b->board[end_row][end_col].color != EMPTY) break;

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
            if (p.color != b->turn) continue;

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
                            if (dr == 0 && dc == 0) continue;
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

    if (p.type == 'P' && abs(m.er - m.sr) == 2)
        b->ep_row = (m.sr + m.er) / 2, b->ep_col = m.sc;
    else
        b->ep_row = -1, b->ep_col = -1;

    if (p.type == 'P' && m.er == prev_ep_row && m.ec == prev_ep_col && target.color == EMPTY) {
        b->board[m.sr][m.ec] = empty_piece();
        capture = true;
    }

    if (target.type == 'R') {
        if (m.er == 7 && m.ec == 0 && target.color == WHITE) b->white_rook_a_moved = true;
        if (m.er == 7 && m.ec == 7 && target.color == WHITE) b->white_rook_h_moved = true;
        if (m.er == 0 && m.ec == 0 && target.color == BLACK) b->black_rook_a_moved = true;
        if (m.er == 0 && m.ec == 7 && target.color == BLACK) b->black_rook_h_moved = true;
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
        if (p.color == WHITE) b->white_king_moved = true;
        else b->black_king_moved = true;
    }

    if (p.type == 'R') {
        if (m.sr == 7 && m.sc == 0 && p.color == WHITE) b->white_rook_a_moved = true;
        if (m.sr == 7 && m.sc == 7 && p.color == WHITE) b->white_rook_h_moved = true;
        if (m.sr == 0 && m.sc == 0 && p.color == BLACK) b->black_rook_a_moved = true;
        if (m.sr == 0 && m.sc == 7 && p.color == BLACK) b->black_rook_h_moved = true;
    }

    b->board[m.er][m.ec] = p;
    b->board[m.sr][m.sc] = empty_piece();

    if (p.type == 'P' && (m.er == 0 || m.er == 7))
        b->board[m.er][m.ec].type = (m.promotion != '\0') ? m.promotion : 'Q';

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
    if (file < 'a' || file > 'h' || rank < '1' || rank > '8') return false;
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

    if (length != 4 && length != 5) return MOVE_TEXT_BAD_LENGTH;
    if (!parse_square(text, &move->sr, &move->sc) ||
        !parse_square(text + 2, &move->er, &move->ec))
        return MOVE_TEXT_BAD_SQUARE;

    move->promotion = '\0';
    if (length == 5) {
        move->promotion = (char)toupper((unsigned char)text[4]);
        if (!strchr("QRBN", move->promotion)) return MOVE_TEXT_BAD_PROMOTION;
    }

    return MOVE_TEXT_VALID;
}

static InputResult read_input_line(char *input, int capacity) {
    if (!fgets(input, capacity, stdin)) return INPUT_READ_EOF;

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

static void display(const Board *b) {
    puts("\n    a   b   c   d   e   f   g   h");
    puts("  +---+---+---+---+---+---+---+---+");

    for (int r = 0; r < 8; ++r) {
        printf("%d |", 8 - r);
        for (int c = 0; c < 8; ++c) {
            Piece p = b->board[r][c];
            if (p.color == EMPTY) printf("   |");
            else printf(" %c |", p.type);
        }
        printf(" %d\n", 8 - r);
        puts("  +---+---+---+---+---+---+---+---+");
    }

    puts("    a   b   c   d   e   f   g   h\n");
}

static bool dead_position(const Board *b) {
    int minors = 0, bishops = 0;
    int bishop_color = -1;
    bool bishops_same_color = true;

    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            Piece p = b->board[r][c];
            if (p.color == EMPTY || p.type == 'K') continue;
            if (p.type != 'B' && p.type != 'N') return false;
            ++minors;
            if (p.type == 'B') {
                ++bishops;
                if (bishop_color == -1) bishop_color = (r + c) % 2;
                else if (bishop_color != (r + c) % 2) bishops_same_color = false;
            }
        }
    }

    if (minors == 0 || minors == 1) return true;
    if (bishops == minors && bishops_same_color) return true;
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
        if (!legal_move(b, intended)) return false;
        after_move = *b;
        make_move(&after_move, intended);
        position = &after_move;
    }

    if (position->halfmove_clock >= 100) {
        *reason = "50-move rule";
        return true;
    }
    position_key(position, key);
    if (repetition_count(b, key) >= (has_intended ? 2u : 3u)) {
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

int main(void) {
    Board game = {0};
    char input[128];
    bool draw_offered = false;

#ifdef _WIN32
    (void)system("chcp 65001 > nul");
#endif

    reset_board(&game);
    puts("=== Simple 2-Player Chess ===");

    for (;;) {
        bool stalemate;
        const char *draw_reason = NULL;
        Move m;

        display(&game);

        if (game_over(&game, &stalemate, &draw_reason)) {
            if (stalemate) puts("Game Over! Stalemate.");
            else if (draw_reason) printf("Game Over! Draw by %s.\n", draw_reason);
            else printf("Checkmate! %s wins!\n", game.turn == WHITE ? "Black" : "White");
            break;
        }

        if (is_in_check(&game, game.turn)) printf(CYAN "Your king is in check!\n" RESET);

        puts("Moves: e2e4, e7e8q | Commands: claim [move], offer move, accept, resign, restart, quit");
        if (draw_offered) puts("A draw has been offered. Type 'accept' to agree.");
        printf("Turn -> %s: ", game.turn == WHITE ? "White" : "Black");
        fflush(stdout);

        InputResult input_result = read_input_line(input, sizeof(input));
        if (input_result == INPUT_READ_EOF) break;
        if (input_result == INPUT_READ_TOO_LONG) continue;

        if (!strcmp(input, "quit") || !strcmp(input, "exit")) break;
        if (!strcmp(input, "restart")) {
            reset_board(&game);
            draw_offered = false;
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

        bool claim = !strncmp(input, "claim", 5) &&
                     (input[5] == '\0' || input[5] == ' ');
        bool offer = !strncmp(input, "offer ", 6);
        const char *move_text = input;
        if (claim) move_text = input[5] == ' ' ? input + 6 : input + 5;
        if (offer) move_text = input + 6;
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
            if (draw_offered) puts("Draw offer sent.");
        } else {
            puts(RED "Illegal move. Try again." RESET);
        }
    }

    free(game.history);
    return 0;
}
