/* sensor.c - Sensor Framework with Data Logging for littleOS */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sensor.h"
#include "dmesg.h"
#include "hal/adc.h"
#include "hal/i2c.h"
#include "hal/spi.h"

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#endif

/* ---------- internal state ---------- */

static sensor_descriptor_t sensors[SENSOR_MAX_REGISTERED];
static bool                sensor_slot_used[SENSOR_MAX_REGISTERED];
static bool                sensor_initialized = false;

static sensor_log_entry_t  log_buffer[SENSOR_LOG_MAX_ENTRIES];
static int                 log_head  = 0;   /* next write position */
static int                 log_count = 0;   /* entries currently stored */

/* ---------- helpers ---------- */

static uint32_t sensor_now_ms(void)
{
#ifdef PICO_BUILD
    return to_ms_since_boot(get_absolute_time());
#else
    return 0;
#endif
}

static float reading_to_float(const sensor_reading_t *r)
{
    switch (r->type) {
    case SENSOR_DATA_FLOAT: return r->value.f_val;
    case SENSOR_DATA_INT:   return (float)r->value.i_val;
    default:                return 0.0f;
    }
}

static void log_append(uint8_t sensor_id, const sensor_reading_t *reading)
{
    sensor_log_entry_t *e = &log_buffer[log_head];
    e->sensor_id    = sensor_id;
    e->timestamp_ms = reading->timestamp_ms;
    e->reading      = *reading;

    log_head = (log_head + 1) % SENSOR_LOG_MAX_ENTRIES;
    if (log_count < SENSOR_LOG_MAX_ENTRIES)
        log_count++;
}

static void check_alert(uint8_t sensor_id, sensor_descriptor_t *s,
                         const sensor_reading_t *cur)
{
    if (s->alert_type == SENSOR_ALERT_NONE || s->alert_callback == NULL)
        return;

    float val = reading_to_float(cur);
    bool triggered = false;

    switch (s->alert_type) {
    case SENSOR_ALERT_ABOVE:
        triggered = (val > s->alert_threshold);
        break;
    case SENSOR_ALERT_BELOW:
        triggered = (val < s->alert_threshold);
        break;
    case SENSOR_ALERT_CHANGE: {
        float prev = reading_to_float(&s->last_reading);
        float diff = val - prev;
        if (diff < 0) diff = -diff;
        triggered = (diff > s->alert_threshold);
        break;
    }
    default:
        break;
    }

    if (triggered) {
        s->alert_count++;
        s->alert_callback(sensor_id, (sensor_reading_t *)cur, s->alert_user_data);
    }
}

/* ---------- public API ---------- */

int sensor_init(void)
{
    memset(sensors, 0, sizeof(sensors));
    memset(sensor_slot_used, 0, sizeof(sensor_slot_used));
    memset(log_buffer, 0, sizeof(log_buffer));
    log_head  = 0;
    log_count = 0;
    sensor_initialized = true;
    dmesg_info("sensor: framework initialized (%d slots, %d log entries)",
               SENSOR_MAX_REGISTERED, SENSOR_LOG_MAX_ENTRIES);
    return 0;
}

int sensor_register(const char *name, sensor_type_t type, sensor_data_type_t data_type,
                    uint8_t bus_instance, uint8_t device_addr, uint8_t reg_addr,
                    uint8_t read_len, uint32_t poll_interval_ms)
{
    if (!sensor_initialized)
        sensor_init();

    if (!name)
        return -1;

    /* find free slot */
    int slot = -1;
    for (int i = 0; i < SENSOR_MAX_REGISTERED; i++) {
        if (!sensor_slot_used[i]) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        dmesg_err("sensor: no free slot for '%s'", name);
        return -1;
    }

    sensor_descriptor_t *s = &sensors[slot];
    memset(s, 0, sizeof(*s));
    strncpy(s->name, name, SENSOR_NAME_LEN - 1);
    s->name[SENSOR_NAME_LEN - 1] = '\0';
    s->type             = type;
    s->data_type        = data_type;
    s->bus_instance     = bus_instance;
    s->device_addr      = device_addr;
    s->reg_addr         = reg_addr;
    s->read_len         = read_len;
    s->poll_interval_ms = poll_interval_ms;
    s->enabled          = true;
    s->logging          = false;
    s->alert_type       = SENSOR_ALERT_NONE;

    sensor_slot_used[slot] = true;

    dmesg_info("sensor: registered '%s' id=%d type=%d interval=%lu ms",
               s->name, slot, (int)type, (unsigned long)poll_interval_ms);
    return slot;
}

int sensor_unregister(uint8_t sensor_id)
{
    if (sensor_id >= SENSOR_MAX_REGISTERED || !sensor_slot_used[sensor_id])
        return -1;

    dmesg_info("sensor: unregistered '%s' id=%d", sensors[sensor_id].name, sensor_id);
    memset(&sensors[sensor_id], 0, sizeof(sensor_descriptor_t));
    sensor_slot_used[sensor_id] = false;
    return 0;
}

int sensor_enable(uint8_t sensor_id, bool enable)
{
    if (sensor_id >= SENSOR_MAX_REGISTERED || !sensor_slot_used[sensor_id])
        return -1;

    sensors[sensor_id].enabled = enable;
    dmesg_debug("sensor: '%s' %s", sensors[sensor_id].name,
                enable ? "enabled" : "disabled");
    return 0;
}

int sensor_set_logging(uint8_t sensor_id, bool enable)
{
    if (sensor_id >= SENSOR_MAX_REGISTERED || !sensor_slot_used[sensor_id])
        return -1;

    sensors[sensor_id].logging = enable;
    dmesg_debug("sensor: '%s' logging %s", sensors[sensor_id].name,
                enable ? "on" : "off");
    return 0;
}

int sensor_read(uint8_t sensor_id, sensor_reading_t *reading)
{
    if (sensor_id >= SENSOR_MAX_REGISTERED || !sensor_slot_used[sensor_id])
        return -1;

    sensor_descriptor_t *s = &sensors[sensor_id];
    sensor_reading_t r;
    memset(&r, 0, sizeof(r));
    r.type = s->data_type;
    r.timestamp_ms = sensor_now_ms();

    int rc = 0;

    switch (s->type) {
    case SENSOR_TYPE_ADC: {
        adc_hal_channel_init(s->bus_instance);
        if (s->data_type == SENSOR_DATA_FLOAT) {
            r.value.f_val = adc_hal_read_voltage(s->bus_instance);
        } else if (s->data_type == SENSOR_DATA_INT) {
            r.value.i_val = (int32_t)adc_hal_read_raw(s->bus_instance);
        } else {
            uint16_t raw = adc_hal_read_raw(s->bus_instance);
            r.value.raw[0] = (uint8_t)(raw & 0xFF);
            r.value.raw[1] = (uint8_t)((raw >> 8) & 0xFF);
        }
        r.valid = true;
        break;
    }

    case SENSOR_TYPE_I2C: {
        uint8_t buf[8];
        size_t len = s->read_len;
        if (len == 0) len = 1;
        if (len > sizeof(buf)) len = sizeof(buf);

        /* write register address, then read data */
        rc = i2c_hal_write_read(s->bus_instance, s->device_addr,
                                &s->reg_addr, 1, buf, len);
        if (rc < 0) {
            r.valid = false;
            s->error_count++;
            break;
        }
        if (s->data_type == SENSOR_DATA_INT) {
            /* interpret first 1-4 bytes as int, big-endian */
            int32_t val = 0;
            for (size_t i = 0; i < len && i < 4; i++)
                val = (val << 8) | buf[i];
            r.value.i_val = val;
        } else if (s->data_type == SENSOR_DATA_FLOAT) {
            /* interpret first 2 bytes as 16-bit signed, scale to float */
            int16_t raw16 = (int16_t)((buf[0] << 8) | (len > 1 ? buf[1] : 0));
            r.value.f_val = (float)raw16;
        } else {
            memcpy(r.value.raw, buf, len > 8 ? 8 : len);
        }
        r.valid = true;
        break;
    }

    case SENSOR_TYPE_SPI: {
        uint8_t tx[9], rx[9];
        size_t len = s->read_len;
        if (len == 0) len = 1;
        if (len > 8) len = 8;

        memset(tx, 0, sizeof(tx));
        tx[0] = s->reg_addr | 0x80; /* read bit (common convention) */

        spi_hal_cs_select(s->bus_instance);
        rc = spi_hal_transfer(s->bus_instance, tx, rx, len + 1);
        spi_hal_cs_deselect(s->bus_instance);

        if (rc < 0) {
            r.valid = false;
            s->error_count++;
            break;
        }
        /* skip first byte (sent during address) */
        if (s->data_type == SENSOR_DATA_INT) {
            int32_t val = 0;
            for (size_t i = 0; i < len && i < 4; i++)
                val = (val << 8) | rx[i + 1];
            r.value.i_val = val;
        } else if (s->data_type == SENSOR_DATA_FLOAT) {
            int16_t raw16 = (int16_t)((rx[1] << 8) | (len > 1 ? rx[2] : 0));
            r.value.f_val = (float)raw16;
        } else {
            memcpy(r.value.raw, &rx[1], len > 8 ? 8 : len);
        }
        r.valid = true;
        break;
    }

    case SENSOR_TYPE_VIRTUAL:
        /* virtual sensor: just return whatever was last set */
        r = s->last_reading;
        r.timestamp_ms = sensor_now_ms();
        break;

    default:
        r.valid = false;
        s->error_count++;
        break;
    }

    s->total_reads++;

    if (r.valid) {
        /* check alert before overwriting last_reading (needed for CHANGE) */
        check_alert(sensor_id, s, &r);
        s->last_reading = r;

        if (s->logging)
            log_append(sensor_id, &r);
    }

    if (reading)
        *reading = r;

    return r.valid ? 0 : -1;
}

int sensor_set_alert(uint8_t sensor_id, sensor_alert_type_t type,
                     float threshold, sensor_alert_cb_t cb, void *user_data)
{
    if (sensor_id >= SENSOR_MAX_REGISTERED || !sensor_slot_used[sensor_id])
        return -1;

    sensor_descriptor_t *s = &sensors[sensor_id];
    s->alert_type      = type;
    s->alert_threshold = threshold;
    s->alert_callback  = cb;
    s->alert_user_data = user_data;
    s->alert_count     = 0;

    dmesg_debug("sensor: '%s' alert type=%d threshold=%.2f", s->name, (int)type, (double)threshold);
    return 0;
}

int sensor_clear_alert(uint8_t sensor_id)
{
    if (sensor_id >= SENSOR_MAX_REGISTERED || !sensor_slot_used[sensor_id])
        return -1;

    sensor_descriptor_t *s = &sensors[sensor_id];
    s->alert_type      = SENSOR_ALERT_NONE;
    s->alert_threshold = 0.0f;
    s->alert_callback  = NULL;
    s->alert_user_data = NULL;
    s->alert_count     = 0;
    return 0;
}

void sensor_poll(void)
{
    uint32_t now = sensor_now_ms();

    for (int i = 0; i < SENSOR_MAX_REGISTERED; i++) {
        if (!sensor_slot_used[i] || !sensors[i].enabled)
            continue;

        sensor_descriptor_t *s = &sensors[i];
        uint32_t elapsed = now - s->last_reading.timestamp_ms;

        if (elapsed >= s->poll_interval_ms) {
            sensor_read((uint8_t)i, NULL);
        }
    }
}

int sensor_get_info(uint8_t sensor_id, sensor_descriptor_t *info)
{
    if (sensor_id >= SENSOR_MAX_REGISTERED || !sensor_slot_used[sensor_id])
        return -1;
    if (info)
        *info = sensors[sensor_id];
    return 0;
}

int sensor_find(const char *name)
{
    if (!name)
        return -1;
    for (int i = 0; i < SENSOR_MAX_REGISTERED; i++) {
        if (sensor_slot_used[i] && strncmp(sensors[i].name, name, SENSOR_NAME_LEN) == 0)
            return i;
    }
    return -1;
}

/* ---------- log access ---------- */

int sensor_log_count(void)
{
    return log_count;
}

int sensor_log_get(int index, sensor_log_entry_t *entry)
{
    if (index < 0 || index >= log_count || !entry)
        return -1;

    /* oldest entry position */
    int start;
    if (log_count < SENSOR_LOG_MAX_ENTRIES)
        start = 0;
    else
        start = log_head; /* head points to oldest when full */

    int pos = (start + index) % SENSOR_LOG_MAX_ENTRIES;
    *entry = log_buffer[pos];
    return 0;
}

void sensor_log_clear(void)
{
    log_head  = 0;
    log_count = 0;
    memset(log_buffer, 0, sizeof(log_buffer));
    dmesg_debug("sensor: log cleared");
}

int sensor_log_export_csv(char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0)
        return -1;

    int written = 0;
    int n;

    /* header */
    n = snprintf(buf + written, buf_size - (size_t)written,
                 "timestamp_ms,sensor_name,value\r\n");
    if (n < 0 || (size_t)(written + n) >= buf_size)
        return written;
    written += n;

    for (int i = 0; i < log_count; i++) {
        sensor_log_entry_t e;
        if (sensor_log_get(i, &e) < 0)
            break;

        /* resolve sensor name */
        const char *name = "?";
        if (e.sensor_id < SENSOR_MAX_REGISTERED && sensor_slot_used[e.sensor_id])
            name = sensors[e.sensor_id].name;

        /* format value */
        char val_str[32];
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

        n = snprintf(buf + written, buf_size - (size_t)written,
                     "%lu,%s,%s\r\n",
                     (unsigned long)e.timestamp_ms, name, val_str);
        if (n < 0 || (size_t)(written + n) >= buf_size)
            break;
        written += n;
    }

    return written;
}

void sensor_print_all(void)
{
    static const char *type_names[] = { "ADC", "I2C", "SPI", "VIRT" };

    printf("  ID  Name             Type  Bus  Ena  Log  Reads  Errs  Last Value\r\n");
    printf("  --  ---------------  ----  ---  ---  ---  -----  ----  ----------\r\n");

    int count = 0;
    for (int i = 0; i < SENSOR_MAX_REGISTERED; i++) {
        if (!sensor_slot_used[i])
            continue;

        sensor_descriptor_t *s = &sensors[i];
        const char *tname = (s->type <= SENSOR_TYPE_VIRTUAL) ? type_names[s->type] : "???";

        char val_str[24];
        if (!s->last_reading.valid) {
            snprintf(val_str, sizeof(val_str), "---");
        } else {
            switch (s->data_type) {
            case SENSOR_DATA_FLOAT:
                snprintf(val_str, sizeof(val_str), "%.4f", (double)s->last_reading.value.f_val);
                break;
            case SENSOR_DATA_INT:
                snprintf(val_str, sizeof(val_str), "%ld", (long)s->last_reading.value.i_val);
                break;
            case SENSOR_DATA_RAW:
            default:
                snprintf(val_str, sizeof(val_str), "0x%02X%02X",
                         s->last_reading.value.raw[0], s->last_reading.value.raw[1]);
                break;
            }
        }

        printf("  %2d  %-15s  %4s  %3d  %3s  %3s  %5lu  %4lu  %s\r\n",
               i, s->name, tname, s->bus_instance,
               s->enabled ? "yes" : "no",
               s->logging ? "yes" : "no",
               (unsigned long)s->total_reads,
               (unsigned long)s->error_count,
               val_str);
        count++;
    }

    if (count == 0)
        printf("  (no sensors registered)\r\n");
}
