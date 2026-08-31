/* TCMIPS image viewer: browse embedded BMP/PNG/JPEG, decode on the fly and
 * blit to the pixel console; ASCII console shows an info line.
 *
 * Device keys: left/right prev-next, up = 1:1, down = fit, Del = quit.
 * Host keys:   h/p l/n prev-next, k up j down, s screenshot, q quit. */
#include "image.h"
#include "imgdata.h"
#include <stdio.h>
#include <sys/time.h>

#ifdef TCM_HOST
uint32_t *iv_screen_init(int width);
void iv_ascii(const char *line);
int iv_wait_action(void);
#else
#include <dev/console.h>
#include <dev/keyboard.h>
#include <dev/syscall.h>
#endif

#define VIEW_W 640

/* normalized actions */
#define IV_NONE 0
#define IV_NEXT 101
#define IV_PREV 102
#define IV_FIT 103
#define IV_ONE 104
#define IV_QUIT 105
#define IV_SHOT 106

static int scr_w = VIEW_W, scr_h = VIEW_W * 3 / 4;

static uint32_t
now_ms(void)
{
#ifdef TCM_HOST
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint32_t) (tv.tv_sec * 1000u + tv.tv_usec / 1000u);
#else
    return tcm_syscall_get_timestamp() * 1000u +
           tcm_syscall_get_timestamp_milli();
#endif
}

static int
wait_action(void)
{
#ifdef TCM_HOST
    return iv_wait_action();
#else
    uint32_t c;
    for (;;) {
        c = tcm_keyboard_get_code();
        if (c)
            break;
    }
    switch (c) {
    case __TCM_KEY_CODE_UP:
        return IV_ONE;
    case __TCM_KEY_CODE_DOWN:
        return IV_FIT;
    case __TCM_KEY_CODE_LEFT:
        return IV_PREV;
    case __TCM_KEY_CODE_RIGHT:
        return IV_NEXT;
    case __TCM_KEY_CODE_DEL:
        return IV_QUIT;
    default:
        if (c == 'q')
            return IV_QUIT;
        return IV_NONE;
    }
#endif
}

static void
fill(uint32_t *fb, uint32_t c)
{
    int i, n = scr_w * scr_h;
    for (i = 0; i < n; i++)
        fb[i] = c;
}

/* mode 0: fit to screen with integer downscale, centered.
 * mode 1: 1:1 centered (cropping overflow). */
static void
blit(uint32_t *fb, const img_surface *im, int mode)
{
    int step = 1, dw, dh, ox, oy, x, y, sx0, sy0;
    if (mode == 0) {
        while ((im->w + step - 1) / step > scr_w ||
               (im->h + step - 1) / step > scr_h)
            step++;
        dw = im->w / step;
        dh = im->h / step;
        sx0 = sy0 = 0;
    } else {
        dw = im->w < scr_w ? im->w : scr_w;
        dh = im->h < scr_h ? im->h : scr_h;
        sx0 = (im->w - dw) / 2;
        sy0 = (im->h - dh) / 2;
    }
    ox = (scr_w - dw) / 2;
    oy = (scr_h - dh) / 2;
    for (y = 0; y < dh; y++) {
        uint32_t *dst = fb + (size_t)(oy + y) * scr_w + ox;
        const uint32_t *src = im->px + (size_t)(sy0 + y * step) * im->w + sx0;
        for (x = 0; x < dw; x++)
            dst[x] = mode == 1 ? src[x] : src[x * step];
    }
}

static const char *
fmt_name(int f)
{
    return f == 1 ? "BMP" : f == 2 ? "PNG" : f == 3 ? "JPEG" : "?";
}

int
main(void)
{
    uint32_t *fb;
    img_surface im;
    int idx = 0, mode = 0, run = 1, total;
    char line[128];

#ifdef TCM_HOST
    fb = iv_screen_init(VIEW_W);
#else
    tcm_ascii_console_init();
    tcm_ascii_console_clear();
    fb = (uint32_t *) tcm_pixel_console_init(CONSOLE_MODE_PIXEL_32, VIEW_W);
    if (!fb) {
        tcm_ascii_console_write_string("pixel console init failed\n");
        return 1;
    }
#endif
    total = tcm_img_file_count;
    if (total <= 0)
        return 1;

    while (run) {
        const tcm_img_file_t *f = &tcm_img_files[idx];
        uint32_t t0, t1;
        int rc, act;

        t0 = now_ms();
        rc = img_decode(f->data, f->size, &im);
        t1 = now_ms();

        fill(fb, 0);
        if (rc == IMG_OK) {
            uint32_t ms = (t1 >= t0) ? (t1 - t0) : 0u;
            blit(fb, &im, mode);
            snprintf(line, sizeof line, "[%d/%d] %s %s %dx%d  %ums %s",
                     idx + 1, total, f->name,
                     fmt_name(img_probe(f->data, f->size)), im.w, im.h,
                     (unsigned) ms, mode ? "1:1" : "fit");
            img_free(&im);
        } else {
            snprintf(line, sizeof line, "[%d/%d] %s  DECODE ERROR %d",
                     idx + 1, total, f->name, rc);
        }

#ifdef TCM_HOST
        iv_ascii(line);
#else
        tcm_ascii_console_clear();
        tcm_ascii_console_write_string(line);
        tcm_ascii_console_write_string(
            "\nleft/right prev-next   up 1:1   down fit   Del quit\n");
#endif

        act = wait_action();
        switch (act) {
        case IV_NEXT:
            idx = (idx + 1) % total;
            break;
        case IV_PREV:
            idx = (idx + total - 1) % total;
            break;
        case IV_FIT:
            mode = 0;
            break;
        case IV_ONE:
            mode = 1;
            break;
        case IV_SHOT:
#ifdef TCM_HOST
            (void) 0; /* handled inside iv_wait_action */
#endif
            break;
        case IV_QUIT:
            run = 0;
            break;
        default:
            break;
        }
    }

#ifndef TCM_HOST
    fill(fb, 0);
    tcm_ascii_console_clear();
    tcm_ascii_console_write_string("bye\n");
#endif
    return 0;
}
