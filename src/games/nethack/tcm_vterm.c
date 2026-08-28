/* Virtual text terminal for NetHack on TCMIPS.
 * Target screen buffer + dirty tracking.
 * Host: ANSI diffs on stdout. Device: glyphs rendered straight into the
 * ASCII console's RGB332 VRAM (arbitrary positioning, per-cell colors). */
#include "tcm_port.h"
#include "nhfont.h"

#include <stdio.h>
#include <string.h>

#include <stdint.h>
#include <stdlib.h>

#ifdef TCM_HOST
#define TCM_VRAM_ASCII_ADDR 0x03C00000u
void *tcm_sim_fb(void);
static unsigned char *tcms_vram;
#else
#include <dev/console.h>
#include <tcm_config.h>
#define tcms_vram ((unsigned char *) TCM_VRAM_ASCII_ADDR)
#endif

#define VCO_MAX 80
#define VLI_MAX 25

int tcm_vterm_CO = VCO_MAX;
int tcm_vterm_LI = VLI_MAX;

static int cur_fg = 7, cur_bg = 0, cur_bold = 0, cur_rev = 0;

typedef struct {
    unsigned char ch;
    unsigned char attr;
} vcell;

#define BLANK_ATTR ((unsigned char) ((7 & 15) | ((0 & 15) << 4)))

static vcell target[VLI_MAX][VCO_MAX];
static vcell commit[VLI_MAX][VCO_MAX];
static int lx, ly;
static int dirty[VLI_MAX];

static const unsigned char pal[16][3] = {
    {0, 0, 0},      {170, 0, 0},     {0, 170, 0},    {170, 85, 0},
    {0, 0, 170},    {170, 0, 170},   {0, 170, 170},  {170, 170, 170},
    {85, 85, 85},   {255, 85, 85},   {85, 255, 85},  {255, 255, 85},
    {85, 85, 255},  {255, 85, 255},  {85, 255, 255}, {255, 255, 255},
};

static unsigned char
cellattr(int fg, int bg, int bold, int rev)
{
    int f = rev ? bg : fg;
    int b = rev ? fg : bg;
    if (bold && f == 7)
        f = 15;
    return (unsigned char) ((f & 15) | ((b & 15) << 4));
}

static void
set_blank(vcell *p)
{
    p->ch = ' ';
    p->attr = BLANK_ATTR;
}

static void
scroll_up(void)
{
    memmove(&target[0][0], &target[1][0],
            sizeof(vcell) * VCO_MAX * (VLI_MAX - 1));
    for (int x = 0; x < VCO_MAX; x++)
        set_blank(&target[VLI_MAX - 1][x]);
    for (int r = 0; r < VLI_MAX - 1; r++)
        dirty[r] = 1;
}

void
tcm_vterm_putc(int c)
{
    c &= 0xff;
    if (c == '\r') {
        lx = 0;
        return;
    }
    if (c == '\n') {
        lx = 0;
        ly++;
        if (ly >= tcm_vterm_LI) {
            ly = tcm_vterm_LI - 1;
            scroll_up();
        }
        return;
    }
    if (c == '\t') {
        do {
            tcm_vterm_putc(' ');
        } while (lx % 8);
        return;
    }
    if (lx >= tcm_vterm_CO) {
        lx = 0;
        ly++;
        if (ly >= tcm_vterm_LI) {
            ly = tcm_vterm_LI - 1;
            scroll_up();
        }
    }
    target[ly][lx].ch = (unsigned char) c;
    target[ly][lx].attr = cellattr(cur_fg, cur_bg, cur_bold, cur_rev);
    lx++;
    dirty[ly] = 1;
}

void
tcm_vterm_cmov(int x, int y)
{
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    if (x >= tcm_vterm_CO)
        x = tcm_vterm_CO - 1;
    if (y >= tcm_vterm_LI)
        y = tcm_vterm_LI - 1;
    lx = x;
    ly = y;
}

void
tcm_vterm_setattr(int fg, int bg, int bold, int rev)
{
    cur_fg = fg & 15;
    cur_bg = bg & 15;
    cur_bold = bold ? 1 : 0;
    cur_rev = rev ? 1 : 0;
}

void
tcm_vterm_cl_end(void)
{
    for (int x = lx; x < tcm_vterm_CO; x++) {
        vcell b;
        set_blank(&b);
        if (target[ly][x].ch != b.ch || target[ly][x].attr != b.attr) {
            target[ly][x] = b;
            dirty[ly] = 1;
        }
    }
}

void
tcm_vterm_cl_eos(void)
{
    vcell b;
    set_blank(&b);
    tcm_vterm_cl_end();
    for (int r = ly + 1; r < tcm_vterm_LI; r++) {
        for (int x = 0; x < tcm_vterm_CO; x++) {
            if (target[r][x].ch != b.ch || target[r][x].attr != b.attr) {
                target[r][x] = b;
            }
        }
        dirty[r] = 1;
    }
}

#ifndef VFLUSH_HOST
void
tcm_vterm_raw(const char *s)
{
    /* low-level raw message channel: render straight into the vterm so
     * streamed debug/panic output can never desync the scanout */
    while (*s) {
        unsigned char c = (unsigned char) *s++;
        if (c == '\n') {
            lx = 0;
            ly = tcm_vterm_LI - 1;
            continue;
        }
        if (c < ' ')
            continue;
        if (lx >= tcm_vterm_CO) {
            lx = 0;
            ly = tcm_vterm_LI - 1;
        }
        target[ly][lx].ch = c;
        target[ly][lx].attr = BLANK_ATTR;
        lx++;
        dirty[ly] = 1;
    }
}
#endif

void
tcm_vterm_clear(void)
{
    for (int r = 0; r < VLI_MAX; r++)
        for (int c = 0; c < VCO_MAX; c++)
            set_blank(&target[r][c]);
    memset(dirty, 0, sizeof(dirty));
    for (int r = 0; r < tcm_vterm_LI; r++)
        dirty[r] = 1;
    lx = ly = 0;
#ifndef VFLUSH_HOST
    /* main() already ran tcm_ascii_console_init() once, so the hardware
     * scanout is at the base page. Never call driver reset again: its
     * re-init toggles the console mode and its own stream writes would
     * move the scanout away from the page we render into. */
    memset((void *) TCM_VRAM_ASCII_ADDR, 0, 30u * 16u * 640u);
#endif
    memset(commit, 0, sizeof(commit));
}

/* Glyph rendering into the ASCII console framebuffer.
 * Device facts (tcmips/dev/console.c, TCM_CONSOLE_CUSTOM_FONT_8X16):
 * 640px-wide RGB332 framebuffer; text cells are 8x16 pixels; page =
 * 30 rows x 80 cols. We bypass the driver's stream cursor entirely. */
#define NH_FB_W 640

#ifdef TCM_HOST
#define NH_VRAM tcms_vram
#else
#define NH_VRAM tcms_vram
#endif

static inline unsigned char
rgb332(const unsigned char *p)
{
    return (unsigned char) (((p[0] >> 5) << 5) | ((p[1] >> 5) << 2)
                            | (p[2] >> 6));
}

static void
render_cell(int row, int col, const vcell *cl)
{
    const unsigned char *g = &nhfont8x16[(unsigned) cl->ch * 16];
    uint8_t *cell = NH_VRAM + (unsigned) row * 16 * NH_FB_W
                    + (unsigned) col * 8;
    unsigned char fg = rgb332(pal[cl->attr & 0x0f]);
    unsigned char bg = rgb332(pal[(cl->attr >> 4) & 0x0f]);

    for (int r = 0; r < 16; r++) {
        uint8_t *line = cell + r * NH_FB_W;
        unsigned char bits = g[r];
        for (int b = 0; b < 8; b++)
            line[b] = (bits & (0x80 >> b)) ? fg : bg;
    }
}

void
tcm_vterm_flush(void)
{
    for (int r = 0; r < tcm_vterm_LI; r++) {
        if (!dirty[r])
            continue;
        for (int c = 0; c < tcm_vterm_CO; c++) {
            if (!memcmp(&target[r][c], &commit[r][c], sizeof(vcell)))
                continue;
            render_cell(r, c, &target[r][c]);
        }
        memcpy(&commit[r][0], &target[r][0], sizeof(vcell) * tcm_vterm_CO);
        dirty[r] = 0;
    }
}

void
tcm_vterm_init(void)
{
    memset(target, 0, sizeof(target));
    memset(commit, 0, sizeof(commit));
    memset(dirty, 0, sizeof(dirty));
    for (int r = 0; r < VLI_MAX; r++)
        for (int c = 0; c < VCO_MAX; c++) {
            set_blank(&target[r][c]);
            set_blank(&commit[r][c]);
        }
    lx = ly = 0;
#ifdef TCM_HOST
    tcms_vram = tcm_sim_fb();
#endif
}

int
tcm_vterm_cur_x(void)
{
    return lx;
}

int
tcm_vterm_cur_y(void)
{
    return ly;
}

int
tcm_dbg_stats(void)
{
    int n = 0, r, c;
    for (r = 0; r < VLI_MAX; r++)
        for (c = 0; c < VCO_MAX; c++)
            if (target[r][c].ch != ' ')
                n++;
    return n;
}

void
tcm_dbg_text(char *out)
{
    int r, c, n = 0;
    for (r = 0; r < tcm_vterm_LI; r++) {
        for (c = 0; c < tcm_vterm_CO; c++)
            out[n++] = target[r][c].ch;
        out[n++] = '\n';
    }
    out[n] = 0;
}
