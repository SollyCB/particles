#include "prg.h"
#include "gpu.h"

struct program *prg;

def_create_prg(create_prg);
def_should_prg_shutdown(should_prg_shutdown);
def_should_prg_reload(should_prg_reload);
def_prg_update(prg_update);

def_prg_load(prg_load)
{
    prg = p;
    gpu = &prg->gpu;
    win = &prg->win;
    vdt = &prg->vdt;
    world = &prg->world;
    
    prg->fn.create = create_prg;
    prg->fn.should_shutdown = should_prg_shutdown;
    prg->fn.should_reload = should_prg_reload;
    prg->fn.update = prg_update;
    
    prg->flags &= ~PRG_RLD;
}

internal struct thread_config {
    u64 scratch_size;
    u64 persist_size;
} thread_configs[] = {
    [MT] = {
        .scratch_size = MAIN_THREAD_SCRATCH_SIZE,
        .persist_size = MAIN_THREAD_BLOCK_SIZE,
    },
};

def_create_prg(create_prg)
{
    create_os();
    prg->thread_count = MAX_THREADS < (os.thread_count >> 1) ? MAX_THREADS : (os.thread_count >> 1);
    
    for(u32 i=0; i < prg->thread_count; ++i) {
        if (thread_configs[i].scratch_size == 0) {
            create_allocator_linear(NULL, THREAD_DEFAULT_SCRATCH_SIZE, &prg->allocs[i].scratch);
        } else {
            create_allocator_linear(NULL, thread_configs[i].scratch_size, &prg->allocs[i].scratch);
        }
        
        if (thread_configs[i].persist_size == 0) {
            create_allocator_arena(os.page_size, &prg->allocs[i].persist);
        } else {
            create_allocator_arena(thread_configs[i].persist_size, &prg->allocs[i].persist);
        }
    }
    
    create_win();
    create_gpu();
    create_world();
}

def_should_prg_shutdown(should_prg_shutdown)
{
    return win_should_close();
}

def_should_prg_reload(should_prg_reload)
{
    return prg->flags & PRG_RLD;
}

#define RLD_WT secs_to_ms(2) /* Time the hot reloader waits before checking for source changes */

def_prg_update(prg_update)
{
    for(u32 i=0; i < cl_array_size(prg->allocs); ++i) {
        reset_allocator(&prg->allocs[i].scratch);
        reset_allocator(&prg->allocs[i].persist);
    }
    
    prg->frames.cnt++;
    
    /* timers */
    prg->time.dms = SDL_GetTicks() - prg->time.ms;
    prg->time.ms += prg->time.dms;
    
    if (prg->frames.cnt > 5 && prg->time.dms > prg->frames.worst)
        prg->frames.worst = prg->time.dms;
    
    prg->frames.avg = prg->time.ms / prg->frames.cnt;
    {
        timed_trigger(frame_time_trigger, false, secs_to_ms(2));
        if (frame_time_trigger && REPORT_FRAME_TIME)
            println("average frame time: %ums", prg->frames.avg);
    }
    
    /* hotloader */
    internal u32 rld_timer = 0;
    if (rld_timer == 0)
        rld_timer = win_ms();
    
    if (rld_timer < win_ms()) {
        rld_timer += RLD_WT;
        
        if (cmpftim(FTIM_MOD, LIB_SRC, LIB_SRC_TEMP) < 0)
            prg->flags |= PRG_RLD;
        
        if (cmpftim(FTIM_MOD, SH_SRC_OUT_URI, SH_SRC_URI) < 0) {
            println("Recompiling shaders");
            // spirv parser to recreate pipeline layout?
            if (gpu_create_sh()) {
                log_error("Failed to recompile shader code after source change");
                if (!gpu->sh.vert || !gpu->sh.frag)
                    return -1;
            }
        }
    }
    
    /* window */
    win_poll();
    
    if (win->flags & WIN_RSZ) {
        if (gpu_handle_win_resize()) {
            log_error("Failed to handle window resize");
            return -1;
        }
    }
    
    /* update */
    world_update();
    
    
    gpu_update();
    
    /* end frame */
    //os_sleep_ms(0); // relinquish time slice
    
    return 0;
}
