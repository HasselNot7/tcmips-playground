/* Keyboard input for NetHack on TCMIPS. */
#include "tcm_port.h"

#ifdef TCM_HOST
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

#undef read /* host stdin must stay real read(); tcm_port.h renames it */
extern ssize_t read(int, void *, size_t); /* real libc read */

void tcm_sim_screenshot(const char *path);
static unsigned char
sim_readbyte(void)
{
    unsigned char c = 0;
    if (read(0, &c, 1) != 1)
        return 0;
    return c;
}

static void
sim_handle_shot(void)
{
    char name[128];
    int i = 0;
    char full[160];
    for (;;) {
        unsigned char c = sim_readbyte();
        if (!c || c == 0x02 || i >= 120)
            break;
        name[i++] = (char) c;
    }
    name[i] = 0;
    snprintf(full, sizeof full, "shots/%s", name);
    tcm_sim_screenshot(full);
    fprintf(stderr, "[shot %s]\n", full);
}
#else
#include <dev/keyboard.h>
#include <dev/syscall.h>
#endif

int
tcm_getch(void)
{
    /* Ensure the vterm display is current before waiting for input.
     * Without this, characters drawn since the last explicit flush
     * stay in the target buffer and are never rendered to VRAM. */
    tcm_vterm_flush();
#ifdef TCM_HOST
    static int pending = -1;
    unsigned char c;
    if (pending >= 0) {
        int p = pending;
        pending = -1;
        return p;
    }
    if ((read)(0, &c, 1) != 1)
        return '\033';
    if (c == 0x01) {
        sim_handle_shot();
        return tcm_getch(); /* recurse for the next real key */
    }
    if (c == 27) {
        /* escape sequence? */
        unsigned char b;
        struct termios ts, ots;
        tcgetattr(0, &ts);
        ots = ts;
        ts.c_cc[VMIN] = 0;
        ts.c_cc[VTIME] = 1; /* 0.1s */
        tcsetattr(0, TCSANOW, &ts);
        if ((read)(0, &b, 1) == 1) {
            if (b == '[' || b == 'O') {
                if ((read)(0, &b, 1) == 1) {
                    tcsetattr(0, TCSANOW, &ots);
                    switch (b) {
                    case 'A':
                        return 'k';
                    case 'B':
                        return 'j';
                    case 'C':
                        return 'l';
                    case 'D':
                        return 'h';
                    case 'H':
                        return 'y';
                    case 'F':
                        return 'n';
                    default:
                        return '\033';
                    }
                }
                tcsetattr(0, TCSANOW, &ots);
                return '\033';
            }
            tcsetattr(0, TCSANOW, &ots);
            pending = b;
            return '\033';
        }
        tcsetattr(0, TCSANOW, &ots);
        return '\033';
    }
    if (c == '\r')
        c = '\n';
    if (c == 127)
        c = 8;
    if (getenv("NHKBDLOG"))
        fprintf(stderr, "[kbd %02x '%c']\n", c, c >= 32 && c < 127 ? c : '.');
    return c;
#else
    for (;;) {
        uint32_t code = tcm_keyboard_get_code();
        if (!code)
            continue;
        switch (code) {
        case __TCM_KEY_CODE_UP:
            return 'k';
        case __TCM_KEY_CODE_DOWN:
            return 'j';
        case __TCM_KEY_CODE_LEFT:
            return 'h';
        case __TCM_KEY_CODE_RIGHT:
            return 'l';
        case __TCM_KEY_CODE_BACKSPACE: /* no ESC key on this machine */
            return '\033';
        case __TCM_KEY_CODE_DEL:
            return 8;
        case __TCM_KEY_CODE_ENTER:
            return '\n';
        case __TCM_KEY_CODE_TAB:
            return '\t';
        default:
            if (code >= 32 && code < 127)
                return (int) code;
            break;
        }
    }
#endif
}
