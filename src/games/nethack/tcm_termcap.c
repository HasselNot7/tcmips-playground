/* Replacement for win/tty/termcap.c on TCMIPS.
 * Provides the same exported symbols but renders through tcm_vterm. */
#include "hack.h"
#include "tcap.h"
#include "wintty.h"

#include "tcm_port.h"

char NEARDATA *hilites[CLR_MAX]; /* terminal escapes (unused here) */

char erase_char = 8;    /* ^H / DEL: erase previous char in getline */
char kill_char = 21;    /* ^U: kill entire input line */

short ospeed = 0;

struct tc_lcl_data tc_lcl_data = { 0, 0, 0, 0, 0, 0, 0, FALSE };

static char nullstr[] = "";

/* attribute state: NetHack HL_* masks */
static int curattr = 0;
static int curcolor = -1;

void
term_startup(int *wid, int *hgt)
{
    *wid = CO = 80;
    *hgt = LI = 25;
    AS = AE = (char *) nullstr; /* no graphics font switching */
    nh_CM = nh_ND = nh_CD = nh_HI = nh_HE = nh_US = nh_UE = (char *) 0;
    ul_hack = FALSE;
    tcm_vterm_init();
}

void
term_shutdown(void)
{
}

/* tty driver replacements (unixtty.c equivalents) */
void
gettty(void)
{
    erase_char = 8;
    kill_char = 21;
}

void
settty(const char *s)
{
    if (s && *s)
        raw_print(s);
}

void
setftty(void)
{
}

void
in_raw(void)
{
}

void
restore_saved_tty(void)
{
}

void
tty_number_pad(int state UNUSED)
{
}

void
tty_decgraphics_termcap_fixup(void)
{
}

void
tty_ascgraphics_hilite_fixup(void)
{
}

void
term_start_screen(void)
{
}

void
term_end_screen(void)
{
}

void
nocmov(int x, int y)
{
    tcm_vterm_cmov(x, y);
}

void
cmov(int x, int y)
{
    tcm_vterm_cmov(x, y);
}

int
xputc(int c)
{
    if (c == '\n')
        tcm_vterm_putc('\n');
    else if (c == '\r')
        tcm_vterm_putc('\r');
    else if (c >= ' ')
        tcm_vterm_putc(c);
    return c;
}

void
xputs(const char *s)
{
    if (!s)
        return;
    while (*s)
        xputc(*s++);
}

void
cl_end(void)
{
    tcm_vterm_cl_end();
    tcm_vterm_flush();
}

void
term_clear_screen(void)
{
    tcm_vterm_clear();
    tcm_vterm_flush();
}

void
home(void)
{
    tcm_vterm_cmov(0, 0);
}

static void
apply_attr(void)
{
    int fg = 7, bg = 0, rev = 0, bold = 0;
    if (curcolor >= 0)
        fg = curcolor & 15;
    if (curattr & HL_BOLD)
        bold = 1;
    if (curattr & HL_INVERSE) {
        rev = 1;
        fg = 0;
        bg = 7;
    }
    if (curattr & HL_DIM)
        fg = (fg == 7) ? 8 : fg;
    if (curattr & HL_BLINK)
        rev = rev ? 0 : 1;
    tcm_vterm_setattr(fg, bg, bold, rev);
}

void
standoutbeg(void)
{
    curattr |= HL_INVERSE;
    apply_attr();
}

void
standoutend(void)
{
    curattr &= ~HL_INVERSE;
    apply_attr();
}

void
revbeg(void)
{
    standoutbeg();
}

void
boldbeg(void)
{
    curattr |= HL_BOLD;
    apply_attr();
}

void
blinkbeg(void)
{
    curattr |= HL_BLINK;
    apply_attr();
}

void
dimbeg(void)
{
    curattr |= HL_DIM;
    apply_attr();
}

void
m_end(void)
{
    curattr = 0;
    curcolor = -1;
    apply_attr();
}

void
backsp(void)
{
    /* rarely used; move cursor left by rewriting via cmov is impossible
       from vterm alone, so emulate with a space erase at current pos-1 */
    extern int tcm_vterm_cur_x(void), tcm_vterm_cur_y(void);
    int x = tcm_vterm_cur_x() - 1;
    if (x < 0)
        x = 0;
    tcm_vterm_cmov(x, tcm_vterm_cur_y());
}

void
tty_nhbell(void)
{
}

void
graph_on(void)
{
}

void
graph_off(void)
{
}

void
tty_delay_output(void)
{
#ifdef TIMED_DELAY
    msleep(50); /* sleep between buffered output lines */
#endif
}

void
cl_eos(void) /* clear from cursor to end of screen */
{
    tcm_vterm_cl_eos();
    tcm_vterm_flush();
}

const char *
s_atr2str(int n)
{
    (void) n;
    return nullstr;
}

const char *
e_atr2str(int n)
{
    (void) n;
    return nullstr;
}

int
term_attr_fixup(int msk)
{
    return msk;
}

void
term_start_attr(int attr)
{
    switch (attr) {
    case ATR_BOLD:
        boldbeg();
        break;
    case ATR_INVERSE:
        standoutbeg();
        break;
    case ATR_ULINE:
    case ATR_BLINK:
        blinkbeg();
        break;
    case ATR_DIM:
        dimbeg();
        break;
    default:
        break;
    }
}

void
term_end_attr(int attr UNUSED)
{
    m_end();
}

void
term_start_raw_bold(void)
{
    boldbeg();
}

void
term_end_raw_bold(void)
{
    m_end();
}

void
term_start_color(int color)
{
    curcolor = color & 15;
    apply_attr();
}

void
term_end_color(void)
{
    curcolor = -1;
    apply_attr();
}

void
term_start_bgcolor(int color UNUSED)
{
}

void
term_curs_set(int visibility UNUSED)
{
}

void
tty_change_color(int color UNUSED, long rgb UNUSED, int reverse UNUSED)
{
}

void
term_start_extracolor(uint32 customcolor UNUSED,
                       uint16 color256idx UNUSED)
{
}

void
term_end_extracolor(void)
{
}
