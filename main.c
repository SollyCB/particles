#include "../solh/sol.h"

#include "prg.h"
#include "vdt.h"

struct program exeprg;

int load_lib(void)
{
    local_persist void *lib = 0;
    
    if (lib) os_destroy_lib(lib);
    copy_file(LIB_SRC, LIB_SRC_TEMP);
    lib = os_create_lib(LIB_SRC);
    
    if (!lib) {
        log_error("Failed to get lib");
        return -1;
    }
    
    prg_load_t *load = (prg_load_t*)os_libproc(lib, "prg_load");
    if (!load) {
        log_error("Failed to get prg_load proc");
        return -1;
    }
    
    load(&exeprg);
    println("Loaded library code");
    return 0;
}

int main() {
    exeprg.vdt.table = exevdt;
    
    // cannot be called from inside the lib.
    if (SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO|SDL_INIT_EVENTS)) {
        log_error("Failed to init sdl");
        return -1;
    }
    
    load_lib();
    exeprg.fn.create();
    
    while(!exeprg.fn.should_shutdown()) {
        if (exeprg.fn.should_reload())
            if (load_lib()) return -1;
        
        if (exeprg.fn.update())
            return -1;
    }
    
    return 0;
}