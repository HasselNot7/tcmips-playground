// TCMIPS entry shim: pre-game options on the ASCII text console, then hand
// over to Wolf4SDL's main loop.
#include <dev/console.h>
#include <dev/keyboard.h>

int tcm_wolf_viewsize = 19;   // consumed by ReadConfig()

static void put(const char *s)
{
    while (*s) tcm_ascii_console_write_char(*s++);
}

static void wait_release(void)
{
    while (tcm_keyboard_get_code()) {}
}

static int wait_key(void)
{
    uint32_t c;
    while (!(c = tcm_keyboard_get_code())) {}
    return (int)c;
}

extern int wolf_main(void);

int main()
{
    tcm_ascii_console_init();
    tcm_ascii_console_clear();

    put("\n=== Wolfenstein 3D (TCMIPS port) ===\n\n");
    put("Select view size (smaller = faster):\n");
    put("  1 = Full     304x182   (slowest)\n");
    put("  2 = Medium   208x126\n");
    put("  3 = Small    144x82    (fastest)\n");
    put("  4 = Tiny      96x54    (fastest+status bar only)\n\n");
    put("Press 1-4: ");

    for (;;)
    {
        int c = wait_key();
        if (c == '1') { tcm_wolf_viewsize = 19; break; }
        if (c == '2') { tcm_wolf_viewsize = 13; break; }
        if (c == '3') { tcm_wolf_viewsize = 9;  break; }
        if (c == '4') { tcm_wolf_viewsize = 6;  break; }
    }
    wait_release();

    extern unsigned char tcm_halfres;
    put("Half-res columns (2x faster, blockier)? y/N: ");
    for (;;)
    {
        int c = wait_key();
        if (c == 'y' || c == 'Y') { tcm_halfres = 1; put("YES\n"); break; }
        if (c == 'n' || c == 'N' || c == __TCM_KEY_CODE_ENTER) { tcm_halfres = 0; put("NO\n"); break; }
    }

    // drain so the key doesn't leak into the game
    for (int i = 0; i < 200000; ++i) { if (!tcm_keyboard_get_code()) break; }
    tcm_keyboard_clear();

    return wolf_main();
}
