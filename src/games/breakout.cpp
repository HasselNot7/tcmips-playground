#include <dev/console.h>
#include <dev/keyboard.h>
#include <dev/seven_segment_display.h>

#include <cstdint>
#include <cstdio>

#include "../common/tcm_util.h"

enum {
  SCREEN_W = 320,
  SCREEN_H = 240,
  BRICK_COLS = 10,
  BRICK_ROWS = 6,
  BRICK_W = 28,
  BRICK_H = 12,
  BRICK_X0 = (SCREEN_W - BRICK_COLS * BRICK_W) / 2,
  BRICK_Y0 = 26,
  BRICK_GAP = 14,
  PADDLE_W = 64,
  PADDLE_H = 8,
  PADDLE_Y = SCREEN_H - 20,
  BALL_SIZE = 6,
  MAX_LIVES = 3,
};

static uint32_t *vram;
static uint8_t bricks[BRICK_ROWS][BRICK_COLS];

static inline uint32_t rgb888(uint32_t r, uint32_t g, uint32_t b) {
  return (b << 16) | (g << 8) | r;
}

static void fill_rect(int x0, int y0, int w, int h, uint32_t color) {
  for (int y = y0; y < y0 + h; ++y)
    for (int x = x0; x < x0 + w; ++x)
      if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H)
        vram[y * SCREEN_W + x] = color;
}

static uint32_t brick_color(uint8_t hp) {
  switch (hp) {
  case 1:
    return rgb888(80, 200, 255);
  case 2:
    return rgb888(120, 255, 120);
  default:
    return rgb888(255, 160, 60);
  }
}

static void draw_brick_row(int row) {
  for (int col = 0; col < BRICK_COLS; ++col) {
    if (bricks[row][col] == 0)
      continue;
    fill_rect(BRICK_X0 + col * BRICK_W, BRICK_Y0 + row * BRICK_GAP, BRICK_W,
              BRICK_H, brick_color(bricks[row][col]));
  }
}

static void redraw_bricks() {
  for (int row = 0; row < BRICK_ROWS; ++row)
    draw_brick_row(row);
}

static void new_level() {
  for (int row = 0; row < BRICK_ROWS; ++row)
    for (int col = 0; col < BRICK_COLS; ++col)
      bricks[row][col] = (uint8_t)((row % 3) + 1);
  redraw_bricks();
}

static bool all_cleared() {
  for (int row = 0; row < BRICK_ROWS; ++row)
    for (int col = 0; col < BRICK_COLS; ++col)
      if (bricks[row][col] != 0)
        return false;
  return true;
}

static int play_game(uint32_t &rng_state) {
  int score = 0;
  int lives = MAX_LIVES;
  int level = 1;
  int px = (SCREEN_W - PADDLE_W) / 2;
  int bx = px + PADDLE_W / 2, by = PADDLE_Y - BALL_SIZE;
  int vx = 1, vy = -2;
  bool launch = false;
  uint32_t base_delay = 10;

  tcm_pixel_console_clear();
  tcm_seven_segment_display_clear();
  new_level();
  fill_rect(px, PADDLE_Y, PADDLE_W, PADDLE_H, rgb888(255, 255, 255));

  while (true) {
    uint32_t code = tcm_keyboard_get_code();
    if (code == 'q' || code == 'Q')
      return -1;
    if (code == 'r' || code == 'R')
      return 0;
    if (code == __TCM_KEY_CODE_LEFT && px > 0)
      px -= 6;
    if (code == __TCM_KEY_CODE_RIGHT && px + PADDLE_W < SCREEN_W)
      px += 6;
    if (code == __TCM_KEY_CODE_UP && !launch) {
      launch = true;
      vx = (tcm_rng_next(rng_state) & 1) ? 1 : -1;
      vy = -2;
    }

    fill_rect(px, PADDLE_Y, PADDLE_W, PADDLE_H, 0);

    if (!launch) {
      bx = px + PADDLE_W / 2 - BALL_SIZE / 2;
      by = PADDLE_Y - BALL_SIZE;
    } else {
      int step = 1;
      for (int s = 0; s < step; ++s) {
        fill_rect(bx, by, BALL_SIZE, BALL_SIZE, 0);

        int nx = bx + vx;
        int ny = by + vy;

        if (nx <= 0) {
          vx = -vx;
          nx = 1;
        } else if (nx + BALL_SIZE >= SCREEN_W) {
          vx = -vx;
          nx = SCREEN_W - BALL_SIZE - 1;
        }
        if (ny <= 0) {
          vy = -vy;
          ny = 1;
        }

        bool hit_paddle = false;
        if (vy > 0 && ny + BALL_SIZE >= PADDLE_Y && ny + BALL_SIZE <= PADDLE_Y + 6 &&
            nx + BALL_SIZE > px && nx < px + PADDLE_W) {
          int center = nx + BALL_SIZE / 2;
          int pc = px + PADDLE_W / 2;
          vx = (center - pc) * 3 / 16;
          if (vx > -1 && vx < 1)
            vx = (center < pc) ? -1 : 1;
          vy = -vy;
          ny = PADDLE_Y - BALL_SIZE;
          hit_paddle = true;
          if (score % 12 == 0 && base_delay > 4)
            base_delay--;
        }

        bool hit_brick = false;
        for (int row = 0; row < BRICK_ROWS && !hit_brick; ++row) {
          for (int col = 0; col < BRICK_COLS && !hit_brick; ++col) {
            if (bricks[row][col] == 0)
              continue;
            int brx = BRICK_X0 + col * BRICK_W;
            int bry = BRICK_Y0 + row * BRICK_GAP;
            if (nx + BALL_SIZE > brx && nx < brx + BRICK_W &&
                ny + BALL_SIZE > bry && ny < bry + BRICK_H) {
              bricks[row][col]--;
              fill_rect(brx, bry, BRICK_W, BRICK_H, 0);
              if (bricks[row][col] > 0)
                draw_brick_row(row);
              score++;
              vy = -vy;
              hit_brick = true;
            }
          }
        }
        if (hit_brick && !hit_paddle)
          break;

        bx = nx;
        by = ny;
      }

      if (by > SCREEN_H) {
        lives--;
        if (lives <= 0)
          break;
        launch = false;
        fill_rect(bx, by, BALL_SIZE, BALL_SIZE, 0);
        continue;
      }

      if (all_cleared()) {
        level++;
        launch = false;
        new_level();
        if (base_delay > 4)
          base_delay--;
        continue;
      }
    }

    fill_rect(px, PADDLE_Y, PADDLE_W, PADDLE_H, rgb888(255, 255, 255));
    fill_rect(bx, by, BALL_SIZE, BALL_SIZE, rgb888(255, 255, 60));

    tcm_seven_segment_display_upper_decimal((uint32_t)score);
    tcm_seven_segment_display_lower_decimal((uint32_t)lives);

    tcm_delay_ms(base_delay);
  }

  tcm_pixel_console_clear();
  printf("GAME OVER  score=%d  level=%d\n", score, level);
  printf("R: restart   Q: quit\n");
  while (true) {
    uint32_t code = tcm_keyboard_get_code();
    if (code == 'r' || code == 'R')
      return 0;
    if (code == 'q' || code == 'Q')
      return -1;
  }
}

int main() {
  uint32_t rng_state = tcm_rand_seed();

  tcm_ascii_console_init();
  tcm_ascii_console_clear();
  printf("===== TCM Breakout =====\n");
  printf("L/R arrows: move paddle\n");
  printf("UP: launch ball    Q: quit\n");
  printf("Press any key to start...\n");
  while (tcm_keyboard_get_code() == 0) {
  }
  tcm_ascii_console_clear();

  vram = (uint32_t *)tcm_pixel_console_init(CONSOLE_MODE_PIXEL_32, SCREEN_W);
  tcm_pixel_console_clear();

  while (true) {
    int result = play_game(rng_state);
    if (result < 0)
      break;
  }

  tcm_pixel_console_clear();
  tcm_ascii_console_init();
  tcm_ascii_console_clear();
  printf("Thanks for playing TCM Breakout!\n");
  return 0;
}