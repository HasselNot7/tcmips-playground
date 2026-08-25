/* TCMIPS ASCII console geometry probe, v2.
 * P1: stream ruler (wrap width).  P2..P4: SET_DATA_OFFSET addressing
 * with multipliers 1/2/4 -- exactly one multiplier draws a clean
 * right-edge column of '#' plus matching left-edge column. */
#include <dev/console.h>
#include <dev/syscall.h>
#include <dev/keyboard.h>

static void
nap(int ms)
{
    uint32_t start = tcm_syscall_get_timestamp_micro();
    while ((uint32_t) (tcm_syscall_get_timestamp_micro() - start)
           < (uint32_t) ms * 1000u) {
    }
}

static void
wait_key(void)
{
    /* consume until an event arrives, then until the queue stays empty,
       so buffered press/release events cannot skip phases */
    for (;;)
        if (tcm_keyboard_get_code())
            break;
    nap(200);
    for (;;) {
        unsigned int a = tcm_keyboard_get_code();
        unsigned int b = tcm_keyboard_get_code();
        if (!a && !b)
            break;
    }
    nap(200);
}

static void
pos_write(unsigned int off, const char *s)
{
    tcm_syscall_ascii_console(1, off); /* CONSOLE_CMD_SET_DATA_OFFSET */
    tcm_ascii_console_write_string(s);
}

int
main(void)
{
    static const struct {
        int mult;
        const char *label;
    } cands[] = { { 1, "P2: offset unit = CELLS" },
                  { 2, "P3: offset unit = BYTES x2" },
                  { 4, "P4: offset unit = BYTES x4" } };
    int i, r;

    tcm_ascii_console_init();
    tcm_ascii_console_clear();

    /* ---- phase 1: natural wrap width ---- */
    tcm_ascii_console_write_string("P1 wrap ruler:\r\n");
    for (i = 0; i < 240; i++) {
        char c = (char) ('0' + (i % 10));
        tcm_ascii_console_write_char(c);
        if (i % 40 == 39)
            tcm_ascii_console_write_char('|'); /* every 40 cells */
    }
    wait_key();

    /* ---- phases 2..4 ---- */
    for (i = 0; i < 3; i++) {
        tcm_ascii_console_clear();
        pos_write((unsigned int) (1 * 80 + 10) * cands[i].mult,
                  cands[i].label); /* label via offset too, tests row 1 */
        for (r = 3; r < 24; r++) {
            pos_write((unsigned int) (r * 80 + 0) * cands[i].mult, "#");
            pos_write((unsigned int) (r * 80 + 79) * cands[i].mult, "#");
            if (r % 5 == 3)
                pos_write((unsigned int) (r * 80 + 40) * cands[i].mult, "+");
        }
        wait_key();
    }

    tcm_ascii_console_clear();
    tcm_ascii_console_write_string(
        "Which phase drew a clean # box? P2 / P3 / P4\r\n");
    wait_key();
    tcm_ascii_console_clear();
    return 0;
}
