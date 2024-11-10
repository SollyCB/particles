#ifndef WORLD_H
#define WORLD_H

#include "../solh/sol.h"

enum world_elem_modifiers {
    // types
    WEM_TYPE_VOID,
    WEM_TYPE_ROCK,
    WEM_TYPE_CNT,
    
    // states
    WEM_STATE_UPDATE = 0x0100,
    WEM_STATE_WET = 0x0200,
    
    WEM_PRESERVE_TYPE = 0x00ff,
};

struct msvc_align(16) world_elem {
    struct offset_u16 pos; // relative to chunk
    struct rgba col;
    
    // Be very careful accessing directly. It is very easy to accidentally modify
    // type when trying to set the state, and vice versa. Prefer the below accessors.
    // This is a very good example of where 'private' would be useful LOL!
    union {
        u16 type; // low 16 bits
        u64 state; // top 48 bits
    };
} gcc_align(16);

static inline void wem_set_type(struct world_elem *e, u8 type)
{
    e->type = type;
}

static inline void wem_add_state(struct world_elem *e, u32 state)
{
    e->state |= state|WEM_PRESERVE_TYPE;
}

static inline void wem_rm_state(struct world_elem *e, u32 state)
{
    e->state &= (~state)|WEM_PRESERVE_TYPE;
}

static inline void wem_clear_state(struct world_elem *e)
{
    e->state &= WEM_PRESERVE_TYPE;
}

#define WAR_CHUNK_DIM_W 128
#define WAR_CHUNK_DIM_H 128

struct world_chunk {
    struct world_elem elem[WAR_CHUNK_DIM_H][WAR_CHUNK_DIM_W];
};

struct world_chunk_map {
    __m128i masks[WAR_CHUNK_DIM_H];
};

struct world {
    os_fd fd;
    
    struct {
        struct extent_u32 dim;
        struct offset_u32 ofs;
        struct world_chunk *chunks;
    } war; // world active region - chunks loaded from disk
    
    struct {
        struct world_chunk_map *maps;
        u32 *chunks;
        u32 size;
    } dcm; // dynamic chunk map - array of chunks that are actively changing, e.g. falling, on fire, etc.
    
    struct {
        struct rgba col;
        struct offset_u32 pos;
    } player;
    
    struct {
        struct world_elem elem;
        u32 brush_width;
    } editor;
};

#ifdef LIB
struct world *world;

#define def_create_world(name) int name(void)
def_create_world(create_world);

#define def_world_update(name) int name(void)
def_world_update(world_update);
#endif

#endif // WORLD_H
