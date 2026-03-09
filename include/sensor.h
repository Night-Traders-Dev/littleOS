/* sensor.h - Sensor Framework with Data Logging for littleOS */
#ifndef LITTLEOS_SENSOR_H
#define LITTLEOS_SENSOR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SENSOR_MAX_REGISTERED   8
#define SENSOR_NAME_LEN         16
#define SENSOR_LOG_MAX_ENTRIES  256  /* Ring buffer of log entries */

/* Sensor interface type */
typedef enum {
    SENSOR_TYPE_ADC = 0,
    SENSOR_TYPE_I2C,
    SENSOR_TYPE_SPI,
    SENSOR_TYPE_VIRTUAL,    /* Software/computed sensor */
} sensor_type_t;

/* Sensor data type */
typedef enum {
    SENSOR_DATA_INT = 0,
    SENSOR_DATA_FLOAT,
    SENSOR_DATA_RAW,        /* Raw bytes */
} sensor_data_type_t;

/* Sensor reading */
typedef struct {
    union {
        int32_t  i_val;
        float    f_val;
        uint8_t  raw[8];
    } value;
    sensor_data_type_t type;
    uint32_t timestamp_ms;
    bool     valid;
} sensor_reading_t;

/* Alert condition */
typedef enum {
    SENSOR_ALERT_NONE = 0,
    SENSOR_ALERT_ABOVE,     /* Trigger when value > threshold */
    SENSOR_ALERT_BELOW,     /* Trigger when value < threshold */
    SENSOR_ALERT_CHANGE,    /* Trigger when value changes by > threshold */
} sensor_alert_type_t;

/* Sensor alert callback */
typedef void (*sensor_alert_cb_t)(uint8_t sensor_id, sensor_reading_t *reading, void *user_data);

/* Sensor descriptor */
typedef struct {
    char            name[SENSOR_NAME_LEN];
    sensor_type_t   type;
    sensor_data_type_t data_type;
    uint8_t         bus_instance;   /* I2C/SPI instance or ADC channel */
    uint8_t         device_addr;    /* I2C address (0 for ADC/SPI) */
    uint8_t         reg_addr;       /* Register to read (I2C/SPI) */
    uint8_t         read_len;       /* Bytes to read */
    uint32_t        poll_interval_ms;
    bool            enabled;
    bool            logging;
    sensor_reading_t last_reading;
    uint32_t        total_reads;
    uint32_t        error_count;
    /* Alert */
    sensor_alert_type_t alert_type;
    float           alert_threshold;
    sensor_alert_cb_t alert_callback;
    void            *alert_user_data;
    uint32_t        alert_count;
} sensor_descriptor_t;

/* Log entry */
typedef struct {
    uint8_t  sensor_id;
    uint32_t timestamp_ms;
    sensor_reading_t reading;
} sensor_log_entry_t;

/* Init sensor framework */
int sensor_init(void);

/* Register a sensor */
int sensor_register(const char *name, sensor_type_t type, sensor_data_type_t data_type,
                    uint8_t bus_instance, uint8_t device_addr, uint8_t reg_addr,
                    uint8_t read_len, uint32_t poll_interval_ms);

/* Unregister a sensor */
int sensor_unregister(uint8_t sensor_id);

/* Enable/disable a sensor */
int sensor_enable(uint8_t sensor_id, bool enable);

/* Enable/disable logging for a sensor */
int sensor_set_logging(uint8_t sensor_id, bool enable);

/* Read a sensor immediately */
int sensor_read(uint8_t sensor_id, sensor_reading_t *reading);

/* Set alert condition */
int sensor_set_alert(uint8_t sensor_id, sensor_alert_type_t type,
                     float threshold, sensor_alert_cb_t cb, void *user_data);

/* Clear alert */
int sensor_clear_alert(uint8_t sensor_id);

/* Poll all sensors (call periodically) */
void sensor_poll(void);

/* Get sensor info */
int sensor_get_info(uint8_t sensor_id, sensor_descriptor_t *info);

/* Find sensor by name */
int sensor_find(const char *name);

/* Log access */
int sensor_log_count(void);
int sensor_log_get(int index, sensor_log_entry_t *entry);
void sensor_log_clear(void);

/* Export log as CSV to a buffer */
int sensor_log_export_csv(char *buf, size_t buf_size);

/* Print sensor summary */
void sensor_print_all(void);

#ifdef __cplusplus
}
#endif

#endif /* LITTLEOS_SENSOR_H */
