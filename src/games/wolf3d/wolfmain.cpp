// TCMIPS entry shim: pre-game options on the ASCII text console, then hand
// over to Wolf4SDL's main loop.
#include <dev/console.h>
#include <dev/keyboard.h>

#include <stdio.h>

int tcm_wolf_viewsize = 19;   // consumed by ReadConfig()

static void wait_release(void)
{
    while (tcm_keyboard_get_code()) {}
}

static int wait_key(void)
{
    int c = tcm_ascii_console_read_char();
    return c;
}

extern int wolf_main(void);

int main()
{
    tcm_ascii_console_init();
    tcm_ascii_console_clear();

    printf("\n=== Wolfenstein 3D (TCMIPS port) ===\n\n"); // use libc printf
    printf("Select view size (smaller = faster):\n");
    printf("  1 = Full     304x182   (slowest)\n");
    printf("  2 = Medium   208x126\n");
    printf("  3 = Small    144x82    (fastest)\n");
    printf("  4 = Tiny      96x54    (fastest+status bar only)\n\n");
    printf("Press 1-4: ");
    fflush(stdout);

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
    puts("\nHalf-res columns (2x faster, blockier)? y/N: ");
    for (;;)
    {
        int c = wait_key();
        if (c == 'y' || c == 'Y') { tcm_halfres = 1; puts("YES\n"); break; }
        if (c == 'n' || c == 'N' || c == __TCM_KEY_CODE_ENTER) { tcm_halfres = 0; puts("NO\n"); break; }
    }

    // drain so the key doesn't leak into the game
    for (int i = 0; i < 200000; ++i) { if (!tcm_keyboard_get_code()) break; }
    tcm_keyboard_clear();

    return wolf_main();
}
