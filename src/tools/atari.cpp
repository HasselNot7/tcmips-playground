#include <dev/console.h>
#include <dev/keyboard.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../common/tcm_util.h"
#include "../emulators/atari_2600.h"
#include "../../asset/roms/pong.h"
#include "../../asset/roms/oystron.h"
#include "../../asset/roms/thrust.h"

using namespace tcm::atari;

static uint32_t *vram;

struct RomEntry {
  const char *name;
  const uint8_t *data;
  uint32_t size;
};

static const RomEntry ROMS[] = {
    {"1. Pong (playfield demo, homebrew)", PONG_ROM, sizeof(PONG_ROM)},
    {"2. Oystron (freeware, Piero Cavina)", OYSTRON_ROM, sizeof(OYSTRON_ROM)},
    {"3. Thrust (public domain, T. Jentzsch)", THRUST_ROM, sizeof(THRUST_ROM)},
};

static uint8_t read_swcha() {
  uint8_t sw = 0xFF;
  uint32_t code = tcm_keyboard_get_code();
  if (code == __TCM_KEY_CODE_UP)
    sw &= ~0x08; // P0 up
  else if (code == __TCM_KEY_CODE_DOWN)
    sw &= ~0x04; // P0 down
  else if (code == __TCM_KEY_CODE_LEFT)
    sw &= ~0x02; // P0 left
  else if (code == __TCM_KEY_CODE_RIGHT)
    sw &= ~0x01; // P0 right
  else if (code == 'w' || code == 'W')
    sw &= ~0x80; // P1 up
  else if (code == 's' || code == 'S')
    sw &= ~0x40; // P1 down
  else if (code == 'a' || code == 'A')
    sw &= ~0x20; // P1 left
  else if (code == 'd' || code == 'D')
    sw &= ~0x10; // P1 right
  return sw;
}

static void present(const Atari2600 &t) {
  for (int y = 0; y < 240; ++y) {
    uint32_t *dst = vram + y * 320;
    const uint8_t *src = t.fb + (y * 192 / 240) * 160;
    for (int x = 0; x < 320; ++x)
      dst[x] = PALETTE[src[x >> 1]];
  }
}

int main() {
  tcm_ascii_console_init();
  tcm_ascii_console_clear();
  printf("===== Atari 2600 emulator (TCMIPS) =====\n");
  printf("Select game:\n");
  for (uint32_t i = 0; i < sizeof(ROMS) / sizeof(ROMS[0]); ++i)
    printf("  %s\n", ROMS[i].name);
  printf("(press 1..%u)\n", (unsigned)(sizeof(ROMS) / sizeof(ROMS[0])));
  int choice = 0;
  while (choice < 1 || choice > (int)(sizeof(ROMS) / sizeof(ROMS[0]))) {
    uint32_t code = tcm_keyboard_get_code();
    if (code >= '1' && code <= '9')
      choice = (int)(code - '0');
  }
  const RomEntry &rom = ROMS[choice - 1];
  tcm_ascii_console_clear();
  printf("Loading: %s\n", rom.name);
  printf("Joystick: arrows + space (fire), W/A/S/D for P1\n");
  printf("R: restart  F: game-reset switch\n");
  printf("Press any key to start...\n");
  while (tcm_keyboard_get_code() == 0) {
  }
  tcm_ascii_console_clear();

  vram = (uint32_t *)tcm_pixel_console_init(CONSOLE_MODE_PIXEL_32, 320);
  tcm_pixel_console_clear();

  Atari2600 t;
  t.reset();
  t.load_rom(rom.data, rom.size);
  t.swchb = 0x1F;

  // Console power-on: let the cartridge warm up, then hold game-reset for
  // a few frames. Some carts only sample the switch once running.
  int reset_warmup = 30;
  int reset_latch = 5;
  while (true) {
    uint32_t code = tcm_keyboard_get_code();
    t.swcha = read_swcha();
    t.inpt4 = (code == ' ' || code == '5' || code == __TCM_KEY_CODE_ENTER);
    t.inpt5 = false;
    if (code == 'r' || code == 'R') {
      t.reset();
      t.load_rom(rom.data, rom.size);
      t.swchb = 0x1F;
      reset_warmup = 30;
      reset_latch = 5;
    } else if (code == 'f' || code == 'F') {
      reset_warmup = 0;
      reset_latch = 20; // hold reset for ~20 frames
    }
    if (reset_warmup > 0) {
      --reset_warmup;
      t.swchb |= 0x01;
    } else if (reset_latch > 0) {
      t.swchb &= ~0x01; // SWCHB bit0 = game reset (active low)
      --reset_latch;
    } else {
      t.swchb |= 0x01;
    }
    t.step_frame();
    present(t);
    tcm_delay_ms(16);
  }
  return 0;
}