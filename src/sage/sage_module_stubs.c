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

Module* create_ml_native_module(ModuleCache* cache) {
    (void)cache;
    return NULL;
}

// Missing POSIX stubs
int access(const char *pathname, int mode) {
    (void)pathname;
    (void)mode;
    return -1;
}

// Missing SGPU stubs
int sgpu_cmd_dispatch() { return 0; }
int sgpu_cmd_push_constants() { return 0; }
int sgpu_update_uniform() { return 0; }
int sgpu_reset_fence() { return 0; }
int sgpu_wait_fence() { return 0; }
int sgpu_present() { return 0; }
int sgpu_acquire_next_image() { return 0; }
int sgpu_submit_with_sync() { return 0; }
int sgpu_cmd_draw_indexed() { return 0; }
int sgpu_cmd_bind_index_buffer() { return 0; }
int sgpu_cmd_bind_vertex_buffer() { return 0; }
int sgpu_cmd_set_scissor() { return 0; }
int sgpu_cmd_set_viewport() { return 0; }
int sgpu_cmd_bind_descriptor_set() { return 0; }
int sgpu_cmd_bind_graphics_pipeline() { return 0; }
int sgpu_cmd_draw() { return 0; }
int sgpu_cmd_end_render_pass() { return 0; }
int sgpu_cmd_begin_render_pass() { return 0; }
int sgpu_end_commands() { return 0; }
int sgpu_begin_commands() { return 0; }
int sgpu_update_input() { return 0; }
int sgpu_mouse_delta() { return 0; }
int sgpu_mouse_pos() { return 0; }
int sgpu_key_down() { return 0; }
int sgpu_key_pressed() { return 0; }
double sgpu_get_time() { return 0.0; }
int sgpu_window_should_close() { return 0; }
int sgpu_poll_events() { return 0; }

// Missing AOT stubs
int aot_init() { return 0; }
int aot_set_var_type() { return 0; }
int aot_compile_program() { return 0; }
int aot_compile_to_binary() { return 0; }
int aot_free() { return 0; }

// Missing JIT stubs
int jit_init() { return 0; }
int jit_record_call() { return 0; }
int jit_get_profile() { return 0; }
int jit_should_compile() { return 0; }
int jit_record_return() { return 0; }
int jit_compile_function() { return 0; }

