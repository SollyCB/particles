#ifndef WORLD_H
#define WORLD_H

#include "../solh/sol.h"

enum world_elem_modifiers {
    WEM_TYPE_VOID,
    WEM_TYPE_ROCK,
    WEM_TYPE_CNT,
    WEM_STATE_UPDATE = 0x0100,
    WEM_STATE_WET = 0x0200,
};

struct world_elem {
    struct offset_u16 pos; // relative to chunk
    struct rgba col;
    union {
        u8 type;
        u32 state;
    };
};

#define WAR_CHUNK_DIM_H 128
#define WAR_CHUNK_DIM_W 128

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
};

#ifdef LIB
struct world *world;

#define def_create_world(name) int name(void)
def_create_world(create_world);

#define def_world_update(name) int name(void)
def_world_update(world_update);
#endif

#endif // WORLD_H
