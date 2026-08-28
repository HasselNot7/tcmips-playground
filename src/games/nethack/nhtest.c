/* Minimal TCMIPS VRAM rendering test - no NetHack, no vterm */
#include <dev/console.h>
#include <dev/syscall.h>

/* 8x16 font for a few characters (H, E, L, O) */
static const unsigned char font_hi[4][16] = {
    /* H */ {0x00,0x00,0x66,0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x66,0x66,0x00,0x00,0x00,0x00},
    /* E */ {0x00,0x00,0x7E,0x60,0x60,0x60,0x7C,0x60,0x60,0x60,0x7E,0x00,0x00,0x00,0x00,0x00},
    /* L */ {0x00,0x00,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00,0x00,0x00,0x00,0x00},
    /* O */ {0x00,0x00,0x3C,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00,0x00,0x00,0x00,0x00},
};

static void put_cell(int row, int col, const unsigned char *glyph,
                     unsigned char fg, unsigned char bg) {
    volatile unsigned char *v = (volatile unsigned char *) 0x03C00000;
    v += (unsigned) row * 16 * 640 + (unsigned) col * 8;
    for (int r = 0; r < 16; r++) {
        volatile unsigned char *line = v + r * 640;
        unsigned char bits = glyph[r];
        for (int b = 0; b < 8; b++)
            line[b] = (bits & (0x80 >> b)) ? fg : bg;
    }
}

int main(void) {
    /* init console to pixel_8 mode */
    tcm_ascii_console_init();

    /* clear first page */
    volatile unsigned char *v = (volatile unsigned char *) 0x03C00000;
    for (unsigned i = 0; i < 30u * 16u * 640u; i++) v[i] = 0;

    /* render "HELLO" at row 5, starting col 10 */
    const char *msg = "HELLO";
    for (int i = 0; msg[i]; i++) {
        int gi = msg[i] - 'H'; /* H=0 E=1 L=2 L=2 O=3 */
        if (msg[i] == 'O') gi = 3;
        put_cell(5, 10 + i, font_hi[gi], 0xFF, 0x00);
    }

    /* done - just idle */
    for (;;) {
        /* blink the border to show we're alive */
        for (volatile int d = 0; d < 100000; d++) {}
        v[0] = v[0] ? 0 : 0xFF;
    }
    return 0;
}
