/* Host simulation of the TCMIPS ASCII console driver.
 * Mirrors tcmips/dev/console.c (TCM_CONSOLE_CUSTOM_FONT_8X16) semantics:
 * 640-wide RGB332 framebuffer pages at TCM_VRAM_ASCII_ADDR, streaming
 * cursor with auto_scroll, SET_DATA_OFFSET scanout, reset(). */
#ifdef TCM_HOST

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>

#define TCM_VRAM_ASCII_ADDR 0x03C00000u
#define CONSOLE_CMD_SET_MODE 0
#define CONSOLE_CMD_SET_DATA_OFFSET 1

#include "nhfont.h"

#define SIM_W 640
#define SIM_CELLS_X 80
#define SIM_CELLS_Y 30
#define SIM_PAGE (SIM_CELLS_Y * 16 * SIM_W)
#define SIM_PAGES 3

static uint8_t *sim_fb;
static uint32_t sim_curr_base;
static uint32_t sim_scanout;
static int sim_cx, sim_cy;
static uint8_t sim_fg332 = 0xe0 >> 0, sim_bg332;
static int sim_mode_set;

uint8_t *tcm_sim_fb(void) { return sim_fb; }
uint32_t tcm_sim_scanout(void) { return sim_scanout; }

void
tcm_syscall_ascii_console(uint32_t cmd, uint32_t a1)
{
    switch (cmd) {
    case CONSOLE_CMD_SET_MODE:
        sim_mode_set = 1;
        break;
    case CONSOLE_CMD_SET_DATA_OFFSET:
        sim_scanout = a1;
        break;
    default:
        break;
    }
}

static void
sim_put_glyph(uint8_t ch)
{
    uint32_t cx = (uint32_t) sim_cx, cy = (uint32_t) sim_cy;
    const unsigned char *g = &nhfont8x16[(unsigned) ch * 16];
    uint8_t *cell = (uint8_t *) sim_curr_base + cy * 16 * SIM_W + cx * 8;
    int r, b;
    for (r = 0; r < 16; r++) {
        uint8_t *line = cell + r * SIM_W;
        unsigned char bits = g[r];
        for (b = 0; b < 8; b++)
            line[b] = (bits & (0x80 >> b)) ? sim_fg332 : sim_bg332;
    }
}

static void
sim_auto_scroll(void)
{
    if (sim_cy >= SIM_CELLS_Y) {
        sim_curr_base += (uint32_t) SIM_W * 16;
        sim_cy = SIM_CELLS_Y - 1;
        if (sim_curr_base + SIM_PAGE >=
            TCM_VRAM_ASCII_ADDR + SIM_PAGE * SIM_PAGES)
            memset(sim_fb, 0, SIM_PAGE * SIM_PAGES);
        sim_scanout = sim_curr_base;
    }
}

void
tcm_ascii_console_write_char(char c)
{
    if (!sim_mode_set)
        return;
    switch (c) {
    case '\r':
        sim_cx = 0;
        break;
    case '\n':
        sim_cx = 0;
        sim_cy++;
        sim_auto_scroll();
        break;
    case '\b':
        if (sim_cx > 0) {
            sim_cx--;
            sim_put_glyph(0);
        }
        break;
    default:
        if ((unsigned char) c >= ' ') {
            sim_put_glyph((unsigned char) c);
            sim_cx++;
            if (sim_cx >= SIM_CELLS_X) {
                sim_cx = 0;
                sim_cy++;
                sim_auto_scroll();
            }
        }
        break;
    }
}

uint32_t
tcm_ascii_console_write_string(const char *s)
{
    uint32_t n = 0;
    while (*s) {
        tcm_ascii_console_write_char(*s++);
        n++;
    }
    return n;
}

void
tcm_ascii_console_set_color(uint8_t fr, uint8_t fg, uint8_t fb, uint8_t br,
                            uint8_t bg, uint8_t bb)
{
    sim_fg332 = (uint8_t) (((fr >> 5) << 5) | ((fg >> 5) << 2) | (fb >> 6));
    sim_bg332 = (uint8_t) (((br >> 5) << 5) | ((bg >> 5) << 2) | (bb >> 6));
}

void
tcm_ascii_console_clear(void)
{
    /* driver semantics: advance to next line boundary, scroll if needed */
    sim_curr_base += (uint32_t) ((sim_cy + 1) * 16) * SIM_W;
    sim_cx = sim_cy = 0;
    sim_auto_scroll();
}

void
tcm_ascii_console_reset(void)
{
    memset(sim_fb, 0, SIM_PAGE * SIM_PAGES);
    sim_curr_base = TCM_VRAM_ASCII_ADDR;
    sim_scanout = sim_curr_base;
    sim_cx = sim_cy = 0;
}

void
tcm_ascii_console_init(void)
{
    if (!sim_fb) {
        sim_fb = mmap((void *) TCM_VRAM_ASCII_ADDR, SIM_PAGE * SIM_PAGES,
                      PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        if (sim_fb == MAP_FAILED) {
            perror("sim mmap");
            exit(3);
        }
    }
    memset(sim_fb, 0, SIM_PAGE * SIM_PAGES);
    sim_curr_base = TCM_VRAM_ASCII_ADDR;
    sim_scanout = sim_curr_base;
    sim_cx = sim_cy = 0;
    sim_fg332 = 0xff;
    sim_bg332 = 0x00;
    sim_mode_set = 1;
}

/* dump the currently displayed page to a PPM */
void
tcm_sim_screenshot(const char *path)
{
    FILE *f = fopen(path, "wb");
    uint32_t row, col;
    if (!f)
        return;
    setvbuf(f, NULL, _IONBF, 0);
    fprintf(f, "P6\n%d %d\n255\n", SIM_W, SIM_CELLS_Y * 16);
    {
        extern int tcm_dbg_stats(void);
        extern void tcm_dbg_text(char *out);
        char txt[4096];
        tcm_dbg_text(txt);
        fprintf(stderr, "[sim shot %s nonzero=%d scan=%#x base=%#x]\n",
                path, tcm_dbg_stats(), sim_scanout, sim_curr_base);
        FILE *t = fopen(strcat(strcpy((char[256]){0}, path), ".txt"), "w");
        if (t) {
            fputs(txt, t);
            fclose(t);
        }
    }
    for (row = 0; row < SIM_CELLS_Y * 16; row++) {
        const uint8_t *line =
            (const uint8_t *) sim_scanout + row * SIM_W;
        for (col = 0; col < SIM_W; col++) {
            uint8_t v = line[col];
            uint8_t rgb[3];
            rgb[0] = (v >> 5) & 7;
            rgb[1] = (v >> 2) & 7;
            rgb[2] = v & 3;
            fputc(rgb[0] * 255 / 7, f);
            fputc(rgb[1] * 255 / 7, f);
            fputc(rgb[2] * 255 / 3, f);
        }
    }
    fclose(f);
}

#endif /* TCM_HOST */
