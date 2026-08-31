/* Format probe + dispatcher shared by the viewer and tests. */
#include "image.h"

#include <stdlib.h>
#include <string.h>

int
img_probe(const uint8_t *data, size_t len)
{
    if (!data || len < 16)
        return 0;
    if (data[0] == 0xFF && data[1] == 0xD8)
        return 3; /* JPEG */
    if (memcmp(data, "\x89PNG\r\n\x1a\n", 8) == 0)
        return 2;
    if (data[0] == 'B' && data[1] == 'M')
        return 1;
    return 0;
}

int
img_decode(const uint8_t *data, size_t len, img_surface *out)
{
    out->px = NULL;
    out->w = out->h = 0;
    switch (img_probe(data, len)) {
    case 1:
        return img_decode_bmp(data, len, out);
    case 2:
        return img_decode_png(data, len, out);
    case 3:
        return img_decode_jpeg(data, len, out);
    default:
        return IMG_ERR_FORMAT;
    }
}

void
img_free(img_surface *s)
{
    free(s->px);
    s->px = NULL;
    s->w = s->h = 0;
}
