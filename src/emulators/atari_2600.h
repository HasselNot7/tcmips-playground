// Minimal Atari 2600 (VCS) emulator core: 6507 CPU + TIA.
// Portable: no OS / dev-kit dependencies, usable from host tests and TCMIPS.
#ifndef TCMIPS_ATARI_2600_H
#define TCMIPS_ATARI_2600_H

#include <cstdint>
#include <cstring>

namespace tcm {
namespace atari {

// NTSC TIA palette: 128 entries, index = color & 0x7F, RGB888.
static const uint32_t PALETTE[128] = {
    0x000000, 0x1a1a1a, 0x323232, 0x464646, 0x585858, 0x6a6a6a, 0x7a7a7a, 0x888888,
    0x949494, 0xa0a0a0, 0xaaaaaa, 0xb4b4b4, 0xbebebe, 0xc8c8c8, 0xd0d0d0, 0xd8d8d8,
    0x000000, 0x201600, 0x322a04, 0x3f3c12, 0x4a4a1c, 0x555626, 0x5f602e, 0x696936,
    0x71703e, 0x797646, 0x7e7c4e, 0x838254, 0x88885a, 0x8c8e60, 0x909466, 0x949a6c,
    0x000000, 0x1a1600, 0x2a2410, 0x372f1e, 0x423a2a, 0x4c4434, 0x554d3c, 0x5d5544,
    0x645c4c, 0x6b6252, 0x71685a, 0x766e60, 0x7b7366, 0x80786c, 0x847d72, 0x888178,
    0x000000, 0x160e00, 0x241c0e, 0x2f281a, 0x393324, 0x423d2e, 0x4b4637, 0x524e3f,
    0x595647, 0x5f5d4f, 0x656356, 0x6a695d, 0x6f6e63, 0x747369, 0x78786f, 0x7c7d75,
    0x000000, 0x120800, 0x1f140c, 0x291f16, 0x32281f, 0x3a3028, 0x423830, 0x493f38,
    0x50453f, 0x564b46, 0x5c504d, 0x615553, 0x665a59, 0x6b5f5f, 0x706464, 0x756969,
    0x000000, 0x0e0400, 0x1a1008, 0x231911, 0x2b2118, 0x33291f, 0x3a3026, 0x41362c,
    0x473c32, 0x4d4238, 0x52473d, 0x574c43, 0x5c5148, 0x61564d, 0x655b52, 0x696057,
    0x000000, 0x0a0200, 0x150c06, 0x1e130d, 0x261a13, 0x2d2119, 0x34271f, 0x3b2d25,
    0x41322a, 0x473730, 0x4c3c35, 0x51413a, 0x56463f, 0x5b4b44, 0x5f5048, 0x64554d,
    0x000000, 0x080000, 0x12080a, 0x1a1010, 0x211717, 0x281e1d, 0x2f2423, 0x352a29,
    0x3b302e, 0x413534, 0x463a39, 0x4b3f3e, 0x504443, 0x554948, 0x594e4c, 0x5e5351,
};

struct Atari2600 {
  // ---- Console state -----------------------------------------------------
  uint8_t ram[128];
  uint8_t rom[32768];
  uint32_t rom_size;
  uint8_t bank;        // current cartridge bank (4K units)
  uint8_t switch_type; // 0=none/2K, 1=F8 (2x4K), 2=F6 (4x4K), 3=F4 (8x4K)

  // RIOT interval timer state
  uint64_t timer_start;
  uint64_t timer_prescale;
  uint8_t timer_load;
  bool timer_underflow;

  // ---- CPU registers -----------------------------------------------------
  uint16_t pc;
  uint8_t sp, a, x, y, p; // p = NV-BDIZC (bit7..bit0)
  uint64_t cycles;

  // ---- TIA write registers ----------------------------------------------
  uint8_t vsync, vblank;
  uint8_t nusiz0, nusiz1;
  uint8_t colup0, colup1, colupf, colubk;
  uint8_t ctrlpf, refp0, refp1;
  uint8_t pf0, pf1, pf2;
  uint8_t grp0, grp1, grp0d, grp1d;
  uint8_t enam0, enam1, enabl;
  uint8_t hmp0, hmp1, hmm0, hmm1, hmbl;
  uint8_t vdelp0, vdelp1, resmp0, resmp1;
  uint8_t audc0, audc1, audf0, audf1, audv0, audv1;

  // ---- TIA object positions ----------------------------------------------
  int pos0, pos1, posm0, posm1, posbl;
  bool hmove_pending;

  // ---- TIA collisions (latched) ------------------------------------------
  uint8_t cxm0p, cxm1p, cxp0fb, cxp1fb, cxm0fb, cxm1fb, cxblpf, cxppmm;

  // ---- Timing ------------------------------------------------------------
  int clock_in_line;   // 0..227
  int scanline;        // 0..261

  // ---- Inputs ------------------------------------------------------------
  uint8_t swcha, swchb; // joysticks / switches
  bool inpt4, inpt5;    // fire buttons

  // ---- Output framebuffer (palette indices) ------------------------------
  uint8_t fb[160 * 192];

  static const int VISIBLE_CLOCKS = 160;
  static const int VISIBLE_LINES = 192;
  static const int LINE_CLOCKS = 228;
  static const int LINES_PER_FRAME = 262;

  void reset() {
    memset(this, 0, sizeof(*this));
    pc = 0xF000;
    sp = 0xFF;
    p = 0x34; // unused flag + interrupt disable
    pos0 = pos1 = posm0 = posm1 = posbl = -100;
    swcha = 0xFF;
    swchb = 0x08; // color mode
    timer_prescale = 1;
  }

  void load_rom(const uint8_t *data, uint32_t size) {
    rom_size = size;
    memset(rom, 0xFF, sizeof(rom));
    for (uint32_t i = 0; i < size; ++i)
      rom[i] = data[i];
    bank = 0;
    if (size <= 2048) {
      switch_type = 0; // 2K mirrored at F000-FFFF
    } else if (size <= 4096) {
      switch_type = 0; // plain 4K
    } else if (size <= 8192) {
      switch_type = 1; // F8
      bank = 1;        // power-up maps the last bank
    } else if (size <= 16384) {
      switch_type = 2; // F6
      bank = 3;
    } else {
      switch_type = 3; // F4
      bank = 7;
    }
  }

  // ---- Memory interface --------------------------------------------------
  void write(uint16_t addr, uint8_t val);
  uint8_t read(uint16_t addr);

  // ---- CPU step ----------------------------------------------------------
  void step(); // executes one instruction, updates cycles/clock_in_line

  // ---- TIA ---------------------------------------------------------------
  void tia_write(uint8_t reg, uint8_t val);
  uint8_t tia_read(uint8_t reg);
  void render_line();
  bool step_frame(); // returns true when a visible frame was completed

  // ---- Flags -------------------------------------------------------------
  bool flag_n() const { return (p & 0x80) != 0; }
  bool flag_z() const { return (p & 0x02) != 0; }
  bool flag_c() const { return (p & 0x01) != 0; }
  bool flag_v() const { return (p & 0x40) != 0; }
  void set_nz(uint8_t v) { p = (p & 0x7D) | ((v & 0x80) ? 0x80 : 0) | (v == 0 ? 0x02 : 0); }
  void push(uint8_t v) { ram[sp-- & 0x7F] = v; }
  uint8_t pop() { return ram[++sp & 0x7F]; }
};

uint64_t tia_write_count(uint8_t reg);
uint64_t bank_switch_count();

} // namespace atari
} // namespace tcm

#endif