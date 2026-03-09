/* cmd_sensor.c - Shell commands for the sensor framework */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sensor.h"

/* ---------- default alert callback for shell ---------- */

static void shell_alert_cb(uint8_t sensor_id, sensor_reading_t *reading, void *user_data)
{
    (void)user_data;
    const char *label = "alert";
    sensor_descriptor_t info;
    if (sensor_get_info(sensor_id, &info) == 0)
        label = info.name;

    printf("[ALERT] sensor %d ('%s'): ", sensor_id, label);
    if (reading->type == SENSOR_DATA_FLOAT)
        printf("%.4f", (double)reading->value.f_val);
    else if (reading->type == SENSOR_DATA_INT)
        printf("%ld", (long)reading->value.i_val);
    else
        printf("raw 0x%02X%02X", reading->value.raw[0], reading->value.raw[1]);
    printf("\r\n");
}

/* ---------- usage ---------- */

static void cmd_sensor_usage(void)
{
    printf("Sensor framework commands:\r\n");
    printf("  sensor list                                    - List all sensors\r\n");
    printf("  sensor add <name> adc|i2c|spi <bus> [addr] [reg] [len] <interval_ms>\r\n");
    printf("                                                 - Register a sensor\r\n");
    printf("  sensor remove <id>                             - Unregister a sensor\r\n");
    printf("  sensor read <id>                               - Read sensor immediately\r\n");
    printf("  sensor enable <id>                             - Enable sensor\r\n");
    printf("  sensor disable <id>                            - Disable sensor\r\n");
    printf("  sensor log start <id>                          - Start logging\r\n");
    printf("  sensor log stop <id>                           - Stop logging\r\n");
    printf("  sensor log show                                - Show log entries\r\n");
    printf("  sensor log export                              - Export log as CSV\r\n");
    printf("  sensor log clear                               - Clear log\r\n");
    printf("  sensor alert <id> above|below|change <thresh>  - Set alert\r\n");
    printf("  sensor alert clear <id>                        - Clear alert\r\n");
}

/* ---------- sub-commands ---------- */

static int cmd_sensor_list(void)
{
    printf("Registered sensors:\r\n");
    sensor_print_all();
    printf("Log entries: %d\r\n", sensor_log_count());
    return 0;
}

static int cmd_sensor_add(int argc, char *argv[])
{
    /* sensor add <name> adc|i2c|spi <bus> [addr] [reg] [len] <interval_ms>
     * Minimum: sensor add name adc bus interval  => argc = 6
     */
    if (argc < 6) {
        printf("Usage: sensor add <name> adc|i2c|spi <bus> [addr] [reg] [len] <interval_ms>\r\n");
        return -1;
    }

    const char *name = argv[2];
    const char *type_str = argv[3];

    sensor_type_t type;
    if (strcmp(type_str, "adc") == 0)
        type = SENSOR_TYPE_ADC;
    else if (strcmp(type_str, "i2c") == 0)
        type = SENSOR_TYPE_I2C;
    else if (strcmp(type_str, "spi") == 0)
        type = SENSOR_TYPE_SPI;
    else {
        printf("Unknown sensor type '%s' (use adc, i2c, or spi)\r\n", type_str);
        return -1;
    }

    uint8_t bus = (uint8_t)atoi(argv[4]);
    uint8_t addr = 0;
    uint8_t reg = 0;
    uint8_t read_len = 2;
    uint32_t interval_ms;

    /*
     * Determine which positional args are present based on type and argc.
     * ADC:  sensor add name adc <ch> <interval>                -> argc 6
     * I2C:  sensor add name i2c <inst> <addr> <reg> <len> <interval> -> argc 9
     * SPI:  sensor add name spi <inst> <reg> <len> <interval>  -> argc 8
     */
    if (type == SENSOR_TYPE_ADC) {
        /* bus = ADC channel, last arg = interval */
        interval_ms = (uint32_t)strtoul(argv[5], NULL, 0);
    } else if (type == SENSOR_TYPE_I2C) {
        if (argc < 9) {
            printf("Usage: sensor add <name> i2c <inst> <addr> <reg> <len> <interval>\r\n");
            return -1;
        }
        addr     = (uint8_t)strtoul(argv[5], NULL, 0);
        reg      = (uint8_t)strtoul(argv[6], NULL, 0);
        read_len = (uint8_t)atoi(argv[7]);
        interval_ms = (uint32_t)strtoul(argv[8], NULL, 0);
    } else { /* SPI */
        if (argc < 8) {
            printf("Usage: sensor add <name> spi <inst> <reg> <len> <interval>\r\n");
            return -1;
        }
        reg      = (uint8_t)strtoul(argv[5], NULL, 0);
        read_len = (uint8_t)atoi(argv[6]);
        interval_ms = (uint32_t)strtoul(argv[7], NULL, 0);
    }

    /* default data type: float for ADC, int for others */
    sensor_data_type_t data_type = (type == SENSOR_TYPE_ADC)
                                   ? SENSOR_DATA_FLOAT
                                   : SENSOR_DATA_INT;

    int id = sensor_register(name, type, data_type, bus, addr, reg, read_len, interval_ms);
    if (id < 0) {
        printf("Failed to register sensor\r\n");
        return -1;
    }
    printf("Sensor '%s' registered with id %d\r\n", name, id);
    return 0;
}

static int cmd_sensor_remove(int argc, char *argv[])
{
    if (argc < 3) {
        printf("Usage: sensor remove <id>\r\n");
        return -1;
    }
    uint8_t id = (uint8_t)atoi(argv[2]);
    int r = sensor_unregister(id);
    if (r < 0) {
        printf("Failed to remove sensor %d\r\n", id);
        return -1;
    }
    printf("Sensor %d removed\r\n", id);
    return 0;
}

static int cmd_sensor_read(int argc, char *argv[])
{
    if (argc < 3) {
        printf("Usage: sensor read <id>\r\n");
        return -1;
    }
    uint8_t id = (uint8_t)atoi(argv[2]);
    sensor_reading_t reading;
    int r = sensor_read(id, &reading);
    if (r < 0) {
        printf("Read failed for sensor %d\r\n", id);
        return -1;
    }

    sensor_descriptor_t info;
    const char *name = "?";
    if (sensor_get_info(id, &info) == 0)
        name = info.name;

    printf("Sensor %d ('%s') @ %lu ms: ", id, name, (unsigned long)reading.timestamp_ms);
    switch (reading.type) {
    case SENSOR_DATA_FLOAT:
        printf("%.4f", (double)reading.value.f_val);
        break;
    case SENSOR_DATA_INT:
        printf("%ld", (long)reading.value.i_val);
        break;
    case SENSOR_DATA_RAW:
    default:
        for (int i = 0; i < 8; i++)
            printf("%02X ", reading.value.raw[i]);
        break;
    }
    printf("\r\n");
    return 0;
}

static int cmd_sensor_enable_disable(int argc, char *argv[], bool enable)
{
    if (argc < 3) {
        printf("Usage: sensor %s <id>\r\n", enable ? "enable" : "disable");
        return -1;
    }
    uint8_t id = (uint8_t)atoi(argv[2]);
    int r = sensor_enable(id, enable);
    if (r < 0) {
        printf("Failed to %s sensor %d\r\n", enable ? "enable" : "disable", id);
        return -1;
    }
    printf("Sensor %d %s\r\n", id, enable ? "enabled" : "disabled");
    return 0;
}

static int cmd_sensor_log(int argc, char *argv[])
{
    if (argc < 3) {
        printf("Usage: sensor log start|stop|show|export|clear [id]\r\n");
        return -1;
    }

    if (strcmp(argv[2], "start") == 0) {
        if (argc < 4) {
            printf("Usage: sensor log start <id>\r\n");
            return -1;
        }
        uint8_t id = (uint8_t)atoi(argv[3]);
        int r = sensor_set_logging(id, true);
        if (r < 0) {
            printf("Failed to start logging for sensor %d\r\n", id);
            return -1;
        }
        printf("Logging started for sensor %d\r\n", id);
        return 0;
    }

    if (strcmp(argv[2], "stop") == 0) {
        if (argc < 4) {
            printf("Usage: sensor log stop <id>\r\n");
            return -1;
        }
        uint8_t id = (uint8_t)atoi(argv[3]);
        int r = sensor_set_logging(id, false);
        if (r < 0) {
            printf("Failed to stop logging for sensor %d\r\n", id);
            return -1;
        }
        printf("Logging stopped for sensor %d\r\n", id);
        return 0;
    }

    if (strcmp(argv[2], "show") == 0) {
        int count = sensor_log_count();
        if (count == 0) {
            printf("Log is empty\r\n");
            return 0;
        }
        printf("Sensor log (%d entries):\r\n", count);
        printf("  %-10s  %-4s  %-15s  %s\r\n", "Time(ms)", "ID", "Name", "Value");
        printf("  ----------  ----  ---------------  ----------\r\n");

        for (int i = 0; i < count; i++) {
            sensor_log_entry_t e;
            if (sensor_log_get(i, &e) < 0)
                break;

            const char *name = "?";
            sensor_descriptor_t info;
            if (sensor_get_info(e.sensor_id, &info) == 0)
                name = info.name;

            char val_str[24];
            switch (e.reading.type) {
            case SENSOR_DATA_FLOAT:
                snprintf(val_str, sizeof(val_str), "%.4f", (double)e.reading.value.f_val);
                break;
            case SENSOR_DATA_INT:
                snprintf(val_str, sizeof(val_str), "%ld", (long)e.reading.value.i_val);
                break;
            case SENSOR_DATA_RAW:
            default:
                snprintf(val_str, sizeof(val_str), "0x%02X%02X%02X%02X",
                         e.reading.value.raw[0], e.reading.value.raw[1],
                         e.reading.value.raw[2], e.reading.value.raw[3]);
                break;
            }

            printf("  %10lu  %4d  %-15s  %s\r\n",
                   (unsigned long)e.timestamp_ms, e.sensor_id, name, val_str);
        }
        return 0;
    }

    if (strcmp(argv[2], "export") == 0) {
        int count = sensor_log_count();
        if (count == 0) {
            printf("Log is empty\r\n");
            return 0;
        }
        /* allocate a generous buffer: ~60 chars per line */
        size_t buf_size = (size_t)(count + 1) * 64;
        if (buf_size > 16384)
            buf_size = 16384;

        char *buf = (char *)malloc(buf_size);
        if (!buf) {
            printf("Out of memory for export buffer\r\n");
            return -1;
        }

        int written = sensor_log_export_csv(buf, buf_size);
        if (written > 0) {
            printf("%s", buf);
        } else {
            printf("Export failed\r\n");
        }
        free(buf);
        return 0;
    }

    if (strcmp(argv[2], "clear") == 0) {
        sensor_log_clear();
        printf("Log cleared\r\n");
        return 0;
    }

    printf("Unknown log command '%s'\r\n", argv[2]);
    return -1;
}

static int cmd_sensor_alert(int argc, char *argv[])
{
    if (argc < 3) {
        printf("Usage: sensor alert <id> above|below|change <threshold>\r\n");
        printf("       sensor alert clear <id>\r\n");
        return -1;
    }

    /* sensor alert clear <id> */
    if (strcmp(argv[2], "clear") == 0) {
        if (argc < 4) {
            printf("Usage: sensor alert clear <id>\r\n");
            return -1;
        }
        uint8_t id = (uint8_t)atoi(argv[3]);
        int r = sensor_clear_alert(id);
        if (r < 0) {
            printf("Failed to clear alert for sensor %d\r\n", id);
            return -1;
        }
        printf("Alert cleared for sensor %d\r\n", id);
        return 0;
    }

    /* sensor alert <id> above|below|change <threshold> */
    if (argc < 5) {
        printf("Usage: sensor alert <id> above|below|change <threshold>\r\n");
        return -1;
    }

    uint8_t id = (uint8_t)atoi(argv[2]);
    const char *type_str = argv[3];
    float threshold = (float)atof(argv[4]);

    sensor_alert_type_t atype;
    if (strcmp(type_str, "above") == 0)
        atype = SENSOR_ALERT_ABOVE;
    else if (strcmp(type_str, "below") == 0)
        atype = SENSOR_ALERT_BELOW;
    else if (strcmp(type_str, "change") == 0)
        atype = SENSOR_ALERT_CHANGE;
    else {
        printf("Unknown alert type '%s' (use above, below, or change)\r\n", type_str);
        return -1;
    }

    int r = sensor_set_alert(id, atype, threshold, shell_alert_cb, NULL);
    if (r < 0) {
        printf("Failed to set alert for sensor %d\r\n", id);
        return -1;
    }
    printf("Alert set for sensor %d: %s %.4f\r\n", id, type_str, (double)threshold);
    return 0;
}

/* ---------- main entry ---------- */

int cmd_sensor(int argc, char *argv[])
{
    if (argc < 2) {
        cmd_sensor_usage();
        return -1;
    }

    const char *sub = argv[1];

    if (strcmp(sub, "list") == 0)
        return cmd_sensor_list();

    if (strcmp(sub, "add") == 0)
        return cmd_sensor_add(argc, argv);

    if (strcmp(sub, "remove") == 0)
        return cmd_sensor_remove(argc, argv);

    if (strcmp(sub, "read") == 0)
        return cmd_sensor_read(argc, argv);

    if (strcmp(sub, "enable") == 0)
        return cmd_sensor_enable_disable(argc, argv, true);

    if (strcmp(sub, "disable") == 0)
        return cmd_sensor_enable_disable(argc, argv, false);

    if (strcmp(sub, "log") == 0)
        return cmd_sensor_log(argc, argv);

    if (strcmp(sub, "alert") == 0)
        return cmd_sensor_alert(argc, argv);

    printf("Unknown sensor command '%s'\r\n", sub);
    cmd_sensor_usage();
    return -1;
}
