#include "win.h"
#include "gpu.h"

struct win *win;

typedef typeof(win->input.read) keycode_t;

keycode_t win_scancode_to_key(SDL_Scancode sc)
{
    return (keycode_t) sc;
}

internal inline keycode_t win_input_inc(keycode_t pos)
{
    return (keycode_t) inc_and_wrap(pos, cl_array_size(win->input.buf));
}

internal int win_kb_add(struct window_input ki)
{
    keycode_t w = win_input_inc(win->input.write);
    if (w == win->input.read) return -1;
    
    win->input.buf[w] = ki;
    win->input.write = w;
    return 0;
}

def_create_win(create_win)
{
    win->dim.w = INIT_WIN_W;
    win->dim.h = INIT_WIN_H;
    win->rdim.w = 1.0f / win->dim.w;
    win->rdim.h = 1.0f / win->dim.h;
    win->handle = SDL_CreateWindow("Window Title",
                                   SDL_WINDOWPOS_CENTERED,
                                   SDL_WINDOWPOS_CENTERED,
                                   win->dim.w, win->dim.h,
                                   SDL_WINDOW_VULKAN|SDL_WINDOW_RESIZABLE);
    
    if (!win->handle) {
        log_error("Failed to create window");
        return -1;
    }
    
    SDL_DisplayMode dm;
    if (SDL_GetDesktopDisplayMode(0, &dm)) {
        log_error("Failed to get screen extent");
        return -1;
    }
    win->max.w = (u16)dm.w;
    win->max.h = (u16)dm.h;
    
    return 0;
}

def_win_inst_exts(win_inst_exts)
{
    SDL_Vulkan_GetInstanceExtensions(win->handle, count, exts);
}

def_win_create_surf(win_create_surf)
{
    bool ok = SDL_Vulkan_CreateSurface(win->handle, (SDL_vulkanInstance)gpu->inst,
                                       (SDL_vulkanSurface*)&gpu->surf);
    if (!ok) {
        log_error("Failed to create draw surface - %s", SDL_GetError());
        return -1;
    }
    return 0;
}

def_win_poll(win_poll)
{
    win->flags &= ~WIN_SZ;
    
    memset(&win->mouse.current_motion.mov, 0, sizeof(win->mouse.current_motion.mov));
    for(u32 i=0; i <  cl_array_size(win->mouse.current_button.buttons); ++i) {
        if (win->mouse.current_button.buttons[i] == RELEASE)
            win->mouse.current_button.buttons[i] = 0x0;
    }
    
    memset(&win->mouse.motion_buffer, 0, sizeof(win->mouse.motion_buffer));
    memset(&win->mouse.button_buffer, 0, sizeof(win->mouse.button_buffer));
    win->mouse.motion_buffer_size = 0;
    win->mouse.button_buffer_size = 0;
    
    SDL_Event e;
    while(SDL_PollEvent(&e) || (win->flags & WIN_MIN)) {
        switch(e.type) {
            
            case SDL_QUIT:
            win->flags |= WIN_CLO;
            break;
            
            case SDL_KEYDOWN:
            case SDL_KEYUP: {
                struct window_input ki;
                ki.key = win_scancode_to_key(e.key.keysym.scancode);
                ki.mod = e.type == SDL_KEYDOWN ? PRESS : RELEASE;
                
                if (e.key.keysym.mod & KMOD_CTRL) ki.mod |= CTRL;
                if (e.key.keysym.mod & KMOD_ALT) ki.mod |= ALT;
                if (e.key.keysym.mod & KMOD_SHIFT) ki.mod |= SHIFT;
                if (e.key.keysym.mod & KMOD_CAPS) ki.mod |= SHIFT;
                
                if (win_kb_add(ki)) {
                    log_error("Window key buffer is overflowing!");
                    return -1;
                }
            } break;
            
            case SDL_MOUSEMOTION: {
                win->mouse.current_motion.pos.x = e.motion.x;
                win->mouse.current_motion.pos.y = e.motion.y;
                win->mouse.current_motion.mov.x = e.motion.xrel;
                win->mouse.current_motion.mov.y = e.motion.yrel;
                win->mouse.current_motion.button_state = e.motion.state;
                win->mouse.current_motion.time = e.motion.timestamp;
                
                u32 i = win->mouse.motion_buffer_size;
                win->mouse.motion_buffer[i] = win->mouse.current_motion;
                ++win->mouse.motion_buffer_size;
            } break;
            
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP: {
                u8 action = e.type == SDL_MOUSEBUTTONDOWN ? PRESS : RELEASE;
                u32 i = win->mouse.button_buffer_size;
                
                win->mouse.current_button.buttons[e.button.button-1] = action;
                win->mouse.current_button.pos.x = e.button.x;
                win->mouse.current_button.pos.y = e.button.y;
                win->mouse.current_button.i = e.button.button-1;
                win->mouse.current_button.time = action;
                
                win->mouse.button_buffer[i] = win->mouse.current_button;
                ++win->mouse.button_buffer_size;
            } break;
            
            case SDL_WINDOWEVENT: {
                switch(e.window.event) {
                    case SDL_WINDOWEVENT_RESTORED:
                    println("Window size restored");
                    case SDL_WINDOWEVENT_RESIZED: {
                        win->flags &= ~WIN_SZ;
                        win->flags |= WIN_RSZ;
                    } break;
                    
                    case SDL_WINDOWEVENT_MINIMIZED: {
                        win->flags &= ~WIN_SZ;
                        win->flags |= WIN_MIN;
                        
                        local_persist u32 pause_msg_timer = 0;
                        if (pause_msg_timer == 0)
                            pause_msg_timer = win_ms();
                        
                        if (pause_msg_timer < win_ms())
                            pause_msg_timer += secs_to_ms(2);
                        
                        println("Paused while minimized");
                        os_sleep_ms(0);
                    } break;
                    
                    case SDL_WINDOWEVENT_MAXIMIZED: {
                        win->flags &= ~WIN_SZ;
                        win->flags |= WIN_RSZ;
                    } break;
                    
                    case SDL_WINDOWEVENT_ENTER: break;
                    case SDL_WINDOWEVENT_LEAVE: break;
                    case SDL_WINDOWEVENT_FOCUS_GAINED: break;
                    case SDL_WINDOWEVENT_FOCUS_LOST: break;
                    
                    default:
                    break;
                }
            } break;
            
            default:
            break;
        }
    }
    if (win->flags & WIN_RSZ) {
        SDL_GetWindowSize(win->handle, (int*)&win->dim.w, (int*)&win->dim.h);
        win->rdim.w = 1.0f / win->dim.w;
        win->rdim.h = 1.0f / win->dim.h;
    }
    
    return 0;
}

def_win_kb_next(win_kb_next)
{
    if (win->input.read == win->input.write) return false;
    win->input.read = win_input_inc(win->input.read);
    *ki = win->input.buf[win->input.read];
    return true;
}

// @TODO This needs to be a table index rather than a switch
def_win_key_to_char(win_key_to_char)
{
    switch(ki.key) {
        case KEY_A: if (ki.mod & SHIFT) return 'A'; else return 'a';
        case KEY_B: if (ki.mod & SHIFT) return 'B'; else return 'b';
        case KEY_C: if (ki.mod & SHIFT) return 'C'; else return 'c';
        case KEY_D: if (ki.mod & SHIFT) return 'D'; else return 'd';
        case KEY_E: if (ki.mod & SHIFT) return 'E'; else return 'e';
        case KEY_F: if (ki.mod & SHIFT) return 'F'; else return 'f';
        case KEY_G: if (ki.mod & SHIFT) return 'G'; else return 'g';
        case KEY_H: if (ki.mod & SHIFT) return 'H'; else return 'h';
        case KEY_I: if (ki.mod & SHIFT) return 'I'; else return 'i';
        case KEY_J: if (ki.mod & SHIFT) return 'J'; else return 'j';
        case KEY_K: if (ki.mod & SHIFT) return 'K'; else return 'k';
        case KEY_L: if (ki.mod & SHIFT) return 'L'; else return 'l';
        case KEY_M: if (ki.mod & SHIFT) return 'M'; else return 'm';
        case KEY_N: if (ki.mod & SHIFT) return 'N'; else return 'n';
        case KEY_O: if (ki.mod & SHIFT) return 'O'; else return 'o';
        case KEY_P: if (ki.mod & SHIFT) return 'P'; else return 'p';
        case KEY_Q: if (ki.mod & SHIFT) return 'Q'; else return 'q';
        case KEY_R: if (ki.mod & SHIFT) return 'R'; else return 'r';
        case KEY_S: if (ki.mod & SHIFT) return 'S'; else return 's';
        case KEY_T: if (ki.mod & SHIFT) return 'T'; else return 't';
        case KEY_U: if (ki.mod & SHIFT) return 'U'; else return 'u';
        case KEY_V: if (ki.mod & SHIFT) return 'V'; else return 'v';
        case KEY_W: if (ki.mod & SHIFT) return 'W'; else return 'w';
        case KEY_X: if (ki.mod & SHIFT) return 'X'; else return 'x';
        case KEY_Y: if (ki.mod & SHIFT) return 'Y'; else return 'y';
        case KEY_Z: if (ki.mod & SHIFT) return 'Z'; else return 'z';
        case KEY_1: if (ki.mod & SHIFT) return '1'; else return '1';
        case KEY_2: if (ki.mod & SHIFT) return '2'; else return '2';
        case KEY_3: if (ki.mod & SHIFT) return '3'; else return '3';
        case KEY_4: if (ki.mod & SHIFT) return '4'; else return '4';
        case KEY_5: if (ki.mod & SHIFT) return '5'; else return '5';
        case KEY_6: if (ki.mod & SHIFT) return '6'; else return '6';
        case KEY_7: if (ki.mod & SHIFT) return '7'; else return '7';
        case KEY_8: if (ki.mod & SHIFT) return '8'; else return '8';
        case KEY_9: if (ki.mod & SHIFT) return '9'; else return '9';
        case KEY_0: if (ki.mod & SHIFT) return '0'; else return '0';
        
        case KEY_MINUS: if (ki.mod & SHIFT) return '_'; else return '-';
        case KEY_EQUALS: if (ki.mod & SHIFT) return '+'; else return '=';
        case KEY_LEFTBRACKET: if (ki.mod & SHIFT) return '{'; else return '[';
        case KEY_RIGHTBRACKET: if (ki.mod & SHIFT) return '}'; else return ']';
        case KEY_BACKSLASH: if (ki.mod & SHIFT) return '|'; else return '\\';
        case KEY_SEMICOLON: if (ki.mod & SHIFT) return ':'; else return ';';
        case KEY_APOSTROPHE: if (ki.mod & SHIFT) return '"'; else return '\'';
        case KEY_GRAVE: if (ki.mod & SHIFT) return '~'; else return '`';
        case KEY_COMMA: if (ki.mod & SHIFT) return '<'; else return ',';
        case KEY_PERIOD: if (ki.mod & SHIFT) return '>'; else return '.';
        case KEY_SLASH: if (ki.mod & SHIFT) return '?'; else return '/';
        
        case KEY_RETURN: return '\n';
        case KEY_TAB: return '\t';
        case KEY_SPACE: return ' ';
        
        default:
        break;
    }
    return -1;
}