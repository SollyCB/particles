#include "world.h"

#define WORLD_FILE_URI "world.bin"

inline_fn struct offset_u32 world_war_midpoint(void)
{
    return OFFSET(WAR_CHUNK_DIM_W * world->war.dim.w / 2,
                  WAR_CHUNK_DIM_H * world->war.dim.h / 2, u32);
}

// first war coordinates that are inside the window
inline_fn struct offset_u32 world_first_visible_elem(void) {
    return OFFSET(world_war_midpoint().x - (win->dim.w / 2),
                  world_war_midpoint().y - (win->dim.h / 2), u32);
}

// first war coordinates that are outside the window
inline_fn struct offset_u32 world_first_hidden_elem(void) {
    return OFFSET(world_war_midpoint().x + (win->dim.w / 2),
                  world_war_midpoint().y + (win->dim.h / 2), u32);
}

// screen coordinates to war coordinates
inline_fn struct offset_u32 world_screen_pos_to_elem(struct offset_u32 pos) {
    return OFFSET(((s32)pos.x - (win->dim.w / 2)) + world_war_midpoint().x,
                  ((s32)pos.y - (win->dim.h / 2)) + world_war_midpoint().y, u32);
}

// convert chunk coordinates to index for accessing war.chunks
inline_fn u32 world_chunk_i(struct offset_u32 p) {
    u32 x = ((p.x + world->war.ofs.x) % world->war.dim.w);
    u32 y = ((p.y + world->war.ofs.y) % world->war.dim.h) * world->war.dim.w;
    return x + y;
}

// convert an index returned from world_chunk_i back to coordinates
inline_fn struct offset_u32 world_chunk_i_to_ofs(u32 i) {
    return OFFSET(((i % world->war.dim.w) + (world->war.dim.w - world->war.ofs.x)) % world->war.dim.w,
                  ((i / world->war.dim.w) + (world->war.dim.h - world->war.ofs.y)) % world->war.dim.h, u32);
}

// chunk coordinates to a chunk reference
inline_fn struct world_chunk* world_chunk_from_war(struct offset_u32 p) {
    return &world->war.chunks[world_chunk_i(p)];
}

// war coordinates to the coordinates of its parent chunk
inline_fn struct offset_u32 world_elem_to_chunk(struct offset_u32 p) {
    return OFFSET(p.x / WAR_CHUNK_DIM_W, p.y / WAR_CHUNK_DIM_H, u32);
}

inline_fn u32 world_elem_chunk_x(u32 x) { return x % WAR_CHUNK_DIM_W; }
inline_fn u32 world_elem_chunk_y(u32 y) { return y % WAR_CHUNK_DIM_H; }

// war coordinates to an element reference using its parent chunk
inline_fn struct world_elem* world_elem_from_chunk(struct world_chunk *c, struct offset_u32 p) {
    return &c->elem[world_elem_chunk_y(p.y)][world_elem_chunk_x(p.x)];
}

// convert chunk coordinates to screen coordinates in pixels
inline_fn struct offset_u32 world_chunk_to_screen_px(struct offset_u32 c_pos) {
    struct offset_u32 e_beg = world_first_visible_elem();
    struct offset_u32 c_beg = world_elem_to_chunk(e_beg);
    return OFFSET((c_pos.x - c_beg.x) * WAR_CHUNK_DIM_W - (e_beg.x % WAR_CHUNK_DIM_W),
                  (c_pos.y - c_beg.y) * WAR_CHUNK_DIM_H - (e_beg.y % WAR_CHUNK_DIM_H), u32);
}

// draw a circle of radius world.editor.brush_width centred on war coordinates
internal struct offset_u32* world_pos_to_elems(struct offset_u32 pos, u32 *cnt)
{
    u32 h = pos.x;
    u32 k = pos.y;
    f32 r = (f32)world->editor.brush_width / 2;
    struct offset_u32 *ret = salloc(MT, sizeof(*ret) * (u32)ceilf(r) * (u32)ceilf(r));
    *cnt = fill_circle(r, h, k, ret);
    return ret;
}

// travel from 'pos' to 'pos + mov', stepping by radius 'world.editor.brush_width', and drawing a circle
// at each step whose centre lies on 'mov', and whose radius is also 'world.editor.brush_width'
internal struct offset_u32* world_mov_to_elems(struct offset_u32 pos, struct offset_s32 mov, u32 *cnt)
{
    struct offset_s32 p1 = OFFSET((s32)pos.x, (s32)pos.y, s32);
    struct offset_s32 p2 = OFFSET((s32)pos.x + mov.x, (s32)pos.y + mov.y, s32);
    
    u32 l = (u32)sqrtf((f32)((p2.x-p1.x)*(p2.x-p1.x) + (p2.y-p1.y)*(p2.y-p1.y)));
    struct offset_u32 *ret = salloc(MT, sizeof(*ret) * world->editor.brush_width * l);
    
    *cnt = 0;
    f32 r = (f32)world->editor.brush_width / 2;
    
    if (p1.y > p2.y)
        swap(p1,p2);
    
    if (p1.x == p2.x) {
        for(u32 i = p1.y; i < (u32)p2.y; ++i)
            *cnt += fill_circle(r, p1.x, i, ret + *cnt);
    } else if (p1.y == p2.y) {
        if (p1.x > p2.x)
            swap(p1, p2);
        for(u32 i = p1.x; i < (u32)p2.x; ++i)
            *cnt += fill_circle(r, i, p1.y, ret + *cnt);
    }
    
    struct offset_s32 vec = OFFSET(p2.x - p1.x, p2.y - p1.y, s32);
    float f = (f32)(r * r) / (vec.x*vec.x + vec.y*vec.y);
    f32 sh_x = vec.x * f;
    f32 sh_y = vec.y * f;
    
    if (sh_x > 0) {
        for(f32 k = (f32)p1.y, h = (f32)p1.x;
            k < (f32)p2.y && h < (f32)p2.x;
            k += sh_y, h += sh_x)
        {
            *cnt += fill_circle(r, (u32)h, (u32)k, ret + *cnt);
        }
    } else {
        for(f32 k = (f32)p1.y, h = (f32)p1.x;
            k < (f32)p2.y && h > (f32)p2.x;
            k += sh_y, h += sh_x)
        {
            *cnt += fill_circle(r, (u32)h, (u32)k, ret + *cnt);
        }
    }
    
    return ret;
}

// just a random flashing point for now
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

// load chunks from the world file
internal void world_load(void)
{
    return;
}

// write chunks to the world file
internal void world_save(void)
{
    return;
}

// edit the elements between 'pos' and 'pos + mov' using the world.editor config
internal void world_edit_elem(struct offset_u32 pos, struct offset_s32 mov)
{
    struct offset_u32 min = world_first_visible_elem();
    struct offset_u32 max = world_first_hidden_elem();
    
    u32 cnt;
    struct offset_u32 *arr;
    if (mov.x || mov.y)
        arr = world_mov_to_elems(pos, mov, &cnt);
    else
        arr = world_pos_to_elems(pos, &cnt);
    
    for(u32 i = 0; i < cnt; ++i) {
        struct offset_u32 e_pos = arr[i];
        
        if (e_pos.x < min.x || e_pos.x >= max.x ||
            e_pos.y < min.y || e_pos.y >= max.y)
        {
            continue;
        }
        
        struct offset_u32 c_pos = world_elem_to_chunk(e_pos);
        struct world_chunk *c = world_chunk_from_war(c_pos);
        struct world_elem *e = world_elem_from_chunk(c, e_pos);
        
        *e = world->editor.elem;
    }
}

internal void world_handle_input(void)
{
    u8 col_step = 10;
    
    struct window_input ki;
    while(win_kb_next(&ki)) { // @Todo
        switch(ki.key) {
            case KEY_ESCAPE: {
                win->flags |= WIN_CLO;
            } break;
            
            case KEY_S: {
                if (ki.mod & CTRL) world_save();
            } break;
            
            case KEY_R: {
                if (ki.mod & SHIFT)
                    world->editor.elem.col.r -= (world->editor.elem.col.r > col_step) * col_step;
                else
                    world->editor.elem.col.r += (world->editor.elem.col.r < Max_u8 - col_step) * col_step;
                println("RED : %u / 255", (u64)world->editor.elem.col.r);
            } break;
            
            case KEY_G: {
                if (ki.mod & SHIFT)
                    world->editor.elem.col.g -= (world->editor.elem.col.g > col_step) * col_step;
                else
                    world->editor.elem.col.g += (world->editor.elem.col.g < Max_u8 - col_step) * col_step;
                println("GREEN : %u / 255", (u64)world->editor.elem.col.g);
            } break;
            
            case KEY_B: {
                if (ki.mod & SHIFT)
                    world->editor.elem.col.b -= (world->editor.elem.col.b > col_step) * col_step;
                else
                    world->editor.elem.col.b += (world->editor.elem.col.b < Max_u8 - col_step) * col_step;
                println("BLUE : %u / 255", (u64)world->editor.elem.col.b);
            } break;
            
            case KEY_MINUS: {
                if (ki.mod & SHIFT)
                    world->editor.brush_width -= (world->editor.brush_width > 1);
                else
                    world->editor.brush_width += (world->editor.brush_width < 250);
            } break;
            
            default: break;
        }
    }
    if (win->mouse.motion_buffer_size > 0) {
        for(u32 i=0; i < win->mouse.motion_buffer_size; ++i) {
            if (is_mouse_button_pressed(win->mouse.motion_buffer[i].button_state, MOUSE_BUTTON_1)) {
                struct offset_u32 pos = world_screen_pos_to_elem(win->mouse.motion_buffer[i].pos);
                struct offset_s32 mov = OFFSET(win->mouse.motion_buffer[i].mov.x,
                                               win->mouse.motion_buffer[i].mov.y, s32);
                pos = OFFSET_OP(pos, mov, -, u32);
                world_edit_elem(pos, mov);
            }
        }
    } else if (win->mouse.button_buffer_size > 0) {
        for(u32 i=0; i < win->mouse.button_buffer_size; ++i) {
            if (win->mouse.button_buffer[i].b1 == PRESS) {
                struct offset_u32 pos = world_screen_pos_to_elem(win->mouse.button_buffer[i].pos);
                world_edit_elem(pos, OFFSET(0,0,s32));
            }
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
    
    u64 war_size = wcc * (sizeof(*world->war.chunks));
    u64 dcm_size = wcc * (sizeof(*world->dcm.chunks) + sizeof(*world->dcm.maps));
    
    println("\nWorld active region memory requirements (%ux%u screen)", (u64)win->max.w, (u64)win->max.h);
    println("  war chunk array:   %fmb", (f64)war_size / mb(1));
    println("  dynamic chunk map: %fmb", (f64)dcm_size / mb(1));
    
    world->war.chunks = palloc(MT, war_size + dcm_size);
    world->dcm.chunks = (typeof(world->dcm.chunks))(world->war.chunks + wcc);
    world->dcm.maps = (typeof(world->dcm.maps))(world->dcm.chunks + wcc);
    
    world->player.pos = OFFSET(WORLD_DIM_W / 2, WORLD_DIM_H / 2, u32);
    
    world->editor.elem.col = RGBA(255,255,255,255);
    world->editor.brush_width = 1;
    world->editor.elem.type = WEM_TYPE_ROCK;
    world->editor.elem.state = WEM_STATE_NONE;
    
    return 0;
}

def_world_update(world_update)
{
    timed_trigger(frame_time_trigger, false, secs_to_ms(2));
    create_timer(frame_timer);
    
    world_update_player_col();
    world_handle_input();
    
    gpu_add_draw_elem(world->player.col, OFFSET(65535 / 2, 65535 / 2, u16));
    
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
        struct offset_u32 e_ofs = OFFSET(0,0,u32);
        struct extent_u32 e_ext = EXTENT(WAR_CHUNK_DIM_W,WAR_CHUNK_DIM_H,u32);
        
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
                struct offset_u16 nm = win_normalize_screen_px(CAST_OFFSET(px, u16));
                gpu_add_draw_elem(c->elem[j][i].col, nm);
            }
        }
    }
    
    if (frame_time_trigger && REPORT_FRAME_TIME)
        check_timer(frame_timer, "Time to update world: ");
    
    return 0;
}