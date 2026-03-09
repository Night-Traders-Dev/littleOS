/* pio.c - PIO (Programmable I/O) HAL Implementation for RP2040 */
#include "hal/pio.h"
#include "dmesg.h"
#include <stdio.h>
#include <string.h>

#ifdef PICO_BUILD
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#endif

/* RP2040 GPIO range */
#define GPIO_MIN_PIN    0
#define GPIO_MAX_PIN    29

/* Default system clock (125 MHz) used for non-PICO_BUILD calculations */
#define DEFAULT_SYS_CLK_HZ  125000000

/* Internal state tracking for each PIO block */
static pio_hal_program_t loaded_programs[PIO_NUM_BLOCKS];

/* Internal state tracking for each state machine */
static struct {
    bool     active;
    bool     configured;
    float    clkdiv;
    uint8_t  pin_base;
    uint8_t  pin_count;
    uint32_t tx_stalls;
    uint32_t rx_stalls;
} sm_state[PIO_NUM_BLOCKS][PIO_NUM_SM];

static bool pio_initialized = false;

/**
 * @brief Validate PIO block number
 */
static bool is_valid_block(uint8_t pio_block) {
    return (pio_block < PIO_NUM_BLOCKS);
}

/**
 * @brief Validate state machine number
 */
static bool is_valid_sm(uint8_t sm) {
    return (sm < PIO_NUM_SM);
}

/**
 * @brief Validate pin number
 */
static bool is_valid_pin(uint8_t pin) {
    return (pin >= GPIO_MIN_PIN && pin <= GPIO_MAX_PIN);
}

#ifdef PICO_BUILD
/**
 * @brief Get PIO instance pointer from block number
 */
static PIO get_pio_instance(uint8_t pio_block) {
    return (pio_block == 0) ? pio0 : pio1;
}
#endif

/**
 * @brief Initialize the PIO subsystem
 */
int pio_hal_init(void) {
    if (pio_initialized) {
        return 0;
    }

    memset(loaded_programs, 0, sizeof(loaded_programs));
    memset(sm_state, 0, sizeof(sm_state));

    pio_initialized = true;
    dmesg_info("PIO: subsystem initialized (%d blocks, %d SMs each)",
               PIO_NUM_BLOCKS, PIO_NUM_SM);
    return 0;
}

/**
 * @brief Load a PIO program into instruction memory
 */
int pio_hal_load_program(uint8_t pio_block, const uint16_t *instructions,
                         uint8_t length, uint8_t origin) {
    if (!pio_initialized) {
        pio_hal_init();
    }
    if (!is_valid_block(pio_block)) {
        dmesg_err("PIO: invalid block %d", pio_block);
        return -1;
    }
    if (!instructions || length == 0 || length > PIO_MAX_PROGRAM_LEN) {
        dmesg_err("PIO: invalid program (len=%d)", length);
        return -2;
    }
    if (loaded_programs[pio_block].loaded) {
        dmesg_err("PIO%d: program already loaded, unload first", pio_block);
        return -3;
    }

    pio_hal_program_t *prog = &loaded_programs[pio_block];
    memcpy(prog->instructions, instructions, length * sizeof(uint16_t));
    prog->length = length;
    prog->origin = origin;
    prog->wrap_target = 0;
    prog->wrap = length - 1;

#ifdef PICO_BUILD
    PIO pio = get_pio_instance(pio_block);
    struct pio_program sdk_prog = {
        .instructions = prog->instructions,
        .length = length,
        .origin = (origin == 0xFF) ? -1 : (int8_t)origin,
    };
    if (!pio_can_add_program(pio, &sdk_prog)) {
        dmesg_err("PIO%d: no space for %d-instruction program", pio_block, length);
        return -4;
    }
    uint offset = pio_add_program(pio, &sdk_prog);
    prog->origin = (uint8_t)offset;
#endif

    prog->loaded = true;
    dmesg_info("PIO%d: loaded %d-instruction program at offset %d",
               pio_block, length, prog->origin);
    return 0;
}

/**
 * @brief Unload a program from a PIO block
 */
int pio_hal_unload_program(uint8_t pio_block) {
    if (!is_valid_block(pio_block)) {
        return -1;
    }
    if (!loaded_programs[pio_block].loaded) {
        return 0;
    }

    /* Stop all state machines using this block first */
    for (int sm = 0; sm < PIO_NUM_SM; sm++) {
        if (sm_state[pio_block][sm].active) {
            pio_hal_sm_stop(pio_block, (uint8_t)sm);
        }
    }

#ifdef PICO_BUILD
    PIO pio = get_pio_instance(pio_block);
    pio_hal_program_t *prog = &loaded_programs[pio_block];
    struct pio_program sdk_prog = {
        .instructions = prog->instructions,
        .length = prog->length,
        .origin = (int8_t)prog->origin,
    };
    pio_remove_program(pio, &sdk_prog, prog->origin);
#endif

    memset(&loaded_programs[pio_block], 0, sizeof(pio_hal_program_t));
    dmesg_info("PIO%d: program unloaded", pio_block);
    return 0;
}

/**
 * @brief Configure a state machine
 */
int pio_hal_sm_config(uint8_t pio_block, uint8_t sm, const pio_hal_sm_config_t *cfg) {
    if (!is_valid_block(pio_block) || !is_valid_sm(sm) || !cfg) {
        return -1;
    }
    if (!loaded_programs[pio_block].loaded) {
        dmesg_err("PIO%d SM%d: no program loaded", pio_block, sm);
        return -2;
    }

    sm_state[pio_block][sm].clkdiv = cfg->clkdiv;
    sm_state[pio_block][sm].pin_base = cfg->pin_base;
    sm_state[pio_block][sm].pin_count = cfg->pin_count;
    sm_state[pio_block][sm].configured = true;

#ifdef PICO_BUILD
    PIO pio = get_pio_instance(pio_block);
    uint offset = loaded_programs[pio_block].origin;

    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + loaded_programs[pio_block].wrap_target,
                          offset + loaded_programs[pio_block].wrap);
    sm_config_set_clkdiv(&c, cfg->clkdiv);

    if (cfg->pin_count > 0) {
        sm_config_set_set_pins(&c, cfg->pin_base, cfg->pin_count);
        sm_config_set_out_pins(&c, cfg->pin_base, cfg->pin_count);
        for (uint8_t p = cfg->pin_base; p < cfg->pin_base + cfg->pin_count; p++) {
            pio_gpio_init(pio, p);
        }
    }

    if (cfg->sideset_pins > 0) {
        sm_config_set_sideset_pins(&c, cfg->pin_base);
        sm_config_set_sideset(&c, cfg->sideset_pins, false, false);
    }

    sm_config_set_out_shift(&c, cfg->shift_right, cfg->autopull, cfg->pull_threshold);
    sm_config_set_in_shift(&c, cfg->in_shift_right, cfg->autopush, cfg->push_threshold);

    pio_sm_init(pio, sm, offset, &c);
#endif

    dmesg_info("PIO%d SM%d: configured (clkdiv=%.2f, pin=%d, count=%d)",
               pio_block, sm, (double)cfg->clkdiv, cfg->pin_base, cfg->pin_count);
    return 0;
}

/**
 * @brief Start a state machine
 */
int pio_hal_sm_start(uint8_t pio_block, uint8_t sm) {
    if (!is_valid_block(pio_block) || !is_valid_sm(sm)) {
        return -1;
    }
    if (!sm_state[pio_block][sm].configured) {
        dmesg_err("PIO%d SM%d: not configured", pio_block, sm);
        return -2;
    }

#ifdef PICO_BUILD
    PIO pio = get_pio_instance(pio_block);
    pio_sm_set_enabled(pio, sm, true);
#endif

    sm_state[pio_block][sm].active = true;
    dmesg_info("PIO%d SM%d: started", pio_block, sm);
    return 0;
}

/**
 * @brief Stop a state machine
 */
int pio_hal_sm_stop(uint8_t pio_block, uint8_t sm) {
    if (!is_valid_block(pio_block) || !is_valid_sm(sm)) {
        return -1;
    }

#ifdef PICO_BUILD
    PIO pio = get_pio_instance(pio_block);
    pio_sm_set_enabled(pio, sm, false);
#endif

    sm_state[pio_block][sm].active = false;
    dmesg_info("PIO%d SM%d: stopped", pio_block, sm);
    return 0;
}

/**
 * @brief Restart a state machine (reset PC to wrap_target)
 */
int pio_hal_sm_restart(uint8_t pio_block, uint8_t sm) {
    if (!is_valid_block(pio_block) || !is_valid_sm(sm)) {
        return -1;
    }

#ifdef PICO_BUILD
    PIO pio = get_pio_instance(pio_block);
    pio_sm_restart(pio, sm);
    /* Jump back to program start */
    uint offset = loaded_programs[pio_block].origin;
    pio_sm_exec(pio, sm, pio_encode_jmp(offset + loaded_programs[pio_block].wrap_target));
#endif

    dmesg_info("PIO%d SM%d: restarted", pio_block, sm);
    return 0;
}

/**
 * @brief Write a 32-bit value to the TX FIFO (blocking)
 */
int pio_hal_sm_put(uint8_t pio_block, uint8_t sm, uint32_t data) {
    if (!is_valid_block(pio_block) || !is_valid_sm(sm)) {
        return -1;
    }

#ifdef PICO_BUILD
    PIO pio = get_pio_instance(pio_block);
    pio_sm_put_blocking(pio, sm, data);
#else
    (void)data;
#endif

    return 0;
}

/**
 * @brief Read a 32-bit value from the RX FIFO (blocking)
 */
int pio_hal_sm_get(uint8_t pio_block, uint8_t sm, uint32_t *data) {
    if (!is_valid_block(pio_block) || !is_valid_sm(sm) || !data) {
        return -1;
    }

#ifdef PICO_BUILD
    PIO pio = get_pio_instance(pio_block);
    *data = pio_sm_get_blocking(pio, sm);
#else
    *data = 0;
#endif

    return 0;
}

/**
 * @brief Check if TX FIFO is empty
 */
bool pio_hal_sm_tx_empty(uint8_t pio_block, uint8_t sm) {
    if (!is_valid_block(pio_block) || !is_valid_sm(sm)) {
        return true;
    }

#ifdef PICO_BUILD
    PIO pio = get_pio_instance(pio_block);
    return pio_sm_is_tx_fifo_empty(pio, sm);
#else
    return true;
#endif
}

/**
 * @brief Check if RX FIFO is empty
 */
bool pio_hal_sm_rx_empty(uint8_t pio_block, uint8_t sm) {
    if (!is_valid_block(pio_block) || !is_valid_sm(sm)) {
        return true;
    }

#ifdef PICO_BUILD
    PIO pio = get_pio_instance(pio_block);
    return pio_sm_is_rx_fifo_empty(pio, sm);
#else
    return true;
#endif
}

/**
 * @brief Execute a single instruction on a state machine
 */
int pio_hal_sm_exec(uint8_t pio_block, uint8_t sm, uint16_t instruction) {
    if (!is_valid_block(pio_block) || !is_valid_sm(sm)) {
        return -1;
    }

#ifdef PICO_BUILD
    PIO pio = get_pio_instance(pio_block);
    pio_sm_exec(pio, sm, instruction);
#else
    (void)instruction;
#endif

    return 0;
}

/**
 * @brief Get the status of a state machine
 */
int pio_hal_sm_status(uint8_t pio_block, uint8_t sm, pio_sm_status_t *status) {
    if (!is_valid_block(pio_block) || !is_valid_sm(sm) || !status) {
        return -1;
    }

    memset(status, 0, sizeof(pio_sm_status_t));
    status->active = sm_state[pio_block][sm].active;
    status->program_loaded = loaded_programs[pio_block].loaded;
    status->program_offset = loaded_programs[pio_block].origin;
    status->program_length = loaded_programs[pio_block].length;
    status->clkdiv = sm_state[pio_block][sm].clkdiv;
    status->pin_base = sm_state[pio_block][sm].pin_base;
    status->pin_count = sm_state[pio_block][sm].pin_count;
    status->tx_stalls = sm_state[pio_block][sm].tx_stalls;
    status->rx_stalls = sm_state[pio_block][sm].rx_stalls;

    return 0;
}

/**
 * @brief Set pin direction for a PIO-controlled pin
 */
int pio_hal_set_pin_dir(uint8_t pio_block, uint8_t sm, uint8_t pin, bool output) {
    if (!is_valid_block(pio_block) || !is_valid_sm(sm) || !is_valid_pin(pin)) {
        return -1;
    }

#ifdef PICO_BUILD
    PIO pio = get_pio_instance(pio_block);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, output);
#else
    (void)pin;
    (void)output;
#endif

    return 0;
}

/* =========================================================================
 * Built-in PIO Programs
 * ========================================================================= */

/**
 * @brief Built-in blink program: toggles a pin at the given frequency
 *
 * PIO program (2 instructions):
 *   0: set pins, 1   [delay]   ; set pin high, delay
 *   1: set pins, 0   [delay]   ; set pin low, delay
 *
 * The delay is calculated to achieve the desired blink frequency.
 * Each half-period uses one instruction with up to 31 delay cycles,
 * plus the clock divider to reach lower frequencies.
 */
int pio_hal_blink_program(uint8_t pio_block, uint8_t sm, uint8_t pin, float freq_hz) {
    if (!is_valid_block(pio_block) || !is_valid_sm(sm) || !is_valid_pin(pin)) {
        return -1;
    }
    if (freq_hz <= 0.0f) {
        dmesg_err("PIO: blink frequency must be positive");
        return -2;
    }

    /* Unload any existing program on this block */
    if (loaded_programs[pio_block].loaded) {
        pio_hal_unload_program(pio_block);
    }

    /*
     * Calculate clock divider and delay cycles.
     * Each full blink period = 2 instructions * (1 + delay) cycles each.
     * cycles_per_half = sys_clk / (clkdiv * freq_hz * 2)
     * We want cycles_per_half = 1 + delay, where delay <= 31.
     * So we pick delay = 31 (max) and adjust clkdiv.
     */
    uint8_t delay = 31;
    uint32_t sys_clk = DEFAULT_SYS_CLK_HZ;
#ifdef PICO_BUILD
    sys_clk = clock_get_hz(clk_sys);
#endif
    /* clkdiv = sys_clk / (freq_hz * 2 * (1 + delay)) */
    float clkdiv = (float)sys_clk / (freq_hz * 2.0f * (1.0f + (float)delay));
    if (clkdiv < 1.0f) {
        clkdiv = 1.0f;
        /* Recalculate delay for high frequencies */
        float cycles_per_half = (float)sys_clk / (freq_hz * 2.0f);
        delay = (uint8_t)(cycles_per_half - 1.0f);
        if (delay > 31) delay = 31;
    }
    if (clkdiv > 65535.0f) {
        clkdiv = 65535.0f;
    }

    /*
     * Construct PIO blink instructions inline.
     * PIO instruction encoding for "set pins, value [delay]":
     *   Bits [15:13] = 111 (SET instruction)
     *   Bits [12:8]  = delay/sideset
     *   Bits [7:5]   = 000 (set destination = pins)
     *   Bits [4:0]   = value
     */
#ifdef PICO_BUILD
    uint16_t insn_high = pio_encode_set(pio_pins, 1) | pio_encode_delay(delay);
    uint16_t insn_low  = pio_encode_set(pio_pins, 0) | pio_encode_delay(delay);
#else
    /* SET pins = 1 with delay: 111_DDDDD_000_00001 */
    uint16_t insn_high = (uint16_t)(0xE001 | ((uint16_t)delay << 8));
    /* SET pins = 0 with delay: 111_DDDDD_000_00000 */
    uint16_t insn_low  = (uint16_t)(0xE000 | ((uint16_t)delay << 8));
#endif

    uint16_t blink_prog[2] = { insn_high, insn_low };

    int ret = pio_hal_load_program(pio_block, blink_prog, 2, 0xFF);
    if (ret != 0) {
        return ret;
    }
    loaded_programs[pio_block].wrap_target = 0;
    loaded_programs[pio_block].wrap = 1;

    pio_hal_sm_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.pio_block = pio_block;
    cfg.sm = sm;
    cfg.clkdiv = clkdiv;
    cfg.pin_base = pin;
    cfg.pin_count = 1;
    cfg.sideset_pins = 0;
    cfg.autopull = false;
    cfg.autopush = false;
    cfg.pull_threshold = 32;
    cfg.push_threshold = 32;
    cfg.shift_right = true;
    cfg.in_shift_right = true;

    ret = pio_hal_sm_config(pio_block, sm, &cfg);
    if (ret != 0) {
        return ret;
    }

    pio_hal_set_pin_dir(pio_block, sm, pin, true);

    ret = pio_hal_sm_start(pio_block, sm);
    if (ret != 0) {
        return ret;
    }

    dmesg_info("PIO%d SM%d: blink on pin %d at %.2f Hz (clkdiv=%.2f, delay=%d)",
               pio_block, sm, pin, (double)freq_hz, (double)clkdiv, delay);
    return 0;
}

/**
 * @brief Built-in WS2812 (NeoPixel) PIO driver
 *
 * Standard 800kHz WS2812 PIO program.
 * Protocol: each bit is 1.25us total.
 *   '1' bit: high 0.8us, low 0.45us   (T1H=0.8us, T1L=0.45us)
 *   '0' bit: high 0.4us, low 0.85us   (T0H=0.4us, T0L=0.85us)
 *
 * PIO program (using side-set for the data pin):
 *   .side_set 1
 *   .wrap_target
 *   bitloop:
 *       out x, 1        side 0 [T3-1]  ; shift 1 bit, drive low
 *       jmp !x do_zero  side 1 [T1-1]  ; drive high, branch on bit value
 *   do_one:
 *       jmp bitloop      side 1 [T2-1]  ; stay high for '1' bit
 *   do_zero:
 *       nop              side 0 [T2-1]  ; drive low for '0' bit
 *   .wrap
 *
 * Timing at 800kHz with 125MHz sys clock:
 *   T1 = 2 cycles (0.32us high start)
 *   T2 = 5 cycles (0.8us)
 *   T3 = 3 cycles (0.48us)
 *   Total = 10 cycles per bit = 1.6us at 125/10 = 12.5MHz
 *   But we actually run at 800kHz * 10 cycles = 8MHz -> clkdiv = 125/8 ~= 15.625
 *
 * Simplified encoding (manually constructed):
 */
int pio_hal_ws2812_init(uint8_t pio_block, uint8_t sm, uint8_t pin) {
    if (!is_valid_block(pio_block) || !is_valid_sm(sm) || !is_valid_pin(pin)) {
        return -1;
    }

    if (loaded_programs[pio_block].loaded) {
        pio_hal_unload_program(pio_block);
    }

    /*
     * WS2812 PIO program (4 instructions, 1 side-set bit):
     *
     * The encoding uses side-set on the output pin.
     * With 1 side-set pin, instruction format:
     *   Bits [15:13] = opcode
     *   Bit  [12]    = side-set value
     *   Bits [11:8]  = delay (4 bits, since 1 bit used for side-set)
     *   Bits [7:0]   = operand
     *
     * Instructions (T1=2, T2=5, T3=3):
     *   0: out x, 1        side 0 [T3-1=2]  ; 0x6021 -> out x,1 side=0 delay=2
     *   1: jmp !x, 3       side 1 [T1-1=1]  ; 0x1123 -> jmp !x,3 side=1 delay=1
     *   2: jmp 0            side 1 [T2-1=4]  ; 0x1400 -> jmp 0 side=1 delay=4
     *   3: nop              side 0 [T2-1=4]  ; 0xA442 -> nop side=0 delay=4
     */

#ifdef PICO_BUILD
    /* Use SDK encoding for correctness */
    uint16_t ws2812_prog[4];
    /* out x, 1        side 0 [2] */
    ws2812_prog[0] = pio_encode_out(pio_x, 1)   | pio_encode_sideset(1, 0) | pio_encode_delay(2);
    /* jmp !x, 3       side 1 [1] */
    ws2812_prog[1] = pio_encode_jmp_not_x(3)     | pio_encode_sideset(1, 1) | pio_encode_delay(1);
    /* jmp 0           side 1 [4] */
    ws2812_prog[2] = pio_encode_jmp(0)            | pio_encode_sideset(1, 1) | pio_encode_delay(4);
    /* nop             side 0 [4] */
    ws2812_prog[3] = pio_encode_nop()             | pio_encode_sideset(1, 0) | pio_encode_delay(4);
#else
    /* Manual encoding for non-PICO builds */
    uint16_t ws2812_prog[4] = {
        0x6221,  /* out x, 1        side 0 [2] */
        0x1123,  /* jmp !x, 3       side 1 [1] */
        0x1400,  /* jmp 0           side 1 [4] */
        0xA442,  /* nop             side 0 [4] */
    };
#endif

    int ret = pio_hal_load_program(pio_block, ws2812_prog, 4, 0xFF);
    if (ret != 0) {
        return ret;
    }
    loaded_programs[pio_block].wrap_target = 0;
    loaded_programs[pio_block].wrap = 3;

    /* 800kHz * 10 cycles per bit = 8MHz PIO clock
     * clkdiv = sys_clk / 8000000 */
    uint32_t sys_clk = DEFAULT_SYS_CLK_HZ;
#ifdef PICO_BUILD
    sys_clk = clock_get_hz(clk_sys);
#endif
    float clkdiv = (float)sys_clk / 8000000.0f;

    pio_hal_sm_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.pio_block = pio_block;
    cfg.sm = sm;
    cfg.clkdiv = clkdiv;
    cfg.pin_base = pin;
    cfg.pin_count = 1;
    cfg.sideset_pins = 1;
    cfg.autopull = true;
    cfg.autopush = false;
    cfg.pull_threshold = 24;  /* 24 bits per pixel (GRB) */
    cfg.push_threshold = 32;
    cfg.shift_right = false;  /* Shift left (MSB first) */
    cfg.in_shift_right = false;

    ret = pio_hal_sm_config(pio_block, sm, &cfg);
    if (ret != 0) {
        return ret;
    }

    pio_hal_set_pin_dir(pio_block, sm, pin, true);

    ret = pio_hal_sm_start(pio_block, sm);
    if (ret != 0) {
        return ret;
    }

    dmesg_info("PIO%d SM%d: WS2812 initialized on pin %d (clkdiv=%.2f)",
               pio_block, sm, pin, (double)clkdiv);
    return 0;
}

/**
 * @brief Send a GRB color value to WS2812
 */
int pio_hal_ws2812_put(uint8_t pio_block, uint8_t sm, uint32_t grb) {
    if (!is_valid_block(pio_block) || !is_valid_sm(sm)) {
        return -1;
    }

    /* WS2812 expects data left-justified in 32-bit word (top 24 bits) */
    return pio_hal_sm_put(pio_block, sm, grb << 8);
}

/**
 * @brief Built-in PIO UART TX program
 *
 * Standard PIO UART TX: shift out start bit, 8 data bits, stop bit.
 *
 * PIO program (using side-set for TX pin):
 *   .side_set 1 opt
 *
 *       pull       side 1 [7]  ; wait for data, idle high
 *       set x, 7   side 0 [7]  ; start bit (low), set bit counter
 *   bitloop:
 *       out pins, 1            [6]  ; shift out 1 bit
 *       jmp x--, bitloop       [6]  ; loop 8 times
 *       pull  ifempty  side 1  [6]  ; stop bit (high), try to pull next
 *
 * At the desired baud rate, each bit period = sys_clk / (clkdiv * (1+delay))
 * We use 8 cycles per bit: clkdiv = sys_clk / (baud * 8)
 */
int pio_hal_uart_tx_init(uint8_t pio_block, uint8_t sm, uint8_t pin, uint32_t baud) {
    if (!is_valid_block(pio_block) || !is_valid_sm(sm) || !is_valid_pin(pin)) {
        return -1;
    }
    if (baud == 0) {
        dmesg_err("PIO: UART baud rate must be non-zero");
        return -2;
    }

    if (loaded_programs[pio_block].loaded) {
        pio_hal_unload_program(pio_block);
    }

    /*
     * UART TX PIO program (5 instructions):
     *
     * Uses side-set on the TX pin for start/stop bits, and out pins for data bits.
     * 8 cycles per bit period.
     *
     *   0: pull block       side 1 [7]  ; stall until data, TX idle high
     *   1: set x, 7         side 0 [7]  ; assert start bit, init bit counter
     *   2: out pins, 1                [6]  ; shift out data bit
     *   3: jmp x--, 2                 [6]  ; loop for 8 data bits
     *   4: nop              side 1 [6]  ; stop bit (high)
     *
     * With optional side-set (1 bit + opt), encoding:
     *   Bit [12]    = side-set enable
     *   Bit [11]    = side-set value
     *   Bits [10:8] = delay (3 bits available with 1 opt side-set)
     *
     * Actually, for simplicity, use the standard non-sideset approach:
     * Drive pin via SET/OUT, using out pins for all bit output.
     *
     * Simplified UART TX (no side-set):
     *   0: pull block                    ; wait for TX data
     *   1: set pins, 0         [7]       ; start bit (low)
     *   2: out pins, 1         [6]       ; data bit (8x via loop)
     *   3: jmp x--, 2         [6]       ; decrement bit counter
     *   4: set pins, 1         [7]       ; stop bit (high)
     *
     * But we need to set x=7 before the bit loop. Revised:
     *   0: pull block                    ; wait for TX data
     *   1: set x, 7                     ; 8 data bits
     *   2: set pins, 0         [7]       ; start bit
     *   3: out pins, 1         [6]       ; shift out 1 data bit
     *   4: jmp x--, 3         [6]       ; loop 8 bits
     *   5: set pins, 1         [6]       ; stop bit
     *   (wraps back to 0)
     *
     * 8 cycles per bit: instruction + delay cycles
     */

#ifdef PICO_BUILD
    uint16_t uart_prog[6];
    uart_prog[0] = pio_encode_pull(false, true);                           /* pull block */
    uart_prog[1] = pio_encode_set(pio_x, 7);                          /* set x, 7 */
    uart_prog[2] = pio_encode_set(pio_pins, 0) | pio_encode_delay(7); /* start bit [7] */
    uart_prog[3] = pio_encode_out(pio_pins, 1)     | pio_encode_delay(6); /* out pins, 1 [6] */
    uart_prog[4] = pio_encode_jmp_x_dec(3)         | pio_encode_delay(6); /* jmp x--, 3 [6] */
    uart_prog[5] = pio_encode_set(pio_pins, 1) | pio_encode_delay(6); /* stop bit [6] */
#else
    /* Manual encoding for non-PICO builds */
    uint16_t uart_prog[6] = {
        0x8080,  /* pull block (ifempty=0, block=1) */
        0xE027,  /* set x, 7 */
        0xE700,  /* set pins, 0 [7] */
        0x6601,  /* out pins, 1 [6] */
        0x0643,  /* jmp x--, 3 [6] */
        0xE601,  /* set pins, 1 [6] */
    };
#endif

    int ret = pio_hal_load_program(pio_block, uart_prog, 6, 0xFF);
    if (ret != 0) {
        return ret;
    }
    loaded_programs[pio_block].wrap_target = 0;
    loaded_programs[pio_block].wrap = 5;

    /* 8 cycles per bit: clkdiv = sys_clk / (baud * 8) */
    uint32_t sys_clk = DEFAULT_SYS_CLK_HZ;
#ifdef PICO_BUILD
    sys_clk = clock_get_hz(clk_sys);
#endif
    float clkdiv = (float)sys_clk / ((float)baud * 8.0f);
    if (clkdiv < 1.0f) clkdiv = 1.0f;

    pio_hal_sm_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.pio_block = pio_block;
    cfg.sm = sm;
    cfg.clkdiv = clkdiv;
    cfg.pin_base = pin;
    cfg.pin_count = 1;
    cfg.sideset_pins = 0;
    cfg.autopull = false;
    cfg.autopush = false;
    cfg.pull_threshold = 32;
    cfg.push_threshold = 32;
    cfg.shift_right = true;  /* UART is LSB first */
    cfg.in_shift_right = true;

    ret = pio_hal_sm_config(pio_block, sm, &cfg);
    if (ret != 0) {
        return ret;
    }

    /* Set pin high initially (UART idle) */
    pio_hal_set_pin_dir(pio_block, sm, pin, true);

#ifdef PICO_BUILD
    /* Set initial pin state to high (idle) */
    PIO pio = get_pio_instance(pio_block);
    pio_sm_set_pins(pio, sm, 1);
#endif

    ret = pio_hal_sm_start(pio_block, sm);
    if (ret != 0) {
        return ret;
    }

    dmesg_info("PIO%d SM%d: UART TX on pin %d at %lu baud (clkdiv=%.2f)",
               pio_block, sm, pin, (unsigned long)baud, (double)clkdiv);
    return 0;
}

/**
 * @brief Send a single byte via PIO UART TX
 */
int pio_hal_uart_tx_put(uint8_t pio_block, uint8_t sm, uint8_t byte) {
    if (!is_valid_block(pio_block) || !is_valid_sm(sm)) {
        return -1;
    }

    /* Put byte into TX FIFO - the PIO program handles framing */
    return pio_hal_sm_put(pio_block, sm, (uint32_t)byte);
}
