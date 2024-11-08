#ifndef WIN_H
#define WIN_H

#include "SDL2/SDL_vulkan.h"
#include "SDL2/SDL.h"

#include "../solh/sol.h"
#include "keys.h"

enum mod_flags {
    PRESS = 0x01,
    RELEASE = 0x02,
    CTRL = 0x04,
    ALT = 0x08,
    SHIFT = 0x10,
};

enum win_flags {
    WIN_CLO = 0x01,
    WIN_MIN = 0x02,
    WIN_MAX = 0x04,
    WIN_RSZ = 0x08,
    
    WIN_SZ = WIN_MIN|WIN_MAX|WIN_RSZ,
};

struct keyboard_input {
    u16 key; // enum key_codes
    u16 mod; // enum mod_flags
};

#define KEY_BUFFER_SIZE 64

struct win {
    SDL_Window *handle;
    struct extent_u16 dim;
    struct extent_f32 rdim; // reciprocal of dim
    
    struct {
        u16 write,read; // buffer cursors
        struct keyboard_input buf[KEY_BUFFER_SIZE];
    } kb; // keyboard input ring buffer
    
    u32 flags; // enum win_flags
};

#ifdef LIB
extern struct win *win;

static inline bool win_should_close(void)
{
    _mm_sfence();
    return win->flags & WIN_CLO;
}

static inline u32 win_ms(void)
{
    return SDL_GetTicks();
}

#define def_create_win(name) void name(void)
def_create_win(create_win);

#define def_win_inst_exts(name) void name(u32 *count, char **exts)
def_win_inst_exts(win_inst_exts);

#define def_win_create_surf(name) int name(void)
def_win_create_surf(win_create_surf);

#define def_win_poll(name) int win_poll(void)
def_win_poll(win_poll);

#define def_win_kb_next(name) bool name(struct keyboard_input *ki)
def_win_kb_next(win_kb_next);

#define def_win_screen_extent(name) int name(struct extent_u32 *e)
def_win_screen_extent(win_screen_extent);

#define def_win_key_to_char(name) char name(struct keyboard_input ki)
def_win_key_to_char(win_key_to_char);

#endif // LIB

#endif // include guard
