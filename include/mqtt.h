/* mqtt.h - MQTT Client for littleOS */
#ifndef LITTLEOS_MQTT_H
#define LITTLEOS_MQTT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MQTT_MAX_SUBSCRIPTIONS  8
#define MQTT_TOPIC_MAX_LEN      64
#define MQTT_PAYLOAD_MAX_LEN    256
#define MQTT_CLIENT_ID_MAX      32

typedef enum {
    MQTT_DISCONNECTED = 0,
    MQTT_CONNECTING,
    MQTT_CONNECTED,
    MQTT_ERROR
} mqtt_state_t;

typedef void (*mqtt_msg_callback_t)(const char *topic, const uint8_t *payload,
                                     uint16_t len, void *user_data);

typedef struct {
    char topic[MQTT_TOPIC_MAX_LEN];
    mqtt_msg_callback_t callback;
    void *user_data;
    uint8_t qos;
    bool active;
} mqtt_subscription_t;

typedef struct {
    mqtt_state_t state;
    char broker_ip[16];
    uint16_t port;
    char client_id[MQTT_CLIENT_ID_MAX];
    uint32_t last_ping_ms;
    uint32_t last_msg_ms;
    uint32_t msgs_sent;
    uint32_t msgs_received;
    mqtt_subscription_t subs[MQTT_MAX_SUBSCRIPTIONS];
    uint8_t sub_count;
} mqtt_client_t;

int  mqtt_init(void);
int  mqtt_connect(const char *broker_ip, uint16_t port, const char *client_id);
int  mqtt_disconnect(void);
int  mqtt_publish(const char *topic, const void *payload, uint16_t len,
                  uint8_t qos, bool retain);
int  mqtt_subscribe(const char *topic, uint8_t qos,
                    mqtt_msg_callback_t cb, void *user_data);
int  mqtt_unsubscribe(const char *topic);
void mqtt_task(void);
bool mqtt_is_connected(void);
int  mqtt_get_status(char *buf, size_t buflen);

#ifdef __cplusplus
}
#endif

#endif /* LITTLEOS_MQTT_H */
