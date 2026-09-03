#include "../common/tcm_util.h"
#include "xiangqi_bg.h"
#include "xiangqi_book.h"
#include "xiangqi_font32.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dev/console.h>
#include <dev/keyboard.h>

enum { FILES = 9, RANKS = 10, RED = 0, BLACK = 1 };

// Display resolution: doubled from the original 320x240 to 640x480.
// All drawing constants are scaled by SCALE.
enum { SCREEN_W = 640, SCREEN_H = 480, SCALE = 2 };

enum {
  P_PAWN = 0,     // 兵/卒
  P_CANNON = 1,   // 炮/砲
  P_HORSE = 2,    // 马/傌
  P_ROOK = 3,     // 车/俥
  P_ELEPHANT = 4, // 相/象
  P_ADVISOR = 5,  // 仕/士
  P_KING = 6,     // 帅/将
};

static const char PIECE_LETTER[7] = {'P', 'C', 'H', 'R', 'E', 'A', 'K'};
static const int PIECE_VALUE[7] = {100, 450, 400, 900, 200, 200, 0};
static const int MATE = 1000000;
static uint8_t board[RANKS][FILES];
static int king_f[2], king_r[2];
static uint8_t piece_code[2][16];
static uint8_t piece_f[2][16];
static uint8_t piece_r[2][16];
static int piece_n[2];

static inline int color_of(uint8_t code) { return (code - 1) >> 3; }
static inline int type_of(uint8_t code) { return (code - 1) & 7; }
static inline uint8_t mk(int color, int type) { return 1 + color * 8 + type; }

struct Move {
  int8_t from_f, from_r, to_f, to_r;
};

static uint64_t Z[7][2][RANKS * FILES];
static bool z_ready = false;
static uint64_t g_hash = 0;

static void zob_init() {
  if (z_ready)
    return;
  uint32_t s1 = 0x12345678u, s2 = 0x9abcdef0u;
  for (int t = 0; t < 7; ++t)
    for (int col = 0; col < 2; ++col)
      for (int sq = 0; sq < RANKS * FILES; ++sq) {
        s1 = s1 * 1664525u + 1013904223u;
        s2 = s2 * 22695477u + 1u;
        Z[t][col][sq] = ((uint64_t)s1 << 32) | s2;
      }
  z_ready = true;
}

static void init_board() {
  memset(board, 0, sizeof(board));
  g_hash = 0;
  zob_init();
  const uint8_t back[9] = {
      mk(BLACK, P_ROOK),    mk(BLACK, P_HORSE),    mk(BLACK, P_ELEPHANT), mk(BLACK, P_ADVISOR), mk(BLACK, P_KING),
      mk(BLACK, P_ADVISOR), mk(BLACK, P_ELEPHANT), mk(BLACK, P_HORSE),    mk(BLACK, P_ROOK),
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

  piece_n[RED] = piece_n[BLACK] = 0;
  for (int r = 0; r < RANKS; ++r)
    for (int f = 0; f < FILES; ++f) {
      uint8_t c = board[r][f];
      if (c) {
        int col = color_of(c);
        int k = piece_n[col]++;
        piece_code[col][k] = c;
        piece_f[col][k] = (uint8_t)f;
        piece_r[col][k] = (uint8_t)r;
        g_hash ^= Z[type_of(c)][col][f + r * FILES];
      }
    }
}

static inline void piece_remove_sq(int col, int sq) {
  for (int i = 0; i < piece_n[col]; ++i) {
    if (piece_f[col][i] + piece_r[col][i] * FILES == sq) {
      piece_n[col]--;
      if (i < piece_n[col]) {
        piece_code[col][i] = piece_code[col][piece_n[col]];
        piece_f[col][i] = piece_f[col][piece_n[col]];
        piece_r[col][i] = piece_r[col][piece_n[col]];
      }
      return;
    }
  }
}

static inline void piece_add(int col, uint8_t code, int f, int r) {
  int k = piece_n[col]++;
  piece_code[col][k] = code;
  piece_f[col][k] = (uint8_t)f;
  piece_r[col][k] = (uint8_t)r;
}

static inline void make_move(const Move &mv, uint8_t moving, uint8_t captured) {
  int sq_f = mv.from_f + mv.from_r * FILES;
  int sq_t = mv.to_f + mv.to_r * FILES;
  g_hash ^= Z[type_of(moving)][color_of(moving)][sq_f];
  g_hash ^= Z[type_of(moving)][color_of(moving)][sq_t];
  if (captured)
    g_hash ^= Z[type_of(captured)][color_of(captured)][sq_t];

  int col = color_of(moving);
  piece_remove_sq(col, sq_f);
  piece_add(col, moving, mv.to_f, mv.to_r);
  if (captured) {
    piece_remove_sq(1 - col, sq_t);
    if (type_of(captured) == P_KING) {
      king_f[1 - col] = -1;
      king_r[1 - col] = -1;
    }
  }
  board[mv.to_r][mv.to_f] = moving;
  board[mv.from_r][mv.from_f] = 0;
  if (type_of(moving) == P_KING) {
    king_f[col] = mv.to_f;
    king_r[col] = mv.to_r;
  }
}

static inline void unmake_move(const Move &mv, uint8_t moving, uint8_t captured) {
  int sq_f = mv.from_f + mv.from_r * FILES;
  int sq_t = mv.to_f + mv.to_r * FILES;
  g_hash ^= Z[type_of(moving)][color_of(moving)][sq_f];
  g_hash ^= Z[type_of(moving)][color_of(moving)][sq_t];
  if (captured)
    g_hash ^= Z[type_of(captured)][color_of(captured)][sq_t];
  int col = color_of(moving);
  piece_remove_sq(col, sq_t);
  piece_add(col, moving, mv.from_f, mv.from_r);
  if (captured) {
    piece_add(1 - col, captured, mv.to_f, mv.to_r);
    if (type_of(captured) == P_KING) {
      king_f[1 - col] = mv.to_f;
      king_r[1 - col] = mv.to_r;
    }
  }
  board[mv.from_r][mv.from_f] = moving;
  board[mv.to_r][mv.to_f] = captured;
  if (type_of(moving) == P_KING) {
    king_f[col] = mv.from_f;
    king_r[col] = mv.from_r;
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
  for (int i = 0; i < piece_n[by]; ++i) {
    uint8_t c = piece_code[by][i];
    int f = piece_f[by][i];
    int r = piece_r[by][i];
    int t = type_of(c);
    if (t == P_ROOK || t == P_CANNON) {
      if (f == ff || r == rr) {
        int blocked = 0;
        if (f == ff) {
          int lo = r < rr ? r : rr;
          int hi = r < rr ? rr : r;
          for (int k = lo + 1; k < hi; ++k)
            if (board[k][f])
              blocked++;
        } else {
          int lo = f < ff ? f : ff;
          int hi = f < ff ? ff : f;
          for (int k = lo + 1; k < hi; ++k)
            if (board[r][k])
              blocked++;
        }
        if (t == P_ROOK) {
          if (blocked == 0)
            return true;
        } else if (blocked == 1) {
          return true;
        }
      }
    } else if (t == P_HORSE) {
      int df = ff - f, dr = rr - r;
      if ((abs(df) == 1 && abs(dr) == 2) || (abs(df) == 2 && abs(dr) == 1)) {
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
  static const int D4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
  static const int HO[8][2] = {{1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};
  static const int EL[4][2] = {{2, 2}, {2, -2}, {-2, 2}, {-2, -2}};
  static const int AD[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
  static const int KI[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

  for (int i = 0; i < piece_n[side]; ++i) {
    uint8_t c = piece_code[side][i];
    int f = piece_f[side][i];
    int r = piece_r[side][i];
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
      continue;
    }
    if (t == P_CANNON || t == P_ROOK) {
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
      continue;
    }
    if (t == P_HORSE) {
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
      continue;
    }
    if (t == P_ELEPHANT) {
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
      continue;
    }
    if (t == P_ADVISOR) {
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
      continue;
    }
    if (t == P_KING) {
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
      continue;
    }
  }
}

static int gen_legal_moves(int side, Move *out) {
  Move tmp[128];
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

// Two-bucket TT: same total memory as the previous single bucket, but keeps
// two entries per slot so deep entries are less likely to be flushed by
// shallow collisions.
enum { TT_BITS = 17, TT_SIZE = 1 << TT_BITS, TT_MASK = TT_SIZE - 1 };

enum { TT_EXACT = 0, TT_LOWER = 1, TT_UPPER = 2 };

static uint16_t hist[16][RANKS * FILES];

enum { MAX_PLY = 32 };

static uint16_t killer[2][MAX_PLY][2];
static int killer_used[2][MAX_PLY];
static int g_ply;
static int g_cur_side;

struct TTEntry {
  uint64_t key;
  int32_t value;
  uint16_t move;
  uint16_t df;
};

static TTEntry tt[TT_SIZE][2];

static inline uint16_t pack_move(const Move &m) {
  return (uint16_t)(m.from_f | (m.from_r << 4) | (m.to_f << 8) | (m.to_r << 12));
}

static int move_score(const Move &mv, uint16_t pref) {
  uint8_t moving = board[mv.from_r][mv.from_f];
  uint8_t captured = board[mv.to_r][mv.to_f];
  uint16_t mp = pack_move(mv);
  int s = 0;

  if (captured) {
    int mvvlva = PIECE_VALUE[type_of(captured)] * 64 - PIECE_VALUE[type_of(moving)];
    s = 1000000 + mvvlva;
    if (mp == pref)
      s += 500000;
    return s;
  }
  if (mp == pref)
    s += 2000000;
  int p = g_ply;
  if (p < MAX_PLY) {
    int sd = g_cur_side;
    if (killer_used[sd][p] >= 1 && mp == killer[sd][p][0])
      s += 800000;
    if (killer_used[sd][p] >= 2 && mp == killer[sd][p][1])
      s += 700000;
  }
  s += hist[(moving - 1)][mv.to_f + mv.to_r * FILES];
  return s;
}

static void order_moves(Move *moves, int n, uint16_t pref) {
  int sc[128];
  for (int i = 0; i < n; ++i)
    sc[i] = move_score(moves[i], pref);
  for (int i = 1; i < n; ++i) {
    Move mv = moves[i];
    int sv = sc[i];
    int j = i - 1;
    while (j >= 0 && sc[j] < sv) {
      moves[j + 1] = moves[j];
      sc[j + 1] = sc[j];
      --j;
    }
    moves[j + 1] = mv;
    sc[j + 1] = sv;
  }
}

static const int16_t P_ROWS[10] = {50, 45, 40, 34, 28, 18, 8, -4, -8, -12};
static const int16_t P_COLS[9] = {-4, 2, 8, 14, 16, 14, 8, 2, -4};
static const int16_t R_ROWS[10] = {34, 30, 26, 22, 18, 12, 6, 2, 0, -6};
static const int16_t R_COLS[9] = {-8, 0, 6, 14, 20, 14, 6, 0, -8};
static const int16_t H_ROWS[10] = {70, 60, 50, 42, 34, 24, 14, 6, 0, -14};
static const int16_t H_COLS[9] = {-20, -8, 8, 28, 36, 28, 8, -8, -20};
static const int16_t C_ROWS[10] = {28, 26, 24, 20, 16, 10, 4, 2, 0, 0};
static const int16_t C_COLS[9] = {-8, 0, 8, 16, 24, 16, 8, 0, -8};

static const int16_t A_ROWS[10] = {0, 0, 0, 0, 0, 0, 0, 0, 12, 6};
static const int16_t A_COLS[9] = {0, 0, 0, 0, 8, 0, 0, 0, 0};

static const int16_t E_ROWS[10] = {0, 0, 36, 30, 24, 0, 0, 22, 28, 16};
static const int16_t E_COLS[9] = {12, 0, 4, 0, 0, 0, 4, 0, 12};

static const int16_t K_ROWS[10] = {0, 4, 0, 0, 0, 0, 0, 4, 12, 18};
static const int16_t K_COLS[9] = {0, 4, 8, 0, 0, 0, 8, 4, 0};

static int evaluate(int side) {
  int score = 0;
  for (int col = 0; col < 2; ++col) {
    int sign = col == RED ? 1 : -1;
    for (int i = 0; i < piece_n[col]; ++i) {
      uint8_t c = piece_code[col][i];
      int f = piece_f[col][i];
      int r = piece_r[col][i];
      int row = col == RED ? r : 9 - r;
      int t = type_of(c);
      int pos = 0;
      switch (t) {
      case P_PAWN:
        pos = P_ROWS[row] + P_COLS[f];
        break;
      case P_ROOK:
        pos = R_ROWS[row] + R_COLS[f];
        break;
      case P_HORSE:
        pos = H_ROWS[row] + H_COLS[f];
        break;
      case P_CANNON:
        pos = C_ROWS[row] + C_COLS[f];
        break;
      case P_ADVISOR:
        pos = A_ROWS[row] + A_COLS[f];
        break;
      case P_ELEPHANT:
        pos = E_ROWS[row] + E_COLS[f];
        break;
      case P_KING:
        pos = K_ROWS[row] + K_COLS[f];
        break;
      default:
        break;
      }
      int val = PIECE_VALUE[t];
      if (t == P_ROOK || t == P_CANNON || t == P_HORSE) {
        if (attacked(f, r, 1 - col) && !attacked(f, r, col))
          val = val * 3 / 5;
      }
      score += sign * (val + pos);
    }
    for (int opp = 0; opp < 2; ++opp) {
      int me = 1 - opp;
      int sign = me == RED ? 1 : -1;
      int kf = king_f[me], kr = king_r[me];
      if (kf < 0 || kr < 0)
        continue;

      int kr_up = me == RED ? kr - 1 : kr + 1;
      if (kr_up >= 0 && kr_up < RANKS) {
        if (!board[kr_up][kf])
          score -= sign * 15;
      }

      int adv_cnt = 0;
      for (int j = 0; j < piece_n[me]; ++j)
        if (type_of(piece_code[me][j]) == P_ADVISOR)
          adv_cnt++;
      if (adv_cnt < 2)
        score -= sign * (2 - adv_cnt) * 30;

      int ele_cnt = 0;
      for (int j = 0; j < piece_n[me]; ++j)
        if (type_of(piece_code[me][j]) == P_ELEPHANT)
          ele_cnt++;
      if (ele_cnt < 2)
        score -= sign * (2 - ele_cnt) * 25;
    }
  }
  return side == RED ? score : -score;
}

static const uint64_t SIDE_KEY = 0x9e3779b97f4a7c15ull;

static uint64_t rep_hash[128];
static int rep_len = 0;

static const int NO_REPETITION = MATE + 1000;

// Score a repeated position. Returns NO_REPETITION if not a 3-fold repeat.
// On a repeat, distinguish perpetual-check (illegal and losing for the side
// that keeps checking) from a plain draw:
//   - if the side to move is currently in check, the opponent has been
//     perpetually checking, so we score a win for side-to-move;
//   - if the side to move attacks the enemy king, we have been checking,
//     so we score a loss for side-to-move;
//   - otherwise it is a draw.
static int repetition_score(int side) {
  if (rep_len < 4)
    return NO_REPETITION;
  int cnt = 1;
  for (int i = rep_len - 2; i >= 0; i -= 2) {
    if (rep_hash[i] == g_hash)
      cnt++;
    if (cnt >= 3) {
      if (attacked(king_f[side], king_r[side], 1 - side))
        return MATE - 5000;          // opponent perpetual check → side wins
      if (attacked(king_f[1 - side], king_r[1 - side], side))
        return -(MATE - 5000);       // side perpetual check → side loses
      return 0;                      // plain draw
    }
  }
  return NO_REPETITION;
}

static bool is_draw() {
  if (rep_len < 4)
    return false;
  int cnt = 1;
  for (int i = rep_len - 2; i >= 0; i -= 2) {
    if (rep_hash[i] == g_hash)
      cnt++;
    if (cnt >= 3)
      return true;
  }
  return false;
}

static int see(const Move &mv, int side) {
  int tf = mv.to_f, tr = mv.to_r;
  uint8_t victim = board[tr][tf];
  if (!victim)
    return 0;

  static uint8_t sv_board[10][9];
  static int sv_n[2];
  static uint8_t sv_code[2][16], sv_f[2][16], sv_r[2][16];
  memcpy(sv_board, board, sizeof(board));
  for (int c = 0; c < 2; ++c) {
    sv_n[c] = piece_n[c];
    memcpy(sv_code[c], piece_code[c], piece_n[c]);
    memcpy(sv_f[c], piece_f[c], piece_n[c]);
    memcpy(sv_r[c], piece_r[c], piece_n[c]);
  }

  int swap[32];
  int sd = 0;
  swap[0] = PIECE_VALUE[type_of(victim)];
  int cur_col = 1 - side;

  board[tr][tf] = board[mv.from_r][mv.from_f];
  board[mv.from_r][mv.from_f] = 0;
  piece_remove_sq(side, mv.from_f + mv.from_r * FILES);
  piece_remove_sq(1 - side, tf + tr * FILES);
  piece_add(side, board[tr][tf], tf, tr);

  while (sd < 30) {
    int best_val = 999999, best_f = -1, best_r = -1;
    for (int i = 0; i < piece_n[cur_col]; ++i) {
      int pf = piece_f[cur_col][i], pr = piece_r[cur_col][i];
      uint8_t pc = piece_code[cur_col][i];
      int pt = type_of(pc), pv = PIECE_VALUE[pt];
      if (pv >= best_val)
        continue;
      bool can = false;
      if (pt == P_PAWN) {
        if (cur_col == RED) {
          if (pf == tf && pr == tr + 1)
            can = true;
          if (pr <= 4 && abs(pf - tf) == 1 && pr == tr)
            can = true;
        } else {
          if (pf == tf && pr == tr - 1)
            can = true;
          if (pr >= 5 && abs(pf - tf) == 1 && pr == tr)
            can = true;
        }
      } else if (pt == P_HORSE) {
        int df = tf - pf, dr = tr - pr;
        if ((abs(df) == 1 && abs(dr) == 2) || (abs(df) == 2 && abs(dr) == 1)) {
          int lf = (df == 2 || df == -2) ? pf + df / 2 : pf;
          int lr = (dr == 2 || dr == -2) ? pr + dr / 2 : pr;
          if (!board[lr][lf])
            can = true;
        }
      } else if (pt == P_KING) {
        if (abs(pf - tf) + abs(pr - tr) == 1)
          can = true;
      } else if (pt == P_ROOK) {
        if (pf == tf || pr == tr) {
          int blocked = 0;
          if (pf == tf) {
            int lo = pr < tr ? pr : tr, hi = pr < tr ? tr : pr;
            for (int k = lo + 1; k < hi; ++k)
              if (board[k][pf])
                blocked++;
          } else {
            int lo = pf < tf ? pf : tf, hi = pf < tf ? tf : pf;
            for (int k = lo + 1; k < hi; ++k)
              if (board[pr][k])
                blocked++;
          }
          if (blocked == 0)
            can = true;
        }
      } else if (pt == P_CANNON) {
        if (pf == tf || pr == tr) {
          int blocked = 0;
          if (pf == tf) {
            int lo = pr < tr ? pr : tr, hi = pr < tr ? tr : pr;
            for (int k = lo + 1; k < hi; ++k)
              if (board[k][pf])
                blocked++;
          } else {
            int lo = pf < tf ? pf : tf, hi = pf < tf ? tf : pf;
            for (int k = lo + 1; k < hi; ++k)
              if (board[pr][k])
                blocked++;
          }
          if (blocked == 1)
            can = true;
        }
      }
      if (can) {
        best_val = pv;
        best_f = pf;
        best_r = pr;
      }
    }
    if (best_f < 0)
      break;

    sd++;
    swap[sd] = best_val - swap[sd - 1];

    board[tr][tf] = board[best_r][best_f];
    board[best_r][best_f] = 0;
    piece_remove_sq(cur_col, best_f + best_r * FILES);
    piece_remove_sq(1 - cur_col, tf + tr * FILES);
    piece_add(cur_col, board[tr][tf], tf, tr);
    cur_col = 1 - cur_col;
  }

  memcpy(board, sv_board, sizeof(board));
  for (int c = 0; c < 2; ++c) {
    piece_n[c] = sv_n[c];
    memcpy(piece_code[c], sv_code[c], sv_n[c]);
    memcpy(piece_f[c], sv_f[c], sv_n[c]);
    memcpy(piece_r[c], sv_r[c], sv_n[c]);
  }

  int value = swap[sd];
  while (--sd >= 0) {
    if (-swap[sd] > value)
      value = -swap[sd];
  }
  return value;
}

// Sum of non-King piece values for one side (King value is 0, excluded naturally).
// Used for null-move safety: enough material → zugzwang unlikely → skip verification.
static int side_material(int side) {
  int m = 0;
  for (int i = 0; i < piece_n[side]; i++)
    m += PIECE_VALUE[type_of(piece_code[side][i])];
  return m;
}

enum { Q_MAX_PLY = 8 };

static int qsearch(int alpha, int beta, int side, int ply) {
  int stand = evaluate(side);
  if (stand >= beta)
    return stand;
  if (stand > alpha)
    alpha = stand;
  if (ply >= Q_MAX_PLY)
    return stand;

  int rep = repetition_score(side);
  if (rep != NO_REPETITION)
    return rep;

  Move moves[128];
  int n = gen_legal_moves(side, moves);
  if (n == 0)
    return -(MATE - ply);

  bool in_check = attacked(king_f[side], king_r[side], 1 - side);

  int best = stand;
  int c_n = 0;
  int c_scores[128];
  int c_indices[128];

  if (in_check) {
    for (int i = 0; i < n; ++i) {
      uint8_t moving = board[moves[i].from_r][moves[i].from_f];
      uint8_t captured = board[moves[i].to_r][moves[i].to_f];
      int sc = 0;
      if (captured) {
        int s = see(moves[i], side);
        sc = 1000000 + s;
      } else {
        sc = hist[(moving - 1)][moves[i].to_f + moves[i].to_r * FILES];
      }

      c_scores[c_n] = sc;
      c_indices[c_n] = i;
      c_n++;
    }
  } else {
    for (int i = 0; i < n; ++i) {
      uint8_t captured = board[moves[i].to_r][moves[i].to_f];
      if (!captured)
        continue;
      int s = see(moves[i], side);
      if (stand + s + 100 < alpha)
        continue;
      c_scores[c_n] = s;
      c_indices[c_n] = i;
      c_n++;
    }
  }

  for (int i = 1; i < c_n; ++i) {
    int sv = c_scores[i], idx = c_indices[i];
    int j = i - 1;
    while (j >= 0 && c_scores[j] < sv) {
      c_scores[j + 1] = c_scores[j];
      c_indices[j + 1] = c_indices[j];
      --j;
    }
    c_scores[j + 1] = sv;
    c_indices[j + 1] = idx;
  }

  for (int k = 0; k < c_n; ++k) {
    int i = c_indices[k];
    uint8_t moving = board[moves[i].from_r][moves[i].from_f];
    uint8_t captured = board[moves[i].to_r][moves[i].to_f];

    if (captured && type_of(captured) == P_KING) {
      int score = MATE - ply;
      if (score > best)
        best = score;
      if (score > alpha)
        alpha = score;
      if (alpha >= beta)
        break;
      continue;
    }
    uint64_t prev_hash = g_hash;
    rep_hash[rep_len++] = prev_hash;
    make_move(moves[i], moving, captured);
    int score = -qsearch(-beta, -alpha, 1 - side, ply + 1);
    unmake_move(moves[i], moving, captured);
    rep_len--;
    if (score > best)
      best = score;
    if (score > alpha)
      alpha = score;
    if (alpha >= beta)
      break;
  }
  return best;
}

static int search(int depth, int alpha, int beta, int side, int ply, bool no_null = false) {
  if (ply >= MAX_PLY)
    return evaluate(side);

  uint64_t key = g_hash ^ (side ? SIDE_KEY : 0);
  uint32_t idx = (uint32_t)(key & TT_MASK);
  TTEntry &e0 = tt[idx][0];
  TTEntry &e1 = tt[idx][1];

  int rep = repetition_score(side);
  if (rep != NO_REPETITION)
    return rep;

  auto tt_probe = [&](TTEntry &e, int depth, int alpha, int beta, int *out_v) -> bool {
    if (e.key == key) {
      int rec_d = e.df & 0xFF;
      if (rec_d >= depth) {
        int v = e.value, fl = e.df >> 8;
        if (fl == TT_EXACT) {
          *out_v = v;
          return true;
        }
        if (fl == TT_LOWER) {
          if (v >= beta) {
            *out_v = v;
            return true;
          }
        } else { // TT_UPPER
          if (v <= alpha) {
            *out_v = v;
            return true;
          }
        }
      }
    }
    return false;
  };

  int tt_v;
  if (depth >= 1) {
    if (tt_probe(e0, depth, alpha, beta, &tt_v))
      return tt_v;
    if (tt_probe(e1, depth, alpha, beta, &tt_v))
      return tt_v;
  }

  Move moves[128];
  int n = gen_legal_moves(side, moves);
  if (n == 0)
    return -(MATE - ply);

  if (depth == 0) {
    if (attacked(king_f[side], king_r[side], 1 - side))
      depth = 1;
    else
      return qsearch(alpha, beta, side, ply);
  }

  // Futility pruning at frontier nodes (depth == 1, not in check):
  // if even our best quiet hope cannot reach alpha, save a full depth-1
  // search and resolve only captures via qsearch.
  const int FUTIL_MARGIN = 150;
  if (depth == 1 && !attacked(king_f[side], king_r[side], 1 - side)) {
    int stand = evaluate(side);
    if (stand + FUTIL_MARGIN <= alpha)
      return qsearch(alpha, beta, side, ply);
  }

  // Null-move pruning (R=2): skip a turn to get a cheap beta cutoff.
  // Not when in check, not with too few pieces (zugzwang safety).
  // no_null prevents null-move chains and is used by the verification search.
  if (!no_null && depth >= 3 && !attacked(king_f[side], king_r[side], 1 - side) && piece_n[side] >= 3) {
    g_hash ^= SIDE_KEY;
    int null_score = -search(depth - 3, -beta, -beta + 1, 1 - side, ply + 1, true);
    g_hash ^= SIDE_KEY;
    if (null_score >= beta) {
      // NullSafe: enough material → zugzwang unlikely → trust the cutoff.
      if (side_material(side) > 400)
        return beta;
      // Not safe: verify with a real (non-null) search at depth-2.
      // If it also cuts, the null-move result was not a false positive.
      if (search(depth - 2, beta - 1, beta, side, ply, true) >= beta)
        return beta;
      // Verification failed → fall through to normal search.
    }
  }

  uint16_t pref = 0;
  if (e0.key == key)
    pref = e0.move;
  else if (e1.key == key)
    pref = e1.move;
  g_cur_side = side;
  int saved_ply = g_ply;
  g_ply = ply;
  order_moves(moves, n, pref);

  int best = -MATE;
  Move best_move = moves[0];
  int orig_alpha = alpha;

  for (int i = 0; i < n; ++i) {
    uint8_t moving = board[moves[i].from_r][moves[i].from_f];
    uint8_t captured = board[moves[i].to_r][moves[i].to_f];

    if (captured && type_of(captured) == P_KING) {
      int score = MATE - ply;
      if (score > best) {
        best = score;
        best_move = moves[i];
      }
      if (score > alpha)
        alpha = score;
      if (alpha >= beta) {
        int hv = (int)hist[(moving - 1)][moves[i].to_f + moves[i].to_r * FILES] + depth * depth;
        if (hv > 0xFFFF)
          hv = 0xFFFF;
        hist[(moving - 1)][moves[i].to_f + moves[i].to_r * FILES] = (uint16_t)hv;
        break;
      }
      continue;
    }
    uint64_t prev_hash = g_hash;
    rep_hash[rep_len++] = prev_hash;
    make_move(moves[i], moving, captured);

    int gives_check = attacked(king_f[1 - side], king_r[1 - side], side) ? 1 : 0;
    int new_depth = depth - 1 + gives_check;
    int score;
    if (i == 0) {
      score = -search(new_depth, -beta, -alpha, 1 - side, ply + 1);
    } else {
      score = -search(new_depth, -alpha - 1, -alpha, 1 - side, ply + 1);
      if (score > alpha && score < beta)
        score = -search(new_depth, -beta, -alpha, 1 - side, ply + 1);
    }
    unmake_move(moves[i], moving, captured);
    rep_len--;
    if (score > best) {
      best = score;
      best_move = moves[i];
    }
    if (score > alpha)
      alpha = score;
    if (alpha >= beta) {
      int hv = (int)hist[(moving - 1)][moves[i].to_f + moves[i].to_r * FILES] + depth * depth;
      if (hv > 0xFFFF)
        hv = 0xFFFF;
      hist[(moving - 1)][moves[i].to_f + moves[i].to_r * FILES] = (uint16_t)hv;
      if (!captured && ply < MAX_PLY) {
        uint16_t mp = pack_move(moves[i]);
        int sd = side;
        if (killer_used[sd][ply] >= 1 && mp == killer[sd][ply][0]) {
        } else {
          killer[sd][ply][1] = killer[sd][ply][0];
          killer[sd][ply][0] = mp;
          if (killer_used[sd][ply] < 2)
            killer_used[sd][ply]++;
        }
      }
      break;
    }
  }
  g_ply = saved_ply;
  int fl = (alpha >= beta) ? TT_LOWER : (best <= orig_alpha ? TT_UPPER : TT_EXACT);
  uint16_t packed = pack_move(best_move);

  // Two-bucket store: update an existing key if depth is sufficient; otherwise
  // overwrite the shallower (or empty) bucket.
  TTEntry *slot = nullptr;
  if (e0.key == key)
    slot = &e0;
  else if (e1.key == key)
    slot = &e1;
  else {
    int d0 = (e0.key == 0) ? 0 : (e0.df & 0xFF);
    int d1 = (e1.key == 0) ? 0 : (e1.df & 0xFF);
    slot = (d0 <= d1) ? &e0 : &e1;
  }
  if (slot->key != key || depth >= (int)(slot->df & 0xFF)) {
    slot->key = key;
    slot->value = best;
    slot->move = packed;
    slot->df = (uint16_t)(depth | (fl << 8));
  }
  return best;
}

static int g_fullmove = 0;

struct BookInfo {
  bool hit = false;
  uint32_t lock = 0;
  int ncand = 0;
  int choice = -1;
  Move mv{0, 0, 0, 0};
};

static BookInfo g_last_book;

// Compute left-right mirrored hash from current piece positions
static uint64_t compute_mirror_hash() {
  uint64_t h = 0;
  for (int c = 0; c < 2; c++)
    for (int i = 0; i < piece_n[c]; i++) {
      int f = piece_f[c][i], r = piece_r[c][i];
      uint8_t pc = piece_code[c][i];
      h ^= Z[type_of(pc)][c][(8 - f) + r * FILES];
    }
  return h;
}

static inline Move mirror_move(const Move &m) {
  return Move{(int8_t)(8 - m.from_f), m.from_r, (int8_t)(8 - m.to_f), m.to_r};
}

static bool book_probe(Move *mv, BookInfo *info, int side) {
  if (g_fullmove >= 10)
    return false;
  if (info)
    info->hit = false;

  uint64_t key = g_hash ^ (side ? SIDE_KEY : 0);
  uint64_t key_mirror = compute_mirror_hash() ^ (side ? SIDE_KEY : 0);
  uint32_t locks[2] = {(uint32_t)(key >> 32), (uint32_t)(key_mirror >> 32)};
  bool is_mirror[2] = {false, true};

  Move legal[128];
  int ln = gen_legal_moves(side, legal);

  for (int scan = 0; scan < 2; scan++) {
    uint32_t lock = locks[scan];
    int lo = 0, hi = BOOK_HB_N - 1;
    int idx = -1;
    while (lo <= hi) {
      int mid = (lo + hi) / 2;
      if (BOOK_HASH2[mid].lock < lock)
        lo = mid + 1;
      else if (BOOK_HASH2[mid].lock > lock)
        hi = mid - 1;
      else {
        idx = mid;
        break;
      }
    }
    if (idx < 0)
      continue;

    // Scan all entries sharing this 32-bit lock (collisions are likely
    // with 311k entries in a 32-bit lock space); legal-move validation
    // is the only way to tell which one is the actual position.
    Move cand[8];
    int cn = 0;
    int j = idx;
    while (j >= 0 && BOOK_HASH2[j].lock == lock) {
      for (int k = 0; k < 2; k++) {
        uint16_t bmv = (k == 0) ? BOOK_HASH2[j].wmv0 : BOOK_HASH2[j].wmv1;
        if (bmv == 0)
          continue;
        Move t{(int8_t)(bmv & 0xF), (int8_t)((bmv >> 4) & 0xF), (int8_t)((bmv >> 8) & 0xF),
               (int8_t)((bmv >> 12) & 0xF)};
        if (is_mirror[scan])
          t = mirror_move(t);
        for (int i = 0; i < ln; i++)
          if (legal[i].from_f == t.from_f && legal[i].from_r == t.from_r && legal[i].to_f == t.to_f &&
              legal[i].to_r == t.to_r) {
            if (cn < (int)(sizeof(cand) / sizeof(cand[0])))
              cand[cn++] = t;
            break;
          }
      }
      j--;
    }
    j = idx + 1;
    while (j < BOOK_HB_N && BOOK_HASH2[j].lock == lock) {
      for (int k = 0; k < 2; k++) {
        uint16_t bmv = (k == 0) ? BOOK_HASH2[j].wmv0 : BOOK_HASH2[j].wmv1;
        if (bmv == 0)
          continue;
        Move t{(int8_t)(bmv & 0xF), (int8_t)((bmv >> 4) & 0xF), (int8_t)((bmv >> 8) & 0xF),
               (int8_t)((bmv >> 12) & 0xF)};
        if (is_mirror[scan])
          t = mirror_move(t);
        for (int i = 0; i < ln; i++)
          if (legal[i].from_f == t.from_f && legal[i].from_r == t.from_r && legal[i].to_f == t.to_f &&
              legal[i].to_r == t.to_r) {
            if (cn < (int)(sizeof(cand) / sizeof(cand[0])))
              cand[cn++] = t;
            break;
          }
      }
      j++;
    }

    if (cn > 0) {
      int pick = rand() % cn;
      *mv = cand[pick];
      if (info) {
        info->hit = true;
        info->lock = lock;
        info->ncand = cn;
        info->choice = pick;
        info->mv = *mv;
      }
      return true;
    }
  }
  return false;
}

static void ai_move(int depth, Move &best, void (*on_progress)(int, int)) {
  Move book_best;
  BookInfo bi;
  if (book_probe(&book_best, &bi, BLACK)) {
    Move tmp[128];
    int bn = gen_legal_moves(BLACK, tmp);
    bool found = false;
    for (int i = 0; i < bn; ++i)
      if (tmp[i].from_f == book_best.from_f && tmp[i].from_r == book_best.from_r && tmp[i].to_f == book_best.to_f &&
          tmp[i].to_r == book_best.to_r) {
        found = true;
        break;
      }
    if (found) {
      best = book_best;
      g_last_book = bi;
      g_fullmove++;
      return;
    }
  }
  g_last_book.hit = false;

  Move moves[128];
  int n = gen_legal_moves(BLACK, moves);
  if (n == 0) {
    best = {0, 0, 0, 0};
    return;
  }

  Move prev = moves[0];
  for (int d = 1; d <= depth; ++d) {
    order_moves(moves, n, pack_move(prev));
    best = moves[0];
    int alpha = -MATE - 1;
    int beta = MATE + 1;
    for (int i = 0; i < n; ++i) {
      if (on_progress)
        on_progress(i + 1 + (d - 1) * n, n * depth);

      uint8_t moving = board[moves[i].from_r][moves[i].from_f];
      uint8_t captured = board[moves[i].to_r][moves[i].to_f];

      if (captured && type_of(captured) == P_KING) {
        best = moves[i];
        break;
      }
      make_move(moves[i], moving, captured);
      int s;
      if (i == 0) {
        s = -search(d - 1, -beta, -alpha, RED, 1);
      } else {
        s = -search(d - 1, -alpha - 1, -alpha, RED, 1);
        if (s > alpha && s < beta)
          s = -search(d - 1, -beta, -alpha, RED, 1);
      }
      unmake_move(moves[i], moving, captured);
      int sel = (d == depth) ? s + (int)(rand() % 4) : s;
      if (sel > alpha) {
        alpha = sel;
        best = moves[i];
      }
    }
    prev = best;
  }
  g_fullmove++;
}

static uint32_t *VRAM;
static const int X0 = 72 * SCALE, Y0 = 15 * SCALE, CELL = 22 * SCALE;

static inline uint32_t rgb888(uint32_t r, uint32_t g, uint32_t b) { return (b << 16) | (g << 8) | r; }

static void fill_rect(int x, int y, int w, int h, uint32_t color) {
  for (int yy = y; yy < y + h; ++yy)
    for (int xx = x; xx < x + w; ++xx)
      VRAM[yy * SCREEN_W + xx] = color;
}

static void hline(int y, int x0, int x1, uint32_t color) {
  for (int x = x0; x <= x1; ++x)
    VRAM[y * SCREEN_W + x] = color;
}

static void vline(int x, int y0, int y1, uint32_t color) {
  for (int y = y0; y <= y1; ++y)
    VRAM[y * SCREEN_W + x] = color;
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
    {0x00, 0x04, 0x00, 0x00, 0x04, 0x00, 0x00}, // :
    {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}, // -
    {0x01, 0x02, 0x02, 0x04, 0x04, 0x08, 0x10}, // /
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
  if (ch == ':')
    return 39;
  if (ch == '-')
    return 40;
  if (ch == '/')
    return 41;
  return 42;
}

// Scale the built-in 5x7 ASCII font to 10x14 for the 640x480 display.
static void draw_char(int x, int y, char ch, uint32_t color) {
  const uint8_t *g = FONT5x7[char_index(ch)];
  for (int row = 0; row < 7; ++row) {
    uint8_t bits = g[row];
    for (int col = 0; col < 5; ++col) {
      if (bits & (0x10 >> col)) {
        int bx = x + col * SCALE;
        int by = y + row * SCALE;
        VRAM[by * SCREEN_W + bx] = color;
        VRAM[by * SCREEN_W + (bx + 1)] = color;
        VRAM[(by + 1) * SCREEN_W + bx] = color;
        VRAM[(by + 1) * SCREEN_W + (bx + 1)] = color;
      }
    }
  }
}

static void draw_text(int x, int y, const char *s, uint32_t color) {
  while (*s) {
    draw_char(x, y, *s, color);
    x += 6 * SCALE;
    ++s;
  }
}

enum {
  HANZI_CHE_BLACK,   // 車
  HANZI_CHE_RED,     // 车
  HANZI_MA,          // 马
  HANZI_XIANG_RED,   // 相
  HANZI_SHI_RED,     // 仕
  HANZI_SHI_BLACK,   // 士
  HANZI_JIANG,       // 将
  HANZI_SHUAI,       // 帅
  HANZI_XIANG_BLACK, // 象
  HANZI_PING,        // 兵
  HANZI_ZU,          // 卒
  HANZI_PAO,         // 炮
  HANZI_N
};

// Draw a Chinese piece character at 32x32.
// FONT_HANZI32 is a 32*48 byte flat array, with 12 glyphs of 4 bytes per row.
static void draw_hanzi(int x, int y, int idx, uint32_t color) {
  for (int row = 0; row < 32; ++row) {
    const uint8_t *glyph_row = FONT_HANZI32 + row * 48 + idx * 4;
    for (int b = 0; b < 4; ++b) {
      uint8_t bits = glyph_row[b];
      for (int bit = 0; bit < 8; ++bit) {
        if (bits & (0x80 >> bit))
          VRAM[(y + row) * SCREEN_W + (x + b * 8 + bit)] = color;
      }
    }
  }
}

static int piece_hanzi(uint8_t code) {
  int col = color_of(code), t = type_of(code);
  switch (t) {
  case P_PAWN:
    return col == RED ? HANZI_PING : HANZI_ZU;
  case P_CANNON:
    return HANZI_PAO;
  case P_HORSE:
    return HANZI_MA;
  case P_ROOK:
    return col == RED ? HANZI_CHE_RED : HANZI_CHE_BLACK;
  case P_ELEPHANT:
    return col == RED ? HANZI_XIANG_RED : HANZI_XIANG_BLACK;
  case P_ADVISOR:
    return col == RED ? HANZI_SHI_RED : HANZI_SHI_BLACK;
  default:
    return col == RED ? HANZI_SHUAI : HANZI_JIANG;
  }
}

static void draw_piece(int f, int r) {
  uint8_t c = board[r][f];
  if (!c)
    return;
  int cx = X0 + f * CELL;
  int cy = Y0 + r * CELL;
  uint32_t wood = rgb888(224, 180, 116);
  uint32_t edge = rgb888(176, 136, 88);

  uint32_t txt = color_of(c) == RED ? rgb888(210, 30, 30) : rgb888(40, 40, 42);
  // Enlarge the piece slightly so the 32x32 character has a small margin.
  const int PR = 10 * SCALE;
  const int PR2 = PR * PR;
  const int PIR2 = (PR - SCALE - 1) * (PR - SCALE - 1);
  for (int dy = -PR; dy <= PR; ++dy) {
    for (int dx = -PR; dx <= PR; ++dx) {
      int r2 = dx * dx + dy * dy;
      if (r2 > PR2)
        continue;
      VRAM[(cy + dy) * SCREEN_W + (cx + dx)] = (r2 <= PIR2) ? wood : edge;
    }
  }

  // Center the 32x32 Chinese character on the piece.
  // The glyph visual center sits a couple of pixels below the 32x32 cell
  // center, so we shift up by 2 px and right by 1 px for best alignment.
  draw_hanzi(cx - 15, cy - 18, piece_hanzi(c), txt);
}

static void ring(int f, int r, uint32_t col) {
  const int R = 10 * SCALE;
  int x = X0 + f * CELL - R, y = Y0 + r * CELL - R;
  for (int i = 0; i < 2 * R; ++i) {
    VRAM[y * SCREEN_W + x + i] = col;
    VRAM[(y + 1) * SCREEN_W + x + i] = col;
    VRAM[(y + 2 * R - 1) * SCREEN_W + x + i] = col;
    VRAM[(y + 2 * R - 2) * SCREEN_W + x + i] = col;
    VRAM[(y + i) * SCREEN_W + x] = col;
    VRAM[(y + i) * SCREEN_W + x + 1] = col;
    VRAM[(y + i) * SCREEN_W + x + 2 * R - 1] = col;
    VRAM[(y + i) * SCREEN_W + x + 2 * R - 2] = col;
  }
}

static int cx, cy;
static bool selected;
static int sel_f, sel_r;
static Move legals[128];
static int n_legals;
static bool game_over, player_won, ai_thinking, in_check;
static bool ai_highlight;
static int ai_sel_f, ai_sel_r;
static int ai_from_f, ai_from_r;
static int depth;

// Undo/redo state for one full round (RED move + AI move).
struct TurnState {
  uint8_t board[RANKS][FILES];
  uint8_t piece_code[2][16];
  uint8_t piece_f[2][16];
  uint8_t piece_r[2][16];
  int     piece_n[2];
  int     king_f[2];
  int     king_r[2];
  uint64_t g_hash;
  uint64_t rep_hash[128];
  int     rep_len;

  int     cx, cy;
  bool    selected;
  int     sel_f, sel_r;

  bool    ai_highlight;
  int     ai_from_f, ai_from_r;
  int     ai_sel_f, ai_sel_r;

  bool    in_check;
  bool    game_over;
  bool    player_won;

  int     g_fullmove;
  BookInfo g_last_book;

  // source square of the RED piece that was moved from this state
  int     red_from_f;
  int     red_from_r;
};

static TurnState undo_stack[64];
static int       undo_top = 0;
static int       last_red_from_f = -1;
static int       last_red_from_r = -1;

static void save_state(TurnState &st) {
  memcpy(st.board, board, sizeof(board));
  memcpy(st.piece_code, piece_code, sizeof(piece_code));
  memcpy(st.piece_f, piece_f, sizeof(piece_f));
  memcpy(st.piece_r, piece_r, sizeof(piece_r));
  memcpy(st.piece_n, piece_n, sizeof(piece_n));
  memcpy(st.king_f, king_f, sizeof(king_f));
  memcpy(st.king_r, king_r, sizeof(king_r));
  st.g_hash = g_hash;
  memcpy(st.rep_hash, rep_hash, sizeof(rep_hash));
  st.rep_len = rep_len;
  st.cx = cx; st.cy = cy;
  st.selected = selected; st.sel_f = sel_f; st.sel_r = sel_r;
  st.ai_highlight = ai_highlight;
  st.ai_from_f = ai_from_f; st.ai_from_r = ai_from_r;
  st.ai_sel_f = ai_sel_f; st.ai_sel_r = ai_sel_r;
  st.in_check = in_check;
  st.game_over = game_over;
  st.player_won = player_won;
  st.g_fullmove = g_fullmove;
  st.g_last_book = g_last_book;
  st.red_from_f = -1;
  st.red_from_r = -1;
}

static void undo_clear() {
  undo_top = 0;
  last_red_from_f = -1;
  last_red_from_r = -1;
  save_state(undo_stack[0]);
  undo_stack[0].red_from_f = -1;
  undo_stack[0].red_from_r = -1;
  undo_stack[0].ai_highlight = false;
}

// Called at the start of each RED turn to snapshot the state produced by
// RED's last move and the AI response.  The previous top is annotated with
// the source square of RED's move so that undoing puts the cursor there.
static void undo_push_after_ai() {
  if (undo_top + 1 >= (int)(sizeof(undo_stack) / sizeof(undo_stack[0])))
    return;
  undo_stack[undo_top].red_from_f = last_red_from_f;
  undo_stack[undo_top].red_from_r = last_red_from_r;
  ++undo_top;
  save_state(undo_stack[undo_top]);
  undo_stack[undo_top].red_from_f = -1;
  undo_stack[undo_top].red_from_r = -1;
}

static void undo_one_turn() {
  if (undo_top <= 0)
    return;
  --undo_top;
  TurnState &st = undo_stack[undo_top];
  memcpy(board, st.board, sizeof(board));
  memcpy(piece_code, st.piece_code, sizeof(piece_code));
  memcpy(piece_f, st.piece_f, sizeof(piece_f));
  memcpy(piece_r, st.piece_r, sizeof(piece_r));
  memcpy(piece_n, st.piece_n, sizeof(piece_n));
  memcpy(king_f, st.king_f, sizeof(king_f));
  memcpy(king_r, st.king_r, sizeof(king_r));
  g_hash = st.g_hash;
  memcpy(rep_hash, st.rep_hash, sizeof(rep_hash));
  rep_len = st.rep_len;

  selected = st.selected;
  sel_f = st.sel_f;
  sel_r = st.sel_r;
  ai_highlight = st.ai_highlight;
  ai_from_f = st.ai_from_f; ai_from_r = st.ai_from_r;
  ai_sel_f = st.ai_sel_f; ai_sel_r = st.ai_sel_r;
  in_check = st.in_check;
  game_over = st.game_over;
  player_won = st.player_won;
  g_fullmove = st.g_fullmove;
  g_last_book = st.g_last_book;

  // Place RED cursor on the piece that was about to be moved from this state.
  if (st.red_from_f >= 0 && st.red_from_r >= 0) {
    cx = st.red_from_f;
    cy = st.red_from_r;
    selected = false;
    n_legals = 0;
  } else {
    cx = st.cx;
    cy = st.cy;
  }
  ai_thinking = false;
}


static void draw_status(const char *override = nullptr) {
  const int SY0 = 230 * SCALE;
  const int SH = 10 * SCALE;
  const int SY1 = SY0 + SH - 1;
  fill_rect(0, SY0, SCREEN_W, SH, rgb888(20, 18, 14));
  vline(292 * SCALE, SY0, SY1, rgb888(100, 100, 100));
  vline(88 * SCALE, SY0, SY1, rgb888(100, 100, 100));

  const char *s = override;
  uint32_t color = rgb888(255, 255, 255);

  if (!override) {
    if (game_over) {
      s = player_won ? "YOU WIN!  R:RESTART" : "YOU LOSE! R:RESTART";
      color = player_won ? rgb888(0, 255, 0) : rgb888(150, 45, 45);
    } else if (ai_thinking) {
      s = "AI THINKING...";
      color = rgb888(255, 160, 60);
    } else if (in_check) {
      s = "CHECK! YOUR TURN";
      color = rgb888(255, 0, 0);
    } else if (g_last_book.hit) {
      s = "AI BOOK";
      color = rgb888(0, 220, 255);
    } else {
      s = "YOUR TURN";
      color = rgb888(255, 255, 120);
    }
  } else {
    if (ai_thinking)
      color = rgb888(255, 160, 60);
  }

  draw_text(4 * SCALE, 232 * SCALE, s, color);
  draw_text(298 * SCALE, 232 * SCALE, "D:", rgb888(255, 255, 255));
  draw_char(312 * SCALE, 232 * SCALE, (char)('0' + depth), rgb888(255, 255, 255));
}

static void draw_board(bool redraw_piece = true) {
  const uint32_t line = rgb888(150, 130, 100);

  static bool Background_Init = false;
  static uint8_t Board_Piece_Buffer[SCREEN_W * SCREEN_H * 4];

  auto draw_lines_and_marks = [&]() {
    const int BO = 3 * SCALE;
    const int BI = BO - 1;
    const int BC = BO - 2;

    hline(Y0 - BO, X0 - BO, X0 + 8 * CELL + BO, line);
    hline(Y0 - BI, X0 - BI, X0 + 8 * CELL + BI, line);
    hline(Y0 - BC, X0 - BC, X0 + 8 * CELL + BC, line);
    hline(Y0 + 9 * CELL + BO, X0 - BO, X0 + 8 * CELL + BO, line);
    hline(Y0 + 9 * CELL + BI, X0 - BI, X0 + 8 * CELL + BI, line);
    hline(Y0 + 9 * CELL + BC, X0 - BC, X0 + 8 * CELL + BC, line);

    vline(X0 - BO, Y0 - BO, Y0 + 9 * CELL + BO, line);
    vline(X0 - BI, Y0 - BI, Y0 + 9 * CELL + BI, line);
    vline(X0 - BC, Y0 - BC, Y0 + 9 * CELL + BC, line);
    vline(X0 + 8 * CELL + BO, Y0 - BO, Y0 + 9 * CELL + BO, line);
    vline(X0 + 8 * CELL + BI, Y0 - BO, Y0 + 9 * CELL + BI, line);
    vline(X0 + 8 * CELL + BC, Y0 - BO, Y0 + 9 * CELL + BC, line);

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
      VRAM[(Y0 + i) * SCREEN_W + X0 + 3 * CELL + i] = line;
      VRAM[(Y0 + i) * SCREEN_W + X0 + 5 * CELL - i] = line;
      VRAM[(Y0 + 9 * CELL - i) * SCREEN_W + X0 + 3 * CELL + i] = line;
      VRAM[(Y0 + 9 * CELL - i) * SCREEN_W + X0 + 5 * CELL - i] = line;
    }

    auto draw_mark = [&](int x, int y, bool l = true, bool r = true) {
      const int MO = 2 * SCALE;
      const int ML = 4 * SCALE;
      int gx = X0 + x * CELL;
      int gy = Y0 + y * CELL;

      vline(gx - MO, gy - ML, gy - MO, line);
      vline(gx + MO, gy - ML, gy - MO, line);
      vline(gx - MO, gy + MO, gy + ML, line);
      vline(gx + MO, gy + MO, gy + ML, line);

      if (l) {
        hline(gy - MO, gx - ML, gx - MO, line);
        hline(gy + MO, gx - ML, gx - MO, line);
      }

      if (r) {
        hline(gy - MO, gx + MO, gx + ML, line);
        hline(gy + MO, gx + MO, gx + ML, line);
      }
    };

    draw_mark(1, 2);
    draw_mark(1, 7);
    draw_mark(7, 2);
    draw_mark(7, 7);

    draw_mark(0, 3, false, true);
    draw_mark(2, 3);
    draw_mark(4, 3);
    draw_mark(6, 3);
    draw_mark(8, 3, true, false);

    draw_mark(0, 6, false, true);
    draw_mark(2, 6);
    draw_mark(4, 6);
    draw_mark(6, 6);
    draw_mark(8, 6, true, false);
  };

  if (!Background_Init || redraw_piece) {
    memcpy(VRAM, Xiangqi_Background, BG_WIDTH * BG_HEIGHT * 4);
    draw_lines_and_marks();
    for (int r = 0; r < RANKS; ++r)
      for (int f = 0; f < FILES; ++f)
        draw_piece(f, r);
    memcpy(Board_Piece_Buffer, VRAM, SCREEN_W * SCREEN_H * 4);
    Background_Init = true;
  } else {
    memcpy(VRAM, Board_Piece_Buffer, SCREEN_W * SCREEN_H * 4);
  }

  if (selected) {
    for (int i = 0; i < n_legals; ++i)
      fill_rect(X0 + legals[i].to_f * CELL - 3 * SCALE, Y0 + legals[i].to_r * CELL - 3 * SCALE,
                6 * SCALE, 6 * SCALE, rgb888(80, 255, 120));
    ring(sel_f, sel_r, rgb888(255, 120, 120));
  }

  if (ai_highlight) {
    const uint32_t black_sel = rgb888(100, 100, 100);
    const int R = 3 * SCALE;
    int x = X0 + ai_from_f * CELL, y = Y0 + ai_from_r * CELL;
    for (int dy = -R; dy <= R; ++dy)
      for (int dx = -R; dx <= R; ++dx)
        if (dx * dx + dy * dy <= R * R)
          VRAM[(y + dy) * SCREEN_W + (x + dx)] = black_sel;
    ring(ai_sel_f, ai_sel_r, black_sel);
  }

  if (!ai_thinking)
    ring(cx, cy, rgb888(255, 120, 120));
}

static void new_game() {
  srand((unsigned)tcm_rand_seed());
  init_board();
  memset(tt, 0, sizeof(tt));
  memset(hist, 0, sizeof(hist));
  memset(killer, 0, sizeof(killer));
  memset(killer_used, 0, sizeof(killer_used));
  g_fullmove = 0;
  rep_len = 0;
  game_over = false;
  player_won = false;
  in_check = false;
  ai_thinking = false;
  ai_highlight = false;
  selected = false;
  n_legals = 0;
  cx = 4;
  cy = 8;

  undo_clear();

  tcm_ascii_console_clear();

  printf("===== Chinese Chess vs AI =====\n");
  printf("  You are RED.\n\n");
  printf("  KEY         ACTION\n");
  printf("  ----------  -----------------------------\n");
  printf("  %-11s  %s\n", "Arrows", "Move cursor");
  printf("  %-11s  %s\n", "Enter", "Select piece / move");
  printf("  %-11s  %s\n", "C", "Cancel selection");
  printf("  %-11s  %s\n", "Backspace", "Undo one round");
  printf("  %-11s  %s\n", "R", "Restart game");
  printf("  %-11s  %s\n", "Q", "Quit");
  printf("  %-11s  %s\n", "1-4", "AI search depth (default 3)");
  printf("  ----------  -----------------------------\n\n");

  printf("Game start. AI depth: %d\n", depth);
}

static void ai_progress(int progress, int max) {
  fill_rect(90 * SCALE, 232 * SCALE, progress * 200 * SCALE / max, 7 * SCALE, rgb888(255, 160, 60));
}

int main() {
  tcm_ascii_console_init();
  VRAM = (uint32_t *)tcm_pixel_console_init(CONSOLE_MODE_PIXEL_32, SCREEN_W);
  tcm_pixel_console_clear();
  depth = 3;
  new_game();
  uint32_t last_code = 0;
  bool player_moved = false;
  bool restart_game = false;
  draw_board();
  draw_status();

  while (true) {
    uint32_t code = tcm_keyboard_get_code();
    bool fresh = (code != 0 && code != last_code);
    last_code = code;
    if (code == 'q' || code == 'Q')
      break;
    if (fresh) {
      if (code == 'r' || code == 'R') {
        new_game();
        restart_game = true;
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
              ai_highlight = false;
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
                last_red_from_f = sel_f;
                last_red_from_r = sel_r;
                uint8_t moving = board[sel_r][sel_f];
                rep_hash[rep_len++] = g_hash;
                make_move(mv, moving, board[cy][cx]);
                ai_highlight = false;
                tcm_ascii_console_set_color(255, 0, 0, 0, 0, 0);
                printf("RED %c %c%d->%c%d\n", PIECE_LETTER[type_of(moving)], 'a' + sel_f, 9 - sel_r, 'a' + cx, 9 - cy);
                tcm_ascii_console_set_color(255, 255, 255, 0, 0, 0);
                selected = false;
                n_legals = 0;
                in_check = false;
                g_last_book.hit = false;
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
        } else if (code == __TCM_KEY_CODE_BACKSPACE) {
          if (selected) {
            selected = false;
            n_legals = 0;
          } else if (undo_top > 0) {
            undo_one_turn();
            restart_game = true;
          }
        } else if (code == 'c' || code == 'C') {
          selected = false;
          n_legals = 0;
        }
      }
    }

    if (player_moved) {
      Move tmp[128];
      if (gen_legal_moves(BLACK, tmp) == 0) {
        game_over = true;
        player_won = true;
        printf("You win!\n");
      } else {
        ai_thinking = true;
        draw_board();
        draw_status();

        Move best;
        ai_move(depth, best, ai_progress);
        ai_thinking = false;
        uint8_t moving = board[best.from_r][best.from_f];
        rep_hash[rep_len++] = g_hash;
        make_move(best, moving, board[best.to_r][best.to_f]);
        ai_highlight = true;
        ai_sel_f = best.to_f;
        ai_sel_r = best.to_r;
        ai_from_f = best.from_f;
        ai_from_r = best.from_r;
        if (g_last_book.hit)
          printf("BLK BOOK[0x%08x/%d/%d] %c %c%d->%c%d\n", g_last_book.lock, g_last_book.ncand, g_last_book.choice,
                 PIECE_LETTER[type_of(moving)], 'a' + best.from_f, 9 - best.from_r, 'a' + best.to_f, 9 - best.to_r);
        else
          printf("BLK %c %c%d->%c%d\n", PIECE_LETTER[type_of(moving)], 'a' + best.from_f, 9 - best.from_r,
                 'a' + best.to_f, 9 - best.to_r);

        if (gen_legal_moves(RED, tmp) == 0) {
          game_over = true;
          player_won = false;
          printf("You lose.\n");
        } else {
          in_check = attacked(king_f[RED], king_r[RED], BLACK);
          if (in_check)
            printf("CHECK!\n");
        }

        if (!game_over && is_draw()) {
          game_over = true;
          player_won = false;
          printf("Draw by repetition (AI charg)\n");
        }

        // draw_board();
        draw_status();
      }

      if (!game_over)
        undo_push_after_ai();
    }

    if (fresh) {
      draw_board(player_moved || restart_game);
      draw_status();
    }

    player_moved = false;
    restart_game = false;
  }

  tcm_pixel_console_clear();
  tcm_ascii_console_init();
  tcm_ascii_console_clear();
  printf("Thanks for playing Chinese Chess!\n");
  return 0;
}