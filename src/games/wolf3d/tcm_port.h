// TCMIPS platform shim for Wolf4SDL.
// Replaces SDL with the TCMIPS console: an 8-bit paletted shadow surface
// blitted through an RGB32 LUT into the pixel console framebuffer.
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <dev/console.h>
#include <dev/syscall.h>
#include <dev/keyboard.h>

typedef uint8_t  Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef int8_t   Sint8;
typedef int16_t  Sint16;
typedef int32_t  Sint32;

struct SDL_Color { Uint8 r, g, b, unused; };

struct SDL_PixelFormat { Uint8 BitsPerPixel; Uint8 BytesPerPixel; };

struct SDL_Surface
{
    SDL_PixelFormat *format;
    Uint8   *pixels;      // 8-bit palette indexes
    int      w, h, pitch;
};

// --- minimal SDL1.2-compatible key codes (values matter only internally) ---
enum
{
    SDLK_UNKNOWN     = 0,
    SDLK_BACKSPACE   = 8,
    SDLK_TAB         = 9,
    SDLK_RETURN      = 13,
    SDLK_ESCAPE      = 27,
    SDLK_SPACE       = 32,
    SDLK_QUOTE       = 39,
    SDLK_COMMA       = 44,
    SDLK_MINUS       = 45,
    SDLK_PERIOD      = 46,
    SDLK_SLASH       = 47,
    SDLK_SEMICOLON   = 59,
    SDLK_EQUALS      = 61,
    SDLK_LEFTBRACKET = 91,
    SDLK_BACKSLASH   = 92,
    SDLK_RIGHTBRAKET = 93,
    SDLK_BACKQUOTE   = 96,
    SDLK_DELETE      = 127,
    SDLK_UP          = 273,
    SDLK_DOWN        = 274,
    SDLK_RIGHT       = 275,
    SDLK_LEFT        = 276,
    SDLK_INSERT      = 277,
    SDLK_HOME        = 278,
    SDLK_END         = 279,
    SDLK_PAGEUP      = 280,
    SDLK_PAGEDOWN    = 281,
    SDLK_F1          = 282,
    SDLK_F2          = 283,
    SDLK_F3          = 284,
    SDLK_F4          = 285,
    SDLK_F5          = 286,
    SDLK_F6          = 287,
    SDLK_F7          = 288,
    SDLK_F8          = 289,
    SDLK_F9          = 290,
    SDLK_F10         = 291,
    SDLK_F11         = 292,
    SDLK_F12         = 293,
    SDLK_SCROLLOCK   = 302,
    SDLK_CAPSLOCK    = 301,
    SDLK_RSHIFT      = 303,
    SDLK_LSHIFT      = 304,
    SDLK_RCTRL       = 305,
    SDLK_LCTRL       = 306,
    SDLK_RALT        = 307,
    SDLK_LALT        = 308,
    SDLK_PRINT       = 316,
    SDLK_KP_ENTER    = 271,
    SDLK_LAST        = 323
};

// ASCII-valued keys (SDL1.2 keeps them at their code point)
enum
{
    SDLK_0 = 48, SDLK_1, SDLK_2, SDLK_3, SDLK_4, SDLK_5,
    SDLK_6, SDLK_7, SDLK_8, SDLK_9,
    SDLK_a = 97, SDLK_b, SDLK_c, SDLK_d, SDLK_e, SDLK_f, SDLK_g,
    SDLK_h, SDLK_i, SDLK_j, SDLK_k, SDLK_l, SDLK_m, SDLK_n,
    SDLK_o, SDLK_p, SDLK_q, SDLK_r, SDLK_s, SDLK_t, SDLK_u,
    SDLK_v, SDLK_w, SDLK_x, SDLK_y, SDLK_z,
    SDLK_KP1 = 0x1000, SDLK_KP2, SDLK_KP3, SDLK_KP4, SDLK_KP5,
    SDLK_KP6, SDLK_KP7, SDLK_KP8, SDLK_KP9
};

static inline uint32_t tcm_ticks_ms(void)
{
    return tcm_syscall_get_timestamp() * 1000u + tcm_syscall_get_timestamp_milli();
}
#define SDL_GetTicks() tcm_ticks_ms()

void SDL_Delay(Uint32 ms);

// minimal surface API used by the engine (implemented in id_vl.cpp)
#define SDL_HWSURFACE  0
#define SDL_SWSURFACE  0
SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int w, int h, int depth,
                                  Uint32 rm, Uint32 gm, Uint32 bm, Uint32 am);
void SDL_FreeSurface(SDL_Surface *s);
int  SDL_BlitSurface(SDL_Surface *src, void *srcrect, SDL_Surface *dst, void *dstrect);
void SDL_Flip(SDL_Surface *s);
void SDL_SetColors(SDL_Surface *s, SDL_Color *colors, int first, int n);
Uint32 SDL_MapRGB(const SDL_PixelFormat *fmt, Uint8 r, Uint8 g, Uint8 b);
int  SDL_FillRect(SDL_Surface *dst, void *rect, Uint32 color);
int  SDL_GetMouseState(int *x, int *y);
#define SDL_BUTTON(x) (1 << ((x) - 1))
#define SDL_BUTTON_LEFT   1
#define SDL_BUTTON_MIDDLE 2
#define SDL_BUTTON_RIGHT  3
#define KMOD_SHIFT 0x03
#define KMOD_NUM   0x10
#define KMOD_CAPS  0x04

// raw console keyboard code (ASCII-ish, arrows per dev/keyboard.h)
uint32_t tcm_read_keyboard(void);

// shadow framebuffer -> console present (implemented in tcm_vl.cpp)
extern uint32_t *tcm_confb;               // console framebuffer (RGB32)
void tcm_present(void);                   // LUT-blit shadow buffer to console
