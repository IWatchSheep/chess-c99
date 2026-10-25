/* Two-player chess, ISO C99 conversion of the supplied C++ program.
 * Build: gcc -std=c99 -Wall -Wextra -Wpedantic -O2 chess_c99.c -o chess_c99
 * Moves: e2e4, e7e8q; commands: claim, restart, quit.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define RED "\033[31m"
#define CYAN "\033[36m"
#define RESET "\033[0m"
#define KEY_SIZE 80

typedef enum { EMPTY, WHITE_P, BLACK_P } Color;
typedef struct { char type; Color color; } Piece;
typedef struct { int start_x, start_y, end_x, end_y; char promotion; } Move;
typedef struct { char key[KEY_SIZE]; unsigned count; } Position;
typedef struct {
    Piece board[8][8];
    Color turn;
    bool white_king_moved, black_king_moved;
    bool white_rook_a_moved, white_rook_h_moved;
    bool black_rook_a_moved, black_rook_h_moved;
    int ep_x, ep_y, halfmove_clock;
    Position *history;
    size_t history_size, history_capacity;
} Board;

typedef struct { Move *items; size_t count, capacity; } MoveList;

static bool in_bounds(int r, int c) { return r >= 0 && r < 8 && c >= 0 && c < 8; }
static Piece empty_piece(void) { Piece p = {'\0', EMPTY}; return p; }
static Color opposite(Color c) { return c == WHITE_P ? BLACK_P : WHITE_P; }
static bool is_in_check(const Board *b, Color side);

static bool path_clear(const Board *b, int sr, int sc, int er, int ec) {
    int dr = (er > sr) - (er < sr), dc = (ec > sc) - (ec < sc);
    int r = sr + dr, c = sc + dc;
    if (sr == er && sc == ec) return false;
    while (r != er || c != ec) {
        if (!in_bounds(r, c) || b->board[r][c].color != EMPTY) return false;
        r += dr; c += dc;
    }
    return true;
}
static bool can_attack(const Board *b, int sr, int sc, int er, int ec) {
    Piece p;
    int dr, dc, ar, ac;
    if (!in_bounds(sr, sc) || !in_bounds(er, ec) || (sr == er && sc == ec)) return false;
    p = b->board[sr][sc];
    if (p.color == EMPTY) return false;
    dr = er - sr; dc = ec - sc; ar = abs(dr); ac = abs(dc);
    switch (p.type) {
        case 'P': return ac == 1 && dr == (p.color == WHITE_P ? -1 : 1);
        case 'N': return (ar == 2 && ac == 1) || (ar == 1 && ac == 2);
        case 'B': return ar == ac && path_clear(b, sr, sc, er, ec);
        case 'R': return (dr == 0 || dc == 0) && path_clear(b, sr, sc, er, ec);
        case 'Q': return (dr == 0 || dc == 0 || ar == ac) && path_clear(b, sr, sc, er, ec);
        case 'K': return ar <= 1 && ac <= 1;
        default: return false;
    }
}
static bool square_attacked(const Board *b, int er, int ec, Color by) {
    for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 8; ++c)
            if (b->board[r][c].color == by && can_attack(b, r, c, er, ec)) return true;
    return false;
}
static bool is_in_check(const Board *b, Color side) {
    for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 8; ++c)
            if (b->board[r][c].type == 'K' && b->board[r][c].color == side)
                return square_attacked(b, r, c, opposite(side));
    return true; /* A missing king is an invalid/check state. */
}
static bool pseudo_legal(const Board *b, Move m) {
    Piece p, target;
    int dr, dc, ar, ac;
    Color enemy;
    if (!in_bounds(m.start_x, m.start_y) || !in_bounds(m.end_x, m.end_y)) return false;
    p = b->board[m.start_x][m.start_y]; target = b->board[m.end_x][m.end_y];
    if (p.color != b->turn || target.color == b->turn || target.type == 'K') return false;
    if (m.promotion && (p.type != 'P' || (m.end_x != 0 && m.end_x != 7) ||
        !strchr("QRBN", m.promotion))) return false;
    dr = m.end_x - m.start_x; dc = m.end_y - m.start_y;
    ar = abs(dr); ac = abs(dc);
    switch (p.type) {
        case 'P': {
            int dir = p.color == WHITE_P ? -1 : 1;
            int first = p.color == WHITE_P ? 6 : 1;
            if (!dc && dr == dir && target.color == EMPTY) return true;
            if (!dc && dr == 2 * dir && m.start_x == first && target.color == EMPTY &&
                b->board[m.start_x + dir][m.start_y].color == EMPTY) return true;
            if (ac == 1 && dr == dir && target.color != EMPTY) return true;
            if (ac == 1 && dr == dir && target.color == EMPTY &&
                m.end_x == b->ep_x && m.end_y == b->ep_y &&
                b->board[m.start_x][m.end_y].type == 'P' &&
                b->board[m.start_x][m.end_y].color == opposite(p.color)) return true;
            return false;
        }
        case 'N': return (ar == 2 && ac == 1) || (ar == 1 && ac == 2);
        case 'B': return ar == ac && path_clear(b, m.start_x, m.start_y, m.end_x, m.end_y);
        case 'R': return (dr == 0 || dc == 0) && path_clear(b, m.start_x, m.start_y, m.end_x, m.end_y);
        case 'Q': return (dr == 0 || dc == 0 || ar == ac) && path_clear(b, m.start_x, m.start_y, m.end_x, m.end_y);
        case 'K':
            if (ar <= 1 && ac <= 1) return true;
            if (dr != 0 || ac != 2 || is_in_check(b, b->turn)) return false;
            enemy = opposite(b->turn);
            if (b->turn == WHITE_P) {
                if (m.start_x != 7 || m.start_y != 4 || b->white_king_moved) return false;
                if (dc == 2) return !b->white_rook_h_moved && b->board[7][7].type == 'R' &&
                    b->board[7][7].color == WHITE_P && b->board[7][5].color == EMPTY &&
                    b->board[7][6].color == EMPTY && !square_attacked(b, 7, 5, enemy) && !square_attacked(b, 7, 6, enemy);
                if (dc == -2) return !b->white_rook_a_moved && b->board[7][0].type == 'R' &&
                    b->board[7][0].color == WHITE_P && b->board[7][1].color == EMPTY &&
                    b->board[7][2].color == EMPTY && b->board[7][3].color == EMPTY &&
                    !square_attacked(b, 7, 2, enemy) && !square_attacked(b, 7, 3, enemy);
            } else {
                if (m.start_x != 0 || m.start_y != 4 || b->black_king_moved) return false;
                if (dc == 2) return !b->black_rook_h_moved && b->board[0][7].type == 'R' &&
                    b->board[0][7].color == BLACK_P && b->board[0][5].color == EMPTY &&
                    b->board[0][6].color == EMPTY && !square_attacked(b, 0, 5, enemy) && !square_attacked(b, 0, 6, enemy);
                if (dc == -2) return !b->black_rook_a_moved && b->board[0][0].type == 'R' &&
                    b->board[0][0].color == BLACK_P && b->board[0][1].color == EMPTY &&
                    b->board[0][2].color == EMPTY && b->board[0][3].color == EMPTY &&
                    !square_attacked(b, 0, 2, enemy) && !square_attacked(b, 0, 3, enemy);
            }
            return false;
        default: return false;
    }
}
static bool legal_move(const Board *b, Move m) {
    Board temp;
    Piece p;
    if (!pseudo_legal(b, m)) return false;
    temp = *b; /* The pointer to history is borrowed; never modified here. */
    p = temp.board[m.start_x][m.start_y];
    if (p.type == 'P' && m.end_x == b->ep_x && m.end_y == b->ep_y &&
        temp.board[m.end_x][m.end_y].color == EMPTY)
        temp.board[m.start_x][m.end_y] = empty_piece();
    if (p.type == 'P' && (m.end_x == 0 || m.end_x == 7))
        p.type = m.promotion ? m.promotion : 'Q';
    temp.board[m.start_x][m.start_y] = empty_piece();
    temp.board[m.end_x][m.end_y] = p;
    return !is_in_check(&temp, b->turn);
}
/* A position key includes piece placement, side to move, castling rights and
 * an en-passant target only if an en-passant capture is actually legal. */
static void position_key(const Board *b, char key[KEY_SIZE]) {
    size_t n = 0;
    for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 8; ++c) {
            Piece p = b->board[r][c];
            key[n++] = p.color == EMPTY ? '.' :
                (p.color == WHITE_P ? (char)tolower((unsigned char)p.type) : p.type);
        }
    key[n++] = b->turn == WHITE_P ? 'W' : 'B';
    key[n++] = b->white_king_moved ? '1' : '0';
    key[n++] = b->white_rook_a_moved ? '1' : '0';
    key[n++] = b->white_rook_h_moved ? '1' : '0';
    key[n++] = b->black_king_moved ? '1' : '0';
    key[n++] = b->black_rook_a_moved ? '1' : '0';
    key[n++] = b->black_rook_h_moved ? '1' : '0';
    if (in_bounds(b->ep_x, b->ep_y)) {
        int row = b->ep_x + (b->turn == WHITE_P ? 1 : -1);
        if (in_bounds(row, b->ep_y) && b->board[b->ep_x][b->ep_y].color == EMPTY &&
            b->board[row][b->ep_y].type == 'P' &&
            b->board[row][b->ep_y].color == opposite(b->turn)) {
            for (int side = -1; side <= 1; side += 2) {
                int col = b->ep_y + side;
                if (in_bounds(row, col) && b->board[row][col].type == 'P' &&
                    b->board[row][col].color == b->turn) {
                    Board temp = *b;
                    temp.board[row][col] = empty_piece();
                    temp.board[row][b->ep_y] = empty_piece();
                    temp.board[b->ep_x][b->ep_y] = (Piece){'P', b->turn};
                    if (!is_in_check(&temp, b->turn)) {
                        key[n++] = (char)('0' + b->ep_x);
                        key[n++] = ',';
                        key[n++] = (char)('0' + b->ep_y);
                        key[n] = '\0';
                        return;
                    }
                }
            }
        }
    }
    key[n++] = '-'; key[n] = '\0';
}
static void record_position(Board *b) {
    char key[KEY_SIZE];
    position_key(b, key);
    for (size_t i = 0; i < b->history_size; ++i)
        if (strcmp(key, b->history[i].key) == 0) {
            ++b->history[i].count;
            return;
        }
    if (b->history_size == b->history_capacity) {
        size_t next = b->history_capacity ? b->history_capacity * 2 : 32;
        Position *new_history = realloc(b->history, next * sizeof(*new_history));
        if (!new_history) { fputs("Out of memory storing chess history.\n", stderr); exit(EXIT_FAILURE); }
        b->history = new_history; b->history_capacity = next;
    }
    strcpy(b->history[b->history_size].key, key);
    b->history[b->history_size].count = 1;
    ++b->history_size;
}
static void reset_board(Board *b) {
    static const char back[] = "RNBQKBNR";
    b->turn = WHITE_P;
    b->white_king_moved = b->black_king_moved = false;
    b->white_rook_a_moved = b->white_rook_h_moved = false;
    b->black_rook_a_moved = b->black_rook_h_moved = false;
    b->ep_x = b->ep_y = -1;
    b->halfmove_clock = 0;
    b->history_size = 0;
    for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 8; ++c) b->board[r][c] = empty_piece();
    for (int c = 0; c < 8; ++c) {
        b->board[0][c] = (Piece){back[c], BLACK_P};
        b->board[1][c] = (Piece){'P', BLACK_P};
        b->board[6][c] = (Piece){'P', WHITE_P};
        b->board[7][c] = (Piece){back[c], WHITE_P};
    }
    record_position(b);
}
static void display(const Board *b) {
    puts("\n    a   b   c   d   e   f   g   h");
    puts("  +--------------------------------+");
    for (int r = 0; r < 8; ++r) {
        printf("%d |", 8 - r);
        for (int c = 0; c < 8; ++c) {
            Piece p = b->board[r][c];
            const char *bg = (r + c) % 2 == 0 ? "\033[44m" : "\033[47m";
            const char *fg = p.color == WHITE_P ? "\033[97m\033[1m" :
                             p.color == BLACK_P ? "\033[93m\033[1m" : "\033[37m";
            printf("%s %s%c %s|", bg, fg, p.color == EMPTY ? ' ' : p.type, RESET);
        }
        printf(" %d\n", 8 - r);
        puts("  +--------------------------------+");
    }
    puts("    a   b   c   d   e   f   g   h\n");
}
static void append_move(MoveList *list, Move move) {
    if (list->count == list->capacity) {
        size_t next = list->capacity ? list->capacity * 2 : 64;
        Move *more = realloc(list->items, next * sizeof(*more));
        if (!more) { fputs("Out of memory generating moves.\n", stderr); exit(EXIT_FAILURE); }
        list->items = more; list->capacity = next;
    }
    list->items[list->count++] = move;
}
static void consider_move(const Board *b, MoveList *list, int sr, int sc, int er, int ec) {
    Move m = {sr, sc, er, ec, '\0'};
    if (!in_bounds(er, ec) || !legal_move(b, m)) return;
    if (b->board[sr][sc].type == 'P' && (er == 0 || er == 7)) {
        const char *promos = "QRBN";
        for (int i = 0; promos[i]; ++i) {
            m.promotion = promos[i]; append_move(list, m);
        }
    } else append_move(list, m);
}
static MoveList legal_moves(const Board *b) {
    static const int drs[8] = {-1, 1, 0, 0, -1, -1, 1, 1};
    static const int dcs[8] = {0, 0, -1, 1, -1, 1, -1, 1};
    static const int knight_r[8] = {-2, -2, -1, -1, 1, 1, 2, 2};
    static const int knight_c[8] = {-1, 1, -2, 2, -2, 2, -1, 1};
    MoveList result = {NULL, 0, 0};
    for (int r = 0; r < 8; ++r) for (int c = 0; c < 8; ++c) {
        Piece p = b->board[r][c];
        if (p.color != b->turn) continue;
        if (p.type == 'N') {
            for (int i = 0; i < 8; ++i)
                consider_move(b, &result, r, c, r + knight_r[i], c + knight_c[i]);
        } else if (p.type == 'K') {
            for (int dr = -1; dr <= 1; ++dr)
                for (int dc = -1; dc <= 1; ++dc)
                    if (dr || dc) consider_move(b, &result, r, c, r + dr, c + dc);
            consider_move(b, &result, r, c, r, c + 2);
            consider_move(b, &result, r, c, r, c - 2);
        } else if (p.type == 'P') {
            int dir = p.color == WHITE_P ? -1 : 1;
            consider_move(b, &result, r, c, r + dir, c);
            consider_move(b, &result, r, c, r + 2 * dir, c);
            consider_move(b, &result, r, c, r + dir, c - 1);
            consider_move(b, &result, r, c, r + dir, c + 1);
        } else {
            for (int i = 0; i < 8; ++i) {
                if (p.type == 'R' && i >= 4) continue;
                if (p.type == 'B' && i < 4) continue;
                for (int step = 1; ; ++step) {
                    int er = r + drs[i] * step, ec = c + dcs[i] * step;
                    if (!in_bounds(er, ec)) break;
                    consider_move(b, &result, r, c, er, ec);
                    if (b->board[er][ec].color != EMPTY) break;
                }
            }
        }
    }
    return result;
}
static void make_move(Board *b, Move m) {
    Piece p = b->board[m.start_x][m.start_y];
    Piece target = b->board[m.end_x][m.end_y];
    bool capture = target.color != EMPTY;
    int next_ep_x = -1, next_ep_y = -1;
    if (p.type == 'P' && abs(m.end_x - m.start_x) == 2) {
        next_ep_x = (m.start_x + m.end_x) / 2; next_ep_y = m.start_y;
    }
    if (p.type == 'P' && m.end_x == b->ep_x && m.end_y == b->ep_y && target.color == EMPTY) {
        b->board[m.start_x][m.end_y] = empty_piece(); capture = true;
    }
    if (target.type == 'R') {
        if (m.end_x == 7 && m.end_y == 0 && target.color == WHITE_P) b->white_rook_a_moved = true;
        if (m.end_x == 7 && m.end_y == 7 && target.color == WHITE_P) b->white_rook_h_moved = true;
        if (m.end_x == 0 && m.end_y == 0 && target.color == BLACK_P) b->black_rook_a_moved = true;
        if (m.end_x == 0 && m.end_y == 7 && target.color == BLACK_P) b->black_rook_h_moved = true;
    }
    if (p.type == 'K' && abs(m.end_y - m.start_y) == 2) {
        if (m.end_y == 6) {
            b->board[m.start_x][5] = b->board[m.start_x][7];
            b->board[m.start_x][7] = empty_piece();
        } else {
            b->board[m.start_x][3] = b->board[m.start_x][0];
            b->board[m.start_x][0] = empty_piece();
        }
    }
    if (p.type == 'K') {
        if (p.color == WHITE_P) b->white_king_moved = true;
        else b->black_king_moved = true;
    }
    if (p.type == 'R') {
        if (m.start_x == 7 && m.start_y == 0 && p.color == WHITE_P) b->white_rook_a_moved = true;
        if (m.start_x == 7 && m.start_y == 7 && p.color == WHITE_P) b->white_rook_h_moved = true;
        if (m.start_x == 0 && m.start_y == 0 && p.color == BLACK_P) b->black_rook_a_moved = true;
        if (m.start_x == 0 && m.start_y == 7 && p.color == BLACK_P) b->black_rook_h_moved = true;
    }
    b->board[m.end_x][m.end_y] = p;
    b->board[m.start_x][m.start_y] = empty_piece();
    if (p.type == 'P' && (m.end_x == 0 || m.end_x == 7))
        b->board[m.end_x][m.end_y].type = m.promotion ? m.promotion : 'Q';
    b->ep_x = next_ep_x; b->ep_y = next_ep_y;
    b->turn = opposite(b->turn);
    if (p.type == 'P' || capture) {
        b->halfmove_clock = 0;
        b->history_size = 0; /* Irreversible move: prior positions cannot repeat. */
    } else ++b->halfmove_clock;
    record_position(b);
}
static bool insufficient_material(const Board *b) {
    int wp = 0, bp = 0, wn = 0, bn = 0, wb = 0, bb = 0;
    int wc = -1, bc = -1;
    for (int r = 0; r < 8; ++r) for (int c = 0; c < 8; ++c) {
        Piece p = b->board[r][c];
        if (p.color == EMPTY || p.type == 'K') continue;
        if (p.type == 'P' || p.type == 'Q' || p.type == 'R') return false;
        if (p.color == WHITE_P) {
            ++wp;
            if (p.type == 'N') ++wn;
            if (p.type == 'B') { ++wb; if (wc == -1) wc = (r + c) % 2; else if (wc != (r+c)%2) wc = -2; }
        } else {
            ++bp;
            if (p.type == 'N') ++bn;
            if (p.type == 'B') { ++bb; if (bc == -1) bc = (r + c) % 2; else if (bc != (r+c)%2) bc = -2; }
        }
    }
    if (!wp && !bp) return true;
    if (wp == 1 && !bp && (wn == 1 || wb == 1)) return true;
    if (bp == 1 && !wp && (bn == 1 || bb == 1)) return true;
    if (wp == 1 && bp == 1 && wb == 1 && bb == 1 && wc >= 0 && wc == bc) return true;
    return false;
}
static bool automatic_draw(const Board *b, const char **reason) {
    if (b->halfmove_clock >= 150) { *reason = "75-move rule"; return true; }
    for (size_t i = 0; i < b->history_size; ++i)
        if (b->history[i].count >= 5) { *reason = "Fivefold repetition"; return true; }
    if (insufficient_material(b)) { *reason = "Insufficient material"; return true; }
    return false;
}
static bool claimable_draw(const Board *b, const char **reason) {
    char key[KEY_SIZE];
    if (b->halfmove_clock >= 100) { *reason = "50-move rule"; return true; }
    position_key(b, key);
    for (size_t i = 0; i < b->history_size; ++i)
        if (strcmp(b->history[i].key, key) == 0 && b->history[i].count >= 3) {
            *reason = "Threefold repetition"; return true;
        }
    return false;
}
static bool game_over(const Board *b, bool *stalemate, const char **reason) {
    MoveList moves = legal_moves(b);
    bool no_moves = moves.count == 0;
    free(moves.items);
    *stalemate = false;
    if (no_moves) { *stalemate = !is_in_check(b, b->turn); return true; }
    return automatic_draw(b, reason);
}
static bool parse_square(const char *s, int *r, int *c) {
    char file = (char)tolower((unsigned char)s[0]), rank = s[1];
    if (file < 'a' || file > 'h' || rank < '1' || rank > '8') return false;
    *c = file - 'a'; *r = 8 - (rank - '0'); return true;
}
int main(void) {
    Board game = {0};
    char input[128];
#ifdef _WIN32
    /* Enable ANSI colors on compatible modern Windows terminals. */
    (void)system("chcp 65001 > nul");
#endif
    reset_board(&game);
    puts("=== 2-Player C99 Chess for PowerShell / Terminal ===");
    for (;;) {
        bool stalemate;
        const char *draw_reason = NULL;
        display(&game);
        if (game_over(&game, &stalemate, &draw_reason)) {
            if (draw_reason) printf("Game Over! Draw by %s.\n", draw_reason);
            else if (stalemate) puts("Game Over! It's a Stalemate.");
            else printf("Checkmate! %s wins!\n", game.turn == WHITE_P ? "Black" : "White");
            break;
        }
        if (is_in_check(&game, game.turn)) printf(CYAN "WARNING: Your King is in check!\n" RESET);
        puts("Format: e2e4 or e7e8q | 'claim' draw | 'restart' | 'quit'.");
        if (claimable_draw(&game, &draw_reason))
            printf(CYAN "You may claim a draw (%s) by typing 'claim'.\n" RESET, draw_reason);
        printf("Turn -> %s Player's Move: ", game.turn == WHITE_P ? "White" : "Black");
        fflush(stdout);
        if (!fgets(input, sizeof(input), stdin)) break;
        size_t len = strlen(input);
        if (len && input[len - 1] == '\n') input[--len] = '\0';
        else if (len == sizeof(input) - 1) {
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF) { /* discard long line */ }
            puts(RED "Input too long." RESET);
            continue;
        }
        /* Remove an optional Windows carriage return. */
        if (len && input[len - 1] == '\r') input[--len] = '\0';
        if (!strcmp(input, "quit") || !strcmp(input, "exit")) break;
        if (!strcmp(input, "restart")) { reset_board(&game); puts("Game restarted."); continue; }
        if (!strcmp(input, "claim")) {
            if (claimable_draw(&game, &draw_reason)) {
                printf("Game Over! Draw claimed by %s.\n", draw_reason); break;
            }
            puts(RED "No draw claim is currently available." RESET); continue;
        }
        if (len != 4 && len != 5) { puts(RED "Invalid format! Example: e2e4" RESET); continue; }
        Move m = {0, 0, 0, 0, '\0'};
        if (!parse_square(input, &m.start_x, &m.start_y) ||
            !parse_square(input + 2, &m.end_x, &m.end_y)) {
            puts(RED "Invalid squares! Use columns a-h and rows 1-8." RESET); continue;
        }
        if (len == 5) {
            m.promotion = (char)toupper((unsigned char)input[4]);
            if (!strchr("QRBN", m.promotion)) {
                puts(RED "Invalid promotion piece! Use Q, R, B, or N." RESET); continue;
            }
        }
        if (legal_move(&game, m)) make_move(&game, m);
        else puts(RED "Illegal move! Try again." RESET);
    }
    free(game.history);
    return 0;
}
