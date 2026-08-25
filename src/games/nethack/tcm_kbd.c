/* Keyboard input for NetHack on TCMIPS. */
#include "tcm_port.h"

#ifdef TCM_HOST
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#else
#include <dev/keyboard.h>
#include <dev/syscall.h>
#endif

int
tcm_getch(void)
{
#ifdef TCM_HOST
    static int pending = -1;
    unsigned char c;
    if (pending >= 0) {
        int p = pending;
        pending = -1;
        return p;
    }
    if (read(0, &c, 1) != 1)
        return '\033';
    if (c == 27) {
        /* escape sequence? */
        unsigned char b;
        struct termios ts, ots;
        tcgetattr(0, &ts);
        ots = ts;
        ts.c_cc[VMIN] = 0;
        ts.c_cc[VTIME] = 1; /* 0.1s */
        tcsetattr(0, TCSANOW, &ts);
        if (read(0, &b, 1) == 1) {
            if (b == '[' || b == 'O') {
                if (read(0, &b, 1) == 1) {
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
