#ifndef WORLD_H
#define WORLD_H

#include "../solh/sol.h"

enum world_elem_types {
    WEM_TYPE_VOID,
    WEM_TYPE_ROCK,
    WEM_TYPE_CNT,
};
enum world_elem_states {
    WEM_STATE_NONE = 0x00,
    WEM_STATE_UPDATE = 0x01,
    WEM_STATE_WET = 0x02,
};

struct world_elem {
    struct rgba col;
    u32 type;
    u64 state;
}; // 16 bytes

#define WAR_CHUNK_DIM_W 128
#define WAR_CHUNK_DIM_H 128

// could go 64 bit player.pos and edit some coordinate calculations,
// but 2 billion elements across I think is fine.
#define WORLD_DIM_W ((u32)align(billion(2), WAR_CHUNK_DIM_W))
#define WORLD_DIM_H ((u32)align(billion(2), WAR_CHUNK_DIM_H))

struct world_chunk {
    struct world_elem elem[WAR_CHUNK_DIM_H][WAR_CHUNK_DIM_W];
};

struct world_chunk_map {
    __m128i masks[WAR_CHUNK_DIM_H];
};

struct world {
    os_fd fd;
    
    struct {
        struct extent_u32 dim; // chunks
        struct offset_u32 ofs; // chunks
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
