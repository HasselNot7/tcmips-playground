// TCMIPS platform glue for x2600 (see x2600_port.h for provenance).

#include <string.h>

#include "types.h"
#include "options.h"
#include "display.h"
#include "vmachine.h"
#include "emuapi.h"
#include "x2600_port.h"

// Globals normally defined in At2600.c / Atari2600EmulatorGlobals.c
int bThreadRunning = 0;
byte nOptions_SkipFrames = 1;
int nOptions_SoundOn = 0;
int nOptions_Interlace = 0;
int nOptions_Landscape = 0;
int nOptions_Color = 1;
int nOptions_P1Diff = 1;
int nOptions_P2Diff = 1;
int pausing = 0;

unsigned int tcm_keys = 0;

static uint32_t tcm_palette[256];

// Static arena instead of heap: all core allocations are fixed size.
// theCart 16K + cartScratch 4K + cartRam 1K + VBuf ~30K + colvect 224
// (allocated lazily by Collision.c) + slack.
static unsigned char arena[65536];
static unsigned int arena_pos = 0;

void *emu_Malloc(int size) {
  unsigned int need = (unsigned int)((size + 7) & ~7);
  if (arena_pos + need > sizeof(arena))
    return 0;
  void *p = &arena[arena_pos];
  arena_pos += need;
  return p;
}

void emu_Free(void *pt) {
  (void)pt;
}

void emu_SetPaletteEntry(unsigned char r, unsigned char g, unsigned char b,
                         int index) {
  if (index >= 0 && index < 256)
    tcm_palette[index] = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

void emu_DrawScreen(unsigned char *VBuf, int width, int height, int stride) {
  (void)VBuf;
  (void)width;
  (void)height;
  (void)stride;
}

void emu_DrawVsync(void) {}

unsigned char *vcs_Framebuffer(void) { return VBuf; }
int vcs_FrameWidth(void) { return tv_width; }
int vcs_FrameHeight(void) { return tv_height; }
const uint32_t *vcs_Palette(void) { return tcm_palette; }

int emu_ReadKeys(void) { return (int)tcm_keys; }
int emu_GetPad(void) { return 0; }

void emu_sndInit(void) {}
void emu_sndPlaySound(int chan, int volume, int freq) {
  (void)chan;
  (void)volume;
  (void)freq;
}
void emu_sndPlayBuzz(int size, int val) {
  (void)size;
  (void)val;
}

int emu_FileOpen(char *filename) {
  (void)filename;
  return 0;
}
int emu_FileRead(char *buf, int size) {
  (void)buf;
  (void)size;
  return 0;
}
unsigned char emu_FileGetc(void) { return 0; }
int emu_FileSeek(int seek) {
  (void)seek;
  return 0;
}
void emu_FileClose(void) {}
int emu_FileSize(char *filename) {
  (void)filename;
  return 0;
}
int emu_LoadFile(char *filename, char *buf, int size) {
  (void)filename;
  (void)buf;
  (void)size;
  return 0;
}
int emu_LoadFileSeek(char *filename, char *buf, int size, int seek) {
  (void)filename;
  (void)buf;
  (void)size;
  (void)seek;
  return 0;
}

void emu_init(void) {}
void emu_printf(char *text) {
  (void)text;
}
void emu_printi(int val) {
  (void)val;
}