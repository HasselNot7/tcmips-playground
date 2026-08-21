#include <dev/console.h>
#include <dev/keyboard.h>
#include <dev/seven_segment_display.h>

#include <cstdint>
#include <cstdio>

#include "../common/tcm_util.h"

enum {
  CELL = 8,
  GRID_W = 40,
  GRID_H = 30,
  SCREEN_W = GRID_W * CELL,
  SCREEN_H = GRID_H * CELL,
};

struct Seg {
  int16_t x, y;
};

static uint32_t *vram;
static Seg body[GRID_W * GRID_H];

static inline uint32_t rgb888(uint32_t r, uint32_t g, uint32_t b) {
  return (b << 16) | (g << 8) | r;
}

static void fill_rect(int x0, int y0, int w, int h, uint32_t color) {
  for (int y = y0; y < y0 + h; ++y)
    for (int x = x0; x < x0 + w; ++x)
      vram[y * SCREEN_W + x] = color;
}

static void draw_cell(int gx, int gy, uint32_t color) {
  fill_rect(gx * CELL, gy * CELL, CELL, CELL, color);
}

static void place_food(int fx, int fy) {
  draw_cell(fx, fy, rgb888(255, 40, 40));
}

static int play_game(uint32_t &rng_state) {
  tcm_pixel_console_clear();
  tcm_seven_segment_display_clear();

  int head_x = 12, head_y = 15;
  int dir_x = 1, dir_y = 0;
  int len = 3;
  body[0] = {(int16_t)head_x, (int16_t)head_y};
  body[1] = {(int16_t)(head_x - 1), (int16_t)head_y};
  body[2] = {(int16_t)(head_x - 2), (int16_t)head_y};
  int score = 0;
  uint32_t delay = 130;

  int fx = 20, fy = 15;
  for (int y = 0; y < GRID_H; ++y)
    for (int x = 0; x < GRID_W; ++x) {
      bool on_snake = false;
      for (int i = 0; i < len; ++i)
        if (body[i].x == x && body[i].y == y)
          on_snake = true;
      if (on_snake)
        draw_cell(x, y, rgb888(0, 220, 60));
    }
  place_food(fx, fy);

  while (true) {
    uint32_t code = tcm_keyboard_get_code();
    if (code == 'q' || code == 'Q')
      return -1;
    if (code == __TCM_KEY_CODE_UP && dir_y == 0) {
      dir_x = 0;
      dir_y = -1;
    } else if (code == __TCM_KEY_CODE_DOWN && dir_y == 0) {
      dir_x = 0;
      dir_y = 1;
    } else if (code == __TCM_KEY_CODE_LEFT && dir_x == 0) {
      dir_x = -1;
      dir_y = 0;
    } else if (code == __TCM_KEY_CODE_RIGHT && dir_x == 0) {
      dir_x = 1;
      dir_y = 0;
    }

    int nx = head_x + dir_x;
    int ny = head_y + dir_y;

    if (nx < 0 || nx >= GRID_W || ny < 0 || ny >= GRID_H)
      break;

    bool dead = false;
    for (int i = 0; i < len; ++i)
      if (body[i].x == nx && body[i].y == ny)
        dead = true;
    if (dead)
      break;

    bool ate = (nx == fx && ny == fy);
    if (ate) {
      len++;
      score++;
      if (delay > 45)
        delay -= 6;
      do {
        fx = (int)(tcm_rng_next(rng_state) % GRID_W);
        fy = (int)(tcm_rng_next(rng_state) % GRID_H);
        bool on_snake = false;
        for (int i = 0; i < len; ++i)
          if (body[i].x == fx && body[i].y == fy)
            on_snake = true;
        if (!on_snake)
          break;
      } while (true);
    } else {
      draw_cell(body[len - 1].x, body[len - 1].y, 0);
    }

    for (int i = len - 1; i > 0; --i)
      body[i] = body[i - 1];
    body[0] = {(int16_t)nx, (int16_t)ny};
    head_x = nx;
    head_y = ny;

    for (int i = 0; i < len; ++i) {
      uint32_t g = 60 + (uint32_t)(195 * (len - i) / len);
      draw_cell(body[i].x, body[i].y, i == 0 ? rgb888(120, 255, 90) : rgb888(0, g, 60));
    }
    place_food(fx, fy);

    tcm_seven_segment_display_upper_decimal((uint32_t)score);
    tcm_seven_segment_display_lower_decimal((uint32_t)len);

    tcm_delay_ms(delay);
  }

  for (int y = 0; y < GRID_H; ++y)
    for (int x = 0; x < GRID_W; ++x)
      if (body[0].x != x || body[0].y != y)
        draw_cell(x, y, 0);
  draw_cell(body[0].x, body[0].y, rgb888(255, 60, 60));

  printf("GAME OVER  score=%d  length=%d\n", score, len);
  printf("R: restart   Q: quit\n");
  while (true) {
    uint32_t code = tcm_keyboard_get_code();
    if (code == 'r' || code == 'R')
      return score;
    if (code == 'q' || code == 'Q')
      return -1;
  }
}

int main() {
  uint32_t rng_state = tcm_rand_seed();

  tcm_ascii_console_init();
  tcm_ascii_console_clear();
  printf("===== TCM Snake =====\n");
  printf("Arrows: move    Q: quit\n");
  printf("Score on 7-seg, eat red food\n");
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
  printf("Thanks for playing TCM Snake!\n");
  return 0;
}