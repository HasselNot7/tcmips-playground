/* BMP decoder: 1/4/8/16/24/32bpp, top-down/bottom-up, palettes,
 * BI_BITFIELDS masks, BI_RLE8/4.  Output 0x00BBGGRR. */
#include "image.h"

#include <stdlib.h>
#include <string.h>

static uint16_t
le16(const uint8_t *q)
{
    return (uint16_t) (q[0] | (q[1] << 8));
}
static int32_t
le32(const uint8_t *q)
{
    return (int32_t) ((uint32_t) q[0] | ((uint32_t) q[1] << 8) |
                      ((uint32_t) q[2] << 16) | ((uint32_t) q[3] << 24));
}

typedef struct {
    int w, h, topdown, bits, rowsize;
    const uint8_t *pixels;
    const uint8_t *pend;
    const uint8_t *pal;
    int pal_ent;
    int ncolors;
    uint32_t rmask, gmask, bmask;
    uint32_t *fb;
} bmp_ctx;

static void
put_px(bmp_ctx *b, int x, int y, uint32_t rgb)
{
    if (x >= 0 && x < b->w && y >= 0 && y < b->h)
        b->fb[(size_t) y * b->w + x] = rgb;
}

static int
pal_color(bmp_ctx *b, int idx, uint32_t *out)
{
    const uint8_t *e;
    if (idx < 0 || idx >= b->ncolors)
        return IMG_ERR_CORRUPT;
    e = b->pal + (size_t) idx * b->pal_ent;
    *out = (uint32_t) e[2] | ((uint32_t) e[1] << 8) | ((uint32_t) e[0] << 16);
    return IMG_OK;
}

/* extract a masked channel and widen to 8 bits (works for 1..8 bit masks) */
static unsigned
hi_color(unsigned v, uint32_t mask)
{
    uint32_t m;
    int shift = 0, bits = 0;
    unsigned val;
    if (!mask)
        return 0;
    m = mask;
    while (!(m & 1)) {
        m >>= 1;
        shift++;
    }
    while (m) {
        bits++;
        m >>= 1;
    }
    val = (v & mask) >> shift;
    if (bits >= 8)
        return val >> (bits - 8);
    return (val << (8 - bits)) | (bits * 2 > 8 ? val >> (bits * 2 - 8) : 0);
}

static int
bmp_decode_plain(bmp_ctx *b)
{
    int y, x;
    if ((size_t) b->rowsize * (size_t) b->h > (size_t) (b->pend - b->pixels) &&
        b->pend != NULL)
        return IMG_ERR_CORRUPT;
    for (y = 0; y < b->h; y++) {
        int sy = b->topdown ? y : b->h - 1 - y;
        const uint8_t *row = b->pixels + (size_t) sy * b->rowsize;
        for (x = 0; x < b->w; x++) {
            uint32_t c;
            switch (b->bits) {
            case 1: {
                int idx = (row[x >> 3] >> (7 - (x & 7))) & 1;
                if (pal_color(b, idx, &c))
                    return IMG_ERR_CORRUPT;
                break;
            }
            case 4: {
                int idx = (x & 1) ? (row[x >> 1] & 15) : (row[x >> 1] >> 4);
                if (pal_color(b, idx, &c))
                    return IMG_ERR_CORRUPT;
                break;
            }
            case 8:
                if (pal_color(b, row[x], &c))
                    return IMG_ERR_CORRUPT;
                break;
            case 16: {
                unsigned v = le16(row + (size_t) x * 2);
                c = hi_color(v, b->rmask) | (hi_color(v, b->gmask) << 8) |
                    (hi_color(v, b->bmask) << 16);
                break;
            }
            case 24: {
                const uint8_t *p = row + (size_t) x * 3;
                c = (uint32_t) p[2] | ((uint32_t) p[1] << 8) |
                    ((uint32_t) p[0] << 16);
                break;
            }
            case 32: {
                const uint8_t *p = row + (size_t) x * 4;
                c = (uint32_t) p[2] | ((uint32_t) p[1] << 8) |
                    ((uint32_t) p[0] << 16);
                break;
            }
            default:
                return IMG_ERR_UNSUPPORTED;
            }
            put_px(b, x, y, c);
        }
    }
    return IMG_OK;
}

static int
bmp_decode_rle(bmp_ctx *b, int four)
{
    const uint8_t *p = b->pixels;
    int x = 0, y = 0;
    uint32_t last = 0; /* repeat for odd RLE4 tails */

    while (p + 2 <= b->pend) {
        int cnt = p[0];
        if (cnt == 0) {
            int mode = p[1];
            p += 2;
            if (mode == 0) { /* EOL */
                x = 0;
                y++;
                last = 0;
                continue;
            }
            if (mode == 1) /* EOF */
                break;
            if (mode == 2) { /* delta */
                if (p + 2 > b->pend)
                    return IMG_ERR_CORRUPT;
                x += p[0];
                y += p[1];
                p += 2;
                last = 0;
                continue;
            }
            /* raw mode: `mode` pixels follow */
            {
                int n = mode, i;
                if (four) {
                    int bytes = (n + 1) >> 1;
                    if (p + bytes > b->pend)
                        return IMG_ERR_CORRUPT;
                    for (i = 0; i < n; i++) {
                        uint32_t c;
                        int idx;
                        if (i & 1) {
                            idx = last;
                        } else {
                            last = p[(i + 1) >> 1] & 15;
                            idx = p[i >> 1] >> 4;
                        }
                        if (pal_color(b, idx, &c) == 0)
                            put_px(b, x++, y, c);
                        if (x >= b->w) {
                            x = 0;
                            y++;
                        }
                    }
                    p += (bytes + 1) & ~1; /* runs pad to word */
                } else {
                    if (p + n > b->pend)
                        return IMG_ERR_CORRUPT;
                    for (i = 0; i < n; i++) {
                        uint32_t c;
                        if (pal_color(b, p[i], &c) == 0)
                            put_px(b, x++, y, c);
                        if (x >= b->w) {
                            x = 0;
                            y++;
                        }
                    }
                    p += (n + 1) & ~1;
                }
            }
            continue;
        }
        /* encoded run */
        {
            int i;
            if (p + 2 > b->pend)
                return IMG_ERR_CORRUPT;
            if (four) {
                uint8_t v = p[1];
                uint8_t hi = v >> 4, lo = v & 15;
                p += 2;
                for (i = 0; i < cnt; i++) {
                    uint32_t c;
                    if (pal_color(b, (i & 1) ? lo : hi, &c) == 0)
                        put_px(b, x++, y, c);
                    if (x >= b->w) {
                        x = 0;
                        y++;
                    }
                }
            } else {
                uint8_t v = p[1];
                p += 2;
                for (i = 0; i < cnt; i++) {
                    uint32_t c;
                    if (pal_color(b, v, &c) == 0)
                        put_px(b, x++, y, c);
                    if (x >= b->w) {
                        x = 0;
                        y++;
                    }
                }
            }
        }
        if (y >= b->h)
            break;
    }
    return IMG_OK;
}

int
img_decode_bmp(const uint8_t *data, size_t len, img_surface *out)
{
    bmp_ctx b;
    int32_t hdrsize, off;
    uint32_t comp;
    int h_abs;

    if (!data || len < 26 || data[0] != 'B' || data[1] != 'M')
        return IMG_ERR_FORMAT;

    memset(&b, 0, sizeof b);
    off = le32(data + 10);
    hdrsize = le32(data + 14);
    if (hdrsize < 12 || 14 + (size_t) hdrsize > len || off < 0 ||
        (size_t) off > len)
        return IMG_ERR_FORMAT;

    if (hdrsize == 12) { /* BITMAPCOREHEADER */
        b.w = le16(data + 18);
        b.h = le16(data + 20);
        b.bits = le16(data + 24);
        comp = 0;
        b.topdown = 0;
        b.pal = data + 14 + 12;
        b.pal_ent = 3;
    } else { /* INFOHEADER / V4 / V5 */
        b.w = le32(data + 18);
        b.h = le32(data + 22);
        b.bits = le16(data + 28);
        comp = (uint32_t) le32(data + 30);
        b.ncolors = le32(data + 46);
        b.topdown = (b.h < 0);
        b.pal = data + 14 + (size_t) hdrsize;
        b.pal_ent = 4;
        if (comp == 3) {
            if ((size_t) 54 + 12 > len && (size_t)(off) >= (size_t)54 + 12)
                return IMG_ERR_CORRUPT;
            if ((size_t) 54 + 12 <= len) {
                b.rmask = (uint32_t) le32(data + 54);
                b.gmask = (uint32_t) le32(data + 58);
                b.bmask = (uint32_t) le32(data + 62);
            }
        }
    }
    h_abs = b.topdown ? -b.h : b.h;
    if (h_abs <= 0 || b.w <= 0 || b.w > IMG_MAX_DIM || h_abs > IMG_MAX_DIM)
        return IMG_ERR_SIZE;
    b.h = h_abs;

    if (b.bits <= 8) {
        if (b.ncolors <= 0 || b.ncolors > (1 << b.bits))
            b.ncolors = 1 << b.bits;
    } else {
        b.ncolors = 0;
    }
    if (b.bits == 16 && !b.rmask) {
        b.rmask = 0x7C00;
        b.gmask = 0x03E0;
        b.bmask = 0x001F;
    }

    b.rowsize = (((int32_t) b.w * b.bits + 31) & ~31) / 8;
    b.pixels = data + off;
    b.pend = data + len;

    b.fb = (uint32_t *) malloc((size_t) b.w * b.h * 4);
    if (!b.fb)
        return IMG_ERR_NOMEM;
    memset(b.fb, 0, (size_t) b.w * b.h * 4);

    if (comp == 0 || comp == 3) {
        int rc = bmp_decode_plain(&b);
        if (rc) {
            free(b.fb);
            return rc;
        }
    } else if (comp == 1 || (comp == 4 && (b.bits == 8 || b.bits == 4))) {
        bmp_decode_rle(&b, 0);
    } else if (comp == 2 || (comp == 4 && b.bits == 4)) {
        bmp_decode_rle(&b, 1);
    } else {
        free(b.fb);
        return IMG_ERR_UNSUPPORTED;
    }

    out->px = b.fb;
    out->w = b.w;
    out->h = b.h;
    return IMG_OK;
}
