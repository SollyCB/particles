#define STB_TRUETYPE_IMPLEMENTATION
#include "external/stb_truetype.h"
#undef STB_TRUETYPE_IMPLEMENTATION

#define SOL_DEF
#include "../solh/sol.h"
#undef SOL_DEF

#include "defs.h"

#define LIB
#include "prg.c"
#include "win.c"
#include "gpu.c"
#include "vdt.c"