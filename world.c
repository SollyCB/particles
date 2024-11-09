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

/**************************************************************************/
// Header functions
def_create_world(create_world)
{
    memset(world, 0, sizeof(*world));
    
    world->fd = create_fd(WORLD_FILE_URI, CREATE_FD_READ|CREATE_FD_WRITE);
    
    world->war.dim.w = win->max.w / WAR_CHUNK_DIM_W + 2;
    world->war.dim.h = win->max.h / WAR_CHUNK_DIM_H + 2;
    
    u32 wcc = world->war.dim.w * world->war.dim.h;
    
    u64 war_size = wcc * sizeof(*world->war.chunks);
    u64 dcm_size = wcc * (sizeof(*world->dcm.chunks) + sizeof(*world->dcm.maps));
    
    println("\nWorld active region memory requirements (%ux%u screen)", win->max.w, win->max.h);
    println("  chunk pool:        %fmb", (f64)war_size / mb(1));
    println("  dynamic chunk map: %fmb", (f64)dcm_size / mb(1));
    
    world->war.chunks = palloc(MT, war_size + dcm_size);
    world->dcm.chunks = (typeof(world->dcm.chunks))(world->war.chunks + wcc);
    world->dcm.maps = (typeof(world->dcm.maps))(world->dcm.chunks + wcc);
    
    return 0;
}

def_world_update(world_update)
{
    world_update_player_col();
    
    gpu_add_draw_elem(world->player.col, OFFSET_U16(65535 / 2, 65535 / 2));
    
    return 0;
}