/* Baseline sequential JPEG (ITU-T T.81) decoder -> 0x00BBGGRR surface.
 *
 * Supported: 1 or 3 components, sampling factors 1..4, 8-bit precision,
 * RSTn restart markers.  Rejected: progressive, arithmetic, multi-scan.
 * IDCT: separable 2-pass direct sum, 10-bit fixed point.
 */
#include "image.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint16_t count[17];
    uint8_t symbol[256];
    int present;
} jhuff;

typedef struct {
    int id, h, v, tq;
    int td, ta;
    int pw, ph;
    uint8_t *plane;
} jcomp;

typedef struct {
    const uint8_t *p, *end;
    uint32_t bitbuf;
    int bitcnt;
    int marker_hit; /* entropy reader stopped at a marker */
    int16_t qtab[4][64];
    jhuff hd[4], ha[4];
    jcomp c[4];
    int ncomp, maxh, maxv;
    int w, h;
    int32_t pred[4];
    uint32_t *fb;
} jpeg_ctx;

/* forward zigzag: scan index -> natural (row-major) position */
static const uint8_t zigzag[64] = {
    0,   1,  8,  16,  9,  2,  3, 10, 17,  24, 32, 25,  18,  11, 4,  5,
    12, 19,  26, 33,  40, 48, 41, 34, 27,  20, 13,  6,   7,  14, 21, 28,
    35, 42,  49, 56,  57, 50, 43, 36, 29,  22, 15,  23,  30, 37, 44, 51,
    58, 59,  52, 45,  38, 31,  39, 46, 53,  60, 61,  54,  47, 55, 62, 63
};

/* Cs[u][x] = round(1024 * alpha_u * cos((2x+1) u pi / 16)) */
static const int16_t idct_cs[8][8] = {
    { 724, 724, 724, 724, 724, 724, 724, 724 },
    { 1004, 851, 569, 200, -200, -569, -851, -1004 },
    { 946, 392, -392, -946, -946, -392, 392, 946 },
    { 851, -200, -1004, -569, 569, 1004, 200, -851 },
    { 724, -724, -724, 724, 724, -724, -724, 724 },
    { 569, -1004, 200, 851, -851, -200, 1004, -569 },
    { 392, -946, 946, -392, -392, 946, -946, 392 },
    { 200, -569, 851, -1004, 1004, -851, 569, -200 }
};

/* ---------------- bit input (FF00 stuffing, stop at markers) ------------ */

static int
jfill(jpeg_ctx *j)
{
    uint8_t v;
    if (j->p >= j->end)
        return 0;
    v = *j->p;
    if (v == 0xFF) {
        if (j->p + 1 >= j->end) {
            j->p = j->end;
            return 0;
        }
        if (j->p[1] != 0) {
            j->marker_hit = 1; /* FF nn (nn!=0): stop, marker unconsumed */
            return 0;
        }
        j->p += 2;
    } else {
        j->p += 1;
    }
    j->bitbuf = (j->bitbuf << 8) | v;
    j->bitcnt += 8;
    return 1;
}

static int
jbits(jpeg_ctx *j, int need)
{
    uint32_t v;
    if (need == 0)
        return 0;
    while (j->bitcnt < need) {
        if (!jfill(j))
            return j->marker_hit ? -2 : -1;
    }
    v = (j->bitbuf >> (j->bitcnt - need)) & ((1u << need) - 1u);
    j->bitcnt -= need;
    return (int) v;
}

static int
jhdec(jpeg_ctx *j, const jhuff *h)
{
    int code = 0, first = 0, index = 0, len;
    if (!h->present)
        return -1;
    for (len = 1; len <= 16; len++) {
        int b = jbits(j, 1);
        if (b < 0)
            return b;
        code |= b;
        if (code - (int) h->count[len] < first)
            return (int) h->symbol[index + (code - first)];
        index += h->count[len];
        first += h->count[len];
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

static int
jextend(int v, int n)
{
    if (v < 0)
        return v;
    if (n == 0)
        return 0;
    return (v < (1 << (n - 1))) ? v - ((1 << n) - 1) : v;
}

/* ---------------- header helpers ---------------- */

static int
build_table(jhuff *h, const uint8_t *bits, const uint8_t *syms, int tot)
{
    int i;
    for (i = 1; i <= 16; i++)
        h->count[i] = bits[i - 1];
    if (tot > 256)
        return -1;
    for (i = 0; i < tot; i++)
        h->symbol[i] = syms[i];
    h->present = 1;
    return 0;
}

static int
rd16(const uint8_t *q)
{
    return (q[0] << 8) | q[1];
}

/* ---------------- block decode + IDCT ---------------- */

static int
decode_block(jpeg_ctx *j, const jcomp *cp, int32_t *blk, int ci)
{
    const int16_t *q = j->qtab[cp->tq];
    int k, sym, s, r, val;

    memset(blk, 0, 64 * sizeof(blk[0]));

    sym = jhdec(j, &j->hd[cp->td]);
    if (sym < 0)
        return sym;
    val = jbits(j, sym);
    if (val < 0)
        return val;
    j->pred[ci] += jextend(val, sym);
    val = (int) ((long) j->pred[ci] * q[0]);
    blk[0] = (int32_t) (val < -32768 ? -32768 : (val > 32768 ? 32768 : val));

    for (k = 1; k < 64;) {
        sym = jhdec(j, &j->ha[cp->ta]);
        if (sym < 0)
            return sym;
        s = sym & 15;
        r = sym >> 4;
        if (s == 0) {
            if (r != 15)
                break;
            k += 16;
            continue;
        }
        k += r;
        if (k > 63)
            return IMG_ERR_CORRUPT;
        val = jbits(j, s);
        if (val < 0)
            return val;
        val = jextend(val, s);
        val = (int) ((long) val * q[zigzag[k]]);
        blk[zigzag[k]] =
            (int32_t) (val < -32768 ? -32768 : (val > 32768 ? 32768 : val));
        k++;
    }
    return 1;
}

static void
idct_to_plane(const int32_t *blk, uint8_t *plane, int stride, int ox, int oy,
              int cw, int ch)
{
    int32_t t[64];
    int x, u, v, b;
    if (oy >= ch)
        return;
    for (x = 0; x < 8; x++) {
        for (v = 0; v < 8; v++) {
            int32_t s = 0;
            for (u = 0; u < 8; u++)
                s += (int32_t) idct_cs[u][x] * blk[u * 8 + v];
            t[x * 8 + v] = (s + 512) >> 10;
        }
    }
    for (x = 0; x < 8 && oy + x < ch; x++) {
        uint8_t *out = plane + (size_t) (oy + x) * stride;
        for (v = 0; v < 8; v++) {
            int gx = ox + v;
            int64_t a = 0;
            int px;
            if (gx >= cw)
                continue;
            for (u = 0; u < 8; u++)
                a += (int64_t) idct_cs[u][v] * t[x * 8 + u];
            /* each pass divides by 1024; total a/1024 == 4*f -> >>2 for /4 */
            b = (int32_t) ((a + 512) >> 10);
            px = ((b + 2) >> 2) + 128;
            if (px < 0)
                px = 0;
            else if (px > 255)
                px = 255;
            out[gx] = (uint8_t) px;
        }
    }
}

/* ---------------- entropy scan (single scan per SOS) ---------------- */

static int next_marker(jpeg_ctx *j);

/* returns: 1 = scan ended at marker (marker still pending),
 *          negative = error */
static int
decode_scan(jpeg_ctx *j)
{
    static int32_t blk[64];
    int mcu_cols = (j->w + j->maxh * 8 - 1) / (j->maxh * 8);
    int mcu_rows = (j->h + j->maxv * 8 - 1) / (j->maxv * 8);
    int mrow, mcol, ci, by, bx;
    int restarts_allowed = 1;

    for (mrow = 0; mrow < mcu_rows; mrow++) {
        for (mcol = 0; mcol < mcu_cols; mcol++) {
            for (ci = 0; ci < j->ncomp; ci++) {
                jcomp *cp = &j->c[ci];
                int bx0 = mcol * cp->h, by0 = mrow * cp->v;
                for (by = 0; by < cp->v; by++) {
                    for (bx = 0; bx < cp->h; bx++) {
                    retry:;
                        int rc = decode_block(j, cp, blk, ci);
                        if (rc == -2) {
                            int m;
                            if (!restarts_allowed)
                                return 1;
                            /* consume the marker */
                            if (j->p >= j->end || *j->p != 0xFF)
                                return IMG_ERR_CORRUPT;
                            m = next_marker(j);
                            if (m >= 0xD0 && m <= 0xD7) {
                                j->bitbuf = 0;
                                j->bitcnt = 0;
                                j->marker_hit = 0;
                                memset(j->pred, 0, sizeof(j->pred));
                                goto retry;
                            }
                            return 1; /* end of scan */
                        }
                        if (rc < 0)
                            return rc;
                        idct_to_plane(blk, cp->plane, cp->pw,
                                      (bx0 + bx) * 8, (by0 + by) * 8, cp->pw,
                                      cp->ph);
                    }
                }
            }
        }
    }
    return 1;
}

static int
next_marker(jpeg_ctx *j)
{
    if (j->p >= j->end || *j->p != 0xFF)
        return -1;
    do {
        j->p++;
        if (j->p >= j->end)
            return -1;
    } while (*j->p == 0xFF);
    return *j->p++;
}

/* ---------------- YCbCr -> RGB with bilinear chroma upsample ---------------- */

/* bilinear sample of component plane at output pixel (x,y) center-mapped */
static int
samp2d(const jcomp *cp, int x, int y, int w, int h)
{
    int fx = (2 * x + 1) * cp->pw * 2048 / w - 2048;
    int fy = (2 * y + 1) * cp->ph * 2048 / h - 2048;
    int x0, x1, y0, y1, tx, ty;
    int64_t top, bot;
    if (fx < 0)
        fx = 0;
    if (fy < 0)
        fy = 0;
    x0 = fx >> 12;
    tx = fx & 4095;
    y0 = fy >> 12;
    ty = fy & 4095;
    if (x0 >= cp->pw)
        x0 = cp->pw - 1;
    x1 = x0 + 1 < cp->pw ? x0 + 1 : x0;
    if (y0 >= cp->ph)
        y0 = cp->ph - 1;
    y1 = y0 + 1 < cp->ph ? y0 + 1 : y0;
    top = cp->plane[(size_t) y0 * cp->pw + x0] * (4096 - tx) +
          cp->plane[(size_t) y0 * cp->pw + x1] * tx;
    bot = cp->plane[(size_t) y1 * cp->pw + x0] * (4096 - tx) +
          cp->plane[(size_t) y1 * cp->pw + x1] * tx;
    return (top * (4096 - ty) + bot * ty) >> 24;
}



static int
finalize(jpeg_ctx *j)
{
    int x, y;
    if (j->ncomp == 1) {
        jcomp *cy = &j->c[0];
        for (y = 0; y < j->h; y++) {
            const uint8_t *row;
            uint32_t *out = j->fb + (size_t) y * j->w;
            int gy = y * cy->v / j->maxv;
            if (gy >= cy->ph)
                gy = cy->ph - 1;
            row = cy->plane + (size_t) gy * cy->pw;
            for (x = 0; x < j->w; x++) {
                int gx = x * cy->h / j->maxh;
                uint32_t g;
                if (gx >= cy->pw)
                    gx = cy->pw - 1;
                g = row[gx];
                out[x] = g | (g << 8) | (g << 16);
            }
        }
        return IMG_OK;
    }
    if (j->ncomp != 3)
        return IMG_ERR_UNSUPPORTED;
    {
        jcomp *cy = &j->c[0], *cb = &j->c[1], *cr = &j->c[2];
        for (y = 0; y < j->h; y++) {
            uint32_t *out = j->fb + (size_t) y * j->w;
            const uint8_t *yr;
            int yby = y * cy->v / j->maxv;
            if (yby >= cy->ph)
                yby = cy->ph - 1;
            yr = cy->plane + (size_t) yby * cy->pw;
            for (x = 0; x < j->w; x++) {
                int bx = x * cy->h / j->maxh;
                int yy, u, v, r, g, b;
                if (bx >= cy->pw)
                    bx = cy->pw - 1;
                yy = yr[bx];
                u = samp2d(cb, x, y, j->w, j->h) - 128;
                v = samp2d(cr, x, y, j->w, j->h) - 128;
                r = yy + ((v * 1436) >> 10);
                g = yy - ((u * 352 + v * 731) >> 10);
                b = yy + ((u * 1814) >> 10);
                if (r < 0)
                    r = 0;
                else if (r > 255)
                    r = 255;
                if (g < 0)
                    g = 0;
                else if (g > 255)
                    g = 255;
                if (b < 0)
                    b = 0;
                else if (b > 255)
                    b = 255;
                out[x] = (uint32_t) r | ((uint32_t) g << 8) |
                         ((uint32_t) b << 16);
            }
        }
    }
    return IMG_OK;
}

/* ---------------- main entry ---------------- */

int
img_decode_jpeg(const uint8_t *data, size_t len, img_surface *out)
{
    jpeg_ctx *j;
    int rc = IMG_ERR_CORRUPT;
    const uint8_t *seg;
    size_t seglen;

    if (!data || len < 4 || data[0] != 0xFF || data[1] != 0xD8)
        return IMG_ERR_FORMAT;

    j = (jpeg_ctx *) calloc(1, sizeof(*j));
    if (!j)
        return IMG_ERR_NOMEM;
    j->p = data + 2;
    j->end = data + len;

    for (;;) {
        int m = next_marker(j);
        if (m < 0)
            goto done;
        if (m >= 0xD0 && m <= 0xD7) /* RST without SOS context: skip */
            continue;
        if (m == 0xD9) { /* EOI */
            rc = IMG_OK;
            goto done;
        }
        if (m == 0x01)
            continue;
        if (j->p + 2 > j->end)
            goto done;
        seglen = (size_t) rd16(j->p);
        if (seglen < 2 || j->p + seglen > j->end)
            goto done;
        seg = j->p + 2;
        seglen -= 2;

        switch (m) {
        case 0xDB: /* DQT */
        {
            const uint8_t *q = seg;
            size_t n = seglen;
            while (n >= 65) {
                int prec = q[0] >> 4, id = q[0] & 15, i;
                int step = prec ? 129 : 65;
                if (id > 3 || n < (size_t) step)
                    goto done;
                for (i = 0; i < 64; i++) {
                    int v = prec ? rd16(q + 1 + i * 2) : q[1 + i];
                    if (v > 32767)
                        v = 32767;
                    j->qtab[id][zigzag[i]] = (int16_t) v;
                }
                q += step;
                n -= step;
            }
            break;
        }
        case 0xC4: /* DHT */
        {
            const uint8_t *q = seg;
            size_t n = seglen;
            while (n > 17) {
                int tc = q[0] >> 4, th = q[0] & 15, tot = 0, i;
                jhuff *t;
                if (tc > 1 || th > 3)
                    goto done;
                t = tc ? &j->ha[th] : &j->hd[th];
                for (i = 1; i <= 16; i++)
                    tot += q[i];
                if (n < (size_t) (17 + tot))
                    goto done;
                if (build_table(t, q + 1, q + 17, tot))
                    goto done;
                q += 17 + tot;
                n -= 17 + tot;
            }
            break;
        }
        case 0xC0: /* SOF0 baseline */
        case 0xC1: /* SOF1 extended */
        {
            int i;
            if (seglen < 6 || seg[0] != 8)
                goto done;
            j->h = rd16(seg + 1);
            j->w = rd16(seg + 3);
            j->ncomp = seg[5];
            if (j->ncomp != 1 && j->ncomp != 3)
                goto done;
            if (j->w <= 0 || j->h <= 0 || j->w > IMG_MAX_DIM ||
                j->h > IMG_MAX_DIM) {
                rc = IMG_ERR_SIZE;
                goto done;
            }
            if (seglen < (size_t) (6 + 3 * j->ncomp))
                goto done;
            j->maxh = 1;
            j->maxv = 1;
            for (i = 0; i < j->ncomp; i++) {
                j->c[i].id = seg[6 + i * 3];
                j->c[i].h = seg[7 + i * 3] >> 4;
                j->c[i].v = seg[7 + i * 3] & 15;
                j->c[i].tq = seg[8 + i * 3] & 3;
                if (j->c[i].h < 1 || j->c[i].h > 4 || j->c[i].v < 1 ||
                    j->c[i].v > 4)
                    goto done;
                if (j->c[i].h > j->maxh)
                    j->maxh = j->c[i].h;
                if (j->c[i].v > j->maxv)
                    j->maxv = j->c[i].v;
            }
            break;
        }
        case 0xC2: /* SOF2 progressive */
            rc = IMG_ERR_UNSUPPORTED;
            goto done;
        case 0xDD: /* DRI - accepted; restarts handled via RST markers */
            break;
        case 0xDA: /* SOS */
        {
            int ns, i;
            if (j->w <= 0)
                goto done;
            if (j->fb == NULL) {
                j->fb = (uint32_t *) malloc((size_t) j->w * j->h * 4);
                if (!j->fb) {
                    rc = IMG_ERR_NOMEM;
                    goto done;
                }
            }
            ns = seg[0];
            if (ns != j->ncomp || seglen < (size_t) (1 + 2 * ns + 3))
                goto done;
            for (i = 0; i < ns; i++) {
                int id = seg[1 + i * 2];
                int k;
                for (k = 0; k < j->ncomp; k++)
                    if (j->c[k].id == id)
                        break;
                if (k == j->ncomp)
                    goto done;
                j->c[k].td = (seg[2 + i * 2] >> 4) & 3;
                j->c[k].ta = seg[2 + i * 2] & 3;
            }
            if (seg[1 + ns * 2] != 0 || seg[2 + ns * 2] != 63 ||
                (seg[3 + ns * 2] & 15) != 0) {
                rc = IMG_ERR_UNSUPPORTED; /* progressive */
                goto done;
            }
            /* allocate planes now (after SOF dims + factors known) */
            for (i = 0; i < j->ncomp; i++) {
                if (j->c[i].plane)
                    continue;
                j->c[i].pw = (j->w * j->c[i].h + j->maxh - 1) / j->maxh;
                j->c[i].ph = (j->h * j->c[i].v + j->maxv - 1) / j->maxv;
                j->c[i].plane =
                    (uint8_t *) malloc((size_t) j->c[i].pw * j->c[i].ph + 16);
                if (!j->c[i].plane) {
                    rc = IMG_ERR_NOMEM;
                    goto done;
                }
                memset(j->c[i].plane, 0,
                       (size_t) j->c[i].pw * j->c[i].ph + 16);
            }
            j->p += seglen + 2; /* move to entropy data */
            memset(j->pred, 0, sizeof(j->pred));
            j->bitbuf = 0;
            j->bitcnt = 0;
            j->marker_hit = 0;
            rc = decode_scan(j);
            if (rc < 0)
                goto done;
            /* ensure cursor sits on the marker that ended the scan */
            while (j->p < j->end) {
                if (*j->p == 0xFF)
                    break;
                j->p++;
            }
            if (j->p >= j->end) {
                rc = IMG_ERR_CORRUPT;
                goto done;
            }
            continue; /* outer loop reads the pending marker */
        }
        default:
            break; /* skip unknown segment (APPn, COM, ...) */
        }
        j->p += seglen + 2;
    }

done:
    if (rc == IMG_OK && j->w > 0 && j->fb && j->c[0].plane) {
        rc = finalize(j);
        if (rc == IMG_OK) {
            out->px = j->fb;
            out->w = j->w;
            out->h = j->h;
            j->fb = NULL;
        }
    } else if (rc == IMG_OK) {
        rc = IMG_ERR_CORRUPT;
    }
    free(j->fb);
    {
        int i;
        for (i = 0; i < 4; i++)
            free(j->c[i].plane);
    }
    free(j);
    return rc;
}
