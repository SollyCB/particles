#ifndef PRG_H
#define PRG_H

#include "../solh/sol.h"

#include "gpu.h"
#include "win.h"
#include "vdt.h"

#define FONT_URI "fonts/liberation-mono.ttf"
#define FONT_HEIGHT 15

#define FG_RED 0
#define FG_GRN 0
#define FG_BLU 0
#define BG_RED 255
#define BG_GRN 255
#define BG_BLU 255

// alpha channel of bg holds char index
#define FG_COL ((struct rgba) {.r = FG_RED, .g = FG_GRN, .b = FG_BLU, .a = 255})
#define BG_COL ((struct rgba) {.r = BG_RED, .g = BG_GRN, .b = BG_BLU, .a = 0})

#define CSR_FG ((struct rgba) {.r = BG_RED, .g = BG_GRN, .b = BG_BLU, .a = 255})
#define CSR_BG ((struct rgba) {.r = FG_RED, .g = FG_GRN, .b = FG_BLU, .a = 0})

#define INIT_WIN_W 640
#define INIT_WIN_H 480

#define TOTAL_MEM mb(32)
#define MAX_THREADS 1 /* 1 == only main thread */
#define MT 0

#define MAIN_THREAD_SCRATCH_SIZE (TOTAL_MEM >> 2)
#define THREAD_DEFAULT_SCRATCH_SIZE ((TOTAL_MEM - MAIN_THREAD_SCRATCH_SIZE) / MAX_THREADS)

// 0 == use page size
#define MAIN_THREAD_BLOCK_SIZE 0
#define THREAD_DEFAULT_BLOCK_SIZE 0

struct program;

#define def_prg_load(name) void name(struct program *p)
typedef def_prg_load(prg_load_t);
dll_export def_prg_load(prg_load);

#define def_create_prg(name) void name(void)
typedef def_create_prg(create_prg_t);

#define def_should_prg_shutdown(name) bool name(void)
typedef def_should_prg_shutdown(should_prg_shutdown_t);

#define def_should_prg_reload(name) bool name(void)
typedef def_should_prg_reload(should_prg_reload_t);

#define def_prg_update(name) int name(void)
typedef def_prg_update(prg_update_t);

enum program_flags {
    PRG_RLD = 0x01,
};

struct program {
    struct {
        create_prg_t (*create);
        should_prg_shutdown_t (*should_shutdown);
        should_prg_reload_t (*should_reload);
        prg_update_t (*update);
    } fn;
    
    struct {
        allocator_t scratch;
        allocator_t persist;
    } allocs[MAX_THREADS];
    
    struct gpu gpu;
    struct win win;
    struct vdt vdt;
    
    u32 flags;
    u32 thread_count;
    
    struct {
        u32 ms; // time elapsed
        u32 dms;
    } time;
    
    struct {
        u32 cnt;
        u32 avg; // 1ms
        u32 worst; // 7-10ms
    } frames;
};

#ifdef LIB
extern struct program *prg;

#define get_thread_alloc(thread_index) prg->allocs[thread_index]
#define salloc(thread_index, sz) allocate(&prg->allocs[thread_index].scratch, sz)
#define palloc(thread_index, sz) allocate(&prg->allocs[thread_index].persist, sz)
#define pfree(thread_index, p) deallocate(&prg->allocs[thread_index].persist, p)
#endif

#endif // PRG_H
