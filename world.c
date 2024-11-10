#include "world.h"

#define WORLD_FILE_URI "world.bin"

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

internal void world_save(void)
{
    return;
}

internal void world_edit_elem(void)
{
    println("editing world element at pixel %u %u", (u64)win->mouse.pos.x, (u64)win->mouse.pos.y);
    
    u32 x = ((s32)win->mouse.pos.x - (win->dim.w / 2)) + world->player.pos.x;
    u32 y = ((s32)win->mouse.pos.y - (win->dim.h / 2)) + world->player.pos.y;
    
    u32 cx = (x / WAR_CHUNK_DIM_W + world->war.ofs.x) % world->war.dim.w;
    u32 cy = (y / WAR_CHUNK_DIM_H + world->war.ofs.y) % world->war.dim.h;
    
    struct world_chunk *c = &world->war.chunks[world->war.dim.w * cy + cx];
    struct world_elem *e = &c->elem[y % WAR_CHUNK_DIM_H][x % WAR_CHUNK_DIM_W];
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
    
    world->war.dim.w = win->max.w * 2 / WAR_CHUNK_DIM_W;
    world->war.dim.h = win->max.h * 2 / WAR_CHUNK_DIM_H;
    
    u32 wcc = world->war.dim.w * world->war.dim.h;
    
    u64 war_size = wcc * sizeof(*world->war.chunks);
    u64 dcm_size = wcc * (sizeof(*world->dcm.chunks) + sizeof(*world->dcm.maps));
    
    println("\nWorld active region memory requirements (%ux%u screen)", win->max.w, win->max.h);
    println("  chunk pool:        %fmb", (f64)war_size / mb(1));
    println("  dynamic chunk map: %fmb", (f64)dcm_size / mb(1));
    
    world->war.chunks = palloc(MT, war_size + dcm_size);
    world->dcm.chunks = (typeof(world->dcm.chunks))(world->war.chunks + wcc);
    world->dcm.maps = (typeof(world->dcm.maps))(world->dcm.chunks + wcc);
    
    world->player.pos = OFFSET_U32(world->war.dim.w * WAR_CHUNK_DIM_W / 2, world->war.dim.h * WAR_CHUNK_DIM_H / 2);
    
    world->editor.elem.col = RGBA(255,255,255,255);
    world->editor.elem.type = WEM_TYPE_ROCK;
    world->editor.elem.state = 0;
    
    return 0;
}

def_world_update(world_update)
{
    world_update_player_col();
    world_handle_input();
    
    gpu_add_draw_elem(world->player.col, OFFSET_U16(65535 / 2, 65535 / 2));
    
    for(u32 i=0; i < world->war.dim.h * world->war.dim.w; ++i) {
        if (world->war.chunks[i].type == WEM_TYPE_ROCK) {
            struct world_chunk *c = world->war.chunks[i];
            u32 x = c->pos.x + ; // @TODO CurrentTask!!! This is fun!
            struct offset_u16 pos;
            gpu_add_draw_elem(chunk->col, pos)
        }
    }
    
    return 0;
}