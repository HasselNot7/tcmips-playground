/* Minimal vterm test - init console, render via tcm_vterm, blink */
#include <dev/console.h>
#include <dev/keyboard.h>

extern void tcm_vterm_init(void);
extern void tcm_vterm_clear(void);
extern void tcm_vterm_flush(void);
extern void tcm_vterm_putc(int);
extern void tcm_vterm_cmov(int, int);

int main(void) {
    tcm_ascii_console_init();
    tcm_vterm_init();

    tcm_vterm_clear();
    tcm_vterm_cmov(10, 5);
    const char *msg = "VTEST2";
    for (int i = 0; msg[i]; i++)
        tcm_vterm_putc(msg[i]);
    tcm_vterm_flush();

    /* idle: confirm alive, scan for keypress */
    for (;;) {
        volatile unsigned char *v = (volatile unsigned char *) 0x03C00000;
        for (volatile int d = 0; d < 300000; d++) {}
        v[0] = v[0] ? 0 : 0xFF;
        if (tcm_keyboard_get_code() == 27 /* ESC */)
            break;
    }
    return 0;
}
