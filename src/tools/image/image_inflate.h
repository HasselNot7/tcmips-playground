/* DEFLATE stream reader shared by PNG (raw inflate). */
#ifndef TCM_IMAGE_INFLATE_H
#define TCM_IMAGE_INFLATE_H

#include <stddef.h>
#include <stdint.h>

/* Inflate a raw DEFLATE stream into dst[0..dlen). Returns IMG_OK and sets
 * *written on success, negative IMG_ERR_* otherwise. */
int img_inflate(const uint8_t *src, size_t slen, uint8_t *dst, size_t dlen,
                size_t *written);

#endif /* TCM_IMAGE_INFLATE_H */
