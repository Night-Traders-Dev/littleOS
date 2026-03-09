/* cmd_dma.c - DMA Engine shell commands */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hal/dma.h"

#define DMA_TEST_BUF_SIZE 256

static void cmd_dma_usage(void) {
    printf("DMA engine commands:\r\n");
    printf("  dma status                              - Show all channel status\r\n");
    printf("  dma claim [ch]                           - Claim channel (auto if omitted)\r\n");
    printf("  dma release <ch>                         - Release channel\r\n");
    printf("  dma memcpy <dst_hex> <src_hex> <len>     - DMA memory copy\r\n");
    printf("  dma test                                 - Self-test: DMA memcpy verify\r\n");
    printf("  dma sniff <ch> crc32|crc32r|crc16|sum    - Enable sniffer on channel\r\n");
    printf("  dma timer <num> <numerator> <denominator> - Set timer pacer (0-3)\r\n");
}

static int cmd_dma_status(void) {
    printf("DMA Channel Status:\r\n");
    printf("  CH  Claimed  Busy  Transfers  Bytes\r\n");
    printf("  --  -------  ----  ---------  -----\r\n");

    for (int ch = 0; ch < DMA_NUM_CHANNELS; ch++) {
        dma_channel_status_t st;
        int r = dma_hal_status(ch, &st);
        if (r != 0) {
            printf("  %2d  (error reading status)\r\n", ch);
            continue;
        }
        printf("  %2d  %-7s  %-4s  %9lu  %lu\r\n",
               ch,
               st.claimed ? "yes" : "no",
               st.busy ? "yes" : "no",
               (unsigned long)st.total_transfers,
               (unsigned long)st.total_bytes);
    }
    return 0;
}

static int cmd_dma_claim(int argc, char *argv[]) {
    int channel = -1;
    if (argc >= 3) {
        channel = atoi(argv[2]);
        if (channel < 0 || channel >= DMA_NUM_CHANNELS) {
            printf("Invalid channel %d (0-%d)\r\n", channel, DMA_NUM_CHANNELS - 1);
            return -1;
        }
    }

    int result = dma_hal_claim(channel);
    if (result < 0) {
        printf("Failed to claim DMA channel\r\n");
        return -1;
    }
    printf("DMA channel %d claimed\r\n", result);
    return 0;
}

static int cmd_dma_release(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: dma release <channel>\r\n");
        return -1;
    }

    int channel = atoi(argv[2]);
    int r = dma_hal_release(channel);
    if (r != 0) {
        printf("Failed to release DMA channel %d\r\n", channel);
        return -1;
    }
    printf("DMA channel %d released\r\n", channel);
    return 0;
}

static int cmd_dma_memcpy(int argc, char *argv[]) {
    if (argc < 5) {
        printf("Usage: dma memcpy <dst_hex> <src_hex> <len>\r\n");
        return -1;
    }

    void *dst = (void *)strtoul(argv[2], NULL, 16);
    const void *src = (const void *)strtoul(argv[3], NULL, 16);
    size_t len = (size_t)strtoul(argv[4], NULL, 0);

    if (len == 0) {
        printf("Length must be > 0\r\n");
        return -1;
    }

    /* Claim a temporary channel for the copy */
    int ch = dma_hal_claim(-1);
    if (ch < 0) {
        printf("Failed to claim DMA channel\r\n");
        return -1;
    }

    printf("DMA memcpy: 0x%08lx -> 0x%08lx (%lu bytes) on ch%d\r\n",
           (unsigned long)(uintptr_t)src,
           (unsigned long)(uintptr_t)dst,
           (unsigned long)len, ch);

    int r = dma_hal_memcpy(ch, dst, src, len);
    if (r != 0) {
        printf("DMA memcpy start failed: %d\r\n", r);
        dma_hal_release(ch);
        return -1;
    }

    r = dma_hal_wait(ch, 5000);
    if (r != 0) {
        printf("DMA memcpy timed out\r\n");
        dma_hal_abort(ch);
        dma_hal_release(ch);
        return -1;
    }

    printf("DMA memcpy complete\r\n");
    dma_hal_release(ch);
    return 0;
}

static int cmd_dma_test(void) {
    printf("DMA self-test: allocating %d-byte buffers...\r\n", DMA_TEST_BUF_SIZE);

    static uint8_t src_buf[DMA_TEST_BUF_SIZE];
    static uint8_t dst_buf[DMA_TEST_BUF_SIZE];

    /* Fill source with a known pattern */
    for (int i = 0; i < DMA_TEST_BUF_SIZE; i++) {
        src_buf[i] = (uint8_t)(i & 0xFF);
    }
    memset(dst_buf, 0, DMA_TEST_BUF_SIZE);

    /* Claim a channel */
    int ch = dma_hal_claim(-1);
    if (ch < 0) {
        printf("FAIL: could not claim DMA channel\r\n");
        return -1;
    }
    printf("  Claimed channel %d\r\n", ch);

    /* Start DMA memcpy */
    int r = dma_hal_memcpy(ch, dst_buf, src_buf, DMA_TEST_BUF_SIZE);
    if (r != 0) {
        printf("FAIL: dma_hal_memcpy returned %d\r\n", r);
        dma_hal_release(ch);
        return -1;
    }

    /* Wait for completion */
    r = dma_hal_wait(ch, 5000);
    if (r != 0) {
        printf("FAIL: DMA wait timed out\r\n");
        dma_hal_abort(ch);
        dma_hal_release(ch);
        return -1;
    }

    /* Verify destination matches source */
    int errors = 0;
    for (int i = 0; i < DMA_TEST_BUF_SIZE; i++) {
        if (dst_buf[i] != src_buf[i]) {
            if (errors < 8) {
                printf("  MISMATCH at offset %d: expected 0x%02X got 0x%02X\r\n",
                       i, src_buf[i], dst_buf[i]);
            }
            errors++;
        }
    }

    /* Check channel status */
    dma_channel_status_t st;
    dma_hal_status(ch, &st);
    printf("  Channel %d: %lu transfers, %lu bytes total\r\n",
           ch, (unsigned long)st.total_transfers, (unsigned long)st.total_bytes);

    dma_hal_release(ch);

    if (errors == 0) {
        printf("DMA self-test PASSED: %d bytes verified OK\r\n", DMA_TEST_BUF_SIZE);
        return 0;
    } else {
        printf("DMA self-test FAILED: %d byte mismatches\r\n", errors);
        return -1;
    }
}

static int cmd_dma_sniff(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: dma sniff <channel> crc32|crc32r|crc16|sum\r\n");
        return -1;
    }

    int ch = atoi(argv[2]);
    const char *mode_str = argv[3];
    dma_sniff_mode_t mode;

    if (strcmp(mode_str, "crc32") == 0) {
        mode = DMA_SNIFF_CRC32;
    } else if (strcmp(mode_str, "crc32r") == 0) {
        mode = DMA_SNIFF_CRC32R;
    } else if (strcmp(mode_str, "crc16") == 0) {
        mode = DMA_SNIFF_CRC16;
    } else if (strcmp(mode_str, "sum") == 0) {
        mode = DMA_SNIFF_SUM;
    } else {
        printf("Unknown sniff mode '%s'. Use: crc32, crc32r, crc16, sum\r\n", mode_str);
        return -1;
    }

    int r = dma_hal_sniff_enable(ch, mode);
    if (r != 0) {
        printf("Failed to enable sniffer on channel %d\r\n", ch);
        return -1;
    }

    dma_hal_sniff_reset(0xFFFFFFFF);
    printf("DMA sniffer enabled on ch%d mode=%s (seed=0xFFFFFFFF)\r\n", ch, mode_str);
    printf("  Current result: 0x%08lX\r\n", (unsigned long)dma_hal_sniff_result());
    return 0;
}

static int cmd_dma_timer(int argc, char *argv[]) {
    if (argc < 5) {
        printf("Usage: dma timer <num 0-3> <numerator> <denominator>\r\n");
        return -1;
    }

    uint8_t timer_num = (uint8_t)atoi(argv[2]);
    uint16_t num = (uint16_t)strtoul(argv[3], NULL, 0);
    uint16_t den = (uint16_t)strtoul(argv[4], NULL, 0);

    int r = dma_hal_set_timer(timer_num, num, den);
    if (r != 0) {
        printf("Failed to set DMA timer %u\r\n", timer_num);
        return -1;
    }

    printf("DMA timer%u set to %u/%u (rate = sys_clk * %u / %u)\r\n",
           timer_num, num, den, num, den);
    return 0;
}

int cmd_dma(int argc, char *argv[]) {
    if (argc < 2) {
        cmd_dma_usage();
        return -1;
    }

    /* Ensure DMA subsystem is initialized */
    dma_hal_init();

    if (strcmp(argv[1], "status") == 0) return cmd_dma_status();
    if (strcmp(argv[1], "claim") == 0)  return cmd_dma_claim(argc, argv);
    if (strcmp(argv[1], "release") == 0) return cmd_dma_release(argc, argv);
    if (strcmp(argv[1], "memcpy") == 0) return cmd_dma_memcpy(argc, argv);
    if (strcmp(argv[1], "test") == 0)   return cmd_dma_test();
    if (strcmp(argv[1], "sniff") == 0)  return cmd_dma_sniff(argc, argv);
    if (strcmp(argv[1], "timer") == 0)  return cmd_dma_timer(argc, argv);

    cmd_dma_usage();
    return -1;
}
