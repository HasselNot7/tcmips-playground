/* T6: keyboard echo test - verify tcm_keyboard_get_code on device */
#include <stdio.h>
#include <string.h>
#include <dev/console.h>
#include <dev/keyboard.h>

int main(void)
{
    char tb[64];
    tcm_ascii_console_init();
    tcm_ascii_console_clear();
    tcm_ascii_console_write_string("T6 kb test\r\n");

    tcm_keyboard_clear();

    for (;;) {
        uint32_t code = tcm_keyboard_get_code();
        if (!code)
            continue;
        char mapped = '?';
        switch (code) {
        case __TCM_KEY_CODE_UP: mapped = 'k'; break;
        case __TCM_KEY_CODE_DOWN: mapped = 'j'; break;
        case __TCM_KEY_CODE_LEFT: mapped = 'h'; break;
        case __TCM_KEY_CODE_RIGHT: mapped = 'l'; break;
        case __TCM_KEY_CODE_BACKSPACE: mapped = 'E'; break;
        case __TCM_KEY_CODE_DEL: mapped = 'D'; break;
        case __TCM_KEY_CODE_ENTER: mapped = 'N'; break;
        case __TCM_KEY_CODE_TAB: mapped = 'T'; break;
        default:
            if (code >= 32 && code < 127)
                mapped = (char) code;
            break;
        }
        snprintf(tb, sizeof tb, "key=%lu map=%c\r\n",
                 (unsigned long) code, mapped);
        tcm_ascii_console_write_string(tb);
    }
}
