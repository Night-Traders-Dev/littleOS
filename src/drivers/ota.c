/* ota.c - Over-the-Air update mechanism for littleOS */

#include "ota.h"
#include "dmesg.h"
#include <stdio.h>
#include <string.h>

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

/* ---------- Flash helpers (must run from RAM) ---------- */

static void __not_in_flash_func(ota_flash_erase_sector)(uint32_t offset) {
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(offset, FLASH_SECTOR_SIZE);
    restore_interrupts(ints);
}

static void __not_in_flash_func(ota_flash_program)(uint32_t offset,
                                                    const uint8_t *data,
                                                    uint32_t len)
{
    uint32_t ints = save_and_disable_interrupts();
    flash_range_program(offset, data, len);
    restore_interrupts(ints);
}

/* ---------- Metadata I/O ---------- */

static void ota_read_metadata(void) {
    const uint8_t *flash_ptr = (const uint8_t *)(XIP_BASE + OTA_METADATA_OFFSET);
    memcpy(&metadata, flash_ptr, sizeof(metadata));
}

static int __not_in_flash_func(ota_write_metadata)(void) {
    /* Compute metadata CRC (zero out CRC field first) */
    ota_metadata_t tmp = metadata;
    tmp.metadata_crc32 = 0;
    metadata.metadata_crc32 = ota_crc32((const uint8_t *)&tmp, sizeof(tmp));

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(OTA_METADATA_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(OTA_METADATA_OFFSET, (const uint8_t *)&metadata, sizeof(metadata));
    restore_interrupts(ints);

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
            ota_flash_erase_sector(s);
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
        ota_flash_program(aligned_addr, page_buf, OTA_CHUNK_SIZE);

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
    if (!ota_initialized) return OTA_ERR_INIT;

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

    if (hdr->image_size == 0 || hdr->image_size > OTA_MAX_IMAGE_SIZE) {
        dmesg_err("ota: invalid image size %u", hdr->image_size);
        current_state = OTA_STATE_ERROR;
        return OTA_ERR_SIZE;
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

    metadata.active_slot = metadata.pending_slot;
    metadata.pending_slot = 0xFF;   /* Clear pending */

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
void ota_get_progress(uint32_t *received, uint32_t *total) {
    if (received) *received = 0;
    if (total)    *total    = 0;
}
void ota_print_status(void) {
    printf("OTA not available (requires PICO_BUILD)\r\n");
}

#endif /* PICO_BUILD */
