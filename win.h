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

enum {
    WIN_INPUT_KEY,
    WIN_INPUT_MOUSE,
};

struct window_input {
    u32 type;
    union {
        struct {
            u16 key; // enum key_codes
            u16 mod; // enum mod_flags
        };
    };
};

struct mouse_button {
    union {
        u8 buttons[4];
        struct { u8 b1,b2,b3,b4; };
    };
    struct offset_u32 pos;
    u32 i,time;
};

enum mouse_button_indices {
    MOUSE_BUTTON_1,
    MOUSE_BUTTON_2,
    MOUSE_BUTTON_3,
    MOUSE_BUTTON_4,
};
#define set_mouse_button_pressed(button_state, button_index) (button_state |= (1 << (button_index)))
#define unset_mouse_button_pressed(button_state, button_index) (button_state |= ~(1 << (button_index)))
#define is_mouse_button_pressed(button_state, button_index) (button_state & (1<<(button_index)))

struct mouse_motion {
    struct offset_u32 pos;
    struct offset_s32 mov;
    u32 button_state;
    u32 time;
};

#define KEY_BUFFER_SIZE 64
#define MOUSE_BUFFER_SIZE 16

struct win {
    SDL_Window *handle;
    struct extent_u16 max; // largest possible window dimensions
    struct extent_u16 dim;
    struct extent_f32 rdim; // reciprocal of dim
    
    struct {
        u16 write,read; // buffer cursors
        struct window_input buf[KEY_BUFFER_SIZE];
    } input; // keyboard input ring buffer
    
    struct {
        struct mouse_motion current_motion;
        struct mouse_button current_button;
        struct mouse_motion motion_buffer[MOUSE_BUFFER_SIZE];
        struct mouse_button button_buffer[MOUSE_BUFFER_SIZE];
        u32 motion_buffer_size;
        u32 button_buffer_size;
    } mouse;
    
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

static inline struct offset_u16 win_normalize_screen_px(struct offset_u16 p)
{
    return OFFSET((f32)p.x * win->rdim.w * 65535, (f32)p.y * win->rdim.h * 65535, u16);
}

#define def_create_win(name) void name(void)
def_create_win(create_win);

#define def_win_inst_exts(name) void name(u32 *count, char **exts)
def_win_inst_exts(win_inst_exts);

#define def_win_create_surf(name) int name(void)
def_win_create_surf(win_create_surf);

#define def_win_poll(name) int win_poll(void)
def_win_poll(win_poll);

#define def_win_kb_next(name) bool name(struct window_input *ki)
def_win_kb_next(win_kb_next);

#define def_win_key_to_char(name) char name(struct window_input ki)
def_win_key_to_char(win_key_to_char);

#define timed_trigger(name, initial_value, step) \
local_persist u32 m__ ## name ## _timer = step; \
bool name = initial_value; \
if (m__ ## name ## _timer < win_ms()) { \
name = true; \
m__ ## name ## _timer= win_ms() + step; \
}

#define create_timer(name) u32 name = win_ms();
#define check_timer(name, msg) println("%s%ums", msg, (u64)win_ms() - name);

#endif // LIB

#endif // include guard
