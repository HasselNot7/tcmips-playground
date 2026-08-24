// On-device diagnostics: milestone letters on the ASCII console plus a
// live status square in the framebuffer corner (blinks when presenting).
#include "wl_def.h"

volatile uint8_t tcm_dbg_stage = 0;

extern "C" void tcm_dbg_putc(char c)
{
    tcm_ascii_console_write_char(c);
}

extern "C" void tcm_dbg_str(const char *s)
{
    while (*s) tcm_dbg_putc(*s++);
}

void tcm_dbg_mark(char c)
{
    tcm_dbg_putc(c);
    tcm_ascii_console_write_char('\n');
}
