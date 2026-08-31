/* Minimal DEFLATE (RFC 1951) decompressor for PNG IDAT streams.
 * Fixed + dynamic Huffman blocks and stored blocks. 32 KiB window is
 * implicit in the output buffer (PNG guarantees the window fits).
 * Canonical Huffman decode follows the puff.c structure. */
#include "image_inflate.h"
#include "image.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#define INFDBG_ON (getenv("INFDBG") != NULL)

typedef struct {
    const uint8_t *src;
    size_t sp, slen;
    uint32_t bitbuf;
    int bitcnt;
    uint8_t *dst;
    size_t dp, dlen;
} inf_ctx;

typedef struct {
    uint16_t count[16]; /* symbols per bit length */
    uint16_t *symbol;   /* symbols ordered by canonical code */
} inf_huff;

static int
inf_bits(inf_ctx *z, int need)
{
    uint32_t v;
    if (need == 0)
        return 0;
    while (z->bitcnt < need) {
        if (z->sp >= z->slen)
            return -1;
        z->bitbuf |= (uint32_t) z->src[z->sp++] << z->bitcnt;
        z->bitcnt += 8;
    }
    v = z->bitbuf & ((1u << need) - 1u);
    z->bitbuf >>= need;
    z->bitcnt -= need;
    return (int) v;
}

static int
inf_decode(inf_ctx *z, const inf_huff *h)
{
    int code = 0, first = 0, index = 0, len;
    for (len = 1; len <= 15; len++) {
        int b = inf_bits(z, 1);
        if (b < 0)
            return -1;
        code |= b;
        if (code - (int) h->count[len] < first && code >= first)
            return (int) h->symbol[index + (code - first)];
        index += h->count[len];
        first += h->count[len];
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

/* Build canonical tables from a length vector.  scratch must hold >= maxsym. */
static int
inf_build(inf_huff *h, const uint8_t *len, int maxsym, uint16_t *scratch)
{
    uint16_t offs[16];
    int i;
    for (i = 0; i < 16; i++)
        h->count[i] = 0;
    for (i = 0; i < maxsym; i++)
        h->count[len[i]]++;
    h->count[0] = 0;

    offs[1] = 0;
    for (i = 2; i < 16; i++)
        offs[i] = (uint16_t) (offs[i - 1] + h->count[i - 1]);

    h->symbol = scratch;
    /* copy length-ordered symbols; offs doubles as running position */
    for (i = 0; i < maxsym; i++)
        if (len[i])
            scratch[offs[len[i]]++] = (uint16_t) i;
    return 0;
}

/* RFC1951 length codes 257..285: base + extra-bits */
static const uint16_t lc_base[29] = {
    3,  4,  5,  6,  7,  8,  9,  10, 11, 13, 15, 17,  19,  23,  27,  31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};
static const uint8_t lc_extra[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1,
                                      2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4,
                                      5, 5, 5, 5, 0 };

static const uint16_t dc_base[30] = { 1,     2,     3,     4,     5,     7,
                                      9,     13,    17,    25,    33,    49,
                                      65,    97,    129,   193,   257,   385,
                                      513,   769,   1025,  1537,  2049,  3073,
                                      4097,  6145,  8193,  12289, 16385, 24577 };
static const uint8_t dc_extra[30] = { 0, 0, 0,  0,  1,  1,  2,  2,  3,  3,
                                      4, 4, 5,  5,  6,  6,  7,  7,  8,  8,
                                      9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

static int
inf_stored(inf_ctx *z)
{
    uint32_t n;
    int b1, b2;
    z->bitbuf = 0;
    z->bitcnt = 0;
    if (z->sp + 4 > z->slen)
        return IMG_ERR_CORRUPT;
    b1 = z->src[z->sp];
    b2 = z->src[z->sp + 1];
    n = (uint32_t) (b1 | (b2 << 8));
    z->sp += 2;
    b1 = z->src[z->sp];
    b2 = z->src[z->sp + 1];
    if ((uint32_t) (b1 | (b2 << 8)) != (~n & 0xFFFFu))
        return IMG_ERR_CORRUPT;
    z->sp += 2;
    if (z->sp + n > z->slen || z->dp + n > z->dlen)
        return IMG_ERR_CORRUPT;
    memcpy(z->dst + z->dp, z->src + z->sp, n);
    z->dp += n;
    z->sp += n;
    return IMG_OK;
}

static int
inf_fixed_tables(inf_huff *hl, inf_huff *hd, uint16_t *hl_sym,
                 uint16_t *hd_sym)
{
    uint8_t lens[288];
    int i;
    for (i = 0; i < 144; i++)
        lens[i] = 8;
    for (; i < 256; i++)
        lens[i] = 9;
    for (; i < 280; i++)
        lens[i] = 7;
    for (; i < 288; i++)
        lens[i] = 8;
    hl->symbol = hl_sym;
    if (inf_build(hl, lens, 288, hl_sym))
        return IMG_ERR_CORRUPT;
    for (i = 0; i < 30; i++)
        lens[i] = 5;
    hd->symbol = hd_sym;
    if (inf_build(hd, lens, 30, hd_sym))
        return IMG_ERR_CORRUPT;
    return IMG_OK;
}

static int
inf_dyn_tables(inf_ctx *z, inf_huff *hl, inf_huff *hd, uint8_t *lens,
               uint16_t *hl_sym, uint16_t *hd_sym)
{
    static const uint8_t corder[19] = { 16, 17, 18, 0, 8,  7, 9,  6, 10, 5,
                                        11, 4, 12, 3, 13, 2, 14, 1, 15 };
    uint8_t clens[19];
    uint16_t cl_sym[19];
    inf_huff cl;
    int hlit, hdist, hclen, i, n, sym, prev = 0;

    /* RFC1951 dynamic header: HLIT(5)+257, HDIST(5)+1, HCLEN(4)+4 */
    hlit = inf_bits(z, 5);
    hdist = inf_bits(z, 5);
    hclen = inf_bits(z, 4);
    if (hclen < 0 || hdist < 0 || hlit < 0)
        return IMG_ERR_CORRUPT;
    hlit += 257;
    hdist += 1;
    hclen += 4;
    if (hclen > 19 || hlit > 288 || hdist > 30)
        return IMG_ERR_CORRUPT;

    for (i = 0; i < 19; i++)
        clens[i] = 0;
    for (i = 0; i < hclen; i++) {
        int b = inf_bits(z, 3);
        if (b < 0)
            return IMG_ERR_CORRUPT;
        clens[corder[i]] = (uint8_t) b;
    }
    if (inf_build(&cl, clens, 19, cl_sym))
        return IMG_ERR_CORRUPT;

    i = 0;
    while (i < hlit + hdist) {
        sym = inf_decode(z, &cl);
        if (sym < 0)
            return IMG_ERR_CORRUPT;
        if (sym < 16) {
            lens[i++] = (uint8_t) sym;
            prev = sym;
        } else if (sym == 16) {
            n = inf_bits(z, 2);
            if (n < 0 || i == 0)
                return IMG_ERR_CORRUPT;
            n += 3;
            if (i + n > hlit + hdist)
                return IMG_ERR_CORRUPT;
            while (n--)
                lens[i++] = (uint8_t) prev;
        } else if (sym == 17) {
            n = inf_bits(z, 3);
            if (n < 0)
                return IMG_ERR_CORRUPT;
            n += 3;
            if (i + n > hlit + hdist)
                n = hlit + hdist - i;
            while (n--)
                lens[i++] = 0;
        } else {
            n = inf_bits(z, 7);
            if (n < 0)
                return IMG_ERR_CORRUPT;
            n += 11;
            if (i + n > hlit + hdist)
                n = hlit + hdist - i;
            while (n--)
                lens[i++] = 0;
        }
    }

    hl->symbol = hl_sym;
    hd->symbol = hd_sym;
    if (inf_build(hl, lens, hlit, hl_sym))
        return IMG_ERR_CORRUPT;
    if (inf_build(hd, lens + hlit, hdist, hd_sym))
        return IMG_ERR_CORRUPT;
    return IMG_OK;
}

static int
inf_huff_block(inf_ctx *z, const inf_huff *hl, const inf_huff *hd)
{
    for (;;) {
        int sym = inf_decode(z, hl);
        int len, dist, dsym;
        uint32_t src;
        if (sym < 0)
            return IMG_ERR_CORRUPT;
        if (sym == 256)
            return IMG_OK;
        if (sym < 256) {
            if (z->dp >= z->dlen)
                return IMG_ERR_CORRUPT;
            z->dst[z->dp++] = (uint8_t) sym;
            continue;
        }
        sym -= 257;
        if (sym >= 29)
            return IMG_ERR_CORRUPT;
        len = lc_base[sym];
        if (lc_extra[sym]) {
            int e = inf_bits(z, lc_extra[sym]);
            if (e < 0)
                return IMG_ERR_CORRUPT;
            len += e;
        }
        dsym = inf_decode(z, hd);
        if (dsym < 0 || dsym >= 30)
            return IMG_ERR_CORRUPT;
        dist = dc_base[dsym];
        if (dc_extra[dsym]) {
            int e = inf_bits(z, dc_extra[dsym]);
            if (e < 0)
                return IMG_ERR_CORRUPT;
            dist += e;
        }
        if (dist <= 0 || (size_t) dist > z->dp)
            return IMG_ERR_CORRUPT;
        src = (uint32_t) (z->dp - (size_t) dist);
        while (len--) {
            if (z->dp >= z->dlen)
                return IMG_ERR_CORRUPT;
            z->dst[z->dp++] = z->dst[src++];
        }
    }
}

int
img_inflate(const uint8_t *src, size_t slen, uint8_t *dst, size_t dlen,
            size_t *written)
{
    inf_ctx z;
    uint16_t hl_sym[288], hd_sym[30];
    uint8_t lens[318];
    inf_huff hl, hd;
    int last, type;

    memset(&z, 0, sizeof z);
    z.src = src;
    z.slen = slen;
    z.dst = dst;
    z.dlen = dlen;

    do {
        last = inf_bits(&z, 1);
        type = inf_bits(&z, 2);
        if (last < 0 || type < 0)
            return IMG_ERR_CORRUPT;
        if (INFDBG_ON) fprintf(stderr, "[inf] blk last=%d type=%d\n", last, type);
        if (type == 0) {
            int r = inf_stored(&z);
            if (r) {
                if (INFDBG_ON) fprintf(stderr, "[inf] stored fail@%zu\n", z.sp);
                return r;
            }
        } else if (type == 1) {
            if (inf_fixed_tables(&hl, &hd, hl_sym, hd_sym)) {
                if (INFDBG_ON) fprintf(stderr, "[inf] fixed tbl fail\n");
                return IMG_ERR_CORRUPT;
            }
            {
                int r = inf_huff_block(&z, &hl, &hd);
                if (r) {
                    if (INFDBG_ON) fprintf(stderr, "[inf] fixed data fail sp=%zu dp=%zu\n", z.sp, z.dp);
                    return r;
                }
            }
        } else if (type == 2) {
            if (inf_dyn_tables(&z, &hl, &hd, lens, hl_sym, hd_sym)) {
                if (INFDBG_ON) fprintf(stderr, "[inf] dyn tbl fail\n");
                return IMG_ERR_CORRUPT;
            }
            {
                int r = inf_huff_block(&z, &hl, &hd);
                if (r) {
                    if (INFDBG_ON) fprintf(stderr, "[inf] dyn data fail sp=%zu dp=%zu\n", z.sp, z.dp);
                    return r;
                }
            }
        } else {
            return IMG_ERR_CORRUPT;
        }
    } while (!last);

    if (written)
        *written = z.dp;
    return IMG_OK;
}
