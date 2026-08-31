/* PNG decoder (ISO/IEC 10128) -> 0x00BBGGRR surface.
 * Supports color types 0/2/3/4/6, bit depths 1/2/4/8/16, Adam7 interlacing,
 * tRNS (blended over black).  IDAT decompression via image_inflate.c.
 *
 * Memory: raw = inflate output (all passes, ~1.05x PNG pixel data),
 * prev  = one scanline, fb  = output surface.  Streamed per scanline. */
#include "image.h"
#include "image_inflate.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const uint8_t png_sig[8] = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a,
                                    0x0a };

static uint32_t
be32(const uint8_t *q)
{
    return ((uint32_t) q[0] << 24) | ((uint32_t) q[1] << 16) |
           ((uint32_t) q[2] << 8) | q[3];
}

static unsigned
scale8(unsigned v, int depth)
{
    switch (depth) {
    case 16:
        return v >> 8;
    case 8:
        return v;
    case 4:
        return v * 17u;
    case 2:
        return v * 85u;
    default:
        return v ? 255u : 0u;
    }
}

/* Adam7 pass geometry: nonzero dims for pass 0..6 */
static int
pass_w(int w, int pass)
{
    static const int off[7] = { 0, 4, 0, 2, 0, 1, 0 };
    static const int stp[7] = { 8, 8, 4, 4, 2, 2, 1 };
    return (w > off[pass]) ? ((w - off[pass] + stp[pass] - 1) / stp[pass]) : 0;
}
static int
pass_h(int h, int pass)
{
    static const int off[7] = { 0, 0, 4, 0, 2, 0, 1 };
    static const int stp[7] = { 8, 8, 8, 4, 4, 2, 2 };
    return (h > off[pass]) ? ((h - off[pass] + stp[pass] - 1) / stp[pass]) : 0;
}
static const int pass_xoff[7] = { 0, 4, 0, 2, 0, 1, 0 };
static const int pass_yoff[7] = { 0, 0, 4, 0, 2, 0, 1 };
static const int pass_xstep[7] = { 8, 8, 4, 4, 2, 2, 1 };
static const int pass_ystep[7] = { 8, 8, 8, 4, 4, 2, 2 };

typedef struct {
    int w, h, depth, color, interlace, channels;
    int plte_n;
    uint8_t plte[768];
    int trns_idx_n;
    uint8_t trns_idx[256];
    int trns_color_set;
    unsigned trnc[4]; /* color/gray tRNS, depth-scaled */
    const uint8_t *sp;   /* read cursor into raw */
    const uint8_t *send; /* one past inflate output */
    uint8_t *rowcur;     /* unfiltered current row */
    uint8_t *rowprev;    /* unfiltered previous row (zeroed at pass start) */
    uint32_t *fb;
} png_ctx;

static int
png_ihdr(png_ctx *i, const uint8_t *d, size_t n)
{
    if (n < 13)
        return IMG_ERR_CORRUPT;
    i->w = (int) be32(d);
    i->h = (int) be32(d + 4);
    i->depth = d[8];
    i->color = d[9];
    if (d[10] != 0 || d[11] != 0 || d[12] > 1)
        return IMG_ERR_UNSUPPORTED;
    i->interlace = d[12];
    switch (i->color) {
    case 0:
    case 3:
        i->channels = 1;
        break;
    case 2:
        i->channels = 3;
        break;
    case 4:
        i->channels = 2;
        break;
    case 6:
        i->channels = 4;
        break;
    default:
        return IMG_ERR_UNSUPPORTED;
    }
    if (i->w <= 0 || i->h <= 0 || i->w > IMG_MAX_DIM || i->h > IMG_MAX_DIM)
        return IMG_ERR_SIZE;
    if (i->depth != 1 && i->depth != 2 && i->depth != 4 && i->depth != 8 &&
        i->depth != 16)
        return IMG_ERR_UNSUPPORTED;
    if (i->color == 3 && i->depth > 8)
        return IMG_ERR_UNSUPPORTED;
    return IMG_OK;
}

/* fetch channel value (raw units) of pixel x from unfiltered row */
static unsigned
sample_at(const uint8_t *row, int x, int c, const png_ctx *i)
{
    if (i->depth >= 8) {
        int bpc = (i->depth == 16) ? 2 : 1;
        const uint8_t *p = row + (size_t) (x * i->channels + c) * bpc;
        return (bpc == 2) ? (unsigned)((p[0] << 8) | p[1]) : (unsigned) p[0];
    }
    {
        size_t bit = (size_t) x * i->depth;
        int shift = 8 - (int) (bit & 7u) - i->depth;
        return (row[bit >> 3] >> shift) & ((1u << i->depth) - 1u);
    }
}

static int
paeth(int a, int b, int c)
{
    int p = a + b - c;
    int pa = p > a ? p - a : a - p;
    int pb = p > b ? p - b : b - p;
    int pc = p > c ? p - c : c - p;
    return (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
}

/* decode + emit one scanline of the current pass.  Uses double-buffer swap:
 * rowcur receives the unfiltered bytes, then becomes rowprev. */
static int
png_line(png_ctx *i, int pw, int gy, int gx0, int stepx)
{
    int row_bytes = (pw * i->channels * i->depth + 7) / 8;
    int bpp = (i->depth < 8) ? 1 : (i->channels * ((i->depth == 16) ? 2 : 1));
    int ft, x;
    uint8_t *cur = i->rowcur;
    const uint8_t *prv = i->rowprev;
    const uint8_t *s;

    if ((size_t) (i->send - i->sp) < (size_t) (row_bytes + 1))
        return IMG_ERR_CORRUPT;
    ft = *i->sp++;
    s = i->sp;
    i->sp += row_bytes;

    for (x = 0; x < row_bytes; x++) {
        int raw = s[x];
        int a = (x >= bpp) ? cur[x - bpp] : 0;
        int b = prv[x];
        int c = (x >= bpp) ? prv[x - bpp] : 0;
        switch (ft) {
        case 0: cur[x] = (uint8_t) raw; break;
        case 1: cur[x] = (uint8_t) (raw + a); break;
        case 2: cur[x] = (uint8_t) (raw + b); break;
        case 3: cur[x] = (uint8_t) (raw + ((a + b) >> 1)); break;
        case 4: cur[x] = (uint8_t) (raw + paeth(a, b, c)); break;
        default: return IMG_ERR_CORRUPT;
        }
    }

    for (x = 0; x < pw; x++) {
        unsigned r, g, b, a = 255;
        int gxx = gx0 + x * stepx;
        switch (i->color) {
        case 0:
            r = g = b = scale8(sample_at(cur, x, 0, i), i->depth);
            if (i->trns_color_set && r == i->trnc[0])
                a = 0;
            break;
        case 2:
            r = scale8(sample_at(cur, x, 0, i), i->depth);
            g = scale8(sample_at(cur, x, 1, i), i->depth);
            b = scale8(sample_at(cur, x, 2, i), i->depth);
            if (i->trns_color_set && r == i->trnc[0] && g == i->trnc[1] &&
                b == i->trnc[2])
                a = 0;
            break;
        case 3: {
            unsigned idx = sample_at(cur, x, 0, i);
            r = g = b = 0;
            if ((int) idx < i->plte_n) {
                r = i->plte[idx * 3];
                g = i->plte[idx * 3 + 1];
                b = i->plte[idx * 3 + 2];
            }
            if (i->trns_idx_n > 0 && (int) idx < i->trns_idx_n)
                a = i->trns_idx[idx];
            break;
        }
        case 4:
            r = g = b = scale8(sample_at(cur, x, 0, i), i->depth);
            a = scale8(sample_at(cur, x, 1, i), i->depth);
            break;
        default: /* 6 */
            r = scale8(sample_at(cur, x, 0, i), i->depth);
            g = scale8(sample_at(cur, x, 1, i), i->depth);
            b = scale8(sample_at(cur, x, 2, i), i->depth);
            a = scale8(sample_at(cur, x, 3, i), i->depth);
            break;
        }
        i->fb[(size_t) gy * i->w + gxx] =
            (uint32_t)((r * a / 255u) | ((g * a / 255u) << 8) |
                       ((b * a / 255u) << 16));
    }
    /* swap rowcur/rowprev */
    i->rowprev = i->rowcur;
    i->rowcur = (uint8_t *) prv;
    return IMG_OK;
}

int
img_decode_png(const uint8_t *data, size_t len, img_surface *out)
{
    png_ctx i;
    const uint8_t *p;
    size_t n, idat_total = 0;
    uint8_t *idat = NULL, *raw = NULL;
    size_t raw_size = 0, idat_off;
    int pass, rc;

    if (!data || len < 8 || memcmp(data, png_sig, 8) != 0)
        return IMG_ERR_FORMAT;
    memset(&i, 0, sizeof i);

    /* scan chunks */
    for (p = data + 8, n = len - 8; n >= 12; ) {
        uint32_t clen = be32(p);
        const uint8_t *ct = p + 4;
        const uint8_t *cd = p + 8;
        if (clen > n - 12)
            return IMG_ERR_CORRUPT;
        if (memcmp(ct, "IHDR", 4) == 0) {
            rc = png_ihdr(&i, cd, clen);
            if (rc)
                return rc;
        } else if (memcmp(ct, "PLTE", 4) == 0) {
            if (clen > 768)
                return IMG_ERR_CORRUPT;
            memcpy(i.plte, cd, clen);
            i.plte_n = (int) (clen / 3);
        } else if (memcmp(ct, "tRNS", 4) == 0) {
            if (i.color == 3 && clen <= 256) {
                memcpy(i.trns_idx, cd, clen);
                i.trns_idx_n = (int) clen;
            } else if (i.color == 0 && clen >= 2) {
                i.trnc[0] = scale8((cd[0] << 8) | cd[1], i.depth);
                i.trns_color_set = 1;
            } else if (i.color == 2 && clen >= 6) {
                i.trnc[0] = scale8((cd[0] << 8) | cd[1], i.depth);
                i.trnc[1] = scale8((cd[2] << 8) | cd[3], i.depth);
                i.trnc[2] = scale8((cd[4] << 8) | cd[5], i.depth);
                i.trns_color_set = 1;
            }
        } else if (memcmp(ct, "IDAT", 4) == 0) {
            idat_total += clen;
        } else if (memcmp(ct, "IEND", 4) == 0) {
            break;
        }
        p += 12 + clen;
        n -= 12 + clen;
    }
    if (i.w <= 0 || idat_total == 0)
        return IMG_ERR_CORRUPT;
    if (i.color == 3 && i.plte_n == 0)
        return IMG_ERR_CORRUPT;

    idat = (uint8_t *) malloc(idat_total);
    if (!idat)
        return IMG_ERR_NOMEM;
    idat_off = 0;
    for (p = data + 8, n = len - 8; n >= 12; ) {
        uint32_t clen = be32(p);
        if (clen > n - 12)
            break;
        if (memcmp(p + 4, "IDAT", 4) == 0) {
            memcpy(idat + idat_off, p + 8, clen);
            idat_off += clen;
        }
        p += 12 + clen;
        n -= 12 + clen;
    }

    /* compute inflate output size */
    for (pass = 0; pass < (i.interlace ? 7 : 1); pass++) {
        int pw = i.interlace ? pass_w(i.w, pass) : i.w;
        int ph = i.interlace ? pass_h(i.h, pass) : i.h;
        if (pw <= 0 || ph <= 0)
            continue;
        raw_size += (size_t) ph *
                    (size_t) (1 + (pw * i.channels * i.depth + 7) / 8);
    }
    raw = (uint8_t *) malloc(raw_size + 16);
    {
        int maxrow = (i.w * i.channels * i.depth + 7) / 8 + 8;
        i.rowcur = (uint8_t *) malloc((size_t) maxrow);
        i.rowprev = (uint8_t *) calloc((size_t) maxrow, 1);
    }
    i.fb = (uint32_t *) malloc((size_t) i.w * i.h * 4);
    if (!raw || !i.rowcur || !i.rowprev || !i.fb) {
        free(idat);
        free(raw);
        free(i.rowcur);
        free(i.rowprev);
        free(i.fb);
        return IMG_ERR_NOMEM;
    }
    memset(i.fb, 0, (size_t) i.w * i.h * 4);

    if (idat_total < 2 || (idat[0] & 0x0f) != 8) {
        /* PNG IDAT is a zlib stream: strip CMF/FLG before raw DEFLATE */
        free(idat);
        free(raw);
        free(i.rowcur);
        free(i.rowprev);
        free(i.fb);
        return IMG_ERR_UNSUPPORTED;
    }
    rc = img_inflate(idat + 2, idat_total - 2, raw, raw_size, NULL);
    free(idat);
    if (rc != IMG_OK) {
        free(raw);
        free(i.rowcur);
        free(i.rowprev);
        free(i.fb);
        return rc;
    }

    i.send = raw + raw_size;
    i.sp = raw;

    if (i.interlace) {
        for (pass = 0; pass < 7; pass++) {
            int pw = pass_w(i.w, pass), ph = pass_h(i.h, pass);
            int y;
            uint8_t *t;
            if (pw <= 0 || ph <= 0)
                continue;
            /* start fresh: rowcur holds current writes, rowprev must be 0 */
            t = i.rowprev;
            i.rowprev = i.rowcur;
            i.rowcur = t;
            memset(i.rowprev, 0,
                   (size_t) (1 + (pw * i.channels * i.depth + 7) / 8));
            for (y = 0; y < ph; y++) {
                rc = png_line(&i, pw, pass_yoff[pass] + y * pass_ystep[pass],
                              pass_xoff[pass], pass_xstep[pass]);
                if (rc)
                    goto fail;
            }
        }
    } else {
        int y;
        memset(i.rowprev, 0,
               (size_t) (1 + (i.w * i.channels * i.depth + 7) / 8));
        for (y = 0; y < i.h; y++) {
            rc = png_line(&i, i.w, y, 0, 1);
            if (rc)
                goto fail;
        }
    }

    free(raw);
    free(i.rowcur);
    free(i.rowprev);
    out->px = i.fb;
    out->w = i.w;
    out->h = i.h;
    return IMG_OK;

fail:
    free(raw);
    free(i.rowcur);
    free(i.rowprev);
    free(i.fb);
    return rc;
}
