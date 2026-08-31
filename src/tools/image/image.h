/* Image decoding for TCMIPS: BMP / PNG / JPEG -> 0x00BBGGRR framebuffer.
 *
 * Integer-only implementation tuned for the 4.6 MHz MIPS32 soft-float core.
 * Output matches the device pixel console CONSOLE_MODE_PIXEL_32 layout
 * (one uint32 per pixel, low bytes R,G,B). */
#ifndef TCM_IMAGE_H
#define TCM_IMAGE_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t *px; /* w*h pixels, row-major, 0x00BBGGRR */
    int w, h;
} img_surface;

enum {
    IMG_OK = 0,
    IMG_ERR_FORMAT = -1,      /* not a stream of this format */
    IMG_ERR_CORRUPT = -2,     /* malformed data */
    IMG_ERR_UNSUPPORTED = -3, /* valid but unsupported feature */
    IMG_ERR_NOMEM = -4,
    IMG_ERR_SIZE = -5         /* beyond IMG_MAX_DIM */
};

#define IMG_MAX_DIM 2048

/* 0 = unknown, 1 = BMP, 2 = PNG, 3 = JPEG */
int img_probe(const uint8_t *data, size_t len);

/* Auto-dispatch probe + decode. Caller frees with img_free(). */
int img_decode(const uint8_t *data, size_t len, img_surface *out);

void img_free(img_surface *s);

int img_decode_bmp(const uint8_t *data, size_t len, img_surface *out);
int img_decode_png(const uint8_t *data, size_t len, img_surface *out);
int img_decode_jpeg(const uint8_t *data, size_t len, img_surface *out);

#endif /* TCM_IMAGE_H */
