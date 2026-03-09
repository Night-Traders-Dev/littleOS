/* cmd_pio.c - PIO shell commands */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hal/pio.h"

static void cmd_pio_usage(void) {
    printf("PIO (Programmable I/O) commands:\r\n");
    printf("  pio status                        - Show PIO status\r\n");
    printf("  pio load <block> <hex insns...>    - Load program\r\n");
    printf("  pio unload <block>                 - Unload program\r\n");
    printf("  pio config <block> <sm> <clkdiv> <pin> <count> - Configure SM\r\n");
    printf("  pio start <block> <sm>             - Start state machine\r\n");
    printf("  pio stop <block> <sm>              - Stop state machine\r\n");
    printf("  pio exec <block> <sm> <hex_insn>   - Execute instruction\r\n");
    printf("  pio put <block> <sm> <value>       - Write to TX FIFO\r\n");
    printf("  pio get <block> <sm>               - Read from RX FIFO\r\n");
    printf("  pio blink <pin> <freq>             - Built-in blink program\r\n");
    printf("  pio ws2812 <pin> <grb_hex>         - WS2812 LED control\r\n");
    printf("  pio uart <pin> <baud> <text>       - PIO UART transmit\r\n");
}

static int cmd_pio_status(void) {
    printf("PIO Status:\r\n");
    printf("=========================================\r\n");

    for (int block = 0; block < PIO_NUM_BLOCKS; block++) {
        printf("PIO%d:\r\n", block);
        for (int sm = 0; sm < PIO_NUM_SM; sm++) {
            pio_sm_status_t status;
            int ret = pio_hal_sm_status((uint8_t)block, (uint8_t)sm, &status);
            if (ret != 0) {
                printf("  SM%d: error reading status\r\n", sm);
                continue;
            }

            printf("  SM%d: %s", sm, status.active ? "RUNNING" : "STOPPED");
            if (status.program_loaded) {
                printf(" | prog: %d insns @ offset %d",
                       status.program_length, status.program_offset);
            } else {
                printf(" | no program");
            }
            if (status.clkdiv > 0.0f) {
                printf(" | clkdiv=%.2f", (double)status.clkdiv);
            }
            if (status.pin_count > 0) {
                printf(" | pins=%d-%d", status.pin_base,
                       status.pin_base + status.pin_count - 1);
            }
            if (status.tx_stalls > 0 || status.rx_stalls > 0) {
                printf(" | stalls: tx=%lu rx=%lu",
                       (unsigned long)status.tx_stalls,
                       (unsigned long)status.rx_stalls);
            }
            printf("\r\n");
        }
    }
    return 0;
}

static int cmd_pio_load(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: pio load <block> <hex_insn1> [hex_insn2 ...]\r\n");
        printf("  Example: pio load 0 e001 e000\r\n");
        return -1;
    }

    uint8_t block = (uint8_t)atoi(argv[2]);
    int num_insns = argc - 3;
    if (num_insns > PIO_MAX_PROGRAM_LEN) {
        printf("Error: program too long (max %d instructions)\r\n", PIO_MAX_PROGRAM_LEN);
        return -1;
    }

    uint16_t instructions[PIO_MAX_PROGRAM_LEN];
    for (int i = 0; i < num_insns; i++) {
        instructions[i] = (uint16_t)strtoul(argv[3 + i], NULL, 16);
    }

    int ret = pio_hal_load_program(block, instructions, (uint8_t)num_insns, 0xFF);
    if (ret == 0) {
        printf("PIO%d: loaded %d instructions\r\n", block, num_insns);
    } else {
        printf("PIO%d: load failed (%d)\r\n", block, ret);
    }
    return ret;
}

static int cmd_pio_unload(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: pio unload <block>\r\n");
        return -1;
    }

    uint8_t block = (uint8_t)atoi(argv[2]);
    int ret = pio_hal_unload_program(block);
    if (ret == 0) {
        printf("PIO%d: program unloaded\r\n", block);
    } else {
        printf("PIO%d: unload failed (%d)\r\n", block, ret);
    }
    return ret;
}

static int cmd_pio_config(int argc, char *argv[]) {
    if (argc < 7) {
        printf("Usage: pio config <block> <sm> <clkdiv> <pin> <count>\r\n");
        printf("  Example: pio config 0 0 125.0 25 1\r\n");
        return -1;
    }

    pio_hal_sm_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.pio_block = (uint8_t)atoi(argv[2]);
    cfg.sm = (uint8_t)atoi(argv[3]);
    cfg.clkdiv = (float)atof(argv[4]);
    cfg.pin_base = (uint8_t)atoi(argv[5]);
    cfg.pin_count = (uint8_t)atoi(argv[6]);
    cfg.autopull = false;
    cfg.autopush = false;
    cfg.pull_threshold = 32;
    cfg.push_threshold = 32;
    cfg.shift_right = true;
    cfg.in_shift_right = true;

    int ret = pio_hal_sm_config(cfg.pio_block, cfg.sm, &cfg);
    if (ret == 0) {
        printf("PIO%d SM%d: configured (clkdiv=%.2f, pin=%d, count=%d)\r\n",
               cfg.pio_block, cfg.sm, (double)cfg.clkdiv, cfg.pin_base, cfg.pin_count);
    } else {
        printf("PIO%d SM%d: config failed (%d)\r\n", cfg.pio_block, cfg.sm, ret);
    }
    return ret;
}

static int cmd_pio_start(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: pio start <block> <sm>\r\n");
        return -1;
    }

    uint8_t block = (uint8_t)atoi(argv[2]);
    uint8_t sm = (uint8_t)atoi(argv[3]);
    int ret = pio_hal_sm_start(block, sm);
    if (ret == 0) {
        printf("PIO%d SM%d: started\r\n", block, sm);
    } else {
        printf("PIO%d SM%d: start failed (%d)\r\n", block, sm, ret);
    }
    return ret;
}

static int cmd_pio_stop(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: pio stop <block> <sm>\r\n");
        return -1;
    }

    uint8_t block = (uint8_t)atoi(argv[2]);
    uint8_t sm = (uint8_t)atoi(argv[3]);
    int ret = pio_hal_sm_stop(block, sm);
    if (ret == 0) {
        printf("PIO%d SM%d: stopped\r\n", block, sm);
    } else {
        printf("PIO%d SM%d: stop failed (%d)\r\n", block, sm, ret);
    }
    return ret;
}

static int cmd_pio_exec(int argc, char *argv[]) {
    if (argc < 5) {
        printf("Usage: pio exec <block> <sm> <hex_instruction>\r\n");
        printf("  Example: pio exec 0 0 e001\r\n");
        return -1;
    }

    uint8_t block = (uint8_t)atoi(argv[2]);
    uint8_t sm = (uint8_t)atoi(argv[3]);
    uint16_t insn = (uint16_t)strtoul(argv[4], NULL, 16);

    int ret = pio_hal_sm_exec(block, sm, insn);
    if (ret == 0) {
        printf("PIO%d SM%d: executed 0x%04X\r\n", block, sm, insn);
    } else {
        printf("PIO%d SM%d: exec failed (%d)\r\n", block, sm, ret);
    }
    return ret;
}

static int cmd_pio_put(int argc, char *argv[]) {
    if (argc < 5) {
        printf("Usage: pio put <block> <sm> <value>\r\n");
        printf("  Value can be decimal or hex (0x prefix)\r\n");
        return -1;
    }

    uint8_t block = (uint8_t)atoi(argv[2]);
    uint8_t sm = (uint8_t)atoi(argv[3]);
    uint32_t value = (uint32_t)strtoul(argv[4], NULL, 0);

    int ret = pio_hal_sm_put(block, sm, value);
    if (ret == 0) {
        printf("PIO%d SM%d: put 0x%08lX (%lu)\r\n", block, sm,
               (unsigned long)value, (unsigned long)value);
    } else {
        printf("PIO%d SM%d: put failed (%d)\r\n", block, sm, ret);
    }
    return ret;
}

static int cmd_pio_get(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: pio get <block> <sm>\r\n");
        return -1;
    }

    uint8_t block = (uint8_t)atoi(argv[2]);
    uint8_t sm = (uint8_t)atoi(argv[3]);

    if (pio_hal_sm_rx_empty(block, sm)) {
        printf("PIO%d SM%d: RX FIFO empty\r\n", block, sm);
        return 0;
    }

    uint32_t data = 0;
    int ret = pio_hal_sm_get(block, sm, &data);
    if (ret == 0) {
        printf("PIO%d SM%d: got 0x%08lX (%lu)\r\n", block, sm,
               (unsigned long)data, (unsigned long)data);
    } else {
        printf("PIO%d SM%d: get failed (%d)\r\n", block, sm, ret);
    }
    return ret;
}

static int cmd_pio_blink(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: pio blink <pin> <freq_hz>\r\n");
        printf("  Example: pio blink 25 1.0   (blink LED at 1Hz)\r\n");
        return -1;
    }

    uint8_t pin = (uint8_t)atoi(argv[2]);
    float freq = (float)atof(argv[3]);

    /* Use PIO block 0, SM 0 by default for blink */
    int ret = pio_hal_blink_program(0, 0, pin, freq);
    if (ret == 0) {
        printf("Blink started on pin %d at %.2f Hz (PIO0 SM0)\r\n",
               pin, (double)freq);
    } else {
        printf("Blink failed: %d\r\n", ret);
    }
    return ret;
}

static int cmd_pio_ws2812(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: pio ws2812 <pin> <grb_hex>\r\n");
        printf("  Example: pio ws2812 16 00FF00   (green)\r\n");
        printf("  GRB format: GGRRBB\r\n");
        return -1;
    }

    uint8_t pin = (uint8_t)atoi(argv[2]);
    uint32_t grb = (uint32_t)strtoul(argv[3], NULL, 16);

    /* Use PIO block 1, SM 0 for WS2812 */
    int ret = pio_hal_ws2812_init(1, 0, pin);
    if (ret != 0) {
        printf("WS2812 init failed: %d\r\n", ret);
        return ret;
    }

    ret = pio_hal_ws2812_put(1, 0, grb);
    if (ret == 0) {
        printf("WS2812 pin %d: sent GRB 0x%06lX\r\n", pin, (unsigned long)grb);
    } else {
        printf("WS2812 send failed: %d\r\n", ret);
    }
    return ret;
}

static int cmd_pio_uart(int argc, char *argv[]) {
    if (argc < 5) {
        printf("Usage: pio uart <pin> <baud> <text>\r\n");
        printf("  Example: pio uart 4 9600 Hello\r\n");
        return -1;
    }

    uint8_t pin = (uint8_t)atoi(argv[2]);
    uint32_t baud = (uint32_t)atoi(argv[3]);

    /* Use PIO block 0, SM 1 for UART TX */
    int ret = pio_hal_uart_tx_init(0, 1, pin, baud);
    if (ret != 0) {
        printf("PIO UART init failed: %d\r\n", ret);
        return ret;
    }

    /* Concatenate remaining args as the message text */
    for (int i = 4; i < argc; i++) {
        const char *str = argv[i];
        for (int j = 0; str[j] != '\0'; j++) {
            ret = pio_hal_uart_tx_put(0, 1, (uint8_t)str[j]);
            if (ret != 0) {
                printf("PIO UART TX failed at byte %d: %d\r\n", j, ret);
                return ret;
            }
        }
        /* Add space between args (except after last) */
        if (i < argc - 1) {
            pio_hal_uart_tx_put(0, 1, (uint8_t)' ');
        }
    }

    /* Send CR+LF at end */
    pio_hal_uart_tx_put(0, 1, (uint8_t)'\r');
    pio_hal_uart_tx_put(0, 1, (uint8_t)'\n');

    printf("PIO UART TX: sent on pin %d at %lu baud\r\n", pin, (unsigned long)baud);
    return 0;
}

int cmd_pio(int argc, char *argv[]) {
    /* Initialize PIO on first use */
    pio_hal_init();

    if (argc < 2) {
        cmd_pio_usage();
        return 0;
    }

    const char *subcmd = argv[1];

    if (strcmp(subcmd, "status") == 0) {
        return cmd_pio_status();
    } else if (strcmp(subcmd, "load") == 0) {
        return cmd_pio_load(argc, argv);
    } else if (strcmp(subcmd, "unload") == 0) {
        return cmd_pio_unload(argc, argv);
    } else if (strcmp(subcmd, "config") == 0) {
        return cmd_pio_config(argc, argv);
    } else if (strcmp(subcmd, "start") == 0) {
        return cmd_pio_start(argc, argv);
    } else if (strcmp(subcmd, "stop") == 0) {
        return cmd_pio_stop(argc, argv);
    } else if (strcmp(subcmd, "exec") == 0) {
        return cmd_pio_exec(argc, argv);
    } else if (strcmp(subcmd, "put") == 0) {
        return cmd_pio_put(argc, argv);
    } else if (strcmp(subcmd, "get") == 0) {
        return cmd_pio_get(argc, argv);
    } else if (strcmp(subcmd, "blink") == 0) {
        return cmd_pio_blink(argc, argv);
    } else if (strcmp(subcmd, "ws2812") == 0) {
        return cmd_pio_ws2812(argc, argv);
    } else if (strcmp(subcmd, "uart") == 0) {
        return cmd_pio_uart(argc, argv);
    } else {
        printf("Unknown PIO command: %s\r\n", subcmd);
        cmd_pio_usage();
        return -1;
    }
}
