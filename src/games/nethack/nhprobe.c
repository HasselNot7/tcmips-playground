/* TCMIPS ASCII console geometry probe for NetHack.
 * Phase 1: stream digits to reveal natural wrap width.
 * Phase 2: place markers at candidate strides via SET_DATA_OFFSET.
 * Press a key between phases. */
#include <stdio.h>
#include <dev/console.h>
#include <dev/syscall.h>
#include <dev/keyboard.h>

static void
wait_key(const char *msg)
{
    tcm_ascii_console_write_string(msg);
    tcm_ascii_console_write_string(" [press key]\r\n");
    while (!tcm_keyboard_get_code()) {
    }
}

int
main(void)
{
    int i;
    unsigned int off;

    tcm_ascii_console_init();
    tcm_ascii_console_clear();

    /* phase 1: natural wrap width */
    tcm_ascii_console_write_string("PHASE1 wrap-width ruler:\r\n");
    for (i = 0; i < 300; i++) {
        char c = (char) ('0' + (i % 10));
        tcm_ascii_console_write_char(c);
        if (i % 50 == 49)
            tcm_ascii_console_write_char('|'); /* marker every 50 */
    }
    wait_key("\r\nPHASE1 done");

    /* phase 2: absolute positioning via SET_DATA_OFFSET (cmd 1) */
    tcm_ascii_console_clear();
    tcm_ascii_console_write_string("PHASE2 offset probes\r\n");
    {
        static const unsigned int cands[] = { 40,  48,  64,  72,  80,
                                              96,  100, 120, 128, 132,
                                              160, 192, 200, 240, 256 };
        for (i = 0; i < (int) (sizeof(cands) / sizeof(cands[0])); i++) {
            off = cands[i];
            tcm_syscall_ascii_console(1, off);
            tcm_ascii_console_write_char((char) ('A' + i));
            tcm_syscall_ascii_console(1, off);
            tcm_ascii_console_write_string("*"); /* overwrite marker char */
        }
        /* restore stream position to bottom */
        tcm_ascii_console_write_string("\r\n\r\n");
    }
    tcm_ascii_console_write_string(
        "Report: digits-per-row from PHASE1,\r\n");
    tcm_ascii_console_write_string(
        "and which letters (A..O) appeared + where.\r\n");
    wait_key("");
    tcm_ascii_console_clear();
    return 0;
}
