/* ota.c - Over-the-Air update mechanism for littleOS */

#include "ota.h"
#include "config_storage.h"
#include "dmesg.h"
#include "hal/flash.h"
#include <stdio.h>
#include <string.h>

#define OTA_AUTH_KEY_CONFIG "ota_hmac_key"
#define SHA256_BLOCK_SIZE   64u

#ifdef PICO_BUILD
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "pico/bootrom.h"
#endif

/* ============================================================================
 * CRC32 (same algorithm as fs_core.c)
 * ============================================================================ */

static uint32_t ota_crc32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (-(crc & 1u)));
        }
    }
    return ~crc;
}

/* ============================================================================
 * SHA-256 / HMAC-SHA256 helpers
 * ============================================================================ */

typedef struct {
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t  data[64];
    size_t   datalen;
} sha256_ctx_t;

static const uint32_t sha256_k[64] = {
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

static uint32_t sha256_rotr(uint32_t value, uint32_t bits) {
    return (value >> bits) | (value << (32u - bits));
}

static void sha256_transform(sha256_ctx_t *ctx, const uint8_t block[64]) {
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h;

    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i * 4] << 24) |
               ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] << 8) |
               (uint32_t)block[i * 4 + 3];
    }

    for (int i = 16; i < 64; i++) {
        uint32_t s0 = sha256_rotr(w[i - 15], 7) ^
                      sha256_rotr(w[i - 15], 18) ^
                      (w[i - 15] >> 3);
        uint32_t s1 = sha256_rotr(w[i - 2], 17) ^
                      sha256_rotr(w[i - 2], 19) ^
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
        uint32_t s1 = sha256_rotr(e, 6) ^ sha256_rotr(e, 11) ^ sha256_rotr(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + s1 + ch + sha256_k[i] + w[i];
        uint32_t s0 = sha256_rotr(a, 2) ^ sha256_rotr(a, 13) ^ sha256_rotr(a, 22);
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

static void sha256_init(sha256_ctx_t *ctx) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->state[0] = 0x6a09e667u;
    ctx->state[1] = 0xbb67ae85u;
    ctx->state[2] = 0x3c6ef372u;
    ctx->state[3] = 0xa54ff53au;
    ctx->state[4] = 0x510e527fu;
    ctx->state[5] = 0x9b05688cu;
    ctx->state[6] = 0x1f83d9abu;
    ctx->state[7] = 0x5be0cd19u;
}

static void sha256_update(sha256_ctx_t *ctx, const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        ctx->data[ctx->datalen++] = data[i];
        if (ctx->datalen == sizeof(ctx->data)) {
            sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512u;
            ctx->datalen = 0;
        }
    }
}

static void sha256_final(sha256_ctx_t *ctx, uint8_t hash[32]) {
    size_t i = ctx->datalen;

    ctx->data[i++] = 0x80u;
    if (i > 56) {
        while (i < 64) ctx->data[i++] = 0;
        sha256_transform(ctx, ctx->data);
        i = 0;
    }

    while (i < 56) ctx->data[i++] = 0;

    ctx->bitlen += (uint64_t)ctx->datalen * 8u;
    ctx->data[63] = (uint8_t)(ctx->bitlen);
    ctx->data[62] = (uint8_t)(ctx->bitlen >> 8);
    ctx->data[61] = (uint8_t)(ctx->bitlen >> 16);
    ctx->data[60] = (uint8_t)(ctx->bitlen >> 24);
    ctx->data[59] = (uint8_t)(ctx->bitlen >> 32);
    ctx->data[58] = (uint8_t)(ctx->bitlen >> 40);
    ctx->data[57] = (uint8_t)(ctx->bitlen >> 48);
    ctx->data[56] = (uint8_t)(ctx->bitlen >> 56);
    sha256_transform(ctx, ctx->data);

    for (i = 0; i < 4; i++) {
        hash[i]      = (uint8_t)(ctx->state[0] >> (24 - i * 8));
        hash[i + 4]  = (uint8_t)(ctx->state[1] >> (24 - i * 8));
        hash[i + 8]  = (uint8_t)(ctx->state[2] >> (24 - i * 8));
        hash[i + 12] = (uint8_t)(ctx->state[3] >> (24 - i * 8));
        hash[i + 16] = (uint8_t)(ctx->state[4] >> (24 - i * 8));
        hash[i + 20] = (uint8_t)(ctx->state[5] >> (24 - i * 8));
        hash[i + 24] = (uint8_t)(ctx->state[6] >> (24 - i * 8));
        hash[i + 28] = (uint8_t)(ctx->state[7] >> (24 - i * 8));
    }
}

static void hmac_sha256(const uint8_t *key,
                        size_t key_len,
                        const uint8_t *part1,
                        size_t part1_len,
                        const uint8_t *part2,
                        size_t part2_len,
                        uint8_t out[OTA_HMAC_SIZE])
{
    uint8_t key_block[SHA256_BLOCK_SIZE];
    uint8_t ipad[SHA256_BLOCK_SIZE];
    uint8_t opad[SHA256_BLOCK_SIZE];
    uint8_t inner_hash[OTA_HMAC_SIZE];
    sha256_ctx_t ctx;

    memset(key_block, 0, sizeof(key_block));
    if (key_len > sizeof(key_block)) {
        sha256_init(&ctx);
        sha256_update(&ctx, key, key_len);
        sha256_final(&ctx, key_block);
    } else {
        memcpy(key_block, key, key_len);
    }

    for (size_t i = 0; i < sizeof(key_block); i++) {
        ipad[i] = (uint8_t)(key_block[i] ^ 0x36u);
        opad[i] = (uint8_t)(key_block[i] ^ 0x5cu);
    }

    sha256_init(&ctx);
    sha256_update(&ctx, ipad, sizeof(ipad));
    if (part1 && part1_len > 0) sha256_update(&ctx, part1, part1_len);
    if (part2 && part2_len > 0) sha256_update(&ctx, part2, part2_len);
    sha256_final(&ctx, inner_hash);

    sha256_init(&ctx);
    sha256_update(&ctx, opad, sizeof(opad));
    sha256_update(&ctx, inner_hash, sizeof(inner_hash));
    sha256_final(&ctx, out);
}

static int ota_hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

static bool ota_parse_hex_key(const char *hex_key, uint8_t out[OTA_HMAC_SIZE]) {
    if (!hex_key || strlen(hex_key) != OTA_HMAC_SIZE * 2u) {
        return false;
    }

    for (size_t i = 0; i < OTA_HMAC_SIZE; i++) {
        int hi = ota_hex_nibble(hex_key[i * 2]);
        int lo = ota_hex_nibble(hex_key[i * 2 + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        out[i] = (uint8_t)((hi << 4) | lo);
    }

    return true;
}

static bool ota_load_auth_key(uint8_t key[OTA_HMAC_SIZE]) {
    char hex_key[(OTA_HMAC_SIZE * 2u) + 1u];
    if (config_get(OTA_AUTH_KEY_CONFIG, hex_key, sizeof(hex_key)) != CONFIG_OK) {
        return false;
    }
    return ota_parse_hex_key(hex_key, key);
}

bool ota_has_auth_key(void) {
    uint8_t key[OTA_HMAC_SIZE];
    return ota_load_auth_key(key);
}

int ota_set_auth_key_hex(const char *hex_key) {
    uint8_t key[OTA_HMAC_SIZE];
    if (!ota_parse_hex_key(hex_key, key)) {
        return OTA_ERR_AUTH;
    }
    if (config_set(OTA_AUTH_KEY_CONFIG, hex_key) != CONFIG_OK) {
        return OTA_ERR_FLASH;
    }
    return config_save() ? OTA_OK : OTA_ERR_FLASH;
}

int ota_clear_auth_key(void) {
    config_result_t rc = config_delete(OTA_AUTH_KEY_CONFIG);
    if (rc != CONFIG_OK && rc != CONFIG_ERROR_NOT_FOUND) {
        return OTA_ERR_FLASH;
    }
    return config_save() ? OTA_OK : OTA_ERR_FLASH;
}

static void ota_compute_image_hmac(const ota_image_header_t *hdr,
                                   const uint8_t *image_data,
                                   const uint8_t key[OTA_HMAC_SIZE],
                                   uint8_t out[OTA_HMAC_SIZE])
{
    ota_image_header_t hdr_copy = *hdr;
    memset(hdr_copy.image_hmac, 0, sizeof(hdr_copy.image_hmac));
    hdr_copy.header_crc32 = 0;

    hmac_sha256(key, OTA_HMAC_SIZE,
                (const uint8_t *)&hdr_copy,
                offsetof(ota_image_header_t, header_crc32),
                image_data,
                hdr->image_size,
                out);
}

static bool ota_constant_time_eq(const uint8_t *lhs, const uint8_t *rhs, size_t len) {
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++) {
        diff |= (uint8_t)(lhs[i] ^ rhs[i]);
    }
    return diff == 0;
}

/* ============================================================================
 * PICO_BUILD implementation
 * ============================================================================ */

#ifdef PICO_BUILD

/* ---------- State ---------- */

static ota_state_t      current_state       = OTA_STATE_IDLE;
static ota_metadata_t   metadata;
static uint32_t         bytes_received      = 0;
static uint32_t         total_expected      = 0;
static ota_progress_fn  progress_callback   = NULL;
static bool             ota_initialized     = false;
static bool             ota_flash_session_active = false;

/* ---------- Flash helpers (must run from RAM) ---------- */

static void ota_end_flash_session(void) {
    if (ota_flash_session_active) {
        flash_safe_session_end();
        ota_flash_session_active = false;
    }
}

static int ota_begin_flash_session(void) {
    if (ota_flash_session_active) {
        return OTA_OK;
    }

    if (flash_safe_session_begin() != 0) {
        dmesg_err("ota: Core 1 is busy; cannot safely write flash");
        return OTA_ERR_BUSY;
    }

    ota_flash_session_active = true;
    return OTA_OK;
}

static int ota_flash_erase_sector(uint32_t offset) {
    return flash_safe_erase_range(offset, FLASH_SECTOR_SIZE) == 0 ? OTA_OK : OTA_ERR_FLASH;
}

static int ota_flash_program(uint32_t offset, const uint8_t *data, uint32_t len) {
    return flash_safe_program_range(offset, data, len) == 0 ? OTA_OK : OTA_ERR_FLASH;
}

/* ---------- Metadata I/O ---------- */

static void ota_read_metadata(void) {
    const uint8_t *flash_ptr = (const uint8_t *)(XIP_BASE + OTA_METADATA_OFFSET);
    memcpy(&metadata, flash_ptr, sizeof(metadata));
}

static int ota_write_metadata(void) {
    uint8_t metadata_page[OTA_CHUNK_SIZE];

    /* Compute metadata CRC (zero out CRC field first) */
    ota_metadata_t tmp = metadata;
    tmp.metadata_crc32 = 0;
    metadata.metadata_crc32 = ota_crc32((const uint8_t *)&tmp, sizeof(tmp));

    memset(metadata_page, 0xFF, sizeof(metadata_page));
    memcpy(metadata_page, &metadata, sizeof(metadata));

    if (flash_safe_erase_and_program(OTA_METADATA_OFFSET,
                                     FLASH_SECTOR_SIZE,
                                     metadata_page,
                                     sizeof(metadata_page)) != 0) {
        return OTA_ERR_FLASH;
    }

    return OTA_OK;
}

static bool ota_validate_metadata(void) {
    if (metadata.magic != OTA_MAGIC) return false;

    uint32_t saved_crc = metadata.metadata_crc32;
    ota_metadata_t tmp = metadata;
    tmp.metadata_crc32 = 0;
    uint32_t calc_crc = ota_crc32((const uint8_t *)&tmp, sizeof(tmp));

    return saved_crc == calc_crc;
}

static void ota_create_default_metadata(void) {
    memset(&metadata, 0, sizeof(metadata));
    metadata.magic              = OTA_MAGIC;
    metadata.active_slot        = 0;    /* Slot A */
    metadata.pending_slot       = 0xFF; /* No pending */
    metadata.boot_count         = 0;
    metadata.max_boot_attempts  = 3;
    metadata.slot_a_crc32       = 0;
    metadata.slot_b_crc32       = 0;
    metadata.last_update_time   = 0;
}

static uint32_t ota_slot_offset(uint8_t slot) {
    return slot == 0 ? OTA_SLOT_A_OFFSET : OTA_SLOT_B_OFFSET;
}

static bool ota_read_slot_header(uint8_t slot, ota_image_header_t *out) {
    const ota_image_header_t *hdr;

    if (!out || slot > 1u) {
        return false;
    }

    hdr = (const ota_image_header_t *)(XIP_BASE + ota_slot_offset(slot));
    memcpy(out, hdr, sizeof(*out));
    return true;
}

static bool ota_header_has_valid_structure(const ota_image_header_t *hdr) {
    ota_image_header_t hdr_copy;
    uint32_t hdr_crc;

    if (!hdr) return false;
    if (hdr->magic != OTA_MAGIC) return false;
    if (hdr->version != OTA_VERSION) return false;
    if (hdr->auth_type != OTA_AUTH_HMAC_SHA256) return false;
    if (hdr->image_size == 0 || hdr->image_size > OTA_MAX_IMAGE_SIZE) return false;

    hdr_copy = *hdr;
    hdr_copy.header_crc32 = 0;
    hdr_crc = ota_crc32((const uint8_t *)&hdr_copy, sizeof(hdr_copy));
    return hdr_crc == hdr->header_crc32;
}

static bool ota_seed_rollback_floor_from_active_slot(void) {
    ota_image_header_t hdr;

    if (metadata.last_update_time != 0) {
        return true;
    }

    if (!ota_read_slot_header(metadata.active_slot, &hdr)) {
        return true;
    }
    if (!ota_header_has_valid_structure(&hdr) || hdr.build_timestamp == 0) {
        return true;
    }

    metadata.last_update_time = hdr.build_timestamp;
    return ota_write_metadata() == OTA_OK;
}

/* ---------- Public API ---------- */

int ota_init(void) {
    if (ota_initialized) return OTA_OK;

    ota_read_metadata();

    if (!ota_validate_metadata()) {
        dmesg_warn("ota: no valid metadata, creating defaults");
        ota_create_default_metadata();
        int err = ota_write_metadata();
        if (err != OTA_OK) {
            dmesg_err("ota: failed to write default metadata");
            return OTA_ERR_FLASH;
        }
    }

    if (!ota_seed_rollback_floor_from_active_slot()) {
        dmesg_err("ota: failed to persist rollback floor");
        return OTA_ERR_FLASH;
    }

    current_state = OTA_STATE_IDLE;
    bytes_received = 0;
    total_expected = 0;
    progress_callback = NULL;
    ota_initialized = true;

    dmesg_info("ota: initialized, active slot %c",
               metadata.active_slot == 0 ? 'A' : 'B');
    return OTA_OK;
}

ota_state_t ota_get_state(void) {
    return current_state;
}

uint8_t ota_get_active_slot(void) {
    return metadata.active_slot;
}

int ota_begin_uart(ota_progress_fn progress_cb) {
    if (!ota_initialized) return OTA_ERR_INIT;
    if (current_state == OTA_STATE_RECEIVING) return OTA_ERR_BUSY;
    if (!ota_has_auth_key()) return OTA_ERR_AUTH;
    int flash_err = ota_begin_flash_session();
    if (flash_err != OTA_OK) return flash_err;

    current_state = OTA_STATE_RECEIVING;
    bytes_received = 0;
    total_expected = 0;
    progress_callback = progress_cb;

    dmesg_info("ota: ready to receive via UART (XMODEM)");
    return OTA_OK;
}

int ota_begin_tcp(uint16_t port, ota_progress_fn progress_cb) {
    if (!ota_initialized) return OTA_ERR_INIT;
    if (current_state == OTA_STATE_RECEIVING) return OTA_ERR_BUSY;
    if (!ota_has_auth_key()) return OTA_ERR_AUTH;
    int flash_err = ota_begin_flash_session();
    if (flash_err != OTA_OK) return flash_err;

    current_state = OTA_STATE_RECEIVING;
    bytes_received = 0;
    total_expected = 0;
    progress_callback = progress_cb;

    dmesg_info("ota: ready to receive via TCP on port %u", port);
    return OTA_OK;
}

int ota_write_chunk(const uint8_t *data, uint32_t offset, uint32_t len) {
    if (!ota_initialized) return OTA_ERR_INIT;
    if (!data || len == 0) return OTA_ERR_SIZE;

    /* Validate bounds within slot B */
    if (offset + len > OTA_SLOT_B_SIZE) {
        dmesg_err("ota: chunk exceeds slot B bounds (offset=0x%08X, len=%u)",
                  offset, len);
        current_state = OTA_STATE_ERROR;
        ota_end_flash_session();
        return OTA_ERR_SIZE;
    }

    uint32_t flash_addr = OTA_SLOT_B_OFFSET + offset;

    /* Erase sectors as needed (4KB sector boundary) */
    uint32_t sector_start = flash_addr & ~(FLASH_SECTOR_SIZE - 1);
    uint32_t sector_end   = (flash_addr + len - 1) & ~(FLASH_SECTOR_SIZE - 1);

    for (uint32_t s = sector_start; s <= sector_end; s += FLASH_SECTOR_SIZE) {
        /* Only erase if this is the first write into this sector.
         * Simplified: erase if offset aligns to sector boundary,
         * or if this is the first chunk. */
        if ((flash_addr & (FLASH_SECTOR_SIZE - 1)) == 0 || offset == 0) {
            if (ota_flash_erase_sector(s) != OTA_OK) {
                current_state = OTA_STATE_ERROR;
                ota_end_flash_session();
                return OTA_ERR_FLASH;
            }
        }
    }

    /* Program in OTA_CHUNK_SIZE-aligned pages */
    uint32_t remaining = len;
    uint32_t src_offset = 0;

    while (remaining > 0) {
        uint32_t page_offset = (flash_addr + src_offset) & (OTA_CHUNK_SIZE - 1);
        uint32_t chunk = OTA_CHUNK_SIZE - page_offset;
        if (chunk > remaining) chunk = remaining;

        /* Pad to OTA_CHUNK_SIZE if needed (flash_range_program requires aligned writes) */
        uint8_t page_buf[OTA_CHUNK_SIZE];
        memset(page_buf, 0xFF, OTA_CHUNK_SIZE);
        memcpy(page_buf + page_offset, data + src_offset, chunk);

        uint32_t aligned_addr = (flash_addr + src_offset) & ~(OTA_CHUNK_SIZE - 1);
        if (ota_flash_program(aligned_addr, page_buf, OTA_CHUNK_SIZE) != OTA_OK) {
            current_state = OTA_STATE_ERROR;
            ota_end_flash_session();
            return OTA_ERR_FLASH;
        }

        src_offset += chunk;
        remaining  -= chunk;
    }

    bytes_received = offset + len;

    if (progress_callback && total_expected > 0) {
        progress_callback(bytes_received, total_expected);
    }

    return OTA_OK;
}

int ota_verify(void) {
    uint8_t auth_key[OTA_HMAC_SIZE];
    uint8_t expected_hmac[OTA_HMAC_SIZE];

    if (!ota_initialized) return OTA_ERR_INIT;
    if (!ota_load_auth_key(auth_key)) {
        ota_end_flash_session();
        return OTA_ERR_AUTH;
    }
    ota_end_flash_session();

    current_state = OTA_STATE_VERIFYING;
    dmesg_info("ota: verifying slot B image...");

    /* Read image header from start of slot B */
    const ota_image_header_t *hdr =
        (const ota_image_header_t *)(XIP_BASE + OTA_SLOT_B_OFFSET);

    if (hdr->magic != OTA_MAGIC) {
        dmesg_err("ota: bad magic in slot B (0x%08X)", hdr->magic);
        current_state = OTA_STATE_ERROR;
        return OTA_ERR_VERIFY;
    }

    if (hdr->version != OTA_VERSION) {
        dmesg_err("ota: unsupported version %u", hdr->version);
        current_state = OTA_STATE_ERROR;
        return OTA_ERR_VERIFY;
    }

    if (hdr->auth_type != OTA_AUTH_HMAC_SHA256) {
        dmesg_err("ota: unsupported auth type %u", hdr->auth_type);
        current_state = OTA_STATE_ERROR;
        return OTA_ERR_AUTH;
    }

    if (hdr->image_size == 0 || hdr->image_size > OTA_MAX_IMAGE_SIZE) {
        dmesg_err("ota: invalid image size %u", hdr->image_size);
        current_state = OTA_STATE_ERROR;
        return OTA_ERR_SIZE;
    }

    if (hdr->build_timestamp == 0) {
        dmesg_err("ota: missing build timestamp in slot B image");
        current_state = OTA_STATE_ERROR;
        return OTA_ERR_ROLLBACK;
    }

    if (metadata.last_update_time != 0 &&
        hdr->build_timestamp < metadata.last_update_time) {
        dmesg_err("ota: refusing rollback image (build %u < floor %u)",
                  hdr->build_timestamp,
                  metadata.last_update_time);
        current_state = OTA_STATE_ERROR;
        return OTA_ERR_ROLLBACK;
    }

    /* Verify header CRC */
    ota_image_header_t hdr_copy = *hdr;
    hdr_copy.header_crc32 = 0;
    uint32_t hdr_crc = ota_crc32((const uint8_t *)&hdr_copy, sizeof(hdr_copy));
    if (hdr_crc != hdr->header_crc32) {
        dmesg_err("ota: header CRC mismatch (calc=0x%08X, stored=0x%08X)",
                  hdr_crc, hdr->header_crc32);
        current_state = OTA_STATE_ERROR;
        return OTA_ERR_CRC;
    }

    /* Verify image data CRC (data starts after header) */
    const uint8_t *image_data = (const uint8_t *)(XIP_BASE + OTA_SLOT_B_OFFSET
                                                   + sizeof(ota_image_header_t));
    uint32_t img_crc = ota_crc32(image_data, hdr->image_size);
    if (img_crc != hdr->image_crc32) {
        dmesg_err("ota: image CRC mismatch (calc=0x%08X, stored=0x%08X)",
                  img_crc, hdr->image_crc32);
        current_state = OTA_STATE_ERROR;
        return OTA_ERR_CRC;
    }

    ota_compute_image_hmac(hdr, image_data, auth_key, expected_hmac);
    if (!ota_constant_time_eq(expected_hmac, hdr->image_hmac, sizeof(expected_hmac))) {
        dmesg_err("ota: image HMAC verification failed");
        current_state = OTA_STATE_ERROR;
        return OTA_ERR_AUTH;
    }

    /* Update metadata with slot B CRC */
    metadata.slot_b_crc32 = hdr->image_crc32;

    current_state = OTA_STATE_READY;
    dmesg_info("ota: slot B verified OK (%u bytes, v%s)",
               hdr->image_size, hdr->build_version);
    return OTA_OK;
}

int ota_apply(void) {
    if (!ota_initialized) return OTA_ERR_INIT;
    if (current_state != OTA_STATE_READY) return OTA_ERR_NO_IMAGE;

    current_state = OTA_STATE_APPLYING;
    dmesg_info("ota: applying update (slot B -> pending)");

    metadata.pending_slot = 1; /* Slot B */
    metadata.boot_count = 0;

    int err = ota_write_metadata();
    if (err != OTA_OK) {
        dmesg_err("ota: failed to write metadata");
        current_state = OTA_STATE_ERROR;
        return OTA_ERR_FLASH;
    }

    current_state = OTA_STATE_COMPLETE;
    dmesg_info("ota: rebooting into slot B...");

    /* Trigger reboot via watchdog */
    watchdog_enable(100, 0);
    while (1) {
        tight_loop_contents();
    }

    /* Unreachable */
    return OTA_OK;
}

int ota_confirm_boot(void) {
    uint8_t confirmed_slot;

    if (!ota_initialized) return OTA_ERR_INIT;

    if (metadata.pending_slot == 0xFF) {
        /* No pending slot, nothing to confirm */
        return OTA_OK;
    }

    metadata.boot_count++;

    if (metadata.boot_count > metadata.max_boot_attempts) {
        dmesg_warn("ota: max boot attempts exceeded, rolling back");
        return ota_rollback();
    }

    /* Mark pending slot as active */
    dmesg_info("ota: confirming boot on slot %c (attempt %u/%u)",
               metadata.pending_slot == 0 ? 'A' : 'B',
               metadata.boot_count, metadata.max_boot_attempts);

    confirmed_slot = metadata.pending_slot;
    metadata.active_slot = metadata.pending_slot;
    metadata.pending_slot = 0xFF;   /* Clear pending */

    {
        ota_image_header_t hdr;

        if (ota_read_slot_header(confirmed_slot, &hdr) &&
            ota_header_has_valid_structure(&hdr) &&
            hdr.build_timestamp != 0 &&
            hdr.build_timestamp > metadata.last_update_time) {
            metadata.last_update_time = hdr.build_timestamp;
        }
    }

    int err = ota_write_metadata();
    if (err != OTA_OK) {
        dmesg_err("ota: failed to write confirmed metadata");
        return OTA_ERR_FLASH;
    }

    dmesg_info("ota: boot confirmed, active slot %c",
               metadata.active_slot == 0 ? 'A' : 'B');
    return OTA_OK;
}

int ota_rollback(void) {
    if (!ota_initialized) return OTA_ERR_INIT;

    uint8_t other_slot = (metadata.active_slot == 0) ? 1 : 0;
    dmesg_warn("ota: rolling back to slot %c", other_slot == 0 ? 'A' : 'B');

    metadata.active_slot = other_slot;
    metadata.pending_slot = 0xFF;
    metadata.boot_count = 0;

    int err = ota_write_metadata();
    if (err != OTA_OK) {
        dmesg_err("ota: rollback metadata write failed");
        return OTA_ERR_FLASH;
    }

    dmesg_info("ota: rebooting into slot %c...", other_slot == 0 ? 'A' : 'B');

    watchdog_enable(100, 0);
    while (1) {
        tight_loop_contents();
    }

    /* Unreachable */
    return OTA_OK;
}

int ota_cancel(void) {
    if (!ota_initialized) return OTA_ERR_INIT;

    if (current_state == OTA_STATE_IDLE) return OTA_OK;

    dmesg_info("ota: update cancelled");
    ota_end_flash_session();
    current_state = OTA_STATE_IDLE;
    bytes_received = 0;
    total_expected = 0;
    progress_callback = NULL;
    return OTA_OK;
}

void ota_get_progress(uint32_t *received, uint32_t *total) {
    if (received) *received = bytes_received;
    if (total)    *total    = total_expected;
}

void ota_print_status(void) {
    printf("=== OTA Status ===\r\n");
    printf("  Active slot:   %c\r\n", metadata.active_slot == 0 ? 'A' : 'B');

    if (metadata.pending_slot != 0xFF) {
        printf("  Pending slot:  %c (boot %u/%u)\r\n",
               metadata.pending_slot == 0 ? 'A' : 'B',
               metadata.boot_count, metadata.max_boot_attempts);
    } else {
        printf("  Pending slot:  none\r\n");
    }

    const char *state_str;
    switch (current_state) {
        case OTA_STATE_IDLE:      state_str = "idle";      break;
        case OTA_STATE_RECEIVING: state_str = "receiving"; break;
        case OTA_STATE_VERIFYING: state_str = "verifying"; break;
        case OTA_STATE_READY:     state_str = "ready";     break;
        case OTA_STATE_APPLYING:  state_str = "applying";  break;
        case OTA_STATE_COMPLETE:  state_str = "complete";  break;
        case OTA_STATE_ERROR:     state_str = "error";     break;
        default:                  state_str = "unknown";   break;
    }
    printf("  State:         %s\r\n", state_str);

    if (current_state == OTA_STATE_RECEIVING && total_expected > 0) {
        uint32_t pct = (bytes_received * 100) / total_expected;
        printf("  Progress:      %u / %u bytes (%u%%)\r\n",
               bytes_received, total_expected, pct);
    }

    printf("  Slot A CRC:    0x%08X\r\n", metadata.slot_a_crc32);
    printf("  Slot B CRC:    0x%08X\r\n", metadata.slot_b_crc32);
    printf("  Auth key:      %s\r\n", ota_has_auth_key() ? "configured" : "missing");
    printf("  Last update:   %u\r\n", metadata.last_update_time);
    printf("  Magic:         0x%08X %s\r\n", metadata.magic,
           metadata.magic == OTA_MAGIC ? "(valid)" : "(INVALID)");
}

/* ============================================================================
 * Non-PICO_BUILD stubs
 * ============================================================================ */

#else /* !PICO_BUILD */

static void ota_no_flash(void) {
    printf("OTA not available (requires PICO_BUILD)\r\n");
}

int ota_init(void)                                      { ota_no_flash(); return OTA_ERR_INIT; }
ota_state_t ota_get_state(void)                         { return OTA_STATE_IDLE; }
uint8_t ota_get_active_slot(void)                       { return 0; }
int ota_begin_uart(ota_progress_fn cb)                  { (void)cb; ota_no_flash(); return OTA_ERR_INIT; }
int ota_begin_tcp(uint16_t port, ota_progress_fn cb)    { (void)port; (void)cb; ota_no_flash(); return OTA_ERR_INIT; }
int ota_write_chunk(const uint8_t *data, uint32_t offset, uint32_t len) {
    (void)data; (void)offset; (void)len;
    ota_no_flash(); return OTA_ERR_INIT;
}
int ota_verify(void)                                    { ota_no_flash(); return OTA_ERR_INIT; }
int ota_apply(void)                                     { ota_no_flash(); return OTA_ERR_INIT; }
int ota_confirm_boot(void)                              { ota_no_flash(); return OTA_ERR_INIT; }
int ota_rollback(void)                                  { ota_no_flash(); return OTA_ERR_INIT; }
int ota_cancel(void)                                    { ota_no_flash(); return OTA_ERR_INIT; }
bool ota_has_auth_key(void)                             { return false; }
int ota_set_auth_key_hex(const char *hex_key)           { (void)hex_key; ota_no_flash(); return OTA_ERR_INIT; }
int ota_clear_auth_key(void)                            { ota_no_flash(); return OTA_ERR_INIT; }
void ota_get_progress(uint32_t *received, uint32_t *total) {
    if (received) *received = 0;
    if (total)    *total    = 0;
}
void ota_print_status(void) {
    printf("OTA not available (requires PICO_BUILD)\r\n");
}

#endif /* PICO_BUILD */
