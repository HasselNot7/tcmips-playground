/* Virtual text terminal for NetHack on TCMIPS.
 * Maintains a target screen buffer; flushes diffs to the streaming
 * ASCII console using CONSOLE_CMD_SET_DATA_OFFSET for positioning. */
#include "tcm_port.h"

#include <stdio.h>
#include <string.h>

#ifdef TCM_HOST
#define VFLUSH_HOST 1
#else
#include <dev/console.h>
#include <dev/syscall.h>
#endif

#define VCO_MAX 80
#define VLI_MAX 25

int tcm_vterm_CO = VCO_MAX;
int tcm_vterm_LI = VLI_MAX;

/* attribute byte: low nibble fg palette index, high nibble bg */
static int cur_fg = 7, cur_bg = 0, cur_bold = 0, cur_rev = 0;

typedef struct {
    unsigned char ch;
    unsigned char attr;
} vcell;

#define BLANK_ATTR ((unsigned char) ((7 & 15) | ((0 & 15) << 4)))

static vcell target[VLI_MAX][VCO_MAX];
static vcell commit[VLI_MAX][VCO_MAX];
static int lx, ly; /* logical cursor */
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

/* erase from cursor to end of line */
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

/* clear from cursor to end of screen */
void
tcm_vterm_cl_eos(void)
{
    tcm_vterm_cl_end();
    for (int r = ly + 1; r < tcm_vterm_LI; r++) {
        vcell b;
        set_blank(&b);
        for (int x = 0; x < tcm_vterm_CO; x++) {
            if (target[r][x].ch != b.ch || target[r][x].attr != b.attr) {
                target[r][x] = b;
            }
        }
        dirty[r] = 1;
    }
}

void
tcm_vterm_clear(void)
{
    for (int r = 0; r < tcm_vterm_LI; r++) {
        memset(&target[r][0], 0, sizeof(vcell) * tcm_vterm_CO);
        dirty[r] = 1;
    }
    for (int r = 0; r < VLI_MAX; r++)
        for (int c = 0; c < VCO_MAX; c++)
            set_blank(&target[r][c]);
    lx = ly = 0;
#ifdef VFLUSH_HOST
    fputs("\033[2J\033[H", stdout);
    fflush(stdout);
#else
    tcm_ascii_console_clear();
#endif
    memset(commit, 0, sizeof(commit));
}

#ifdef VFLUSH_HOST
static void
hw_pos(int row, int col)
{
    printf("\033[%d;%dH", row + 1, col + 1);
}
static void
hw_color(unsigned char attr)
{
    static unsigned char last = 0xff;
    if (attr == last)
        return;
    last = attr;
    static const int ansi[16] = { 30, 31, 32, 33, 34, 35, 36, 37,
                                  90, 91, 92, 93, 94, 95, 96, 97 };
    printf("\033[%d;%dm", ansi[attr & 0x0f], ansi[(attr >> 4) & 0x0f] + 10);
}
#else
void
tcm_console_set_data_offset(unsigned int off)
{
    /* ascii console cmd 1 = CONSOLE_CMD_SET_DATA_OFFSET */
    tcm_syscall_ascii_console(1, off);
}
static void
hw_pos(int row, int col, int stride)
{
    tcm_console_set_data_offset((unsigned int) (row * stride + col));
}
static void
hw_color(unsigned char attr)
{
    static unsigned char last = 0xff;
    if (attr == last)
        return;
    last = attr;
    tcm_ascii_console_set_color(pal[attr & 0x0f][0], pal[attr & 0x0f][1],
                                pal[attr & 0x0f][2], pal[(attr >> 4) & 0x0f][0],
                                pal[(attr >> 4) & 0x0f][1],
                                pal[(attr >> 4) & 0x0f][2]);
}
#endif

/* stride between console rows in cells; probed/adjusted on device */
int tcm_vterm_stride = VCO_MAX;

void
tcm_vterm_flush(void)
{
    char line[VCO_MAX + 1];
#ifndef VFLUSH_HOST
    int stride = tcm_vterm_stride;
#endif
    for (int r = 0; r < tcm_vterm_LI; r++) {
        if (!dirty[r])
            continue;
        int c = 0;
        while (c < tcm_vterm_CO) {
            if (!memcmp(&target[r][c], &commit[r][c], sizeof(vcell))) {
                c++;
                continue;
            }
            int start = c;
            unsigned char at = target[r][c].attr;
            while (c < tcm_vterm_CO && memcmp(&target[r][c], &commit[r][c],
                                              sizeof(vcell))
                   && target[r][c].attr == at)
                c++;
            int n = c - start;
#ifdef VFLUSH_HOST
            hw_pos(r, start);
#else
            hw_pos(r, start, stride);
#endif
            hw_color(at);
            for (int i = 0; i < n; i++)
                line[i] = (char) target[r][start + i].ch;
            line[n] = 0;
#ifdef VFLUSH_HOST
            fwrite(line, 1, (size_t) n, stdout);
#else
            tcm_ascii_console_write_string(line);
#endif
        }
        memcpy(&commit[r][0], &target[r][0], sizeof(vcell) * tcm_vterm_CO);
        dirty[r] = 0;
    }
#ifdef VFLUSH_HOST
    fflush(stdout);
#endif
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
