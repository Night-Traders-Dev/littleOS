/* remote_shell.c - Remote Shell over TCP for littleOS (Pico W) */

#include "remote_shell.h"
#include "board/board_config.h"
#include "config_storage.h"
#include "dmesg.h"
#include "system_info.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef PICO_W
#include "pico/rand.h"
#endif

#define RSHELL_TOKEN_RECORD_KEY          "remote_token_v2"
#define RSHELL_LEGACY_TOKEN_KEY          "remote_token"
#define RSHELL_LEGACY_ENCRYPTED_TOKEN_KEY "remote_token_enc"
#define RSHELL_TOKEN_SALT_BYTES          16u
#define RSHELL_TOKEN_VERIFIER_BYTES      32u
#define RSHELL_TOKEN_RECORD_BYTES        (RSHELL_TOKEN_SALT_BYTES + RSHELL_TOKEN_VERIFIER_BYTES)
#define RSHELL_NONCE_BYTES               16u

typedef struct {
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t data[64];
    size_t datalen;
} rshell_sha256_ctx_t;

static const uint32_t rshell_sha256_k[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

static const char rshell_b64_alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static uint32_t rshell_sha256_rotr(uint32_t value, uint32_t bits) {
    return (value >> bits) | (value << (32u - bits));
}

static void rshell_sha256_transform(rshell_sha256_ctx_t *ctx, const uint8_t block[64]) {
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h;

    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i * 4] << 24) |
               ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] << 8) |
               (uint32_t)block[i * 4 + 3];
    }

    for (int i = 16; i < 64; i++) {
        uint32_t s0 = rshell_sha256_rotr(w[i - 15], 7) ^
                      rshell_sha256_rotr(w[i - 15], 18) ^
                      (w[i - 15] >> 3);
        uint32_t s1 = rshell_sha256_rotr(w[i - 2], 17) ^
                      rshell_sha256_rotr(w[i - 2], 19) ^
                      (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    for (int i = 0; i < 64; i++) {
        uint32_t s1 = rshell_sha256_rotr(e, 6) ^ rshell_sha256_rotr(e, 11) ^ rshell_sha256_rotr(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + s1 + ch + rshell_sha256_k[i] + w[i];
        uint32_t s0 = rshell_sha256_rotr(a, 2) ^ rshell_sha256_rotr(a, 13) ^ rshell_sha256_rotr(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void rshell_sha256_init(rshell_sha256_ctx_t *ctx) {
    ctx->datalen = 0;
    ctx->bitlen = 0;
    ctx->state[0] = 0x6a09e667u;
    ctx->state[1] = 0xbb67ae85u;
    ctx->state[2] = 0x3c6ef372u;
    ctx->state[3] = 0xa54ff53au;
    ctx->state[4] = 0x510e527fu;
    ctx->state[5] = 0x9b05688cu;
    ctx->state[6] = 0x1f83d9abu;
    ctx->state[7] = 0x5be0cd19u;
}

static void rshell_sha256_update(rshell_sha256_ctx_t *ctx, const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        ctx->data[ctx->datalen++] = data[i];
        if (ctx->datalen == sizeof(ctx->data)) {
            rshell_sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}

static void rshell_sha256_final(rshell_sha256_ctx_t *ctx, uint8_t hash[32]) {
    size_t i = ctx->datalen;

    ctx->data[i++] = 0x80;
    if (i > 56) {
        while (i < 64) ctx->data[i++] = 0;
        rshell_sha256_transform(ctx, ctx->data);
        i = 0;
    }
    while (i < 56) ctx->data[i++] = 0;

    ctx->bitlen += ctx->datalen * 8u;
    for (int j = 7; j >= 0; j--) {
        ctx->data[56 + (7 - j)] = (uint8_t)(ctx->bitlen >> (j * 8));
    }
    rshell_sha256_transform(ctx, ctx->data);

    for (i = 0; i < 4; i++) {
        hash[i]      = (uint8_t)((ctx->state[0] >> (24 - i * 8)) & 0xFF);
        hash[i + 4]  = (uint8_t)((ctx->state[1] >> (24 - i * 8)) & 0xFF);
        hash[i + 8]  = (uint8_t)((ctx->state[2] >> (24 - i * 8)) & 0xFF);
        hash[i + 12] = (uint8_t)((ctx->state[3] >> (24 - i * 8)) & 0xFF);
        hash[i + 16] = (uint8_t)((ctx->state[4] >> (24 - i * 8)) & 0xFF);
        hash[i + 20] = (uint8_t)((ctx->state[5] >> (24 - i * 8)) & 0xFF);
        hash[i + 24] = (uint8_t)((ctx->state[6] >> (24 - i * 8)) & 0xFF);
        hash[i + 28] = (uint8_t)((ctx->state[7] >> (24 - i * 8)) & 0xFF);
    }
}

static void rshell_sha256_bytes(const uint8_t *data, size_t len, uint8_t out[32]) {
    rshell_sha256_ctx_t ctx;
    rshell_sha256_init(&ctx);
    rshell_sha256_update(&ctx, data, len);
    rshell_sha256_final(&ctx, out);
}

static bool rshell_constant_time_eq(const uint8_t *lhs, const uint8_t *rhs, size_t len) {
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++) {
        diff |= (uint8_t)(lhs[i] ^ rhs[i]);
    }
    return diff == 0;
}

static bool rshell_base64_encode(const uint8_t *src, size_t src_len, char *out, size_t out_size) {
    size_t required = ((src_len + 2u) / 3u) * 4u + 1u;
    size_t pos = 0;

    if (!out || out_size < required) {
        return false;
    }

    for (size_t i = 0; i < src_len; i += 3u) {
        uint32_t triple = (uint32_t)src[i] << 16;
        size_t remaining = src_len - i;

        if (remaining > 1u) triple |= (uint32_t)src[i + 1u] << 8;
        if (remaining > 2u) triple |= (uint32_t)src[i + 2u];

        out[pos++] = rshell_b64_alphabet[(triple >> 18) & 0x3F];
        out[pos++] = rshell_b64_alphabet[(triple >> 12) & 0x3F];
        out[pos++] = (remaining > 1u) ? rshell_b64_alphabet[(triple >> 6) & 0x3F] : '=';
        out[pos++] = (remaining > 2u) ? rshell_b64_alphabet[triple & 0x3F] : '=';
    }

    out[pos] = '\0';
    return true;
}

static int rshell_base64_value(char ch) {
    if (ch >= 'A' && ch <= 'Z') return ch - 'A';
    if (ch >= 'a' && ch <= 'z') return ch - 'a' + 26;
    if (ch >= '0' && ch <= '9') return ch - '0' + 52;
    if (ch == '+') return 62;
    if (ch == '/') return 63;
    if (ch == '=') return -2;
    return -1;
}

static bool rshell_base64_decode(const char *src, uint8_t *out, size_t *out_len, size_t out_cap) {
    size_t src_len = strlen(src);
    size_t pos = 0;

    if ((src_len & 3u) != 0 || !out_len) {
        return false;
    }

    for (size_t i = 0; i < src_len; i += 4u) {
        int a = rshell_base64_value(src[i]);
        int b = rshell_base64_value(src[i + 1u]);
        int c = rshell_base64_value(src[i + 2u]);
        int d = rshell_base64_value(src[i + 3u]);
        uint32_t triple;

        if (a < 0 || b < 0 || c == -1 || d == -1) {
            return false;
        }

        triple = ((uint32_t)a << 18) | ((uint32_t)b << 12);
        if (c >= 0) triple |= (uint32_t)c << 6;
        if (d >= 0) triple |= (uint32_t)d;

        if (pos >= out_cap) return false;
        out[pos++] = (uint8_t)((triple >> 16) & 0xFF);
        if (c != -2) {
            if (pos >= out_cap) return false;
            out[pos++] = (uint8_t)((triple >> 8) & 0xFF);
        }
        if (d != -2) {
            if (pos >= out_cap) return false;
            out[pos++] = (uint8_t)(triple & 0xFF);
        }
    }

    *out_len = pos;
    return true;
}

static bool rshell_parse_hex(const char *hex, uint8_t *out, size_t out_len) {
    if (!hex || strlen(hex) != out_len * 2u) {
        return false;
    }

    for (size_t i = 0; i < out_len; i++) {
        int hi = isdigit((unsigned char)hex[i * 2u]) ?
                 (hex[i * 2u] - '0') :
                 (tolower((unsigned char)hex[i * 2u]) - 'a' + 10);
        int lo = isdigit((unsigned char)hex[i * 2u + 1u]) ?
                 (hex[i * 2u + 1u] - '0') :
                 (tolower((unsigned char)hex[i * 2u + 1u]) - 'a' + 10);
        if (hi < 0 || hi > 15 || lo < 0 || lo > 15) {
            return false;
        }
        out[i] = (uint8_t)((hi << 4) | lo);
    }

    return true;
}

static void rshell_format_hex(const uint8_t *data, size_t len, char *out, size_t out_size) {
    static const char hex[] = "0123456789abcdef";

    if (!out || out_size < (len * 2u + 1u)) {
        return;
    }

    for (size_t i = 0; i < len; i++) {
        out[i * 2u] = hex[(data[i] >> 4) & 0x0F];
        out[i * 2u + 1u] = hex[data[i] & 0x0F];
    }
    out[len * 2u] = '\0';
}

static bool rshell_derive_legacy_device_key(uint8_t key[32]) {
    char board_id[17];

    if (!system_get_board_id(board_id, sizeof(board_id))) {
        return false;
    }

    rshell_sha256_bytes((const uint8_t *)board_id, strlen(board_id), key);
    return true;
}

static void rshell_fill_random_bytes(uint8_t *out, size_t len) {
    size_t offset = 0;

    if (!out || len == 0) {
        return;
    }

#ifdef PICO_W
    while (offset < len) {
        uint32_t word = get_rand_32();
        size_t chunk = len - offset;
        if (chunk > sizeof(word)) {
            chunk = sizeof(word);
        }
        memcpy(out + offset, &word, chunk);
        offset += chunk;
    }
#else
    static uint32_t fallback_state = 0x6d2b79f5u;

    while (offset < len) {
        fallback_state = fallback_state * 1664525u + 1013904223u;
        out[offset++] = (uint8_t)(fallback_state >> 24);
    }
#endif
}

static void rshell_compute_token_verifier(const char *token,
                                          const uint8_t salt[RSHELL_TOKEN_SALT_BYTES],
                                          uint8_t out[RSHELL_TOKEN_VERIFIER_BYTES]) {
    rshell_sha256_ctx_t ctx;

    rshell_sha256_init(&ctx);
    rshell_sha256_update(&ctx, (const uint8_t *)token, strlen(token));
    rshell_sha256_update(&ctx, salt, RSHELL_TOKEN_SALT_BYTES);
    rshell_sha256_final(&ctx, out);
}

static void rshell_compute_auth_response(const uint8_t verifier[RSHELL_TOKEN_VERIFIER_BYTES],
                                         const uint8_t nonce[RSHELL_NONCE_BYTES],
                                         uint8_t out[32]) {
    rshell_sha256_ctx_t ctx;

    rshell_sha256_init(&ctx);
    rshell_sha256_update(&ctx, verifier, RSHELL_TOKEN_VERIFIER_BYTES);
    rshell_sha256_update(&ctx, nonce, RSHELL_NONCE_BYTES);
    rshell_sha256_final(&ctx, out);
}

static bool rshell_store_token_record(const char *token, char *encoded, size_t encoded_size) {
    uint8_t record[RSHELL_TOKEN_RECORD_BYTES];
    size_t len;

    if (!token || !encoded) {
        return false;
    }

    len = strlen(token);
    if (len == 0 || len > REMOTE_SHELL_TOKEN_MAX) {
        return false;
    }

    rshell_fill_random_bytes(record, RSHELL_TOKEN_SALT_BYTES);
    rshell_compute_token_verifier(token, record, record + RSHELL_TOKEN_SALT_BYTES);
    return rshell_base64_encode(record, sizeof(record), encoded, encoded_size);
}

static bool rshell_load_token_record(uint8_t salt[RSHELL_TOKEN_SALT_BYTES],
                                     uint8_t verifier[RSHELL_TOKEN_VERIFIER_BYTES]) {
    char encoded[CONFIG_MAX_VALUE_LEN];
    uint8_t record[RSHELL_TOKEN_RECORD_BYTES];
    size_t record_len = 0;

    if (config_get(RSHELL_TOKEN_RECORD_KEY, encoded, sizeof(encoded)) != CONFIG_OK) {
        return false;
    }
    if (!rshell_base64_decode(encoded, record, &record_len, sizeof(record))) {
        return false;
    }
    if (record_len != sizeof(record)) {
        return false;
    }

    memcpy(salt, record, RSHELL_TOKEN_SALT_BYTES);
    memcpy(verifier, record + RSHELL_TOKEN_SALT_BYTES, RSHELL_TOKEN_VERIFIER_BYTES);
    return true;
}

static bool rshell_decrypt_legacy_token(const char *encoded, char *token, size_t token_size) {
    uint8_t key[32];
    uint8_t ciphertext[REMOTE_SHELL_TOKEN_MAX];
    size_t ciphertext_len = 0;

    if (!encoded || !token || token_size < (REMOTE_SHELL_TOKEN_MAX + 1u)) {
        return false;
    }
    if (!rshell_derive_legacy_device_key(key)) {
        return false;
    }
    if (!rshell_base64_decode(encoded, ciphertext, &ciphertext_len, sizeof(ciphertext))) {
        return false;
    }
    if (ciphertext_len == 0 || ciphertext_len > REMOTE_SHELL_TOKEN_MAX) {
        return false;
    }

    for (size_t i = 0; i < ciphertext_len; i++) {
        token[i] = (char)(ciphertext[i] ^ key[i % sizeof(key)]);
    }
    token[ciphertext_len] = '\0';
    return true;
}

static bool rshell_load_legacy_token(char *token, size_t token_size) {
    char encoded[CONFIG_MAX_VALUE_LEN];

    if (!token || token_size < (REMOTE_SHELL_TOKEN_MAX + 1u)) {
        return false;
    }

    if (config_get(RSHELL_LEGACY_ENCRYPTED_TOKEN_KEY, encoded, sizeof(encoded)) == CONFIG_OK) {
        return rshell_decrypt_legacy_token(encoded, token, token_size);
    }

    return config_get(RSHELL_LEGACY_TOKEN_KEY, token, token_size) == CONFIG_OK;
}

static int rshell_migrate_legacy_token(void) {
    char legacy_token[REMOTE_SHELL_TOKEN_MAX + 1];
    uint8_t salt[RSHELL_TOKEN_SALT_BYTES];
    uint8_t verifier[RSHELL_TOKEN_VERIFIER_BYTES];

    if (rshell_load_token_record(salt, verifier)) {
        return 0;
    }
    if (!rshell_load_legacy_token(legacy_token, sizeof(legacy_token))) {
        return 0;
    }
    if (remote_shell_set_token(legacy_token) != 0) {
        return -1;
    }

    dmesg_info("rshell: migrated legacy token to verifier storage");
    return 0;
}

bool remote_shell_has_token(void) {
    uint8_t salt[RSHELL_TOKEN_SALT_BYTES];
    uint8_t verifier[RSHELL_TOKEN_VERIFIER_BYTES];

    if (rshell_load_token_record(salt, verifier)) {
        return true;
    }

    if (rshell_migrate_legacy_token() == 0 && rshell_load_token_record(salt, verifier)) {
        return true;
    }

    return false;
}

int remote_shell_set_token(const char *token) {
    char encoded[CONFIG_MAX_VALUE_LEN];
    config_result_t clear_rc;
    config_result_t legacy_enc_rc;

    if (!token || token[0] == '\0') {
        return -1;
    }
    if (strlen(token) > REMOTE_SHELL_TOKEN_MAX) {
        return -1;
    }
    if (!rshell_store_token_record(token, encoded, sizeof(encoded))) {
        return -1;
    }
    if (config_set(RSHELL_TOKEN_RECORD_KEY, encoded) != CONFIG_OK) {
        return -1;
    }
    clear_rc = config_delete(RSHELL_LEGACY_TOKEN_KEY);
    legacy_enc_rc = config_delete(RSHELL_LEGACY_ENCRYPTED_TOKEN_KEY);
    if ((clear_rc != CONFIG_OK && clear_rc != CONFIG_ERROR_NOT_FOUND) ||
        (legacy_enc_rc != CONFIG_OK && legacy_enc_rc != CONFIG_ERROR_NOT_FOUND)) {
        return -1;
    }
    return config_save() ? 0 : -1;
}

int remote_shell_clear_token(void) {
    config_result_t rc = config_delete(RSHELL_TOKEN_RECORD_KEY);
    config_result_t legacy_rc = config_delete(RSHELL_LEGACY_TOKEN_KEY);
    config_result_t legacy_enc_rc = config_delete(RSHELL_LEGACY_ENCRYPTED_TOKEN_KEY);

    if ((rc != CONFIG_OK && rc != CONFIG_ERROR_NOT_FOUND) ||
        (legacy_rc != CONFIG_OK && legacy_rc != CONFIG_ERROR_NOT_FOUND) ||
        (legacy_enc_rc != CONFIG_OK && legacy_enc_rc != CONFIG_ERROR_NOT_FOUND)) {
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
    uint8_t        auth_nonce[RSHELL_NONCE_BYTES];
    char           line_buf[RSHELL_LINE_BUF_SIZE];
    int            line_pos;
} rshell_client_t;

static struct tcp_pcb   *listen_pcb = NULL;
static rshell_client_t   clients[REMOTE_SHELL_MAX_CLIENTS];
static uint16_t          listen_port = 0;
static uint32_t          total_connections = 0;

static uint8_t rshell_active_client_count(void);
static int rshell_tcp_send(rshell_client_t *client, const char *data, size_t len);

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

static void rshell_generate_nonce(rshell_client_t *client) {
    if (!client) {
        return;
    }

    rshell_fill_random_bytes(client->auth_nonce, sizeof(client->auth_nonce));
}

static bool rshell_auth_response_matches(const rshell_client_t *client, const char *provided_hex) {
    uint8_t salt[RSHELL_TOKEN_SALT_BYTES];
    uint8_t verifier[RSHELL_TOKEN_VERIFIER_BYTES];
    uint8_t provided[32];
    uint8_t expected[32];

    if (!provided_hex || !rshell_load_token_record(salt, verifier)) {
        return false;
    }
    (void)salt;
    if (!rshell_parse_hex(provided_hex, provided, sizeof(provided))) {
        return false;
    }

    rshell_compute_auth_response(verifier, client->auth_nonce, expected);

    return rshell_constant_time_eq(expected, provided, sizeof(expected));
}

static void rshell_send_auth_instructions(rshell_client_t *client, const char *prefix) {
    uint8_t salt[RSHELL_TOKEN_SALT_BYTES];
    uint8_t verifier[RSHELL_TOKEN_VERIFIER_BYTES];
    char salt_hex[(RSHELL_TOKEN_SALT_BYTES * 2u) + 1u];
    char nonce_hex[(RSHELL_NONCE_BYTES * 2u) + 1u];
    char msg[256];

    if (!client) {
        return;
    }

    if (!rshell_load_token_record(salt, verifier)) {
        rshell_format_output(msg, sizeof(msg),
                             "%sRemote shell is not configured with a token.\r\n"
                             "Type 'exit' to disconnect.\r\n",
                             prefix ? prefix : "");
        rshell_tcp_send(client, msg, strlen(msg));
        return;
    }
    (void)verifier;

    rshell_format_hex(salt, sizeof(salt), salt_hex, sizeof(salt_hex));
    rshell_format_hex(client->auth_nonce, sizeof(client->auth_nonce), nonce_hex, sizeof(nonce_hex));
    rshell_format_output(msg, sizeof(msg),
                         "%sSalt: %s\r\n"
                         "Nonce: %s\r\n"
                         "Reply with: AUTH <sha256(sha256(token||salt)||nonce) hex>\r\n"
                         "Type 'exit' to disconnect.\r\n",
                         prefix ? prefix : "",
                         salt_hex,
                         nonce_hex);
    rshell_tcp_send(client, msg, strlen(msg));
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
                                    "  help     Show this help\r\n"
                                    "  version  Show firmware version\r\n"
                                    "  status   Show remote shell status\r\n"
                                    "  clear    Clear the terminal\r\n"
                                    "  exit     Disconnect\r\n");
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
            rshell_send_auth_instructions(client, "Authentication required.\r\n");
        } else if (strcmp(client->line_buf, "version") == 0) {
            char response[RSHELL_OUTPUT_BUF_SIZE];
            int rlen = remote_shell_execute("version", client, response, sizeof(response));
            if (rlen > 0) {
                rshell_tcp_send(client, response, (size_t)rlen);
            }
        } else if (rshell_is_auth_command(client->line_buf)) {
            const char *provided = client->line_buf + 5;
            while (*provided == ' ') provided++;

            if (rshell_auth_response_matches(client, provided)) {
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
                rshell_generate_nonce(client);
                rshell_send_auth_instructions(client, "");
            }
        } else {
            rshell_send_auth_instructions(client, "Authenticate first.\r\n");
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
                    if (client->authenticated) {
                        rshell_tcp_send(client, "\b \b", 3);
                    }
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
                if (client->authenticated) {
                    rshell_tcp_send(client, &ch, 1);
                }
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
    rshell_generate_nonce(client);

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
    {
        const char *banner = "\r\n=== littleOS Remote Shell ===\r\n";
        rshell_tcp_send(client, banner, strlen(banner));
    }
    rshell_send_auth_instructions(client, "");
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

    if (rshell_migrate_legacy_token() != 0) {
        dmesg_err("rshell: failed to migrate legacy token");
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
