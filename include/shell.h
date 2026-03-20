#ifndef LITTLEOS_SHELL_H
#define LITTLEOS_SHELL_H

#include <stdbool.h>
#include "permissions.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Set by the command timeout alarm. Commands should check this flag
 * in any long-running loop and return early when it becomes true.
 *
 * Example:
 *   while (running) {
 *       if (shell_cmd_abort) { printf("Aborted\r\n"); return -1; }
 *       ...
 *   }
 */
extern volatile bool shell_cmd_abort;

void shell_run(void);
void shell_execute_command(const char *cmd);

void shell_set_security_context(const task_sec_ctx_t *ctx, const char *username);
const task_sec_ctx_t *shell_get_security_context(void);
const char *shell_get_username(void);

bool shell_authorize_command(const task_sec_ctx_t *task_ctx,
                             int argc,
                             char *argv[],
                             bool is_remote,
                             const char **reason);

#ifdef __cplusplus
}
#endif

#endif /* LITTLEOS_SHELL_H */
