/* cmd_adcstream.c - Continuous ADC sampling for littleOS */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#include "hardware/adc.h"
#endif

int cmd_adcstream(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: adc <command> [args...]\r\n");
        printf("Commands:\r\n");
        printf("  read CH             - Single ADC read (ch 0-4)\r\n");
        printf("  stream CH [RATE]    - Stream readings (rate in ms, default 100)\r\n");
        printf("  all                 - Read all ADC channels once\r\n");
        printf("  temp                - Read internal temperature\r\n");
        printf("  cal                 - Show ADC calibration info\r\n");
        printf("  stats CH COUNT      - Sample N readings, show min/max/avg\r\n");
        printf("\r\nChannels: 0-3 = GPIO26-29, 4 = internal temp\r\n");
        return 0;
    }

#ifdef PICO_BUILD
    adc_init();

    if (strcmp(argv[1], "read") == 0) {
        if (argc < 3) { printf("Usage: adc read <channel 0-4>\r\n"); return 1; }
        int ch = atoi(argv[2]);
        if (ch < 0 || ch > 4) { printf("Channel must be 0-4\r\n"); return 1; }

        if (ch == 4) adc_set_temp_sensor_enabled(true);
        else gpio_init((uint)(26 + ch));  /* Initialize ADC pin */

        adc_select_input((uint)ch);
        uint16_t raw = adc_read();
        float voltage = raw * 3.3f / 4096.0f;

        if (ch == 4) {
            float temp = 27.0f - (voltage - 0.706f) / (-0.001721f);
            printf("ADC ch%d: raw=%u voltage=%.3fV temp=%.1f°C\r\n", ch, raw, voltage, temp);
        } else {
            printf("ADC ch%d (GPIO%d): raw=%u voltage=%.3fV\r\n", ch, 26 + ch, raw, voltage);
        }
        return 0;
    }

    if (strcmp(argv[1], "stream") == 0) {
        if (argc < 3) { printf("Usage: adc stream <channel> [rate_ms]\r\n"); return 1; }
        int ch = atoi(argv[2]);
        int rate_ms = (argc >= 4) ? atoi(argv[3]) : 100;
        if (ch < 0 || ch > 4) { printf("Channel must be 0-4\r\n"); return 1; }
        if (rate_ms < 1) rate_ms = 1;

        if (ch == 4) adc_set_temp_sensor_enabled(true);
        adc_select_input((uint)ch);

        printf("Streaming ADC ch%d every %d ms. Press any key to stop.\r\n\r\n", ch, rate_ms);
        printf("Time(ms)   Raw    Voltage  ");
        if (ch == 4) printf("Temp(°C)");
        printf("\r\n");
        printf("--------   -----  -------  ");
        if (ch == 4) printf("--------");
        printf("\r\n");

        uint32_t start = to_ms_since_boot(get_absolute_time());
        int count = 0;

        while (1) {
            uint16_t raw = adc_read();
            float voltage = raw * 3.3f / 4096.0f;
            uint32_t elapsed = to_ms_since_boot(get_absolute_time()) - start;

            printf("%-10lu %-6u %.3f    ", (unsigned long)elapsed, raw, voltage);
            if (ch == 4) {
                float temp = 27.0f - (voltage - 0.706f) / (-0.001721f);
                printf("%.1f", temp);
            }
            printf("\r\n");
            count++;

            sleep_ms((uint32_t)rate_ms);
            int c = getchar_timeout_us(0);
            if (c != PICO_ERROR_TIMEOUT) break;
        }

        printf("\r\n%d samples captured.\r\n", count);
        return 0;
    }

    if (strcmp(argv[1], "all") == 0) {
        printf("=== All ADC Channels ===\r\n");
        for (int ch = 0; ch <= 4; ch++) {
            if (ch == 4) adc_set_temp_sensor_enabled(true);
            adc_select_input((uint)ch);
            uint16_t raw = adc_read();
            float voltage = raw * 3.3f / 4096.0f;

            if (ch == 4) {
                float temp = 27.0f - (voltage - 0.706f) / (-0.001721f);
                printf("  CH%d (temp):   raw=%-5u  %.3fV  %.1f°C\r\n", ch, raw, voltage, temp);
            } else {
                printf("  CH%d (GPIO%d): raw=%-5u  %.3fV\r\n", ch, 26 + ch, raw, voltage);
            }
        }
        return 0;
    }

    if (strcmp(argv[1], "temp") == 0) {
        adc_set_temp_sensor_enabled(true);
        adc_select_input(4);
        uint16_t raw = adc_read();
        float voltage = raw * 3.3f / 4096.0f;
        float temp = 27.0f - (voltage - 0.706f) / (-0.001721f);
        printf("Temperature: %.1f°C (raw=%u, %.3fV)\r\n", temp, raw, voltage);
        return 0;
    }

    if (strcmp(argv[1], "cal") == 0) {
        printf("=== ADC Calibration ===\r\n");
        printf("Resolution: 12-bit (0-4095)\r\n");
        printf("Reference:  3.3V\r\n");
        printf("LSB:        %.4f V\r\n", 3.3f / 4096.0f);
        printf("Temp sensor: V = 0.706 - 0.001721 * (T - 27)\r\n");
        printf("Channels:   0-3 = GPIO26-29, 4 = internal temp\r\n");
        return 0;
    }

    if (strcmp(argv[1], "stats") == 0) {
        if (argc < 4) { printf("Usage: adc stats <channel> <count>\r\n"); return 1; }
        int ch = atoi(argv[2]);
        int count = atoi(argv[3]);
        if (ch < 0 || ch > 4) { printf("Channel must be 0-4\r\n"); return 1; }
        if (count < 1) count = 1;
        if (count > 10000) count = 10000;

        if (ch == 4) adc_set_temp_sensor_enabled(true);
        adc_select_input((uint)ch);

        uint32_t min_val = 4095, max_val = 0;
        uint64_t sum = 0;

        uint32_t start = time_us_32();
        for (int i = 0; i < count; i++) {
            uint16_t raw = adc_read();
            if (raw < min_val) min_val = raw;
            if (raw > max_val) max_val = raw;
            sum += raw;
        }
        uint32_t elapsed = time_us_32() - start;

        float avg = (float)sum / count;
        float avg_v = avg * 3.3f / 4096.0f;
        float min_v = min_val * 3.3f / 4096.0f;
        float max_v = max_val * 3.3f / 4096.0f;

        printf("ADC ch%d: %d samples in %lu us\r\n", ch, count, (unsigned long)elapsed);
        printf("  Min:  %lu (%.3fV)\r\n", (unsigned long)min_val, min_v);
        printf("  Max:  %lu (%.3fV)\r\n", (unsigned long)max_val, max_v);
        printf("  Avg:  %.1f (%.3fV)\r\n", avg, avg_v);
        printf("  Range: %lu LSB (%.3fV)\r\n",
               (unsigned long)(max_val - min_val), max_v - min_v);
        printf("  Rate: %lu samples/sec\r\n",
               (unsigned long)(elapsed > 0 ? (uint64_t)count * 1000000 / elapsed : 0));
        return 0;
    }
#else
    printf("ADC commands require hardware (PICO_BUILD)\r\n");
#endif

    return 1;
}
