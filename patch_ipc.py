import re

with open('src/kernel/ipc.c', 'r') as f:
    content = f.read()

# Add macros and lock definition
header_addition = """
#ifdef PICO_BUILD
#include "pico/stdlib.h"
#include "hardware/sync.h"
#define IPC_TIMESTAMP_MS()  to_ms_since_boot(get_absolute_time())
#else
#define IPC_TIMESTAMP_MS()  0
#endif

#ifdef PICO_BUILD
static spin_lock_t *ipc_lock;
#define IPC_LOCK() uint32_t __save = spin_lock_blocking(ipc_lock)
#define IPC_UNLOCK() spin_unlock(ipc_lock, __save)
#define IPC_RETURN(val) do { int __ret = (val); IPC_UNLOCK(); return __ret; } while(0)
#define IPC_RETURN_PTR(val) do { void *__ret = (val); IPC_UNLOCK(); return __ret; } while(0)
#else
#define IPC_LOCK()
#define IPC_UNLOCK()
#define IPC_RETURN(val) return (val)
#define IPC_RETURN_PTR(val) return (val)
#endif
"""

content = content.replace("""#ifdef PICO_BUILD
#include "pico/stdlib.h"
#define IPC_TIMESTAMP_MS()  to_ms_since_boot(get_absolute_time())
#else
#define IPC_TIMESTAMP_MS()  0
#endif""", header_addition)

init_body_old = """void ipc_init(void)
{
    memset(channels, 0, sizeof(channels));"""
init_body_new = """void ipc_init(void)
{
#ifdef PICO_BUILD
    ipc_lock = spin_lock_instance(spin_lock_claim_unused(true));
#endif
    memset(channels, 0, sizeof(channels));"""
content = content.replace(init_body_old, init_body_new)

def patch_func(func_name, code, ret_type='int'):
    # We find the function start safely. 
    # Usually it's `int func_name(` or `void *func_name(`
    start_idx = code.find(func_name + '(')
    if start_idx == -1: return code
    
    open_brace = code.find('{', start_idx)
    close_brace = open_brace
    brace_count = 1
    i = open_brace + 1
    while brace_count > 0 and i < len(code):
        if code[i] == '{': brace_count += 1
        elif code[i] == '}': brace_count -= 1
        if brace_count == 0:
            close_brace = i
            break
        i += 1
    
    func_body = code[open_brace+1:close_brace]
    
    # insert IPC_LOCK()
    patched_body = "\n    IPC_LOCK();\n" + func_body
    
    # replace returns
    if ret_type == 'int':
        patched_body = re.sub(r'return\s+(.+?);', r'IPC_RETURN(\1);', patched_body)
    else:
        patched_body = re.sub(r'return\s+(.+?);', r'IPC_RETURN_PTR(\1);', patched_body)
    
    return code[:open_brace+1] + patched_body + "\n    IPC_UNLOCK();\n" + code[close_brace:]

funcs_to_patch = [
    'ipc_channel_create', 'ipc_channel_destroy', 'ipc_send', 'ipc_recv',
    'ipc_peek', 'ipc_pending', 'ipc_channel_find', 'ipc_channel_stats',
    'ipc_sem_create', 'ipc_sem_destroy', 'ipc_sem_wait', 'ipc_sem_post',
    'ipc_sem_trywait', 'ipc_sem_getvalue', 'ipc_shmem_create', 'ipc_shmem_destroy',
    'ipc_shmem_lock', 'ipc_shmem_unlock', 'ipc_shmem_write', 'ipc_shmem_read'
]

for f in funcs_to_patch:
    content = patch_func(f, content, ret_type='int')

content = patch_func('ipc_shmem_attach', content, ret_type='ptr')

with open('src/kernel/ipc.c', 'w') as f:
    f.write(content)

print("Patching IPC complete.")