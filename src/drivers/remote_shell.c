/* remote_shell.c - Remote Shell over TCP for littleOS (Pico W) */

#include "remote_shell.h"
#include "board/board_config.h"
#include "config_storage.h"
#include "dmesg.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#define RSHELL_TOKEN_KEY "remote_token"

static bool rshell_load_token(char *token, size_t token_size) {
    return config_get(RSHELL_TOKEN_KEY, token, token_size) == CONFIG_OK;
}

bool remote_shell_has_token(void) {
    char token[REMOTE_SHELL_TOKEN_MAX + 1];
    return rshell_load_token(token, sizeof(token));
}

int remote_shell_set_token(const char *token) {
    if (!token || token[0] == '\0') {
        return -1;
    }
    if (strlen(token) > REMOTE_SHELL_TOKEN_MAX) {
        return -1;
    }
    if (config_set(RSHELL_TOKEN_KEY, token) != CONFIG_OK) {
        return -1;
    }
    return config_save() ? 0 : -1;
}

int remote_shell_clear_token(void) {
    config_result_t rc = config_delete(RSHELL_TOKEN_KEY);
    if (rc != CONFIG_OK && rc != CONFIG_ERROR_NOT_FOUND) {
        return -1;
    }
    return config_save() ? 0 : -1;
}

#ifdef PICO_W
#include "pico/stdlib.h"
#include "lwip/tcp.h"
#include "lwip/err.h"

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
    bool           authenticated;
    uint8_t        auth_failures;
    char           line_buf[RSHELL_LINE_BUF_SIZE];
    int            line_pos;
} rshell_client_t;

static struct tcp_pcb   *listen_pcb = NULL;
static rshell_client_t   clients[REMOTE_SHELL_MAX_CLIENTS];
static uint16_t          listen_port = 0;
static uint32_t          total_connections = 0;

static uint8_t rshell_active_client_count(void);

static int rshell_format_output(char *out, size_t out_size, const char *fmt, ...) {
    va_list args;
    int written;

    if (!out || out_size == 0) {
        return 0;
    }

    va_start(args, fmt);
    written = vsnprintf(out, out_size, fmt, args);
    va_end(args);

    if (written < 0) {
        out[0] = '\0';
        return 0;
    }

    out[out_size - 1] = '\0';
    return (int)strlen(out);
}

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

static bool rshell_token_matches(const char *provided) {
    char stored[REMOTE_SHELL_TOKEN_MAX + 1];
    size_t provided_len;
    size_t stored_len;
    unsigned char diff = 0;

    if (!provided || !rshell_load_token(stored, sizeof(stored))) {
        return false;
    }

    provided_len = strlen(provided);
    stored_len = strlen(stored);
    if (provided_len != stored_len) {
        return false;
    }

    for (size_t i = 0; i < stored_len; i++) {
        diff |= (unsigned char)(stored[i] ^ provided[i]);
    }

    return diff == 0;
}

static bool rshell_is_auth_command(const char *line) {
    return line && strlen(line) >= 5 &&
           toupper((unsigned char)line[0]) == 'A' &&
           toupper((unsigned char)line[1]) == 'U' &&
           toupper((unsigned char)line[2]) == 'T' &&
           toupper((unsigned char)line[3]) == 'H' &&
           line[4] == ' ';
}

static int remote_shell_execute(const char *line,
                                const rshell_client_t *client,
                                char *out,
                                size_t out_size)
{
    char cmd_buf[RSHELL_LINE_BUF_SIZE];
    char *argv[RSHELL_MAX_ARGS];
    int argc;

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

    if (strcmp(argv[0], "help") == 0) {
        return rshell_format_output(out, out_size,
                                    "Available remote commands:\r\n"
                                    "  help      Show this help\r\n"
                                    "  version   Show firmware version\r\n"
                                    "  status    Show remote shell status\r\n"
                                    "  clear     Clear the terminal\r\n"
                                    "  exit      Disconnect\r\n");
    } else if (strcmp(argv[0], "version") == 0) {
        return rshell_format_output(out, out_size,
                                    "littleOS v0.6.0 - %s (remote shell)\r\n",
                                    CHIP_MODEL_STR);
    } else if (strcmp(argv[0], "status") == 0) {
        return rshell_format_output(out, out_size,
                                    "Remote shell status:\r\n"
                                    "  Port: %u\r\n"
                                    "  Active clients: %u / %d\r\n"
                                    "  Total connections: %lu\r\n"
                                    "  This session: auth=yes rx=%lu tx=%lu\r\n",
                                    listen_port,
                                    (unsigned)rshell_active_client_count(),
                                    REMOTE_SHELL_MAX_CLIENTS,
                                    (unsigned long)total_connections,
                                    (unsigned long)client->bytes_rx,
                                    (unsigned long)client->bytes_tx);
    } else if (strcmp(argv[0], "clear") == 0) {
        return rshell_format_output(out, out_size, "\033[2J\033[H");
    } else {
        return rshell_format_output(out, out_size,
                                    "Remote shell only exposes authenticated management commands.\r\n"
                                    "Type 'help' for available commands.\r\n");
    }
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
    if (client->authenticated) {
        rshell_tcp_send(client, "littleos> ", 10);
    } else {
        rshell_tcp_send(client, "auth> ", 6);
    }
}

static uint8_t rshell_active_client_count(void) {
    uint8_t count = 0;
    for (int i = 0; i < REMOTE_SHELL_MAX_CLIENTS; i++) {
        if (clients[i].active) {
            count++;
        }
    }
    return count;
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

    if (!client->authenticated) {
        if (strcmp(client->line_buf, "help") == 0) {
            const char *msg =
                "Authentication required.\r\n"
                "Use: AUTH <token>\r\n"
                "Use 'exit' to disconnect.\r\n";
            rshell_tcp_send(client, msg, strlen(msg));
        } else if (strcmp(client->line_buf, "version") == 0) {
            char response[RSHELL_OUTPUT_BUF_SIZE];
            int rlen = remote_shell_execute("version", client, response, sizeof(response));
            if (rlen > 0) {
                rshell_tcp_send(client, response, (size_t)rlen);
            }
        } else if (rshell_is_auth_command(client->line_buf)) {
            const char *provided = client->line_buf + 5;
            while (*provided == ' ') provided++;

            if (rshell_token_matches(provided)) {
                client->authenticated = true;
                client->auth_failures = 0;
                rshell_tcp_send(client, "Authenticated.\r\n", 16);
            } else {
                client->auth_failures++;
                rshell_tcp_send(client, "Authentication failed.\r\n", 24);
                if (client->auth_failures >= 3) {
                    rshell_tcp_send(client, "Too many failures. Disconnecting.\r\n", 35);
                    tcp_close(client->pcb);
                    client->active = false;
                    client->pcb = NULL;
                    client->line_pos = 0;
                    return;
                }
            }
        } else {
            rshell_tcp_send(client, "Authenticate first with AUTH <token>.\r\n", 39);
        }

        rshell_send_prompt(client);
        client->line_pos = 0;
        return;
    }

    char response[RSHELL_OUTPUT_BUF_SIZE];
    int rlen = remote_shell_execute(client->line_buf, client, response, sizeof(response));

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
        "Authentication required. Use AUTH <token>.\r\n"
        "Type 'help' for assistance, 'exit' to disconnect.\r\n"
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

    if (!remote_shell_has_token()) {
        dmesg_warn("rshell: refusing to start without authentication token");
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
    status->token_configured = remote_shell_has_token();
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
        status->clients[i].auth_failures = clients[i].auth_failures;
        status->clients[i].authenticated = clients[i].authenticated;
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
    if (!status) return -1;
    memset(status, 0, sizeof(*status));
    status->token_configured = remote_shell_has_token();
    return 0;
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
