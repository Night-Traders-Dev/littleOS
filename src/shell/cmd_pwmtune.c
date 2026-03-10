/* cmd_pwmtune.c - Interactive PWM tuner for littleOS */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#endif

int cmd_pwmtune(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: pwmtune <command> [args...]\r\n");
        printf("Commands:\r\n");
        printf("  set PIN FREQ DUTY   - Set PWM (freq Hz, duty 0-100%%)\r\n");
        printf("  stop PIN            - Stop PWM on pin\r\n");
        printf("  status [PIN]        - Show PWM status\r\n");
        printf("  sweep PIN FREQ START END STEP_MS\r\n");
        printf("                      - Sweep duty cycle (start/end 0-100)\r\n");
        printf("  tone PIN FREQ       - Generate square wave (50%% duty)\r\n");
        printf("  servo PIN ANGLE     - Servo control (0-180 degrees)\r\n");
        return 0;
    }

#ifdef PICO_BUILD
    if (strcmp(argv[1], "set") == 0) {
        if (argc < 5) {
            printf("Usage: pwmtune set <pin> <freq_hz> <duty_percent>\r\n");
            return 1;
        }
        int pin = atoi(argv[2]);
        uint32_t freq = (uint32_t)atoi(argv[3]);
        int duty = atoi(argv[4]);

        if (pin < 0 || pin > 29) { printf("Invalid pin (0-29)\r\n"); return 1; }
        if (duty < 0 || duty > 100) { printf("Duty must be 0-100%%\r\n"); return 1; }
        if (freq == 0) { printf("Frequency must be > 0\r\n"); return 1; }

        gpio_set_function((uint)pin, GPIO_FUNC_PWM);
        uint slice = pwm_gpio_to_slice_num((uint)pin);
        uint channel = pwm_gpio_to_channel((uint)pin);

        uint32_t clock = clock_get_hz(clk_sys);
        uint32_t divider16 = clock / freq / 4096 + (clock % (freq * 4096) != 0);
        if (divider16 < 16) divider16 = 16;
        uint32_t wrap = clock * 16 / divider16 / freq - 1;
        if (wrap > 65535) wrap = 65535;

        pwm_set_clkdiv_int_frac(slice, (uint8_t)(divider16 / 16), (uint8_t)(divider16 & 0xF));
        pwm_set_wrap(slice, (uint16_t)wrap);
        pwm_set_chan_level(slice, channel, (uint16_t)(wrap * duty / 100));
        pwm_set_enabled(slice, true);

        uint32_t actual_freq = clock * 16 / divider16 / (wrap + 1);
        printf("PWM GPIO%d: freq=%lu Hz (actual %lu), duty=%d%%, wrap=%lu\r\n",
               pin, (unsigned long)freq, (unsigned long)actual_freq,
               duty, (unsigned long)wrap);
        return 0;
    }

    if (strcmp(argv[1], "stop") == 0) {
        if (argc < 3) { printf("Usage: pwmtune stop <pin>\r\n"); return 1; }
        int pin = atoi(argv[2]);
        if (pin < 0 || pin > 29) { printf("Invalid pin\r\n"); return 1; }

        uint slice = pwm_gpio_to_slice_num((uint)pin);
        pwm_set_enabled(slice, false);
        gpio_set_function((uint)pin, GPIO_FUNC_SIO);
        gpio_put((uint)pin, 0);
        printf("PWM stopped on GPIO%d\r\n", pin);
        return 0;
    }

    if (strcmp(argv[1], "status") == 0) {
        printf("=== PWM Status ===\r\n");
        printf("Slice  Enabled  Wrap     Div      CH_A  CH_B\r\n");
        for (uint s = 0; s < 8; s++) {
            /* Read slice config */
            uint16_t wrap = pwm_hw->slice[s].top;
            uint16_t cc_a = (uint16_t)(pwm_hw->slice[s].cc & 0xFFFF);
            uint16_t cc_b = (uint16_t)(pwm_hw->slice[s].cc >> 16);
            bool enabled = (pwm_hw->slice[s].csr & 1) != 0;
            uint32_t div = pwm_hw->slice[s].div;

            if (enabled || wrap > 0) {
                printf("  %u    %-7s  %-7u  %u.%u    %-5u %-5u\r\n",
                       s, enabled ? "YES" : "no",
                       wrap, (unsigned)(div >> 4), (unsigned)(div & 0xF),
                       cc_a, cc_b);
            }
        }
        return 0;
    }

    if (strcmp(argv[1], "tone") == 0) {
        if (argc < 4) { printf("Usage: pwmtune tone <pin> <freq_hz>\r\n"); return 1; }
        int pin = atoi(argv[2]);
        uint32_t freq = (uint32_t)atoi(argv[3]);

        if (pin < 0 || pin > 29) { printf("Invalid pin\r\n"); return 1; }

        gpio_set_function((uint)pin, GPIO_FUNC_PWM);
        uint slice = pwm_gpio_to_slice_num((uint)pin);
        uint channel = pwm_gpio_to_channel((uint)pin);

        uint32_t clock = clock_get_hz(clk_sys);
        uint32_t wrap = clock / freq - 1;
        uint32_t div = 1;
        while (wrap > 65535) { div++; wrap = clock / (freq * div) - 1; }

        pwm_set_clkdiv(slice, (float)div);
        pwm_set_wrap(slice, (uint16_t)wrap);
        pwm_set_chan_level(slice, channel, (uint16_t)(wrap / 2)); /* 50% duty */
        pwm_set_enabled(slice, true);

        printf("Tone: %lu Hz on GPIO%d (div=%lu, wrap=%lu)\r\n",
               (unsigned long)freq, pin, (unsigned long)div, (unsigned long)wrap);
        return 0;
    }

    if (strcmp(argv[1], "servo") == 0) {
        if (argc < 4) { printf("Usage: pwmtune servo <pin> <angle 0-180>\r\n"); return 1; }
        int pin = atoi(argv[2]);
        int angle = atoi(argv[3]);
        if (pin < 0 || pin > 29) { printf("Invalid pin\r\n"); return 1; }
        if (angle < 0) angle = 0;
        if (angle > 180) angle = 180;

        /* Servo: 50Hz, 1ms-2ms pulse (500us-2500us for extended range) */
        gpio_set_function((uint)pin, GPIO_FUNC_PWM);
        uint slice = pwm_gpio_to_slice_num((uint)pin);
        uint channel = pwm_gpio_to_channel((uint)pin);

        /* 125MHz / 125 / 20000 = 50Hz */
        pwm_set_clkdiv(slice, 125.0f);
        pwm_set_wrap(slice, 20000);

        /* Map 0-180 to 1000-2000 (1ms-2ms) */
        uint16_t pulse = (uint16_t)(1000 + (angle * 1000 / 180));
        pwm_set_chan_level(slice, channel, pulse);
        pwm_set_enabled(slice, true);

        printf("Servo GPIO%d: angle=%d° pulse=%u us\r\n", pin, angle, pulse);
        return 0;
    }

    if (strcmp(argv[1], "sweep") == 0) {
        if (argc < 7) {
            printf("Usage: pwmtune sweep <pin> <freq> <start%%> <end%%> <step_ms>\r\n");
            return 1;
        }
        int pin = atoi(argv[2]);
        uint32_t freq = (uint32_t)atoi(argv[3]);
        int start = atoi(argv[4]);
        int end = atoi(argv[5]);
        int step_ms = atoi(argv[6]);

        if (pin < 0 || pin > 29) { printf("Invalid pin\r\n"); return 1; }

        gpio_set_function((uint)pin, GPIO_FUNC_PWM);
        uint slice = pwm_gpio_to_slice_num((uint)pin);
        uint channel = pwm_gpio_to_channel((uint)pin);

        uint32_t clock = clock_get_hz(clk_sys);
        uint32_t divider16 = clock / freq / 4096 + 1;
        if (divider16 < 16) divider16 = 16;
        uint32_t wrap = clock * 16 / divider16 / freq - 1;
        if (wrap > 65535) wrap = 65535;

        pwm_set_clkdiv_int_frac(slice, (uint8_t)(divider16 / 16), (uint8_t)(divider16 & 0xF));
        pwm_set_wrap(slice, (uint16_t)wrap);
        pwm_set_enabled(slice, true);

        printf("Sweeping GPIO%d duty %d%%->%d%% at %lu Hz, %d ms/step\r\n",
               pin, start, end, (unsigned long)freq, step_ms);
        printf("Press any key to stop...\r\n");

        int dir = (start < end) ? 1 : -1;
        for (int d = start; d != end + dir; d += dir) {
            pwm_set_chan_level(slice, channel, (uint16_t)(wrap * d / 100));
            sleep_ms((uint32_t)step_ms);

            /* Check for keypress to abort */
            int c = getchar_timeout_us(0);
            if (c != PICO_ERROR_TIMEOUT) {
                printf("\r\nStopped at %d%%\r\n", d);
                return 0;
            }
        }
        printf("Sweep complete.\r\n");
        return 0;
    }
#else
    printf("PWM commands require hardware (PICO_BUILD)\r\n");
#endif

    return 1;
}
