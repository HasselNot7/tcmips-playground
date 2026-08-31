/* Host-only decode tester: imgtest INFILE [OUT.ppm]
 * prints "OK name fmt WxH ms" or "ERR code"; optional PPM dump. */
#ifdef TCM_HOST
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "../image.h"

static uint32_t
ms_now(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint32_t) (tv.tv_sec * 1000u + tv.tv_usec / 1000u);
}

int
main(int argc, char **argv)
{
    FILE *f;
    long sz;
    uint8_t *buf;
    img_surface im;
    uint32_t t0, t1;
    int rc;

    if (argc < 2) {
        fprintf(stderr, "usage: imgtest file [out.ppm]\n");
        return 2;
    }
    f = fopen(argv[1], "rb");
    if (!f) {
        fprintf(stderr, "ERR open %s\n", argv[1]);
        return 2;
    }
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = (uint8_t *) malloc((size_t) sz);
    if (fread(buf, 1, (size_t) sz, f) != (size_t) sz) {
        fprintf(stderr, "ERR read\n");
        return 2;
    }
    fclose(f);

    t0 = ms_now();
    rc = img_decode(buf, (size_t) sz, &im);
    t1 = ms_now();
    if (rc != IMG_OK) {
        fprintf(stderr, "ERR %d %s\n", rc, argv[1]);
        free(buf);
        return 1;
    }
    printf("OK %s %dx%d %ums\n", argv[1], im.w, im.h, t1 - t0);
    if (argc > 2) {
        int x, y;
        f = fopen(argv[2], "wb");
        if (f) {
            fprintf(f, "P6\n%d %d\n255\n", im.w, im.h);
            for (y = 0; y < im.h; y++) {
                for (x = 0; x < im.w; x++) {
                    uint32_t px = im.px[(size_t) y * im.w + x];
                    uint8_t rgb[3] = { (uint8_t) px, (uint8_t)(px >> 8),
                                       (uint8_t)(px >> 16) };
                    fwrite(rgb, 1, 3, f);
                }
            }
            fclose(f);
        }
    }
    img_free(&im);
    free(buf);
    return 0;
}
#endif
