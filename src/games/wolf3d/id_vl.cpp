// ID_VL for TCMIPS: 8-bit paletted shadow surfaces + RGB32 LUT present.
#include "wl_def.h"
#include <string.h>
#include <stdlib.h>

// Globals from id_vl.h
SDL_Surface *screen = NULL;
unsigned screenPitch;
SDL_Surface *screenBuffer = NULL;
unsigned bufferPitch;
SDL_Surface *curSurface = NULL;
unsigned curPitch;
unsigned screenWidth = 320;
unsigned screenHeight = 240;
unsigned screenBits = 8;
unsigned scaleFactor;
boolean   screenfaded;
unsigned bordercolor;

boolean fullscreen = true;
boolean usedoublebuffering = false;

#define RGB(r,g,b) {(r)*255/63,(g)*255/63,(b)*255/63,0}
SDL_Color gamepal[] = {
#include "wolfpal.inc"
};
#undef RGB
SDL_Color curpal[256];

namespace
{
    Uint8    scrpixA[320 * 240];          // 'screen' shadow
    Uint8    scrpixB[320 * 240];          // 'screenBuffer' shadow
    short    pixelanglebuf[320];
    int      wallheightbuf[320];
    uint32_t rgbLUT[256];
    SDL_PixelFormat fmt8 = { 8, 1 };
}

SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int w, int h, int depth,
                                  Uint32 rm, Uint32 gm, Uint32 bm, Uint32 am)
{
    (void)flags; (void)depth; (void)rm; (void)gm; (void)bm; (void)am;
    SDL_Surface *s = (SDL_Surface *) malloc(sizeof(SDL_Surface));
    CHECKMALLOCRESULT(s);
    s->pixels = (Uint8 *) malloc(w * h);
    CHECKMALLOCRESULT(s->pixels);
    memset(s->pixels, 0, w * h);
    s->format = &fmt8;
    s->w = w; s->h = h; s->pitch = w;
    return s;
}

void SDL_FreeSurface(SDL_Surface *s)
{
    if (!s) return;
    if (s->pixels != scrpixA && s->pixels != scrpixB) free(s->pixels);
    free(s);
}

int SDL_BlitSurface(SDL_Surface *src, void *srcrect, SDL_Surface *dst, void *dstrect)
{
    (void)srcrect; (void)dstrect;
    if (!src || !dst) return -1;
    for (int y = 0; y < src->h && y < dst->h; y++)
        memcpy(dst->pixels + y * dst->pitch, src->pixels + y * src->pitch, src->w);
    return 0;
}

void SDL_Flip(SDL_Surface *s)
{
    (void)s;
    tcm_present();
}

void SDL_SetColors(SDL_Surface *s, SDL_Color *colors, int first, int n)
{
    (void)s; (void)colors; (void)first; (void)n;
}

int SDL_GetMouseState(int *x, int *y)
{
    if (x) *x = 0;
    if (y) *y = 0;
    return 0;
}

int SDL_FillRect(SDL_Surface *dst, void *rect, Uint32 color)
{
    (void)rect;
    memset(dst->pixels, (Uint8)color, dst->pitch * dst->h);
    return 0;
}

Uint32 SDL_MapRGB(const SDL_PixelFormat *fmt, Uint8 r, Uint8 g, Uint8 b)
{
    (void)fmt;
    return 0xFF000000u | ((Uint32)r << 16) | ((Uint32)g << 8) | b;
}

extern short *pixelangle;
extern int   *wallheight;

uint32_t *tcm_confb = NULL;

#ifdef HOST_TEST
extern "C" void wolf_tick_hook(void);
#endif
void tcm_present(void)
{
    if (!tcm_confb) return;
    const Uint8 *src = screen->pixels;
    uint32_t *dst = tcm_confb;
    for (int i = 0; i < 320 * 240; i++)
        dst[i] = rgbLUT[src[i]];
#ifdef HOST_TEST
    wolf_tick_hook();
#endif
}

#ifdef HOST_TEST
extern "C" void host_delay_ms(uint32_t ms);
void SDL_Delay(Uint32 ms) { host_delay_ms(ms); }
#else
void SDL_Delay(Uint32 ms) {}
#endif

static void BuildLUT(void)
{
    for (int i = 0; i < 256; i++)
    {
        uint32_t r = curpal[i].r, g = curpal[i].g, b = curpal[i].b;
#ifndef TCM_PAL_DIAG
        // TCMIPS console expects BGRX byte order (blue in the high bits)
        rgbLUT[i] = 0xFF000000u | (b << 16) | (g << 8) | r;
#else
        // diagnostic: force visible gray ramp so any structure in the shadow
        // buffer shows up regardless of game palette state
        uint8_t v = (uint8_t)(i < 128 ? 40 + i : 215 - (i - 128));
        rgbLUT[i] = 0xFF000000u | ((uint32_t)v << 16) | ((uint32_t)v << 8) | v;
#endif
    }
}

//===========================================================================

void VL_Shutdown(void)
{
}

void VL_SetTextMode(void)
{
}

void VL_SetVGAPlaneMode(void)
{
    if (!tcm_confb)
        tcm_confb = (uint32_t *) tcm_pixel_console_init(CONSOLE_MODE_PIXEL_32, 320);

    screenWidth = 320;
    screenHeight = 240;
    screenBits = 8;
    scaleFactor = 1;
    pixelangle = pixelanglebuf;
    wallheight = wallheightbuf;

    if (!screen)
    {
        screen = (SDL_Surface *) malloc(sizeof(SDL_Surface));
        CHECKMALLOCRESULT(screen);
        screen->pixels = scrpixA;
        screen->format = &fmt8;
        screen->w = 320; screen->h = 240; screen->pitch = 320;
    }
    if (!screenBuffer)
    {
        screenBuffer = (SDL_Surface *) malloc(sizeof(SDL_Surface));
        CHECKMALLOCRESULT(screenBuffer);
        screenBuffer->pixels = scrpixB;
        screenBuffer->format = &fmt8;
        screenBuffer->w = 320; screenBuffer->h = 240; screenBuffer->pitch = 320;
    }

    memcpy(curpal, gamepal, sizeof(SDL_Color) * 256);
    BuildLUT();

    screenPitch = screen->pitch;
    bufferPitch = screenBuffer->pitch;
    curPitch = bufferPitch;
    curSurface = screenBuffer;

    memset(scrpixA, 0, sizeof(scrpixA));
    memset(scrpixB, 0, sizeof(scrpixB));
    tcm_present();
}

/*
============================================================================

                        PALETTE OPS

        Used to set colors or just write out a palette

============================================================================
*/

void VL_ConvertPalette(byte *srcpal, SDL_Color *destpal, int numColors)
{
    for (int i = 0; i < numColors; i++)
    {
        destpal[i].r = srcpal[i * 3] * 255 / 63;
        destpal[i].g = srcpal[i * 3 + 1] * 255 / 63;
        destpal[i].b = srcpal[i * 3 + 2] * 255 / 63;
        destpal[i].unused = 0;
    }
}

void VL_FillPalette(int red, int green, int blue)
{
    for (int i = 0; i < 256; i++)
    {
        curpal[i].r = red;
        curpal[i].g = green;
        curpal[i].b = blue;
    }
    BuildLUT();
    tcm_present();
}

void VL_SetColor(int color, int red, int green, int blue)
{
    curpal[color].r = red;
    curpal[color].g = green;
    curpal[color].b = blue;
    BuildLUT();
}

void VL_GetColor(int color, int *red, int *green, int *blue)
{
    *red   = curpal[color].r;
    *green = curpal[color].g;
    *blue  = curpal[color].b;
}

void VL_SetPalette(SDL_Color *palette, bool forceupdate)
{
    (void)forceupdate;
    memcpy(curpal, palette, sizeof(SDL_Color) * 256);
    BuildLUT();
}

void VL_GetPalette(SDL_Color *palette)
{
    memcpy(palette, curpal, sizeof(SDL_Color) * 256);
}

void VL_FadeOut(int start, int end, int red, int green, int blue, int steps)
{
    byte orig[256 * 3];
    for (int i = 0; i < 256; i++)
    {
        orig[i * 3 + 0] = curpal[i].r;
        orig[i * 3 + 1] = curpal[i].g;
        orig[i * 3 + 2] = curpal[i].b;
    }

    for (int j = 0; j < steps; j++)
    {
        int dst = start;
        for (int i = start; i <= end; i++)
        {
            curpal[dst].r = orig[i * 3 + 0] * (steps - 1 - j) / steps + red * (j + 1) / steps;
            curpal[dst].g = orig[i * 3 + 1] * (steps - 1 - j) / steps + green * (j + 1) / steps;
            curpal[dst].b = orig[i * 3 + 2] * (steps - 1 - j) / steps + blue * (j + 1) / steps;
            dst++;
        }
        BuildLUT();
        tcm_present();
        SDL_Delay(8);
    }
}

void VL_FadeIn(int start, int end, SDL_Color *palette, int steps)
{
    byte orig[256 * 3];
    for (int i = 0; i < 256; i++)
    {
        orig[i * 3 + 0] = palette[i].r;
        orig[i * 3 + 1] = palette[i].g;
        orig[i * 3 + 2] = palette[i].b;
    }

    for (int j = 0; j < steps; j++)
    {
        int dst = start;
        for (int i = start; i <= end; i++)
        {
            curpal[dst].r = orig[i * 3 + 0] * (j + 1) / steps + curpal[dst].r * (steps - 1 - j) / steps;
            curpal[dst].g = orig[i * 3 + 1] * (j + 1) / steps + curpal[dst].g * (steps - 1 - j) / steps;
            curpal[dst].b = orig[i * 3 + 2] * (j + 1) / steps + curpal[dst].b * (steps - 1 - j) / steps;
            dst++;
        }
        BuildLUT();
        tcm_present();
        SDL_Delay(8);
    }
}

byte *VL_LockSurface(SDL_Surface *surface)
{
    return surface ? surface->pixels : NULL;
}

void VL_UnlockSurface(SDL_Surface *surface)
{
    (void)surface;
}

/*
============================================================================

                            PIXEL OPS

============================================================================
*/

byte VL_GetPixel(int x, int y)
{
    assert(x >= 0 && x < (int)screenWidth && y >= 0 && y < (int)screenHeight);
    return *((byte *)curSurface->pixels + y * curPitch + x);
}

void VL_Plot(int x, int y, int color)
{
    assert(x >= 0 && x < (int)screenWidth && y >= 0 && y < (int)screenHeight);
    *((byte *)curSurface->pixels + y * curPitch + x) = color;
}

void VL_Hlin(unsigned x, unsigned y, unsigned width, int color)
{
    memset((byte *)curSurface->pixels + y * curPitch + x, color, width);
}

void VL_Vlin(int x, int y, int height, int color)
{
    byte *dest = (byte *)curSurface->pixels + y * curPitch + x;
    while (height-- > 0)
    {
        *dest = color;
        dest += curPitch;
    }
}

void VL_BarScaledCoord(int scx, int scy, int scwidth, int scheight, int color)
{
    if (scx < 0) { scwidth += scx; scx = 0; }
    if (scy < 0) { scheight += scy; scy = 0; }
    if (scx + scwidth > (int)screenWidth)  scwidth  = screenWidth  - scx;
    if (scy + scheight > (int)screenHeight) scheight = screenHeight - scy;
    if (scwidth <= 0 || scheight <= 0) return;

    byte *dest = (byte *)curSurface->pixels + scy * curPitch + scx;
    while (scheight-- > 0)
    {
        memset(dest, color, scwidth);
        dest += curPitch;
    }
}

/*
============================================================================

                     VGA COMPATIBILITY ROUTINES

============================================================================
*/

// Graphics chunks are stored column-planar: 4 planes of width/4 bytes.
// This de-munges one row-interleaved picture into linear bytes.

void VL_DrawPicBare(int x, int y, byte *pic, int width, int height)
{
    byte *dest = (byte *)curSurface->pixels + y * curPitch + x;
    while (height-- > 0)
    {
        memcpy(dest, pic, width);
        pic += width;
        dest += curPitch;
    }
}

void VL_MemToLatch(byte *source, int width, int height,
                   SDL_Surface *destSurface, int x, int y)
{
    byte *dest = destSurface->pixels + y * destSurface->pitch + x;
    for (int yp = 0; yp < height; yp++)
    {
        for (int xp = 0; xp < width; xp++)
        {
            dest[xp] = source[(yp * (width >> 2) + (xp >> 2)) + (xp & 3) * (width >> 2) * height];
        }
        dest += destSurface->pitch;
    }
}

void VL_ScreenToScreen(SDL_Surface *source, SDL_Surface *dest)
{
    byte *src = (byte *)source->pixels;
    byte *dst = (byte *)dest->pixels;
    for (unsigned y = 0; y < screenHeight; y++, src += source->pitch, dst += dest->pitch)
        memcpy(dst, src, screenWidth);
}

void VL_MemToScreenScaledCoord(byte *source, int width, int height, int scx, int scy)
{
    byte *dest = (byte *)curSurface->pixels + scy * curPitch + scx;
    for (int yp = 0; yp < height; yp++)
    {
        for (int xp = 0; xp < width; xp++)
        {
            dest[xp] = source[(yp * (width >> 2) + (xp >> 2)) + (xp & 3) * (width >> 2) * height];
        }
        dest += curPitch;
    }
}

void VL_MemToScreenScaledCoord(byte *source, int origwidth, int origheight, int srcx, int srcy,
                               int destx, int desty, int width, int height)
{
    byte *dest = (byte *)curSurface->pixels + desty * curPitch + destx;
    for (int yp = 0; yp < height; yp++)
    {
        for (int xp = 0; xp < width; xp++)
        {
            dest[xp] = source[((yp + srcy) * (origwidth >> 2) + ((xp + srcx) >> 2))
                              + (xp + srcx & 3) * (origwidth >> 2) * origheight];
        }
        dest += curPitch;
    }
}

void VL_LatchToScreenScaledCoord(SDL_Surface *source, int xsrc, int ysrc,
                                 int width, int height, int scxdest, int scydest)
{
    byte *dest = (byte *)curSurface->pixels + scydest * curPitch + scxdest;
    byte *src  = source->pixels + ysrc * source->pitch + xsrc;
    for (int yp = 0; yp < height; yp++, src += source->pitch, dest += curPitch)
        memcpy(dest, src, width);
}

void VL_MaskedToScreen(byte *source, int width, int height, int x, int y)
{
    byte *dest = (byte *)curSurface->pixels + y * curPitch + x;
    for (int c = 0; c < height; c++)
    {
        for (int d = 0; d < width; d++)
        {
            byte pixel = *source++;
            if (pixel != 0xFF) *dest = pixel;
            dest++;
        }
        dest += curPitch - width;
    }
}
