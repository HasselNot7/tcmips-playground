/* Host console/keyboard stub for the image viewer (TCM_HOST builds only).
 * fb = malloc'd buffer standing in for the device pixel console; keys come
 * from stdin; 's' dumps shots/img_N.ppm. */
#ifdef TCM_HOST

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#define IV_NEXT 101
#define IV_PREV 102
#define IV_FIT 103
#define IV_ONE 104
#define IV_QUIT 105
#define IV_SHOT 106

static uint32_t *sim_fb;
static int sim_w, sim_h;
static int shot_no;

uint32_t *
iv_screen_init(int width)
{
    free(sim_fb);
    sim_w = width;
    sim_h = width * 3 / 4;
    sim_fb = (uint32_t *) calloc((size_t) sim_w * sim_h, 4);
    return sim_fb;
}

void
iv_ascii(const char *line)
{
    fprintf(stderr, "\r%-120.120s", line);
    fflush(stderr);
}

static void
dump_ppm(const char *path)
{
    FILE *f = fopen(path, "wb");
    int x, y;
    if (!f || !sim_fb) {
        fprintf(stderr, "[host] shot failed\n");
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", sim_w, sim_h);
    for (y = 0; y < sim_h; y++) {
        for (x = 0; x < sim_w; x++) {
            uint32_t px = sim_fb[(size_t) y * sim_w + x];
            uint8_t rgb[3] = { (uint8_t) px, (uint8_t)(px >> 8),
                               (uint8_t)(px >> 16) };
            fwrite(rgb, 1, 3, f);
        }
    }
    fclose(f);
    fprintf(stderr, "[host] wrote %s\n", path);
}

int
iv_wait_action(void)
{
    unsigned char c;
    for (;;) {
        if (read(0, &c, 1) != 1)
            return IV_QUIT;
        switch (c) {
        case 'l':
        case 'n':
            return IV_NEXT;
        case 'h':
        case 'p':
            return IV_PREV;
        case 'j':
            return IV_FIT;
        case 'k':
            return IV_ONE;
        case 's':
        {
            char name[64];
            snprintf(name, sizeof name, "shots/img_%03d.ppm", shot_no++);
            dump_ppm(name);
            continue;
        }
        case 'q':
            return IV_QUIT;
        case '\x1b': { /* arrow keys: ESC [ A B C D */
            unsigned char a = 0, b = 0;
            if (read(0, &a, 1) == 1 && a == '[' && read(0, &b, 1) == 1) {
                switch (b) {
                case 'A':
                    return IV_ONE;
                case 'B':
                    return IV_FIT;
                case 'C':
                    return IV_NEXT;
                case 'D':
                    return IV_PREV;
                }
            }
            continue;
        }
        default:
            continue;
        }
    }
}

#endif /* TCM_HOST */
