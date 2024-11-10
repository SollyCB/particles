#include "world.h"

#define WORLD_FILE_URI "world.bin"

static inline struct offset_u32 world_first_visible_elem(void)
{
    struct offset_u32 r;
    r.x = world->player.pos.x - (win->dim.w / 2);
    r.y = world->player.pos.y - (win->dim.h / 2);
    return r;
}

static inline struct offset_u32 world_first_hidden_elem(void)
{
    struct offset_u32 r;
    r.x = world->player.pos.x + (win->dim.w / 2);
    r.y = world->player.pos.y + (win->dim.h / 2);
    return r;
}

static inline struct offset_u32 world_mouse_to_elem(void)
{
    struct offset_u32 r;
    r.x = ((s32)win->mouse.pos.x - (win->dim.w / 2)) + world->player.pos.x;
    r.y = ((s32)win->mouse.pos.y - (win->dim.h / 2)) + world->player.pos.y;
    return r;
}

static inline struct offset_u32* world_mouse_mov_to_elems(void)
{
    u32 x_end = win->mouse.pos.x;
    u32 y_end = win->mouse.pos.y;
    u32 x_beg = win->mouse_pos.x - win->mouse.mov.x;
    u32 y_beg = win->mouse_pos.y - win->mouse.mov.y;
    
    struct offset_u32 beg,end;
    beg.x = ((s32)x_beg - (win->dim.w / 2)) + world->player.pos.x;
    beg.y = ((s32)y_beg - (win->dim.h / 2)) + world->player.pos.y;
    end.x = ((s32)x_end - (win->dim.w / 2)) + world->player.pos.x;
    end.y = ((s32)y_end - (win->dim.h / 2)) + world->player.pos.y;
    
    f32 m = ((f32)x_end - x_beg) / ((f32)y_end - y_beg);
    u32 r = world->editor.brush_width;
    
    u32 scr_x = world->player.pos.x - win->dim.w / 2;
    u32 scr_w = world->player.pos.x - win->dim.w / 2;
    u32 scr_y = world->player.pos.y + win->dim.h / 2;
    u32 scr_h = world->player.pos.y + win->dim.h / 2;
    
    for(u32 h = x_beg, k = y_beg;
        h < x_end && k < y_end;
        k += (s32)(m * r), h += (s32)((1 - m) * r))
    {
        if (h - r < scr_x || h + r >= scr_w || k - r < scr_y || k + r >= scr_h)
            continue;
        
        for(u32 x = 0; x < per; ++x) {
            u32 y = (u32)circle(x, r, h, k);
        }
    }
    
    return gay;
}

static inline u32 world_chunk_i(struct offset_u32 p)
{
    p.x = (p.x + world->war.ofs.x) % world->war.dim.w;
    p.y = (p.y + world->war.ofs.y) % world->war.dim.h;
    return world->war.dim.w * p.y + p.x;
}

static inline struct offset_u32 world_chunk_i_to_ofs(u32 i)
{
    u32 x = ((i % world->war.dim.w) + (world->war.dim.w - world->war.ofs.x)) % world->war.dim.w;
    u32 y = ((i / world->war.dim.w) + (world->war.dim.h - world->war.ofs.y)) % world->war.dim.h;
    return OFFSET(u32,x,y);
}

static inline u32 world_elem_chunk_x(u32 x)
{
    return x % WAR_CHUNK_DIM_W;
}

static inline u32 world_elem_chunk_y(u32 y)
{
    return y % WAR_CHUNK_DIM_H;
}

static inline struct world_chunk* world_chunk_from_war(struct offset_u32 p)
{
    return &world->war.chunks[world_chunk_i(p)];
}

static inline struct offset_u32 world_elem_to_chunk(struct offset_u32 p)
{
    return OFFSET(u32,p.x / WAR_CHUNK_DIM_W,p.y / WAR_CHUNK_DIM_H);
}

static inline struct world_elem* world_elem_from_chunk(struct world_chunk *c, struct offset_u32 p)
{
    return &c->elem[world_elem_chunk_y(p.y)][world_elem_chunk_x(p.x)];
}

static inline struct offset_u32 world_chunk_to_screen_px(struct offset_u32 c_pos)
{
    struct offset_u32 e_beg = world_first_visible_elem();
    struct offset_u32 c_beg = world_elem_to_chunk(e_beg);
    struct offset_u32 r = OFFSET(u32,(c_pos.x - c_beg.x) * WAR_CHUNK_DIM_W - (e_beg.x % WAR_CHUNK_DIM_W),
                                 (c_pos.y - c_beg.y) * WAR_CHUNK_DIM_H - (e_beg.y % WAR_CHUNK_DIM_H));
    return r;
}

internal void world_update_player_col(void)
{
    local_persist s8 dir = -3;
    local_persist u32 i = 0;
    u8 *col = (u8*) &world->player.col;
    if (col[i] == 250) {
        dir = -dir;
    } else if (col[i] < 100) {
        dir = -dir;
        i = (i + 1) % 3;
        col[i] = 100;
    }
    col[i] += dir;
}

internal void world_load(void)
{
    return;
}

internal void world_save(void)
{
    return;
}

internal void world_edit_elem(void)
{
    println("editing world element at pixel %u %u", (u64)win->mouse.pos.x, (u64)win->mouse.pos.y);
    
    struct offset_u32 e_pos = world_mouse_to_elem();
    struct offset_u32 c_pos = world_elem_to_chunk(e_pos);
    
    struct world_chunk *c = world_chunk_from_war(c_pos);
    struct world_elem *e = world_elem_from_chunk(c, e_pos);
    
    *e = world->editor.elem;
}

internal void world_handle_input(void)
{
    if (win->mouse.buttons.b1 == PRESS)
        world_edit_elem();
    
    struct window_input ki;
    while(win_kb_next(&ki)) { // @Todo
        switch(ki.key) {
            case KEY_ESCAPE: {
                win->flags |= WIN_CLO;
                gpu_check_leaks();
            } break;
            
            case KEY_S: {
                if (ki.mod & CTRL) world_save();
            } break;
            
            default: break;
        }
    }
}

/**************************************************************************/
// Header functions
def_create_world(create_world)
{
    memset(world, 0, sizeof(*world));
    
    world->fd = create_fd(WORLD_FILE_URI, CREATE_FD_READ|CREATE_FD_WRITE);
    
    world->war.dim.w = win->max.w * 2 / WAR_CHUNK_DIM_W + (win->max.w * 2 / WAR_CHUNK_DIM_W > 0);
    world->war.dim.h = win->max.h * 2 / WAR_CHUNK_DIM_H + (win->max.h * 2 % WAR_CHUNK_DIM_H > 0);
    
    u32 wcc = world->war.dim.w * world->war.dim.h;
    
    u64 war_size = wcc * sizeof(*world->war.chunks);
    u64 dcm_size = wcc * (sizeof(*world->dcm.chunks) + sizeof(*world->dcm.maps));
    
    println("\nWorld active region memory requirements (%ux%u screen)", win->max.w, win->max.h);
    println("  chunk pool:        %fmb", (f64)war_size / mb(1));
    println("  dynamic chunk map: %fmb", (f64)dcm_size / mb(1));
    
    world->war.chunks = palloc(MT, war_size + dcm_size);
    world->dcm.chunks = (typeof(world->dcm.chunks))(world->war.chunks + wcc);
    world->dcm.maps = (typeof(world->dcm.maps))(world->dcm.chunks + wcc);
    
    world->player.pos = OFFSET(u32, world->war.dim.w * WAR_CHUNK_DIM_W / 2, world->war.dim.h * WAR_CHUNK_DIM_H / 2);
    
    world->editor.elem.col = RGBA(255,255,255,255);
    world->editor.elem.brush_width = 10;
    wem_set_type(&world->editor.elem, WEM_TYPE_ROCK);
    wem_clear_state(&world->editor.elem);
    
    return 0;
}

def_world_update(world_update)
{
    world_update_player_col();
    world_handle_input();
    
    gpu_add_draw_elem(world->player.col, OFFSET(u16, 65535 / 2, 65535 / 2));
    
    struct offset_u32 e_beg = world_first_visible_elem();
    struct offset_u32 c_beg = world_elem_to_chunk(e_beg);
    struct offset_u32 e_end = world_first_hidden_elem();
    struct offset_u32 c_end = world_elem_to_chunk(e_end);
    
    u32 stop = world_chunk_i(c_end);
    u32 row_end_stride = (world->war.dim.w - c_end.x) + c_beg.x;
    u32 row_end_w = e_end.x - (c_end.x * WAR_CHUNK_DIM_W);
    u32 col_end_h = e_end.y - (c_end.y * WAR_CHUNK_DIM_H);
    u32 row_beg_x = e_beg.x - (c_beg.x * WAR_CHUNK_DIM_W);
    u32 col_beg_y = e_beg.y - (c_beg.y * WAR_CHUNK_DIM_H);
    
    for(u32 ci = world_chunk_i(c_beg), stride = 1; ci < stop; ci += stride, stride = 1) {
        struct offset_u32 c_pos = world_chunk_i_to_ofs(ci);
        struct offset_u32 e_ofs = OFFSET(u32,0,0);
        struct extent_u32 e_ext = EXTENT(u32,WAR_CHUNK_DIM_W,WAR_CHUNK_DIM_H);
        
        if (c_pos.x == c_end.x) {
            stride = row_end_stride;
            e_ext.w = row_end_w;
        }
        if (c_pos.y == c_end.y)
            e_ext.h = col_end_h;
        
        if (c_pos.x == c_beg.x)
            e_ofs.x = row_beg_x;
        if (c_pos.y == c_beg.y)
            e_ofs.y = col_beg_y;
        
        struct world_chunk *c = &world->war.chunks[ci];
        for(u32 j = e_ofs.y; j < e_ext.h; ++j) {
            for(u32 i = e_ofs.x; i < e_ext.w; ++i) {
                if (c->elem[j][i].type != WEM_TYPE_ROCK)
                    continue;
                struct offset_u32 px = world_chunk_to_screen_px(c_pos);
                px.x += i;
                px.y += j;
                struct offset_u16 nm = win_normalize_screen_px(CAST_OFFSET(u16, px));
                gpu_add_draw_elem(c->elem[j][i].col, nm);
            }
        }
    }
    
    return 0;
}