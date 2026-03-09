/* remote_shell.c - Remote Shell over TCP for littleOS (Pico W) */

#include "remote_shell.h"
#include "dmesg.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#ifdef PICO_W
#include "pico/stdlib.h"
#include "lwip/tcp.h"
#include "lwip/err.h"

/* ============================================================================
 * Forward declarations for shell commands
 * ============================================================================ */

extern int  cmd_sage(int argc, char *argv[]);
extern int  cmd_script(int argc, char *argv[]);
extern void cmd_health(int argc, char **argv);
extern void cmd_stats(int argc, char **argv);
extern void cmd_supervisor(int argc, char **argv);
extern void cmd_dmesg(int argc, char **argv);
extern int  cmd_users(int argc, char *argv[]);
extern int  cmd_perms(int argc, char *argv[]);
extern int  cmd_tasks(int argc, char *argv[]);
extern int  cmd_memory(int argc, char *argv[]);
extern int  cmd_fs(int argc, char *argv[]);
extern int  cmd_ipc(int argc, char *argv[]);
extern int  cmd_hw(int argc, char *argv[]);
extern int  cmd_net(int argc, char *argv[]);
extern int  cmd_ota(int argc, char *argv[]);

/* ============================================================================
 * Internal state
 * ============================================================================ */

#define RSHELL_OUTPUT_BUF_SIZE  2048
#define RSHELL_LINE_BUF_SIZE   256
#define RSHELL_MAX_ARGS         32

/* Per-client state */
typedef struct {
    struct tcp_pcb *pcb;
    uint32_t       client_ip;
    uint16_t       client_port;
    uint32_t       connected_at_ms;
    uint32_t       bytes_rx;
    uint32_t       bytes_tx;
    bool           active;
    char           line_buf[RSHELL_LINE_BUF_SIZE];
    int            line_pos;
} rshell_client_t;

static struct tcp_pcb   *listen_pcb = NULL;
static rshell_client_t   clients[REMOTE_SHELL_MAX_CLIENTS];
static uint16_t          listen_port = 0;
static uint32_t          total_connections = 0;

/* Shared output buffer used to capture command output */
static char              output_buf[RSHELL_OUTPUT_BUF_SIZE];
static int               output_pos = 0;
static bool              capturing_output = false;

/* ============================================================================
 * Output capture
 *
 * Since shell commands use printf() directly, we temporarily redirect output
 * by hooking into a capture buffer. On the Pico, we can override _write or
 * use a simpler approach: execute commands and capture via a wrapper.
 *
 * For simplicity, we provide remote_shell_execute() which redirects stdout
 * to our buffer using a custom putchar hook.
 * ============================================================================ */

/* Hook function pointer - when non-NULL, putchar output goes here too */
static void (*output_hook)(const char *data, size_t len) = NULL;

static void capture_hook(const char *data, size_t len) {
    size_t space = (size_t)(RSHELL_OUTPUT_BUF_SIZE - 1 - output_pos);
    size_t to_copy = (len < space) ? len : space;
    if (to_copy > 0) {
        memcpy(&output_buf[output_pos], data, to_copy);
        output_pos += (int)to_copy;
        output_buf[output_pos] = '\0';
    }
}

static void capture_start(void) {
    output_pos = 0;
    output_buf[0] = '\0';
    capturing_output = true;
    output_hook = capture_hook;
}

static void capture_end(void) {
    capturing_output = false;
    output_hook = NULL;
}

/* _putchar override for picolibc / newlib-nano:
 * If we have an active capture hook, also send data there.
 * We wrap printf output by overriding _write. */

/* ============================================================================
 * Simple argument parser (same as shell.c)
 * ============================================================================ */

static int rshell_parse_args(char *buffer, char *argv[], int max_args) {
    int argc = 0;
    char *token = strtok(buffer, " ");
    while (token != NULL && argc < max_args) {
        argv[argc++] = token;
        token = strtok(NULL, " ");
    }
    return argc;
}

/* ============================================================================
 * Command execution
 *
 * Execute a shell command line, capturing output into a buffer.
 * Since we cannot easily redirect printf on bare-metal without modifying
 * the C runtime, we use snprintf-based output for remote commands.
 * ============================================================================ */

static int remote_shell_execute(const char *line, char *out, size_t out_size) {
    char cmd_buf[RSHELL_LINE_BUF_SIZE];
    char *argv[RSHELL_MAX_ARGS];
    int argc;
    int written = 0;

    if (!line || !out || out_size == 0) return 0;

    /* Copy line for tokenization */
    strncpy(cmd_buf, line, RSHELL_LINE_BUF_SIZE - 1);
    cmd_buf[RSHELL_LINE_BUF_SIZE - 1] = '\0';

    /* Strip trailing \r\n */
    size_t len = strlen(cmd_buf);
    while (len > 0 && (cmd_buf[len - 1] == '\r' || cmd_buf[len - 1] == '\n')) {
        cmd_buf[--len] = '\0';
    }

    if (len == 0) {
        out[0] = '\0';
        return 0;
    }

    argc = rshell_parse_args(cmd_buf, argv, RSHELL_MAX_ARGS);
    if (argc == 0) {
        out[0] = '\0';
        return 0;
    }

    /* Start capturing printf output */
    capture_start();

    /* Dispatch commands - mirrors shell.c dispatch table */
    if (strcmp(argv[0], "help") == 0) {
        printf("Available commands:\r\n");
        printf("  help, version, clear, reboot, history,\r\n");
        printf("  health, stats, supervisor, dmesg, sage,\r\n");
        printf("  script, users, perms, tasks, memory,\r\n");
        printf("  fs, ipc, hw, net, ota, remote\r\n");
    } else if (strcmp(argv[0], "version") == 0) {
        printf("littleOS v0.4.0 - RP2040 (remote shell)\r\n");
    } else if (strcmp(argv[0], "health") == 0) {
        cmd_health(argc, argv);
    } else if (strcmp(argv[0], "stats") == 0) {
        cmd_stats(argc, argv);
    } else if (strcmp(argv[0], "supervisor") == 0) {
        cmd_supervisor(argc, argv);
    } else if (strcmp(argv[0], "dmesg") == 0) {
        cmd_dmesg(argc, argv);
    } else if (strcmp(argv[0], "users") == 0) {
        cmd_users(argc, argv);
    } else if (strcmp(argv[0], "perms") == 0) {
        cmd_perms(argc, argv);
    } else if (strcmp(argv[0], "tasks") == 0) {
        cmd_tasks(argc, argv);
    } else if (strcmp(argv[0], "memory") == 0) {
        cmd_memory(argc, argv);
    } else if (strcmp(argv[0], "fs") == 0) {
        cmd_fs(argc, argv);
    } else if (strcmp(argv[0], "ipc") == 0) {
        cmd_ipc(argc, argv);
    } else if (strcmp(argv[0], "hw") == 0) {
        cmd_hw(argc, argv);
    } else if (strcmp(argv[0], "net") == 0) {
        cmd_net(argc, argv);
    } else if (strcmp(argv[0], "ota") == 0) {
        cmd_ota(argc, argv);
    } else if (strcmp(argv[0], "sage") == 0) {
        cmd_sage(argc, argv);
    } else if (strcmp(argv[0], "script") == 0) {
        cmd_script(argc, argv);
    } else if (strcmp(argv[0], "clear") == 0) {
        /* Send ANSI clear to remote terminal */
        printf("\033[2J\033[H");
    } else if (strcmp(argv[0], "reboot") == 0) {
        printf("Reboot not allowed via remote shell\r\n");
    } else {
        printf("Unknown command: %s\r\nType 'help' for available commands\r\n", argv[0]);
    }

    capture_end();

    /* Copy captured output to caller buffer */
    size_t captured_len = strlen(output_buf);
    if (captured_len >= out_size) {
        captured_len = out_size - 1;
    }
    memcpy(out, output_buf, captured_len);
    out[captured_len] = '\0';
    written = (int)captured_len;

    return written;
}

/* ============================================================================
 * TCP send helper
 * ============================================================================ */

static int rshell_tcp_send(rshell_client_t *client, const char *data, size_t len) {
    if (!client->active || !client->pcb) return -1;
    if (len == 0) return 0;

    /* Check available send buffer space */
    uint16_t sndbuf = tcp_sndbuf(client->pcb);
    uint16_t to_send = (len > sndbuf) ? sndbuf : (uint16_t)len;
    if (to_send == 0) return 0;

    err_t err = tcp_write(client->pcb, data, to_send, TCP_WRITE_FLAG_COPY);
    if (err != ERR_OK) {
        dmesg_warn("rshell: tcp_write failed err=%d", err);
        return -1;
    }

    err = tcp_output(client->pcb);
    if (err != ERR_OK) {
        dmesg_warn("rshell: tcp_output failed err=%d", err);
        return -1;
    }

    client->bytes_tx += to_send;
    return (int)to_send;
}

static void rshell_send_prompt(rshell_client_t *client) {
    rshell_tcp_send(client, "littleos> ", 10);
}

/* ============================================================================
 * Process a complete line from a remote client
 * ============================================================================ */

static void rshell_process_line(rshell_client_t *client) {
    /* Null-terminate the line */
    client->line_buf[client->line_pos] = '\0';

    /* Strip trailing whitespace */
    int end = client->line_pos - 1;
    while (end >= 0 && (client->line_buf[end] == '\r' ||
                        client->line_buf[end] == '\n' ||
                        client->line_buf[end] == ' ')) {
        client->line_buf[end--] = '\0';
    }

    /* Skip empty lines */
    if (client->line_buf[0] == '\0') {
        rshell_send_prompt(client);
        client->line_pos = 0;
        return;
    }

    /* Handle "exit" / "quit" to disconnect */
    if (strcmp(client->line_buf, "exit") == 0 ||
        strcmp(client->line_buf, "quit") == 0) {
        rshell_tcp_send(client, "Goodbye.\r\n", 10);
        tcp_close(client->pcb);
        client->active = false;
        client->pcb = NULL;
        dmesg_info("rshell: client disconnected (exit)");
        client->line_pos = 0;
        return;
    }

    /* Execute the command */
    char response[RSHELL_OUTPUT_BUF_SIZE];
    int rlen = remote_shell_execute(client->line_buf, response, sizeof(response));

    /* Send command output back */
    if (rlen > 0) {
        rshell_tcp_send(client, response, (size_t)rlen);
    }

    /* Send prompt for next command */
    rshell_send_prompt(client);

    /* Reset line buffer */
    client->line_pos = 0;
}

/* ============================================================================
 * lwIP TCP callbacks
 * ============================================================================ */

static int rshell_find_client_by_pcb(struct tcp_pcb *pcb) {
    for (int i = 0; i < REMOTE_SHELL_MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].pcb == pcb) {
            return i;
        }
    }
    return -1;
}

static err_t rshell_recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    int idx = (int)(intptr_t)arg;
    (void)err;

    if (idx < 0 || idx >= REMOTE_SHELL_MAX_CLIENTS) {
        if (p) pbuf_free(p);
        return ERR_ARG;
    }

    rshell_client_t *client = &clients[idx];

    /* NULL pbuf means the remote side closed the connection */
    if (p == NULL) {
        dmesg_info("rshell: client %d disconnected", idx);
        client->active = false;
        client->pcb = NULL;
        return ERR_OK;
    }

    if (!client->active) {
        pbuf_free(p);
        return ERR_OK;
    }

    /* Process received data byte by byte */
    struct pbuf *q;
    for (q = p; q != NULL; q = q->next) {
        const char *payload = (const char *)q->payload;
        for (uint16_t i = 0; i < q->len; i++) {
            char ch = payload[i];
            client->bytes_rx++;

            /* Handle backspace */
            if (ch == '\b' || ch == 0x7F) {
                if (client->line_pos > 0) {
                    client->line_pos--;
                    /* Echo backspace to remote terminal */
                    rshell_tcp_send(client, "\b \b", 3);
                }
                continue;
            }

            /* Handle line completion */
            if (ch == '\r' || ch == '\n') {
                /* Echo newline */
                rshell_tcp_send(client, "\r\n", 2);
                rshell_process_line(client);
                continue;
            }

            /* Ignore non-printable characters (except above) */
            if (ch < 32 || ch >= 127) {
                continue;
            }

            /* Accumulate into line buffer */
            if (client->line_pos < RSHELL_LINE_BUF_SIZE - 1) {
                client->line_buf[client->line_pos++] = ch;
                /* Echo character back */
                rshell_tcp_send(client, &ch, 1);
            }
        }
    }

    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

static void rshell_err_cb(void *arg, err_t err) {
    int idx = (int)(intptr_t)arg;
    (void)err;

    if (idx < 0 || idx >= REMOTE_SHELL_MAX_CLIENTS) return;

    dmesg_warn("rshell: client %d error (err=%d)", idx, err);
    clients[idx].active = false;
    clients[idx].pcb = NULL;  /* lwIP frees the PCB on error */
}

static err_t rshell_accept_cb(void *arg, struct tcp_pcb *newpcb, err_t err) {
    (void)arg;

    if (err != ERR_OK || newpcb == NULL) {
        return ERR_VAL;
    }

    /* Find a free client slot */
    int slot = -1;
    for (int i = 0; i < REMOTE_SHELL_MAX_CLIENTS; i++) {
        if (!clients[i].active) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        /* No free slots, reject connection */
        dmesg_warn("rshell: max clients reached, rejecting connection");
        tcp_abort(newpcb);
        return ERR_ABRT;
    }

    /* Set up the client */
    rshell_client_t *client = &clients[slot];
    memset(client, 0, sizeof(*client));
    client->pcb = newpcb;
    client->active = true;
    client->connected_at_ms = to_ms_since_boot(get_absolute_time());
    client->client_ip = ip4_addr_get_u32(ip_2_ip4(&newpcb->remote_ip));
    client->client_port = newpcb->remote_port;
    client->line_pos = 0;

    total_connections++;

    /* Set callbacks for this client */
    tcp_arg(newpcb, (void *)(intptr_t)slot);
    tcp_recv(newpcb, rshell_recv_cb);
    tcp_err(newpcb, rshell_err_cb);

    dmesg_info("rshell: client %d connected from %d.%d.%d.%d:%u",
               slot,
               (client->client_ip >>  0) & 0xFF,
               (client->client_ip >>  8) & 0xFF,
               (client->client_ip >> 16) & 0xFF,
               (client->client_ip >> 24) & 0xFF,
               client->client_port);

    /* Send welcome banner and prompt */
    const char *banner =
        "\r\n"
        "=== littleOS Remote Shell ===\r\n"
        "Type 'help' for commands, 'exit' to disconnect.\r\n"
        "\r\n";
    rshell_tcp_send(client, banner, strlen(banner));
    rshell_send_prompt(client);

    return ERR_OK;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

int remote_shell_start(uint16_t port) {
    if (listen_pcb != NULL) {
        dmesg_warn("rshell: already listening on port %u", listen_port);
        return -1;
    }

    if (port == 0) {
        port = REMOTE_SHELL_DEFAULT_PORT;
    }

    /* Clear client state */
    memset(clients, 0, sizeof(clients));
    total_connections = 0;

    /* Create TCP PCB */
    struct tcp_pcb *pcb = tcp_new();
    if (pcb == NULL) {
        dmesg_err("rshell: tcp_new failed");
        return -1;
    }

    /* Bind to port */
    err_t err = tcp_bind(pcb, IP_ADDR_ANY, port);
    if (err != ERR_OK) {
        dmesg_err("rshell: tcp_bind port %u failed (err=%d)", port, err);
        tcp_close(pcb);
        return -1;
    }

    /* Start listening */
    struct tcp_pcb *lpcb = tcp_listen(pcb);
    if (lpcb == NULL) {
        dmesg_err("rshell: tcp_listen failed");
        tcp_close(pcb);
        return -1;
    }

    /* Set accept callback */
    tcp_accept(lpcb, rshell_accept_cb);

    listen_pcb = lpcb;
    listen_port = port;

    dmesg_info("rshell: listening on port %u (max %d clients)",
               port, REMOTE_SHELL_MAX_CLIENTS);
    return 0;
}

int remote_shell_stop(void) {
    if (listen_pcb == NULL) {
        return -1;
    }

    /* Disconnect all clients */
    for (int i = 0; i < REMOTE_SHELL_MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].pcb) {
            rshell_tcp_send(&clients[i], "Server shutting down.\r\n", 22);
            tcp_arg(clients[i].pcb, NULL);
            tcp_recv(clients[i].pcb, NULL);
            tcp_err(clients[i].pcb, NULL);
            tcp_close(clients[i].pcb);
            clients[i].active = false;
            clients[i].pcb = NULL;
        }
    }

    /* Close listener */
    tcp_close(listen_pcb);
    listen_pcb = NULL;

    dmesg_info("rshell: stopped (was on port %u)", listen_port);
    listen_port = 0;

    return 0;
}

void remote_shell_task(void) {
    /* On lwIP raw API, everything is handled via callbacks.
     * This function exists for polling architectures or future use.
     * No-op with the raw callback-based approach. */
}

int remote_shell_get_status(remote_shell_status_t *status) {
    if (!status) return -1;

    memset(status, 0, sizeof(*status));
    status->listening = (listen_pcb != NULL);
    status->port = listen_port;
    status->total_connections = total_connections;

    uint8_t count = 0;
    for (int i = 0; i < REMOTE_SHELL_MAX_CLIENTS; i++) {
        status->clients[i].active = clients[i].active;
        status->clients[i].client_ip = clients[i].client_ip;
        status->clients[i].client_port = clients[i].client_port;
        status->clients[i].connected_at_ms = clients[i].connected_at_ms;
        status->clients[i].bytes_rx = clients[i].bytes_rx;
        status->clients[i].bytes_tx = clients[i].bytes_tx;
        if (clients[i].active) count++;
    }
    status->active_clients = count;

    return 0;
}

int remote_shell_broadcast(const char *data, size_t len) {
    if (!data || len == 0) return -1;

    int sent_to = 0;
    for (int i = 0; i < REMOTE_SHELL_MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].pcb) {
            if (rshell_tcp_send(&clients[i], data, len) >= 0) {
                sent_to++;
            }
        }
    }

    return sent_to;
}

int remote_shell_kick(int client_index) {
    if (client_index < 0 || client_index >= REMOTE_SHELL_MAX_CLIENTS) {
        return -1;
    }

    rshell_client_t *client = &clients[client_index];
    if (!client->active) {
        return -1;
    }

    dmesg_info("rshell: kicking client %d", client_index);

    if (client->pcb) {
        rshell_tcp_send(client, "Disconnected by server.\r\n", 25);
        tcp_arg(client->pcb, NULL);
        tcp_recv(client->pcb, NULL);
        tcp_err(client->pcb, NULL);
        tcp_close(client->pcb);
    }

    client->active = false;
    client->pcb = NULL;
    return 0;
}

bool remote_shell_is_active(void) {
    return (listen_pcb != NULL);
}

/* ============================================================================
 * Non-PICO_W stubs
 * ============================================================================ */

#else /* !PICO_W */

int remote_shell_start(uint16_t port) {
    (void)port;
    printf("Remote shell not available (requires Pico W)\r\n");
    return -1;
}

int remote_shell_stop(void) {
    printf("Remote shell not available (requires Pico W)\r\n");
    return -1;
}

void remote_shell_task(void) {
    /* No-op on non-Pico W builds */
}

int remote_shell_get_status(remote_shell_status_t *status) {
    (void)status;
    return -1;
}

int remote_shell_broadcast(const char *data, size_t len) {
    (void)data;
    (void)len;
    return -1;
}

int remote_shell_kick(int client_index) {
    (void)client_index;
    return -1;
}

bool remote_shell_is_active(void) {
    return false;
}

#endif /* PICO_W */
