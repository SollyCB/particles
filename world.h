#ifndef WORLD_H
#define WORLD_H

#include "../solh/sol.h"

enum world_elem_modifiers {
    WEM_TYPE_ROCK,
    WEM_TYPE_CNT,
    WEM_STATE_UPDATE = 0x0100,
    WEM_STATE_WET = 0x0200,
};

struct world_elem {
    struct offset_u32 pos;
    union {
        u8 type;
        u32 state;
    };
};

#define WAR_DIM_W (1920 * 2)
#define WAR_DIM_H (1080 * 2)

#endif // WORLD_H
