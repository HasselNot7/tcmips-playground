// Correct joystick / console-switch / fire-button input for x2600 on TCMIPS.
// Replaces espMCUME's Keyboard.c, whose SWCHA nibble mapping did not match
// the Stella programmer's guide (UP cleared the "right" bit and P0 was
// hard-wired idle).
//
// SWCHA: bit3..0 = P1? no: bit3 P0-up, bit2 P0-down, bit1 P0-left,
//        bit0 P0-right; bits 7..4 same order for P1. Active low.
// SWCHB: bit0 reset, bit1 select (active low), bit3 colour(1),
//        bit6/7 P0/P1 difficulty (1 = A).
// INPT4/INPT5: fire buttons, bit7 clear when pressed.

#include "types.h"
#include "address.h"
#include "vmachine.h"
#include "emuapi.h"

void keyjoy(void) {
  unsigned int key = emu_ReadKeys();
  BYTE sw = 0xFF;

  if (key & MASK_JOY2_UP)
    sw &= ~0x08; // P0 up
  if (key & MASK_JOY2_DOWN)
    sw &= ~0x04; // P0 down
  if (key & MASK_JOY2_LEFT)
    sw &= ~0x02; // P0 left
  if (key & MASK_JOY2_RIGHT)
    sw &= ~0x01; // P0 right
  if (key & MASK_JOY1_UP)
    sw &= ~0x80; // P1 up
  if (key & MASK_JOY1_DOWN)
    sw &= ~0x40; // P1 down
  if (key & MASK_JOY1_LEFT)
    sw &= ~0x20; // P1 left
  if (key & MASK_JOY1_RIGHT)
    sw &= ~0x10; // P1 right

  riotRead[SWCHA] = sw;
}

void keycons(void) {
  unsigned int key = emu_ReadKeys();

  riotRead[SWCHB] |= 0xC8; // colour on, both difficulty A, switches up
  if (key & MASK_KEY_USER1)
    riotRead[SWCHB] &= ~0x01; // game reset held
  if (key & MASK_KEY_USER2)
    riotRead[SWCHB] &= ~0x02; // game select held
}

void keytrig(void) {
  unsigned int key = emu_ReadKeys();

  if (!(tiaWrite[VBLANK] & 0x40)) {
    tiaRead[INPT4] = (key & MASK_JOY2_BTN) ? 0x00 : 0x80;
    tiaRead[INPT5] = (key & MASK_JOY1_BTN) ? 0x00 : 0x80;
  }
}