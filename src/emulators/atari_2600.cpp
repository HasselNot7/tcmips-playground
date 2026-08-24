#include "atari_2600.h"

namespace tcm {
namespace atari {

// Optional instrumentation for ROM debugging (host-only reads).
#ifdef TCM_ATARI_TRACE
static uint64_t g_tia_wc[64];
static uint64_t g_bank_switches;
static uint16_t g_last_pc;
#endif
uint64_t tia_write_count(uint8_t reg) {
#ifdef TCM_ATARI_TRACE
  return g_tia_wc[reg & 0x3F];
#else
  (void)reg;
  return 0;
#endif
}
uint64_t bank_switch_count() {
#ifdef TCM_ATARI_TRACE
  return g_bank_switches;
#else
  return 0;
#endif
}


// ---------------------------------------------------------------------------
// Memory map
// ---------------------------------------------------------------------------
void Atari2600::write(uint16_t addr, uint8_t val) {
  if (addr & 0x1000) {
    // cartridge space writes = bankswitch control
    switch (switch_type) {
      case 1: // F8: $1FF8/$1FF9
        if ((addr & 0x0FFF) >= 0x0FF8) {
          bank = addr & 1;
#ifdef TCM_ATARI_TRACE
          ++g_bank_switches;
#endif
        }
        break;
      case 2: // F6: $1FF6-$1FF9
        if ((addr & 0x0FFF) >= 0x0FF6 && (addr & 0x0FFF) <= 0x0FF9) {
          bank = addr & 3;
#ifdef TCM_ATARI_TRACE
          ++g_bank_switches;
#endif
        }
        break;
      case 3: // F4: $1FF4-$1FFB
        if ((addr & 0x0FFF) >= 0x0FF4 && (addr & 0x0FFF) <= 0x0FFB) {
          bank = addr & 7;
#ifdef TCM_ATARI_TRACE
          ++g_bank_switches;
#endif
        }
        break;
      default:
        break;
    }
    return;
  }
  addr &= 0xFFF;
  if ((addr & 0xF80) == 0x280) {
    uint8_t reg = addr & 0x3F;
    if (reg == 0x00)
      swcha = val; // SWCHA
    else if (reg == 0x02)
      swchb = val; // SWCHB
    else if ((reg >= 0x04 && reg <= 0x07) || (reg >= 0x14 && reg <= 0x17)) {
      // RIOT interval timer: INTIM area $284-$287 or TIMxxT $294-$297
      static const uint32_t prescales[4] = {1, 8, 64, 1024};
      uint8_t sel = (reg >= 0x14) ? (uint8_t)(reg - 0x14) : (uint8_t)(reg - 0x04);
      if (reg >= 0x14)
        sel = reg - 0x14; // $294=TIM1T $295=TIM8T $296=TIM64T $297=T1024T
      else
        sel = reg - 0x04; // $284-$287 same order
      timer_prescale = prescales[sel & 3];
      timer_load = val;
      timer_start = cycles;
      timer_underflow = false;
    }
    return;
  }
  if ((addr & 0x80) == 0) {
    tia_write(addr & 0x3F, val);
  } else {
    ram[addr & 0x7F] = val;
  }
}

uint8_t Atari2600::read(uint16_t addr) {
  if (addr & 0x1000) {
    uint32_t off = (addr - 0xF000) & 0xFFF;
    // Read-triggered bankswitching (also covers F8/F6/F4 carts that
    // switch by reading $1FF4-$1FFB, e.g. Thrust).
    if (switch_type == 1 && off >= 0x0FF8)
      bank = addr & 1;
    else if (switch_type == 2 && off >= 0x0FF6 && off <= 0x0FF9)
      bank = addr & 3;
    else if (switch_type == 3 && off >= 0x0FF4 && off <= 0x0FFB)
      bank = addr & 7;
    if (switch_type == 0 && rom_size <= 2048)
      off &= 0x7FF; // 2K mirror
    return rom[((uint32_t)bank * 4096 + off) & (rom_size - 1)];
  }
  addr &= 0xFFF;
  if ((addr & 0xF80) == 0x280) {
    uint8_t reg = addr & 0x3F;
    if (reg == 0x00)
      return swcha;
    if (reg == 0x02)
      return swchb;
    if (reg == 0x04) {
      // INTIM: low 8 bits of down-counting timer
      uint64_t elapsed = (cycles - timer_start) / timer_prescale;
      int16_t cur = (int16_t)timer_load - (int16_t)elapsed;
      if (cur < 0) {
        timer_underflow = true;
        cur &= 0xFF;
        cur |= 0x80;
      }
      return (uint8_t)cur;
    }
    if (reg == 0x05) {
      // TIMINT: same as INTIM but clears the underflow latch
      timer_underflow = false;
      uint64_t elapsed = (cycles - timer_start) / timer_prescale;
      int16_t cur = (int16_t)timer_load - (int16_t)elapsed;
      if (cur < 0) {
        cur &= 0xFF;
        cur |= 0x80;
      }
      return (uint8_t)cur;
    }
    return 0;
  }
  if ((addr & 0x80) == 0)
    return tia_read(addr & 0x3F);
  return ram[addr & 0x7F];
}

// ---------------------------------------------------------------------------
// TIA
// ---------------------------------------------------------------------------
void Atari2600::tia_write(uint8_t reg, uint8_t val) {
#ifdef TCM_ATARI_TRACE
  g_tia_wc[reg & 0x3F]++;
#endif
  switch (reg) {
    case 0x00: vsync = val; break;
    case 0x01: vblank = val; break;
    case 0x02: clock_in_line = LINE_CLOCKS; break; // WSYNC
    case 0x03: break;                              // RSYNC
    case 0x04: nusiz0 = val; break;
    case 0x05: nusiz1 = val; break;
    // Canonical TIA write register layout ($06..$2D)
    case 0x06: colup0 = val; break;
    case 0x07: colup1 = val; break;
    case 0x08: colupf = val; break;
    case 0x09: colubk = val; break;
    case 0x0a: ctrlpf = val; break;
    case 0x0b: refp0 = val; break;
    case 0x0c: refp1 = val; break;
    case 0x0d: pf0 = val; break;
    case 0x0e: pf1 = val; break;
    case 0x0f: pf2 = val; break;
    case 0x10: pos0 = clock_in_line + 5; break;   // RESP0
    case 0x11: pos1 = clock_in_line + 5; break;   // RESP1
    case 0x12: posm0 = clock_in_line + 9; break;  // RESM0
    case 0x13: posm1 = clock_in_line + 9; break;  // RESM1
    case 0x14: posbl = clock_in_line + 11; break; // RESBL
    case 0x15: audc0 = val; break;
    case 0x16: audc1 = val; break;
    case 0x17: audf0 = val; break;
    case 0x18: audf1 = val; break;
    case 0x19: audv0 = val; break;
    case 0x1a: audv1 = val; break;
    case 0x1b:
      if (vdelp0) {
        grp0d = val;
      } else {
        grp0 = val;
      }
      break;                                     // GRP0
    case 0x1c:
      if (vdelp1) {
        grp1d = val;
      } else {
        grp1 = val;
      }
      break;                                     // GRP1
    case 0x1d: enam0 = val; break;               // ENAM0
    case 0x1e: enam1 = val; break;               // ENAM1
    case 0x1f: enabl = val; break;               // ENABL
    case 0x20: hmp0 = val; break;                // HMP0
    case 0x21: hmp1 = val; break;                // HMP1
    case 0x22: hmm0 = val; break;                // HMM0
    case 0x23: hmm1 = val; break;                // HMM1
    case 0x24: hmbl = val; break;                // HMBL
    case 0x25: vdelp0 = val; break;              // VDELP0
    case 0x26: vdelp1 = val; break;              // VDELP1
    case 0x27: resmp0 = val; break;              // RESMP0
    case 0x28: resmp1 = val; break;              // RESMP1
    case 0x2b: { // HMOVE
      pos0 += (int8_t)(hmp0 << 4) >> 4;
      pos1 += (int8_t)(hmp1 << 4) >> 4;
      posm0 += (int8_t)(hmm0 << 4) >> 4;
      posm1 += (int8_t)(hmm1 << 4) >> 4;
      posbl += (int8_t)(hmbl << 4) >> 4;
      break;
    }
    case 0x2c: hmp0 = hmp1 = hmm0 = hmm1 = hmbl = 0; break; // HMCLR
    case 0x2d: cxm0p = cxm1p = cxp0fb = cxp1fb = cxm0fb = cxm1fb = cxblpf = cxppmm = 0; break;
    default: break;
  }
}

uint8_t Atari2600::tia_read(uint8_t reg) {
  reg &= 0x3F;
  if (reg < 0x08) {
    const uint8_t coll[8] = {cxm0p, cxm1p, cxp0fb, cxp1fb, cxm0fb, cxm1fb, cxblpf, cxppmm};
    return coll[reg];
  }
  if (reg >= 0x10 && reg <= 0x15) {
    bool fire = (reg == 0x14) ? !inpt4 : (reg == 0x15) ? !inpt5 : true;
    return fire ? 0x80 : 0x00;
  }
  return 0;
}

static bool pf_bit(const Atari2600 &t, int c) {
  // c: 0..159 visible clock. Map to full-line clock 68..227.
  int full = c + 68;
  int h = full - 68; // 0..159
  if (h >= 80 && !(t.ctrlpf & 0x20))
    h -= 80; // non-reflected: repeat left half
  else if (h >= 80)
    h = 159 - h; // reflected
  if (h < 0 || h > 79)
    return false;
  int idx = h >> 2; // 0..19
  if (idx < 4)
    return (t.pf0 & (0x80 >> idx)) != 0;          // PF0: bit7 = leftmost
  if (idx < 12)
    return (t.pf1 & (0x80 >> (idx - 4))) != 0;    // PF1: reversed
  return (t.pf2 & (0x01 << (idx - 12))) != 0;     // PF2: straight
}

static uint8_t reflect8(uint8_t g) {
  uint8_t r = 0;
  for (int b = 0; b < 8; ++b)
    r |= ((g >> b) & 1) << (7 - b);
  return r;
}

void Atari2600::render_line() {
  uint8_t *row = fb + (scanline - 40) * VISIBLE_CLOCKS;

  int p0w = ((nusiz0 == 0x07) || (nusiz0 & 0x10)) ? 2 : 1;
  int p1w = ((nusiz1 == 0x07) || (nusiz1 & 0x10)) ? 2 : 1;
  int m0w = (nusiz0 & 0x20) ? (nusiz0 & 0x40) ? 8 : 4 : (nusiz0 & 0x10) ? 2 : 1;
  int m1w = (nusiz1 & 0x20) ? (nusiz1 & 0x40) ? 8 : 4 : (nusiz1 & 0x10) ? 2 : 1;
  int blw = (ctrlpf & 0x10) ? 4 : 1;

  uint8_t g0 = grp0, g1 = grp1;
  if (refp0 & 0x08)
    g0 = reflect8(g0);
  if (refp1 & 0x08)
    g1 = reflect8(g1);

  for (int c = 0; c < VISIBLE_CLOCKS; ++c) {
    bool pfb = pf_bit(*this, c);

    bool p0on = false;
    int d = c - pos0;
    if (d >= 0 && d / p0w < 8 && (g0 & (1 << (d / p0w))))
      p0on = true;

    bool p1on = false;
    d = c - pos1;
    if (d >= 0 && d / p1w < 8 && (g1 & (1 << (d / p1w))))
      p1on = true;

    bool m0on = false, m1on = false;
    d = c - posm0;
    if (d >= 0 && d < m0w && (enam0 & 0x02))
      m0on = true;
    d = c - posm1;
    if (d >= 0 && d < m1w && (enam1 & 0x02))
      m1on = true;

    bool blon = false;
    d = c - posbl;
    if (d >= 0 && d < blw && (enabl & 0x02))
      blon = true;

    if (m0on && p0on) cxm0p |= 0x01;
    if (m0on && p1on) cxm0p |= 0x02;
    if (m1on && p0on) cxm1p |= 0x01;
    if (m1on && p1on) cxm1p |= 0x02;
    if (p0on && pfb) cxp0fb |= 0x01;
    if (p0on && blon) cxp0fb |= 0x02;
    if (p1on && pfb) cxp1fb |= 0x01;
    if (p1on && blon) cxp1fb |= 0x02;
    if (m0on && pfb) cxm0fb |= 0x01;
    if (m0on && blon) cxm0fb |= 0x02;
    if (m1on && pfb) cxm1fb |= 0x01;
    if (m1on && blon) cxm1fb |= 0x02;
    if (blon && pfb) cxblpf |= 0x01;
    if (blon && p0on) cxblpf |= 0x02;
    if (blon && p1on) cxblpf |= 0x04;
    if (m0on && m1on) cxppmm |= 0x01;
    if (p0on && p1on) cxppmm |= 0x02;

    uint8_t color;
    if (ctrlpf & 0x04) {
      if (pfb)
        color = colupf;
      else if (blon)
        color = colup0;
      else if (m1on)
        color = colup1;
      else if (m0on)
        color = colup0;
      else if (p1on)
        color = colup1;
      else if (p0on)
        color = colup0;
      else
        color = colubk;
    } else {
      if (p1on)
        color = colup1;
      else if (m1on)
        color = colup1;
      else if (blon)
        color = colup0;
      else if (p0on)
        color = colup0;
      else if (m0on)
        color = colup0;
      else if (pfb)
        color = colupf;
      else
        color = colubk;
    }
    row[c] = color & 0x7F;
  }
}

bool Atari2600::step_frame() {
  bool done = false;
  while (!done) {
    step();
    while (clock_in_line >= LINE_CLOCKS) {
      clock_in_line -= LINE_CLOCKS;
      if (scanline >= 40 && scanline < 40 + VISIBLE_LINES)
        render_line();
      ++scanline;
      if (scanline >= LINES_PER_FRAME) {
        scanline = 0;
        done = true;
      }
    }
  }
  return true;
}

// ---------------------------------------------------------------------------
// 6502 CPU (table driven)
// ---------------------------------------------------------------------------
enum Mode { M_IMP,M_IMM,M_ZP,M_ZPX,M_ZPY,M_ABS,M_ABSX,M_ABSY,M_INDX,M_INDY,M_IND,M_REL,M_ACC };
static const uint8_t OP_MODE[256] = {
0,8,0,0,0,2,2,0,0,1,12,0,0,5,5,0,11,9,0,0,0,3,3,0,0,7,0,0,0,6,6,0,5,8,0,0,2,2,2,0,0,1,12,0,5,5,5,0,11,9,0,0,0,3,3,0,0,7,0,0,0,6,6,0,0,8,0,0,0,2,2,0,0,1,12,0,5,5,5,0,11,9,0,0,0,3,3,0,0,7,0,0,0,6,6,0,0,8,0,0,2,2,2,0,0,1,12,0,10,5,5,0,11,9,0,0,0,3,3,0,0,7,0,0,0,6,6,0,0,8,0,0,2,2,2,0,0,0,0,0,5,5,5,0,11,9,0,0,3,3,4,0,0,7,0,0,0,6,0,0,1,8,1,0,2,2,2,0,0,1,0,0,5,5,5,0,11,9,0,0,3,3,4,0,0,7,0,0,6,6,7,0,1,8,0,0,2,2,2,0,0,1,0,0,5,5,5,0,11,9,0,0,0,3,3,0,0,7,0,0,0,6,6,0,1,8,0,0,2,2,2,0,0,1,0,0,5,5,5,0,11,9,0,0,0,3,3,0,0,7,0,0,0,6,6,0
};
static const uint8_t OP_LEN[256] = {
1,2,1,1,1,2,2,1,1,2,1,1,1,3,3,1,2,2,1,1,1,2,2,1,1,3,1,1,1,3,3,1,3,2,1,1,2,2,2,1,1,2,1,1,3,3,3,1,2,2,1,1,1,2,2,1,1,3,1,1,1,3,3,1,1,2,1,1,1,2,2,1,1,2,1,1,3,3,3,1,2,2,1,1,1,2,2,1,1,3,1,1,1,3,3,1,1,2,1,1,2,2,2,1,1,2,1,1,3,3,3,1,2,2,1,1,1,2,2,1,1,3,1,1,1,3,3,1,1,2,1,1,2,2,2,1,1,1,1,1,3,3,3,1,2,2,1,1,2,2,2,1,1,3,1,1,1,3,1,1,2,2,2,1,2,2,2,1,1,2,1,1,3,3,3,1,2,2,1,1,2,2,2,1,1,3,1,1,3,3,3,1,2,2,1,1,2,2,2,1,1,2,1,1,3,3,3,1,2,2,1,1,1,2,2,1,1,3,1,1,1,3,3,1,2,2,1,1,2,2,2,1,1,2,1,1,3,3,3,1,2,2,1,1,1,2,2,1,1,3,1,1,1,3,3,1
};
static const uint8_t OP_CYC[256] = {
7,6,2,2,2,3,5,2,3,2,2,2,2,4,6,2,2,5,2,2,2,4,6,2,2,4,2,2,2,4,7,2,6,6,2,2,3,3,5,2,4,2,2,2,4,4,6,2,2,5,2,2,2,4,6,2,2,4,2,2,2,4,7,2,6,6,2,2,2,3,5,2,3,2,2,2,3,4,6,2,2,5,2,2,2,4,6,2,2,4,2,2,2,4,7,2,6,6,2,2,3,3,5,2,4,2,2,2,5,4,6,2,2,5,2,2,2,4,6,2,2,4,2,2,2,4,7,2,2,6,2,2,3,3,3,2,2,2,2,2,4,4,4,2,2,6,2,2,4,4,4,2,2,5,2,2,2,5,2,2,2,6,2,2,3,3,3,2,2,2,2,2,4,4,4,2,2,5,2,2,4,4,4,2,2,4,2,2,4,4,4,2,2,6,2,2,3,3,5,2,2,2,2,2,4,4,6,2,2,5,2,2,2,4,6,2,2,4,2,2,2,4,7,2,2,6,2,2,3,3,5,2,2,2,2,2,4,4,6,2,2,5,2,2,2,4,6,2,2,4,2,2,2,4,7,2
};

static inline void set_c(Atari2600 &c, bool v) { c.p = v ? (c.p | 0x01) : (c.p & ~0x01); }
static inline void set_z(Atari2600 &c, bool v) { c.p = v ? (c.p | 0x02) : (c.p & ~0x02); }
static inline void set_v(Atari2600 &c, bool v) { c.p = v ? (c.p | 0x40) : (c.p & ~0x40); }
static inline void set_n(Atari2600 &c, bool v) { c.p = v ? (c.p | 0x80) : (c.p & ~0x80); }

void Atari2600::step() {
  uint8_t op = read(pc);
  uint8_t mode = OP_MODE[op];
  uint8_t len = OP_LEN[op];
  uint8_t cyc = OP_CYC[op];
  uint8_t lo, hi;
  uint16_t ea;
  bool branch_taken = false;

  switch (op) {
    case 0x00: { // BRK
      push((uint8_t)((pc + 2) >> 8));
      push((uint8_t)((pc + 2) & 0xFF));
      push(p | 0x10);
      p |= 0x04;
      pc = (uint16_t)((uint16_t)read(0xFFFE) | ((uint16_t)read(0xFFFF) << 8));
      goto done;
    }
    case 0x08: push(p); goto after_op;                    // PHP
    case 0x28: p = pop() | 0x30; goto after_op;           // PLP
    case 0x48: push(a); goto after_op;                    // PHA
    case 0x68: a = pop(); set_nz(a); goto after_op;       // PLA
    case 0x18: set_c(*this, false); goto after_op;        // CLC
    case 0x38: set_c(*this, true); goto after_op;         // SEC
    case 0x58: p &= ~0x08; goto after_op;                 // CLI
    case 0x78: p |= 0x04; goto after_op;                  // SEI
    case 0xB8: p &= ~0x40; goto after_op;                 // CLV
    case 0xD8: p &= ~0x08; goto after_op;                 // CLD
    case 0xF8: p |= 0x08; goto after_op;                  // SED
    case 0xEA: goto after_op;                             // NOP
    case 0x4C: { // JMP abs
      lo = read(pc + 1);
      hi = read(pc + 2);
      pc = (uint16_t)((uint16_t)hi << 8 | lo);
      goto done;
    }
    case 0x6C: { // JMP (ind) with 6502 page-wrap bug
      lo = read(pc + 1);
      hi = read(pc + 2);
      uint16_t ptr = (uint16_t)((uint16_t)hi << 8 | lo);
      uint8_t newlo = read(ptr);
      uint8_t newhi = read((ptr & 0xFF00) | ((ptr + 1) & 0xFF));
      pc = (uint16_t)((uint16_t)newhi << 8 | newlo);
      goto done;
    }
    case 0x20: { // JSR
      lo = read(pc + 1);
      hi = read(pc + 2);
      push((uint8_t)((pc + 2) >> 8));
      push((uint8_t)((pc + 2) & 0xFF));
      pc = (uint16_t)((uint16_t)hi << 8 | lo);
      goto done;
    }
    case 0x60: { // RTS
      lo = pop();
      hi = pop();
      pc = (uint16_t)((uint16_t)hi << 8 | lo) + 1;
      goto done;
    }
    case 0x40: { // RTI
      p = pop() | 0x30;
      lo = pop();
      hi = pop();
      pc = (uint16_t)((uint16_t)hi << 8 | lo);
      goto done;
    }
    default: break;
  }

  // branches
  if (mode == M_REL) {
    int8_t off = (int8_t)read(pc + 1);
    bool take = false;
    switch (op) {
      case 0x10: take = !(p & 0x80); break;
      case 0x30: take = (p & 0x80) != 0; break;
      case 0x50: take = !(p & 0x40); break;
      case 0x70: take = (p & 0x40) != 0; break;
      case 0x90: take = !(p & 0x01); break;
      case 0xB0: take = (p & 0x01) != 0; break;
      case 0xD0: take = !(p & 0x02); break;
      case 0xF0: take = (p & 0x02) != 0; break;
    }
    uint16_t base = pc;
    if (take) {
      branch_taken = true;
      pc = (uint16_t)(base + 2 + off);
    } else {
      pc = (uint16_t)(base + 2);
    }
    goto done;
  }

  // fetch operand address
  lo = read(pc + 1);
  hi = read(pc + 2);
  switch (mode) {
    case M_IMM: ea = 0; break;
    case M_ZP: ea = lo; break;
    case M_ZPX: ea = (lo + x) & 0xFF; break;
    case M_ZPY: ea = (lo + y) & 0xFF; break;
    case M_ABS: ea = (uint16_t)((uint16_t)hi << 8 | lo); break;
    case M_ABSX: ea = (uint16_t)((uint16_t)hi << 8 | lo) + x; break;
    case M_ABSY: ea = (uint16_t)((uint16_t)hi << 8 | lo) + y; break;
    case M_INDX: {
      uint8_t zz = (lo + x) & 0xFF;
      ea = (uint16_t)read(zz) | ((uint16_t)read((zz + 1) & 0xFF) << 8);
      break;
    }
    case M_INDY: {
      uint8_t zz = lo;
      ea = (uint16_t)read(zz) | ((uint16_t)read((zz + 1) & 0xFF) << 8);
      ea += y;
      break;
    }
    case M_IND: {
      uint16_t ptr = (uint16_t)((uint16_t)hi << 8 | lo);
      uint8_t newlo = read(ptr);
      uint8_t newhi = read((ptr & 0xFF00) | ((ptr + 1) & 0xFF));
      ea = (uint16_t)((uint16_t)newhi << 8 | newlo);
      break;
    }
    default: ea = 0; break;
  }

  // transfers / stack / reg ops (fixed 1-byte, ea unused)
  switch (op) {
    case 0xAA: x = a; set_nz(x); goto after_op;      // TAX
    case 0x8A: a = x; set_nz(a); goto after_op;      // TXA
    case 0xA8: y = a; set_nz(y); goto after_op;      // TAY
    case 0x98: a = y; set_nz(a); goto after_op;      // TYA
    case 0x9A: sp = x; goto after_op;                // TXS
    case 0xBA: x = sp; set_nz(x); goto after_op;     // TSX
    case 0xE8: ++x; set_nz(x); goto after_op;        // INX
    case 0xC8: ++y; set_nz(y); goto after_op;        // INY
    case 0xCA: --x; set_nz(x); goto after_op;        // DEX
    case 0x88: --y; set_nz(y); goto after_op;        // DEY
    default: break;
  }

  {
    switch (op) {
      // ---- loads
      case 0xA9: case 0xA5: case 0xA1: case 0xB1: case 0xB5: case 0xB9: case 0xBD:
      case 0xAD: // LDA
        a = (mode == M_IMM) ? lo : read(ea);
        set_nz(a);
        break;
      case 0xA2: case 0xA6: case 0xAE: case 0xB6: case 0xBE: // LDX
        x = (mode == M_IMM) ? lo : read(ea);
        set_nz(x);
        break;
      case 0xA0: case 0xA4: case 0xAC: case 0xB4: case 0xBC: // LDY
        y = (mode == M_IMM) ? lo : read(ea);
        set_nz(y);
        break;
      // ---- stores
      case 0x85: case 0x95: case 0x9D: case 0x99: case 0x81: case 0x91: case 0x8D:
        write(ea, a);
        break;
      case 0x86: case 0x96: case 0x8E:
        write(ea, x);
        break;
      case 0x84: case 0x94: case 0x8C:
        write(ea, y);
        break;
      // ---- INC / DEC
      case 0xE6: case 0xF6: case 0xEE: case 0xFE: { // INC
        uint8_t v = read(ea) + 1;
        write(ea, v);
        set_nz(v);
        break;
      }
      case 0xC6: case 0xD6: case 0xCE: case 0xDE: { // DEC
        uint8_t v = read(ea) - 1;
        write(ea, v);
        set_nz(v);
        break;
      }
      // ---- shifts (accumulator)
      case 0x0A: // ASL A
        set_c(*this, (a & 0x80) != 0);
        a <<= 1;
        set_nz(a);
        break;
      case 0x2A: { // ROL A
        bool c = (p & 0x01) != 0;
        set_c(*this, (a & 0x80) != 0);
        a = (uint8_t)((a << 1) | (c ? 1 : 0));
        set_nz(a);
        break;
      }
      case 0x4A: // LSR A
        set_c(*this, (a & 0x01) != 0);
        a >>= 1;
        set_nz(a);
        break;
      case 0x6A: { // ROR A
        bool c = (p & 0x01) != 0;
        set_c(*this, (a & 0x01) != 0);
        a = (uint8_t)((a >> 1) | (c ? 0x80 : 0));
        set_nz(a);
        break;
      }
      // ---- shifts (memory)
      case 0x06: case 0x16: case 0x0E: case 0x1E: { // ASL mem
        uint8_t v = read(ea);
        set_c(*this, (v & 0x80) != 0);
        v <<= 1;
        write(ea, v);
        set_nz(v);
        break;
      }
      case 0x26: case 0x36: case 0x2E: case 0x3E: { // ROL mem
        uint8_t v = read(ea);
        bool c = (p & 0x01) != 0;
        set_c(*this, (v & 0x80) != 0);
        v = (uint8_t)((v << 1) | (c ? 1 : 0));
        write(ea, v);
        set_nz(v);
        break;
      }
      case 0x46: case 0x56: case 0x4E: case 0x5E: { // LSR mem
        uint8_t v = read(ea);
        set_c(*this, (v & 0x01) != 0);
        v >>= 1;
        write(ea, v);
        set_nz(v);
        break;
      }
      case 0x66: case 0x76: case 0x6E: case 0x7E: { // ROR mem
        uint8_t v = read(ea);
        bool c = (p & 0x01) != 0;
        set_c(*this, (v & 0x01) != 0);
        v = (uint8_t)((v >> 1) | (c ? 0x80 : 0));
        write(ea, v);
        set_nz(v);
        break;
      }
      // ---- logic / arithmetic
      case 0x09: case 0x05: case 0x0D: case 0x01: case 0x11: case 0x15: case 0x19:
      case 0x1D: { // ORA
        a |= (mode == M_IMM) ? lo : read(ea);
        set_nz(a);
        break;
      }
      case 0x29: case 0x25: case 0x2D: case 0x21: case 0x31: case 0x35: case 0x39:
      case 0x3D: { // AND
        a &= (mode == M_IMM) ? lo : read(ea);
        set_nz(a);
        break;
      }
      case 0x49: case 0x45: case 0x4D: case 0x41: case 0x51: case 0x55: case 0x59:
      case 0x5D: { // EOR
        a ^= (mode == M_IMM) ? lo : read(ea);
        set_nz(a);
        break;
      }
      case 0x69: case 0x65: case 0x6D: case 0x61: case 0x71: case 0x75: case 0x79:
      case 0x7D: { // ADC
        uint8_t v = (mode == M_IMM) ? lo : read(ea);
        bool c = (p & 0x01) != 0;
        uint16_t sum = (uint16_t)a + v + (c ? 1 : 0);
        set_v(*this, ((a ^ v) & 0x80) == 0 && ((a ^ sum) & 0x80) != 0);
        a = (uint8_t)sum;
        set_c(*this, sum > 0xFF);
        set_nz(a);
        break;
      }
      case 0xE9: case 0xE5: case 0xED: case 0xE1: case 0xF1: case 0xF5: case 0xF9:
      case 0xFD: { // SBC
        uint8_t v = (mode == M_IMM) ? lo : read(ea);
        bool c = (p & 0x01) != 0;
        uint16_t diff = (uint16_t)a - v - (c ? 0 : 1);
        set_v(*this, ((a ^ v) & 0x80) != 0 && ((a ^ diff) & 0x80) != 0);
        a = (uint8_t)diff;
        set_c(*this, diff < 0x100);
        set_nz(a);
        break;
      }
      case 0xC9: case 0xC5: case 0xCD: case 0xC1: case 0xD1: case 0xD5: case 0xD9:
      case 0xDD: { // CMP
        uint8_t v = (mode == M_IMM) ? lo : read(ea);
        set_c(*this, a >= v);
        set_nz((uint8_t)(a - v));
        break;
      }
      case 0xE0: case 0xE4: case 0xEC: { // CPX
        uint8_t v = (mode == M_IMM) ? lo : read(ea);
        set_c(*this, x >= v);
        set_nz((uint8_t)(x - v));
        break;
      }
      case 0xC0: case 0xC4: case 0xCC: { // CPY
        uint8_t v = (mode == M_IMM) ? lo : read(ea);
        set_c(*this, y >= v);
        set_nz((uint8_t)(y - v));
        break;
      }
      case 0x24: case 0x2C: { // BIT
        uint8_t v = read(ea);
        set_n(*this, (v & 0x80) != 0);
        set_v(*this, (v & 0x40) != 0);
        set_z(*this, (a & v) == 0);
        break;
      }
      default:
        break; // illegal opcodes: NOP
    }
  }

after_op:
  pc += len;
done:
  if (branch_taken) {
    cyc += 1;
  }
  (void)hi;
  (void)ea;
  cycles += cyc;
  clock_in_line += cyc * 3;
}
} // namespace atari
} // namespace tcm
