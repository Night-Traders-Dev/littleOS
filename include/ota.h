/* ota.h - Over-the-Air update mechanism for littleOS */
#ifndef LITTLEOS_OTA_H
#define LITTLEOS_OTA_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * OTA Update Protocol
 *
 * Flash layout for OTA:
 *   Slot A: 0x000000 - 0x080000 (512 KB) - Active firmware
 *   Slot B: 0x080000 - 0x100000 (512 KB) - Staging area
 *   FS:     0x100000 - 0x1F0000 (960 KB) - Filesystem
 *   Config: 0x1F0000 - 0x200000 (64 KB)  - Config + OTA metadata
 *
 * Update process:
 *   1. Receive firmware image (via UART XMODEM or TCP)
 *   2. Write to staging slot (B)
 *   3. Verify CRC32 and magic
 *   4. Mark slot B as pending boot
 *   5. Reboot into slot B
 *   6. If boot succeeds, mark slot B as active
 *   7. If boot fails (watchdog reset), revert to slot A
 * ============================================================================ */

#define OTA_SLOT_A_OFFSET       0x000000u
#define OTA_SLOT_A_SIZE         0x080000u   /* 512 KB */
#define OTA_SLOT_B_OFFSET       0x080000u
#define OTA_SLOT_B_SIZE         0x080000u   /* 512 KB */
#define OTA_METADATA_OFFSET     0x1FE000u   /* Last 8KB before end */
#define OTA_METADATA_SIZE       0x001000u   /* 4 KB */

#define OTA_MAGIC               0x4F544155u /* "OTAU" */
#define OTA_VERSION             1u

#define OTA_CHUNK_SIZE          256u        /* Flash program page size */
#define OTA_MAX_IMAGE_SIZE      OTA_SLOT_B_SIZE

/* OTA state */
typedef enum {
    OTA_STATE_IDLE          = 0,
    OTA_STATE_RECEIVING     = 1,
    OTA_STATE_VERIFYING     = 2,
    OTA_STATE_READY         = 3,    /* Verified, ready to apply */
    OTA_STATE_APPLYING      = 4,
    OTA_STATE_COMPLETE      = 5,
    OTA_STATE_ERROR         = 6,
} ota_state_t;

/* OTA transport */
typedef enum {
    OTA_TRANSPORT_UART      = 0,    /* XMODEM over UART */
    OTA_TRANSPORT_TCP       = 1,    /* TCP socket */
} ota_transport_t;

/* OTA image header (written at start of slot B) */
typedef struct {
    uint32_t    magic;              /* OTA_MAGIC */
    uint32_t    version;            /* OTA_VERSION */
    uint32_t    image_size;         /* Firmware image size */
    uint32_t    image_crc32;        /* CRC32 of image data */
    uint32_t    build_timestamp;    /* Build time */
    char        build_version[32];  /* Version string */
    uint32_t    header_crc32;       /* CRC32 of this header */
} ota_image_header_t;

/* OTA metadata (stored in flash metadata sector) */
typedef struct {
    uint32_t    magic;
    uint8_t     active_slot;        /* 0=A, 1=B */
    uint8_t     pending_slot;       /* Slot pending verification */
    uint8_t     boot_count;         /* Boot count since last OTA */
    uint8_t     max_boot_attempts;  /* Max attempts before rollback */
    uint32_t    slot_a_crc32;
    uint32_t    slot_b_crc32;
    uint32_t    last_update_time;
    uint32_t    metadata_crc32;
} ota_metadata_t;

/* OTA progress callback */
typedef void (*ota_progress_fn)(uint32_t bytes_received, uint32_t total_bytes);

/* ============================================================================
 * Public API
 * ============================================================================ */

/* Initialize OTA subsystem */
int ota_init(void);

/* Get current OTA state */
ota_state_t ota_get_state(void);

/* Get active firmware slot (0=A, 1=B) */
uint8_t ota_get_active_slot(void);

/* Start receiving firmware update via UART (XMODEM protocol) */
int ota_begin_uart(ota_progress_fn progress_cb);

/* Start receiving firmware update via TCP */
int ota_begin_tcp(uint16_t port, ota_progress_fn progress_cb);

/* Feed raw firmware data (for custom transports) */
int ota_write_chunk(const uint8_t *data, uint32_t offset, uint32_t len);

/* Finalize and verify received image */
int ota_verify(void);

/* Apply update (marks staging slot as pending, triggers reboot) */
int ota_apply(void);

/* Confirm current boot is successful (call after successful startup) */
int ota_confirm_boot(void);

/* Rollback to previous firmware slot */
int ota_rollback(void);

/* Cancel in-progress update */
int ota_cancel(void);

/* Get update progress */
void ota_get_progress(uint32_t *received, uint32_t *total);

/* Print OTA status */
void ota_print_status(void);

/* ============================================================================
 * Error codes
 * ============================================================================ */

#define OTA_OK              0
#define OTA_ERR_INIT        (-1)
#define OTA_ERR_BUSY        (-2)
#define OTA_ERR_VERIFY      (-3)
#define OTA_ERR_FLASH       (-4)
#define OTA_ERR_SIZE        (-5)
#define OTA_ERR_CRC         (-6)
#define OTA_ERR_TRANSPORT   (-7)
#define OTA_ERR_ROLLBACK    (-8)
#define OTA_ERR_NO_IMAGE    (-9)

#ifdef __cplusplus
}
#endif

#endif /* LITTLEOS_OTA_H */
