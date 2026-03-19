#include "module.h"

// Pico builds don't ship the desktop networking/graphics native modules.
// Returning NULL lets SageLang fall back to normal import resolution instead
// of pretending the module exists with incomplete functionality.

Module* create_socket_module(ModuleCache* cache) {
    (void)cache;
    return NULL;
}

Module* create_tcp_module(ModuleCache* cache) {
    (void)cache;
    return NULL;
}

Module* create_http_module(ModuleCache* cache) {
    (void)cache;
    return NULL;
}

Module* create_ssl_module(ModuleCache* cache) {
    (void)cache;
    return NULL;
}

Module* create_graphics_module(ModuleCache* cache) {
    (void)cache;
    return NULL;
}
