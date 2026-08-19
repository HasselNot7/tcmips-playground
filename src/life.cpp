#include <dev/console.h>
#include <dev/keyboard.h>
#include <dev/seven_segment_display.h>

#include <cstdint>
#include <cstdio>

#include "tcm_util.h"

enum {
  COLS = 80,
  ROWS = 60,
  CELL = 4,
  SCREEN_W = COLS * CELL,
  SCREEN_H = ROWS * CELL,
};

static uint32_t *vram;
static uint8_t grid[ROWS][COLS];
static uint8_t age[ROWS][COLS];
static uint8_t next[ROWS][COLS];
static uint8_t nage[ROWS][COLS];

static inline uint32_t rgb888(uint32_t r, uint32_t g, uint32_t b) {
  return (b << 16) | (g << 8) | r;
}

static uint32_t live_color(uint8_t a) {
  if (a < 3)
    return rgb888(160, 255, 80);
  if (a < 6)
    return rgb888(60, 220, 130);
  if (a < 10)
    return rgb888(50, 160, 210);
  return rgb888(130, 110, 255);
}

static void draw_cell(int x, int y) {
  uint32_t color = grid[y][x] ? live_color(age[y][x]) : 0;
  for (int dy = 0; dy < CELL - 1; ++dy)
    for (int dx = 0; dx < CELL - 1; ++dx)
      vram[(y * CELL + dy) * SCREEN_W + (x * CELL + dx)] = color;
}

static void draw_grid() {
  for (int y = 0; y < ROWS; ++y)
    for (int x = 0; x < COLS; ++x)
      draw_cell(x, y);
}

static void draw_cursor(int x, int y, uint32_t color) {
  int px = x * CELL, py = y * CELL;
  for (int i = 0; i < CELL; ++i) {
    vram[py * SCREEN_W + px + i] = color;
    vram[(py + CELL - 1) * SCREEN_W + px + i] = color;
    vram[(py + i) * SCREEN_W + px] = color;
    vram[(py + i) * SCREEN_W + px + CELL - 1] = color;
  }
}

static void life_step() {
  for (int y = 0; y < ROWS; ++y)
    for (int x = 0; x < COLS; ++x) {
      int n = 0;
      for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
          if (dx == 0 && dy == 0)
            continue;
          n += grid[(y + dy + ROWS) % ROWS][(x + dx + COLS) % COLS];
        }
      if (grid[y][x]) {
        next[y][x] = (n == 2 || n == 3);
        nage[y][x] = next[y][x] ? (uint8_t)(age[y][x] + 1) : 0;
      } else {
        next[y][x] = (n == 3);
        nage[y][x] = 0;
      }
    }
  for (int y = 0; y < ROWS; ++y)
    for (int x = 0; x < COLS; ++x) {
      grid[y][x] = next[y][x];
      age[y][x] = nage[y][x];
    }
}

static int count_live() {
  int c = 0;
  for (int y = 0; y < ROWS; ++y)
    for (int x = 0; x < COLS; ++x)
      c += grid[y][x];
  return c;
}

static void randomize(uint32_t &rng) {
  for (int y = 0; y < ROWS; ++y)
    for (int x = 0; x < COLS; ++x) {
      grid[y][x] = (tcm_rng_next(rng) % 100) < 30;
      age[y][x] = 0;
    }
}

static void clear_all() {
  for (int y = 0; y < ROWS; ++y)
    for (int x = 0; x < COLS; ++x) {
      grid[y][x] = 0;
      age[y][x] = 0;
    }
}

int main() {
  uint32_t rng_state = tcm_rand_seed();

  tcm_ascii_console_init();
  tcm_ascii_console_clear();
  printf("===== Game of Life =====\n");
  printf("Arrows: move cursor\n");
  printf("Enter: toggle cell\n");
  printf("Space: run/pause\n");
  printf("S: step  R: random  C: clear  Q: quit\n");
  printf("Press any key to start...\n");
  while (tcm_keyboard_get_code() == 0) {
  }
  tcm_ascii_console_clear();

  vram = (uint32_t *)tcm_pixel_console_init(CONSOLE_MODE_PIXEL_32, SCREEN_W);
  tcm_pixel_console_clear();
  tcm_seven_segment_display_clear();

  randomize(rng_state);
  draw_grid();

  int cx = COLS / 2, cy = ROWS / 2;
  bool running = false;
  uint32_t generation = 0;
  uint32_t last_code = 0;

  while (true) {
    uint32_t code = tcm_keyboard_get_code();
    bool fresh = (code != 0 && code != last_code);

    if (code == 'q' || code == 'Q')
      break;

    if (fresh) {
      if (code == __TCM_KEY_CODE_UP && cy > 0) {
        draw_cell(cx, cy);
        cy--;
      } else if (code == __TCM_KEY_CODE_DOWN && cy < ROWS - 1) {
        draw_cell(cx, cy);
        cy++;
      } else if (code == __TCM_KEY_CODE_LEFT && cx > 0) {
        draw_cell(cx, cy);
        cx--;
      } else if (code == __TCM_KEY_CODE_RIGHT && cx < COLS - 1) {
        draw_cell(cx, cy);
        cx++;
      } else if (code == __TCM_KEY_CODE_ENTER) {
        grid[cy][cx] = !grid[cy][cx];
        age[cy][cx] = 0;
        draw_cell(cx, cy);
      } else if (code == ' ') {
        running = !running;
        if (running)
          printf("RUNNING  gen=%u  live=%d\n", generation, count_live());
        else
          printf("PAUSED  gen=%u  live=%d\n", generation, count_live());
      } else if (code == 's' || code == 'S') {
        life_step();
        generation++;
        draw_grid();
        running = false;
      } else if (code == 'r' || code == 'R') {
        randomize(rng_state);
        generation = 0;
        draw_grid();
        running = false;
      } else if (code == 'c' || code == 'C') {
        clear_all();
        generation = 0;
        tcm_pixel_console_clear();
        running = false;
      }
    }

    if (running) {
      life_step();
      generation++;
      draw_grid();
    }

    draw_cursor(cx, cy, rgb888(255, 255, 255));

    tcm_seven_segment_display_upper_decimal(generation);
    tcm_seven_segment_display_lower_decimal((uint32_t)count_live());

    if (running)
      tcm_delay_ms(100);

    last_code = code;
  }

  tcm_pixel_console_clear();
  tcm_ascii_console_init();
  tcm_ascii_console_clear();
  printf("Thanks for playing Game of Life!\n");
  return 0;
}