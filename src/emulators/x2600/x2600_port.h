// TCMIPS port glue for the x2600 Atari 2600 emulator core.
//
// Core lineage: x2600 by Alex Hornby (1996, GPL-2.0) -> PocketVCS by Stuart
// Russell -> espvcs by Jean-Marc Harvengt (espMCUME). This file provides the
// emu_* platform hooks the core expects, plus the ROM loader and frame API
// used by the TCMIPS shell.

#ifndef X2600_PORT_H
#define X2600_PORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Load a cartridge from memory and (re)start the machine.
// Supports 2K/4K (no bankswitch), 8K F8, 12K FA(CBS), 16K F6.
void vcs_LoadROM(const uint8_t *data, int size);

// Run exactly one video frame (262/312 scanlines).
void vcs_Step(void);

// Input state, set by the shell every frame before vcs_Step().
// Bit meanings follow emuapi.h MASK_* conventions.
extern unsigned int tcm_keys;

// Framebuffer: 160x192 (NTSC) bytes of raw TIA colour values, and the
// 256-entry RGB888 palette built from the core's colortable.
unsigned char *vcs_Framebuffer(void);
int vcs_FrameWidth(void);
int vcs_FrameHeight(void);
const uint32_t *vcs_Palette(void);

#ifdef __cplusplus
}
#endif

#endif // X2600_PORT_H