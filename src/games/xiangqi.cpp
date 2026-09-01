#include <dev/console.h>
#include <dev/keyboard.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../common/tcm_util.h"

enum { FILES = 9, RANKS = 10, RED = 0, BLACK = 1 };

enum {
  P_PAWN = 0,
  P_CANNON = 1,
  P_HORSE = 2,
  P_ROOK = 3,
  P_ELEPHANT = 4,
  P_ADVISOR = 5,
  P_KING = 6,
};

static const char PIECE_LETTER[7] = {'P', 'C', 'H', 'R', 'E', 'A', 'K'};
static const int PIECE_VALUE[7] = {100, 450, 400, 900, 200, 200, 0};
static const int MATE = 1000000;

static uint8_t board[RANKS][FILES];
static int king_f[2], king_r[2];

static inline int color_of(uint8_t code) { return (code - 1) >> 3; }
static inline int type_of(uint8_t code) { return (code - 1) & 7; }
static inline uint8_t mk(int color, int type) { return 1 + color * 8 + type; }

struct Move {
  int8_t from_f, from_r, to_f, to_r;
};

static void init_board() {
  memset(board, 0, sizeof(board));
  const uint8_t back[9] = {
      mk(BLACK, P_ROOK),   mk(BLACK, P_HORSE),    mk(BLACK, P_ELEPHANT),
      mk(BLACK, P_ADVISOR), mk(BLACK, P_KING),     mk(BLACK, P_ADVISOR),
      mk(BLACK, P_ELEPHANT), mk(BLACK, P_HORSE),   mk(BLACK, P_ROOK),
  };
  for (int f = 0; f < FILES; ++f) {
    board[0][f] = back[f];
    board[9][f] = back[f] - 8;
  }
  board[2][1] = mk(BLACK, P_CANNON);
  board[2][7] = mk(BLACK, P_CANNON);
  board[7][1] = mk(RED, P_CANNON);
  board[7][7] = mk(RED, P_CANNON);
  for (int f = 0; f < FILES; f += 2) {
    board[3][f] = mk(BLACK, P_PAWN);
    board[6][f] = mk(RED, P_PAWN);
  }
  king_f[BLACK] = 4;
  king_r[BLACK] = 0;
  king_f[RED] = 4;
  king_r[RED] = 9;
}

static inline void make_move(const Move &mv, uint8_t moving, uint8_t captured) {
  board[mv.to_r][mv.to_f] = moving;
  board[mv.from_r][mv.from_f] = 0;
  if (type_of(moving) == P_KING) {
    king_f[color_of(moving)] = mv.to_f;
    king_r[color_of(moving)] = mv.to_r;
  }
}

static inline void unmake_move(const Move &mv, uint8_t moving, uint8_t captured) {
  board[mv.from_r][mv.from_f] = moving;
  board[mv.to_r][mv.to_f] = captured;
  if (type_of(moving) == P_KING) {
    king_f[color_of(moving)] = mv.from_f;
    king_r[color_of(moving)] = mv.from_r;
  }
}

static bool attacked(int ff, int rr, int by) {
  uint8_t here = board[rr][ff];
  if (here && type_of(here) == P_KING) {
    for (int r = rr + 1; r < RANKS; ++r) {
      uint8_t c = board[r][ff];
      if (c) {
        if (color_of(c) == by && type_of(c) == P_KING)
          return true;
        break;
      }
    }
    for (int r = rr - 1; r >= 0; --r) {
      uint8_t c = board[r][ff];
      if (c) {
        if (color_of(c) == by && type_of(c) == P_KING)
          return true;
        break;
      }
    }
  }
  for (int r = 0; r < RANKS; ++r)
    for (int f = 0; f < FILES; ++f) {
      uint8_t c = board[r][f];
      if (!c || color_of(c) != by)
        continue;
      int t = type_of(c);
      if (t == P_ROOK || t == P_CANNON) {
        if (f == ff || r == rr) {
          int lo, hi, blocked = 0, screen = 0;
          if (f == ff) {
            lo = r < rr ? r : rr;
            hi = r < rr ? rr : r;
            for (int k = lo + 1; k < hi; ++k)
              if (board[k][f])
                blocked++;
          } else {
            lo = f < ff ? f : ff;
            hi = f < ff ? ff : f;
            for (int k = lo + 1; k < hi; ++k)
              if (board[r][k])
                blocked++;
          }
          if (t == P_ROOK) {
            if (blocked == 0)
              return true;
          } else {
            screen = blocked;
            if (screen == 1)
              return true;
          }
        }
      } else if (t == P_HORSE) {
        int df = ff - f, dr = rr - r;
        if ((df == 1 && dr == 2) || (df == 2 && dr == 1) || (df == -1 && dr == -2) ||
            (df == -2 && dr == -1) || (df == 1 && dr == -2) || (df == -1 && dr == 2) ||
            (df == 2 && dr == -1) || (df == -2 && dr == 1)) {
          int lf = (df == 2 || df == -2) ? f + df / 2 : f;
          int lr = (dr == 2 || dr == -2) ? r + dr / 2 : r;
          if (!board[lr][lf])
            return true;
        }
      } else if (t == P_PAWN) {
        if (by == RED) {
          if (f == ff && r == rr + 1)
            return true;
          if (r <= 4 && abs(f - ff) == 1 && r == rr)
            return true;
        } else {
          if (f == ff && r == rr - 1)
            return true;
          if (r >= 5 && abs(f - ff) == 1 && r == rr)
            return true;
        }
      } else if (t == P_KING) {
        if (abs(f - ff) + abs(r - rr) == 1)
          return true;
      }
    }
  return false;
}

static void gen_pseudo_moves(int side, Move *out, int &n) {
  for (int r = 0; r < RANKS; ++r)
    for (int f = 0; f < FILES; ++f) {
      uint8_t c = board[r][f];
      if (!c || color_of(c) != side)
        continue;
      int t = type_of(c);
      if (t == P_PAWN) {
        int dir = side == RED ? -1 : 1;
        int tr = r + dir;
        if (tr >= 0 && tr < RANKS) {
          uint8_t tgt = board[tr][f];
          if (!tgt || color_of(tgt) != side)
            out[n++] = {(int8_t)f, (int8_t)r, (int8_t)f, (int8_t)tr};
        }
        bool crossed = side == RED ? (r <= 4) : (r >= 5);
        if (crossed) {
          for (int dx = -1; dx <= 1; dx += 2) {
            int tf = f + dx;
            if (tf < 0 || tf >= FILES)
              continue;
            uint8_t tgt = board[r][tf];
            if (!tgt || color_of(tgt) != side)
              out[n++] = {(int8_t)f, (int8_t)r, (int8_t)tf, (int8_t)r};
          }
        }
      } else if (t == P_CANNON || t == P_ROOK) {
        static const int D4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (int d = 0; d < 4; ++d) {
          int ff = f + D4[d][0], rr = r + D4[d][1];
          bool screened = false;
          while (ff >= 0 && ff < FILES && rr >= 0 && rr < RANKS) {
            uint8_t tgt = board[rr][ff];
            if (!tgt) {
              if (t == P_ROOK || !screened)
                out[n++] = {(int8_t)f, (int8_t)r, (int8_t)ff, (int8_t)rr};
            } else if (!screened) {
              if (t == P_ROOK) {
                if (color_of(tgt) != side)
                  out[n++] = {(int8_t)f, (int8_t)r, (int8_t)ff, (int8_t)rr};
                break;
              }
              screened = true;
            } else {
              if (color_of(tgt) != side)
                out[n++] = {(int8_t)f, (int8_t)r, (int8_t)ff, (int8_t)rr};
              break;
            }
            ff += D4[d][0];
            rr += D4[d][1];
          }
        }
      } else if (t == P_HORSE) {
        static const int HO[8][2] = {{1, 2}, {2, 1}, {2, -1}, {1, -2},
                                     {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};
        for (int d = 0; d < 8; ++d) {
          int ff = f + HO[d][0], rr = r + HO[d][1];
          if (ff < 0 || ff >= FILES || rr < 0 || rr >= RANKS)
            continue;
          int lf = (HO[d][0] == 2 || HO[d][0] == -2) ? f + HO[d][0] / 2 : f;
          int lr = (HO[d][1] == 2 || HO[d][1] == -2) ? r + HO[d][1] / 2 : r;
          if (board[lr][lf])
            continue;
          uint8_t tgt = board[rr][ff];
          if (!tgt || color_of(tgt) != side)
            out[n++] = {(int8_t)f, (int8_t)r, (int8_t)ff, (int8_t)rr};
        }
      } else if (t == P_ELEPHANT) {
        static const int EL[4][2] = {{2, 2}, {2, -2}, {-2, 2}, {-2, -2}};
        for (int d = 0; d < 4; ++d) {
          int ff = f + EL[d][0], rr = r + EL[d][1];
          if (ff < 0 || ff >= FILES || rr < 0 || rr >= RANKS)
            continue;
          if (side == RED && rr < 5)
            continue;
          if (side == BLACK && rr > 4)
            continue;
          if (board[r + EL[d][1] / 2][f + EL[d][0] / 2])
            continue;
          uint8_t tgt = board[rr][ff];
          if (!tgt || color_of(tgt) != side)
            out[n++] = {(int8_t)f, (int8_t)r, (int8_t)ff, (int8_t)rr};
        }
      } else if (t == P_ADVISOR) {
        static const int AD[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
        for (int d = 0; d < 4; ++d) {
          int ff = f + AD[d][0], rr = r + AD[d][1];
          if (ff < 3 || ff > 5)
            continue;
          if (side == RED) {
            if (rr < 7 || rr > 9)
              continue;
          } else {
            if (rr < 0 || rr > 2)
              continue;
          }
          uint8_t tgt = board[rr][ff];
          if (!tgt || color_of(tgt) != side)
            out[n++] = {(int8_t)f, (int8_t)r, (int8_t)ff, (int8_t)rr};
        }
      } else if (t == P_KING) {
        static const int KI[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (int d = 0; d < 4; ++d) {
          int ff = f + KI[d][0], rr = r + KI[d][1];
          if (ff < 3 || ff > 5)
            continue;
          if (side == RED) {
            if (rr < 7 || rr > 9)
              continue;
          } else {
            if (rr < 0 || rr > 2)
              continue;
          }
          uint8_t tgt = board[rr][ff];
          if (!tgt || color_of(tgt) != side)
            out[n++] = {(int8_t)f, (int8_t)r, (int8_t)ff, (int8_t)rr};
        }
      }
    }
}

static int gen_legal_moves(int side, Move *out) {
  Move tmp[192];
  int n = 0;
  gen_pseudo_moves(side, tmp, n);
  int m = 0;
  for (int i = 0; i < n; ++i) {
    uint8_t moving = board[tmp[i].from_r][tmp[i].from_f];
    uint8_t captured = board[tmp[i].to_r][tmp[i].to_f];
    make_move(tmp[i], moving, captured);
    bool ok = !attacked(king_f[side], king_r[side], 1 - side);
    unmake_move(tmp[i], moving, captured);
    if (ok)
      out[m++] = tmp[i];
  }
  return m;
}

static int move_score(const Move &mv) {
  uint8_t moving = board[mv.from_r][mv.from_f];
  uint8_t captured = board[mv.to_r][mv.to_f];
  if (captured)
    return PIECE_VALUE[type_of(captured)] * 16 - PIECE_VALUE[type_of(moving)] + 1000000;
  return 0;
}

static void order_moves(Move *moves, int n) {
  for (int i = 1; i < n; ++i) {
    Move mv = moves[i];
    int sc = move_score(mv);
    int j = i - 1;
    while (j >= 0 && move_score(moves[j]) < sc) {
      moves[j + 1] = moves[j];
      --j;
    }
    moves[j + 1] = mv;
  }
}

static int evaluate(int side) {
  int score = 0;
  for (int r = 0; r < RANKS; ++r)
    for (int f = 0; f < FILES; ++f) {
      uint8_t c = board[r][f];
      if (!c)
        continue;
      int v = PIECE_VALUE[type_of(c)];
      if (type_of(c) == P_PAWN) {
        int adv = color_of(c) == RED ? (6 - r) : (r - 3);
        v += adv * 12;
      }
      score += color_of(c) == RED ? v : -v;
    }
  return side == RED ? score : -score;
}

static int search(int depth, int alpha, int beta, int side, int ply) {
  Move moves[192];
  int n = gen_legal_moves(side, moves);
  if (n == 0)
    return -(MATE - ply);
  if (depth == 0)
    return evaluate(side);
  order_moves(moves, n);
  int best = -MATE;
  for (int i = 0; i < n; ++i) {
    uint8_t moving = board[moves[i].from_r][moves[i].from_f];
    uint8_t captured = board[moves[i].to_r][moves[i].to_f];
    make_move(moves[i], moving, captured);
    int score = -search(depth - 1, -beta, -alpha, 1 - side, ply + 1);
    unmake_move(moves[i], moving, captured);
    if (score > best)
      best = score;
    if (score > alpha)
      alpha = score;
    if (alpha >= beta)
      break;
  }
  return best;
}

static void ai_move(int depth, Move &best) {
  Move moves[192];
  int n = gen_legal_moves(BLACK, moves);
  order_moves(moves, n);
  int alpha = -MATE - 1;
  best = moves[0];
  for (int i = 0; i < n; ++i) {
    uint8_t moving = board[moves[i].from_r][moves[i].from_f];
    uint8_t captured = board[moves[i].to_r][moves[i].to_f];
    make_move(moves[i], moving, captured);
    int s = -search(depth - 1, -MATE - 1, -alpha, RED, 1);
    unmake_move(moves[i], moving, captured);
    if (s > alpha) {
      alpha = s;
      best = moves[i];
    }
  }
}

static uint32_t *vram;
static const int X0 = 72, Y0 = 10, CELL = 22;

static inline uint32_t rgb888(uint32_t r, uint32_t g, uint32_t b) {
  return (b << 16) | (g << 8) | r;
}

static void fill_rect(int x, int y, int w, int h, uint32_t color) {
  for (int yy = y; yy < y + h; ++yy)
    for (int xx = x; xx < x + w; ++xx)
      vram[yy * 320 + xx] = color;
}

static void hline(int y, int x0, int x1, uint32_t color) {
  for (int x = x0; x <= x1; ++x)
    vram[y * 320 + x] = color;
}

static void vline(int x, int y0, int y1, uint32_t color) {
  for (int y = y0; y <= y1; ++y)
    vram[y * 320 + x] = color;
}

static const uint8_t FONT5x7[][7] = {
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, // A
    {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}, // B
    {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}, // C
    {0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C}, // D
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}, // E
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}, // F
    {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}, // G
    {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, // H
    {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}, // I
    {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C}, // J
    {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}, // K
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}, // L
    {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}, // M
    {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}, // N
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, // O
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}, // P
    {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}, // Q
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}, // R
    {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}, // S
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, // T
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, // U
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}, // V
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11}, // W
    {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}, // X
    {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}, // Y
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}, // Z
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, // 0
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}, // 1
    {0x0E, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1F}, // 2
    {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E}, // 3
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, // 4
    {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E}, // 5
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, // 6
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}, // 7
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, // 8
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}, // 9
    {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04}, // !
    {0x0E, 0x11, 0x01, 0x06, 0x04, 0x00, 0x04}, // ?
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C}, // .
    {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}, // -
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // space
};

static int char_index(char ch) {
  if (ch >= 'A' && ch <= 'Z')
    return ch - 'A';
  if (ch >= '0' && ch <= '9')
    return 26 + ch - '0';
  if (ch == '!')
    return 36;
  if (ch == '?')
    return 37;
  if (ch == '.')
    return 38;
  if (ch == '-')
    return 39;
  return 40;
}

static void draw_char(int x, int y, char ch, uint32_t color) {
  const uint8_t *g = FONT5x7[char_index(ch)];
  for (int row = 0; row < 7; ++row) {
    uint8_t bits = g[row];
    for (int col = 0; col < 5; ++col)
      if (bits & (0x10 >> col))
        vram[(y + row) * 320 + (x + col)] = color;
  }
}

static void draw_text(int x, int y, const char *s, uint32_t color) {
  while (*s) {
    draw_char(x, y, *s, color);
    x += 6;
    ++s;
  }
}

static void draw_piece(int f, int r) {
  uint8_t c = board[r][f];
  if (!c)
    return;
  int cx = X0 + f * CELL;
  int cy = Y0 + r * CELL;
  uint32_t col = color_of(c) == RED ? rgb888(215, 85, 55) : rgb888(35, 35, 40);
  for (int dy = -9; dy <= 9; ++dy)
    for (int dx = -9; dx <= 9; ++dx)
      if (dx * dx + dy * dy <= 81)
        vram[(cy + dy) * 320 + (cx + dx)] = col;
  draw_char(cx - 2, cy - 3, PIECE_LETTER[type_of(c)], rgb888(255, 255, 255));
}

static void ring(int f, int r, uint32_t col) {
  int x = X0 + f * CELL - 9, y = Y0 + r * CELL - 9;
  for (int i = 0; i < 18; ++i) {
    vram[y * 320 + x + i] = col;
    vram[(y + 1) * 320 + x + i] = col;
    vram[(y + 17) * 320 + x + i] = col;
    vram[(y + 16) * 320 + x + i] = col;
    vram[(y + i) * 320 + x] = col;
    vram[(y + i) * 320 + x + 1] = col;
    vram[(y + i) * 320 + x + 17] = col;
    vram[(y + i) * 320 + x + 16] = col;
  }
}

static int cx, cy;
static bool selected;
static int sel_f, sel_r;
static Move legals[192];
static int n_legals;
static bool game_over, player_won, ai_thinking, in_check;
static int depth;

static void draw_board() {
  const uint32_t bg = rgb888(42, 36, 28);
  const uint32_t line = rgb888(150, 130, 100);
  fill_rect(0, 0, 320, 240, bg);

  for (int r = 0; r <= RANKS - 1; ++r)
    hline(Y0 + r * CELL, X0, X0 + 8 * CELL, line);
  for (int f = 0; f < FILES; ++f) {
    if (f == 0 || f == FILES - 1)
      vline(X0 + f * CELL, Y0, Y0 + 9 * CELL, line);
    else {
      vline(X0 + f * CELL, Y0, Y0 + 4 * CELL, line);
      vline(X0 + f * CELL, Y0 + 5 * CELL, Y0 + 9 * CELL, line);
    }
  }
  for (int i = 0; i <= 2 * CELL; ++i) {
    vram[(Y0 + i) * 320 + X0 + 3 * CELL + i] = line;
    vram[(Y0 + i) * 320 + X0 + 5 * CELL - i] = line;
    vram[(Y0 + 9 * CELL - i) * 320 + X0 + 3 * CELL + i] = line;
    vram[(Y0 + 9 * CELL - i) * 320 + X0 + 5 * CELL - i] = line;
  }

  for (int r = 0; r < RANKS; ++r)
    for (int f = 0; f < FILES; ++f)
      draw_piece(f, r);

  if (selected) {
    for (int i = 0; i < n_legals; ++i)
      fill_rect(X0 + legals[i].to_f * CELL - 3, Y0 + legals[i].to_r * CELL - 3, 6, 6,
                rgb888(80, 255, 120));
    ring(sel_f, sel_r, rgb888(120, 220, 255));
  }
  ring(cx, cy, rgb888(255, 255, 120));

  fill_rect(0, 230, 320, 10, rgb888(20, 18, 14));
  const char *s;
  if (game_over)
    s = player_won ? "YOU WIN!  R:RESTART" : "YOU LOSE! R:RESTART";
  else if (ai_thinking)
    s = "AI THINKING...";
  else if (in_check)
    s = "CHECK! YOUR TURN";
  else
    s = "YOUR TURN";
  draw_text(4, 232, s, rgb888(255, 255, 255));
  draw_text(290, 232, "D:", rgb888(255, 255, 255));
  draw_char(308, 232, (char)('0' + depth), rgb888(255, 255, 255));
}

static void new_game() {
  init_board();
  game_over = false;
  player_won = false;
  in_check = false;
  ai_thinking = false;
  selected = false;
  n_legals = 0;
  cx = 4;
  cy = 8;
  printf("New game. AI depth: %d\n", depth);
}

int main() {
  tcm_ascii_console_init();
  tcm_ascii_console_clear();
  printf("===== Chinese Chess vs AI =====\n");
  printf("You are RED. Arrows: move cursor\n");
  printf("Enter: select piece / move\n");
  printf("Backspace: cancel  R: restart  Q: quit\n");
  printf("1-4: AI depth (default 3)\n");
  printf("Press any key to start...\n");
  while (tcm_keyboard_get_code() == 0) {
  }
  tcm_ascii_console_clear();

  vram = (uint32_t *)tcm_pixel_console_init(CONSOLE_MODE_PIXEL_32, 320);
  tcm_pixel_console_clear();

  depth = 3;
  new_game();

  uint32_t last_code = 0;
  bool player_moved = false;

  draw_board();

  while (true) {
    uint32_t code = tcm_keyboard_get_code();
    bool fresh = (code != 0 && code != last_code);
    last_code = code;

    if (code == 'q' || code == 'Q')
      break;

    if (fresh) {
      if (code == 'r' || code == 'R') {
        new_game();
      } else if (code >= '1' && code <= '4') {
        depth = code - '0';
        printf("AI depth: %d\n", depth);
      } else if (!game_over) {
        if (code == __TCM_KEY_CODE_UP && cy > 0)
          cy--;
        else if (code == __TCM_KEY_CODE_DOWN && cy < RANKS - 1)
          cy++;
        else if (code == __TCM_KEY_CODE_LEFT && cx > 0)
          cx--;
        else if (code == __TCM_KEY_CODE_RIGHT && cx < FILES - 1)
          cx++;
        else if (code == __TCM_KEY_CODE_ENTER) {
          if (!selected) {
            uint8_t c = board[cy][cx];
            if (c && color_of(c) == RED) {
              selected = true;
              sel_f = cx;
              sel_r = cy;
              int n = gen_legal_moves(RED, legals);
              n_legals = 0;
              for (int i = 0; i < n; ++i)
                if (legals[i].from_f == sel_f && legals[i].from_r == sel_r)
                  legals[n_legals++] = legals[i];
            }
          } else {
            if (cx == sel_f && cy == sel_r) {
              selected = false;
              n_legals = 0;
            } else {
              bool is_legal = false;
              for (int i = 0; i < n_legals; ++i)
                if (legals[i].to_f == cx && legals[i].to_r == cy) {
                  is_legal = true;
                  break;
                }
              if (is_legal) {
                Move mv = {(int8_t)sel_f, (int8_t)sel_r, (int8_t)cx, (int8_t)cy};
                uint8_t moving = board[sel_r][sel_f];
                make_move(mv, moving, board[cy][cx]);
                printf("RED %c %c%d->%c%d\n", PIECE_LETTER[type_of(moving)], 'a' + sel_f,
                       10 - sel_r, 'a' + cx, 10 - cy);
                selected = false;
                n_legals = 0;
                in_check = false;
                player_moved = true;
              } else if (board[cy][cx] && color_of(board[cy][cx]) == RED) {
                sel_f = cx;
                sel_r = cy;
                int n = gen_legal_moves(RED, legals);
                n_legals = 0;
                for (int i = 0; i < n; ++i)
                  if (legals[i].from_f == sel_f && legals[i].from_r == sel_r)
                    legals[n_legals++] = legals[i];
              } else {
                selected = false;
                n_legals = 0;
              }
            }
          }
        } else if (code == __TCM_KEY_CODE_BACKSPACE || code == 'x' || code == 'X') {
          selected = false;
          n_legals = 0;
        }
      }
    }

    if (player_moved) {
      player_moved = false;
      Move tmp[192];
      if (gen_legal_moves(BLACK, tmp) == 0) {
        game_over = true;
        player_won = true;
        printf("You win!\n");
      } else {
        ai_thinking = true;
        draw_board();
        Move best;
        ai_move(depth, best);
        ai_thinking = false;
        uint8_t moving = board[best.from_r][best.from_f];
        make_move(best, moving, board[best.to_r][best.to_f]);
        printf("BLK %c %c%d->%c%d\n", PIECE_LETTER[type_of(moving)], 'a' + best.from_f,
               10 - best.from_r, 'a' + best.to_f, 10 - best.to_r);
        if (gen_legal_moves(RED, tmp) == 0) {
          game_over = true;
          player_won = false;
          printf("You lose.\n");
        } else {
          in_check = attacked(king_f[RED], king_r[RED], BLACK);
          if (in_check)
            printf("CHECK!\n");
        }
      }
    }

    if (fresh)
      draw_board();
    tcm_delay_ms(16);
  }

  tcm_pixel_console_clear();
  tcm_ascii_console_init();
  tcm_ascii_console_clear();
  printf("Thanks for playing Chinese Chess!\n");
  return 0;
}