/* mqtt.c - MQTT Client for littleOS */
#include <stdio.h>
#include <string.h>
#include "mqtt.h"
#include "pico/stdlib.h"

static mqtt_client_t mqtt_client;

/* --- PICO_W with lwIP: full MQTT 3.1.1 implementation --- */
#if defined(PICO_W) && __has_include("lwip/tcp.h")
#include "lwip/tcp.h"
#include "lwip/ip_addr.h"

#define MQTT_KEEPALIVE_SEC  30
#define MQTT_PROTO_LEVEL    4   /* MQTT 3.1.1 */

static struct tcp_pcb *mqtt_pcb = NULL;
static uint16_t mqtt_pkt_id = 1;

/* Build CONNECT packet */
static int mqtt_build_connect(uint8_t *buf, size_t buflen) {
    const char *cid = mqtt_client.client_id;
    uint16_t cid_len = (uint16_t)strlen(cid);
    /* Variable header: proto name(6) + level(1) + flags(1) + keepalive(2) = 10 */
    uint16_t var_len = 10 + 2 + cid_len;
    uint16_t rem = var_len;
    int pos = 0;

    if (buflen < (size_t)(2 + rem)) return -1;
    buf[pos++] = 0x10; /* CONNECT */
    buf[pos++] = (uint8_t)rem;
    /* Protocol name "MQTT" */
    buf[pos++] = 0; buf[pos++] = 4;
    buf[pos++] = 'M'; buf[pos++] = 'Q'; buf[pos++] = 'T'; buf[pos++] = 'T';
    buf[pos++] = MQTT_PROTO_LEVEL;
    buf[pos++] = 0x02; /* Clean session */
    buf[pos++] = (MQTT_KEEPALIVE_SEC >> 8) & 0xFF;
    buf[pos++] = MQTT_KEEPALIVE_SEC & 0xFF;
    buf[pos++] = (cid_len >> 8) & 0xFF;
    buf[pos++] = cid_len & 0xFF;
    memcpy(&buf[pos], cid, cid_len); pos += cid_len;
    return pos;
}

/* Build PUBLISH packet */
static int mqtt_build_publish(uint8_t *buf, size_t buflen,
                               const char *topic, const void *payload, uint16_t plen) {
    uint16_t tlen = (uint16_t)strlen(topic);
    uint16_t rem = 2 + tlen + plen;
    int pos = 0;

    if (buflen < (size_t)(2 + rem)) return -1;
    buf[pos++] = 0x30; /* PUBLISH QoS 0 */
    /* Simple remaining length (< 128) */
    if (rem < 128) {
        buf[pos++] = (uint8_t)rem;
    } else {
        buf[pos++] = (rem & 0x7F) | 0x80;
        buf[pos++] = (rem >> 7) & 0x7F;
    }
    buf[pos++] = (tlen >> 8) & 0xFF;
    buf[pos++] = tlen & 0xFF;
    memcpy(&buf[pos], topic, tlen); pos += tlen;
    memcpy(&buf[pos], payload, plen); pos += plen;
    return pos;
}

/* Build SUBSCRIBE packet */
static int mqtt_build_subscribe(uint8_t *buf, size_t buflen,
                                 const char *topic, uint8_t qos) {
    uint16_t tlen = (uint16_t)strlen(topic);
    uint16_t rem = 2 + 2 + tlen + 1; /* pkt_id + topic_len + topic + qos */
    int pos = 0;

    if (buflen < (size_t)(2 + rem)) return -1;
    buf[pos++] = 0x82; /* SUBSCRIBE */
    buf[pos++] = (uint8_t)rem;
    uint16_t pid = mqtt_pkt_id++;
    buf[pos++] = (pid >> 8) & 0xFF;
    buf[pos++] = pid & 0xFF;
    buf[pos++] = (tlen >> 8) & 0xFF;
    buf[pos++] = tlen & 0xFF;
    memcpy(&buf[pos], topic, tlen); pos += tlen;
    buf[pos++] = qos;
    return pos;
}

/* Build PINGREQ */
static int mqtt_build_pingreq(uint8_t *buf, size_t buflen) {
    if (buflen < 2) return -1;
    buf[0] = 0xC0; buf[1] = 0x00;
    return 2;
}

/* Build DISCONNECT */
static int mqtt_build_disconnect(uint8_t *buf, size_t buflen) {
    if (buflen < 2) return -1;
    buf[0] = 0xE0; buf[1] = 0x00;
    return 2;
}

/* TCP receive callback */
static err_t mqtt_recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    (void)arg; (void)err;
    if (!p) {
        mqtt_client.state = MQTT_DISCONNECTED;
        return ERR_OK;
    }
    uint8_t *data = (uint8_t *)p->payload;
    uint8_t pkt_type = data[0] & 0xF0;

    switch (pkt_type) {
        case 0x20: /* CONNACK */
            if (p->len >= 4 && data[3] == 0) {
                mqtt_client.state = MQTT_CONNECTED;
            } else {
                mqtt_client.state = MQTT_ERROR;
            }
            break;
        case 0x30: { /* PUBLISH - incoming message */
            if (p->len > 4) {
                uint16_t tlen = (data[2] << 8) | data[3];
                int hdr_len = 4 + tlen;
                /* Decode remaining length */
                uint8_t rl = data[1];
                int rl_bytes = 1;
                uint16_t remaining = rl & 0x7F;
                if (rl & 0x80) { remaining |= (data[2] & 0x7F) << 7; rl_bytes++; hdr_len++; }
                (void)remaining; (void)rl_bytes;
                if (hdr_len < (int)p->len) {
                    char topic_buf[MQTT_TOPIC_MAX_LEN];
                    if (tlen < MQTT_TOPIC_MAX_LEN) {
                        memcpy(topic_buf, &data[4], tlen);
                        topic_buf[tlen] = '\0';
                        uint16_t plen = (uint16_t)(p->len - hdr_len);
                        /* Dispatch to subscribers */
                        for (int i = 0; i < MQTT_MAX_SUBSCRIPTIONS; i++) {
                            if (mqtt_client.subs[i].active &&
                                strcmp(mqtt_client.subs[i].topic, topic_buf) == 0) {
                                if (mqtt_client.subs[i].callback) {
                                    mqtt_client.subs[i].callback(topic_buf,
                                        &data[hdr_len], plen, mqtt_client.subs[i].user_data);
                                }
                            }
                        }
                        mqtt_client.msgs_received++;
                    }
                }
            }
            break;
        }
        case 0x90: /* SUBACK */
            break;
        case 0xD0: /* PINGRESP */
            break;
    }

    mqtt_client.last_msg_ms = to_ms_since_boot(get_absolute_time());
    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

static err_t mqtt_connected_cb(void *arg, struct tcp_pcb *tpcb, err_t err) {
    (void)arg;
    if (err != ERR_OK) {
        mqtt_client.state = MQTT_ERROR;
        return err;
    }
    /* Send CONNECT packet */
    uint8_t buf[128];
    int len = mqtt_build_connect(buf, sizeof(buf));
    if (len > 0) {
        tcp_write(tpcb, buf, (uint16_t)len, TCP_WRITE_FLAG_COPY);
        tcp_output(tpcb);
    }
    mqtt_client.state = MQTT_CONNECTING;
    return ERR_OK;
}

int mqtt_init(void) {
    memset(&mqtt_client, 0, sizeof(mqtt_client));
    mqtt_client.state = MQTT_DISCONNECTED;
    return 0;
}

int mqtt_connect(const char *broker_ip, uint16_t port, const char *client_id) {
    if (mqtt_client.state != MQTT_DISCONNECTED) return -1;

    strncpy(mqtt_client.broker_ip, broker_ip, sizeof(mqtt_client.broker_ip) - 1);
    mqtt_client.port = port ? port : 1883;
    strncpy(mqtt_client.client_id, client_id ? client_id : "littleos",
            sizeof(mqtt_client.client_id) - 1);

    mqtt_pcb = tcp_new();
    if (!mqtt_pcb) return -1;

    tcp_recv(mqtt_pcb, mqtt_recv_cb);

    ip_addr_t addr;
    if (!ip4addr_aton(broker_ip, &addr)) return -1;

    err_t err = tcp_connect(mqtt_pcb, &addr, mqtt_client.port, mqtt_connected_cb);
    if (err != ERR_OK) {
        tcp_close(mqtt_pcb);
        mqtt_pcb = NULL;
        return -1;
    }

    mqtt_client.state = MQTT_CONNECTING;
    mqtt_client.last_ping_ms = to_ms_since_boot(get_absolute_time());
    return 0;
}

int mqtt_disconnect(void) {
    if (mqtt_pcb && mqtt_client.state == MQTT_CONNECTED) {
        uint8_t buf[2];
        int len = mqtt_build_disconnect(buf, sizeof(buf));
        if (len > 0) {
            tcp_write(mqtt_pcb, buf, (uint16_t)len, TCP_WRITE_FLAG_COPY);
            tcp_output(mqtt_pcb);
        }
        tcp_close(mqtt_pcb);
        mqtt_pcb = NULL;
    }
    mqtt_client.state = MQTT_DISCONNECTED;
    return 0;
}

int mqtt_publish(const char *topic, const void *payload, uint16_t len,
                 uint8_t qos, bool retain) {
    (void)qos; (void)retain;
    if (mqtt_client.state != MQTT_CONNECTED || !mqtt_pcb) return -1;

    uint8_t buf[512];
    int pkt_len = mqtt_build_publish(buf, sizeof(buf), topic, payload, len);
    if (pkt_len < 0) return -1;

    tcp_write(mqtt_pcb, buf, (uint16_t)pkt_len, TCP_WRITE_FLAG_COPY);
    tcp_output(mqtt_pcb);
    mqtt_client.msgs_sent++;
    return 0;
}

int mqtt_subscribe(const char *topic, uint8_t qos,
                   mqtt_msg_callback_t cb, void *user_data) {
    if (mqtt_client.state != MQTT_CONNECTED || !mqtt_pcb) return -1;

    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < MQTT_MAX_SUBSCRIPTIONS; i++) {
        if (!mqtt_client.subs[i].active) { slot = i; break; }
    }
    if (slot < 0) return -1;

    strncpy(mqtt_client.subs[slot].topic, topic, MQTT_TOPIC_MAX_LEN - 1);
    mqtt_client.subs[slot].callback = cb;
    mqtt_client.subs[slot].user_data = user_data;
    mqtt_client.subs[slot].qos = qos;
    mqtt_client.subs[slot].active = true;
    mqtt_client.sub_count++;

    uint8_t buf[128];
    int len = mqtt_build_subscribe(buf, sizeof(buf), topic, qos);
    if (len > 0) {
        tcp_write(mqtt_pcb, buf, (uint16_t)len, TCP_WRITE_FLAG_COPY);
        tcp_output(mqtt_pcb);
    }
    return 0;
}

int mqtt_unsubscribe(const char *topic) {
    for (int i = 0; i < MQTT_MAX_SUBSCRIPTIONS; i++) {
        if (mqtt_client.subs[i].active &&
            strcmp(mqtt_client.subs[i].topic, topic) == 0) {
            mqtt_client.subs[i].active = false;
            mqtt_client.sub_count--;
            return 0;
        }
    }
    return -1;
}

void mqtt_task(void) {
    if (mqtt_client.state != MQTT_CONNECTED || !mqtt_pcb) return;

    uint32_t now = to_ms_since_boot(get_absolute_time());
    /* Send PINGREQ every 30 seconds */
    if (now - mqtt_client.last_ping_ms >= MQTT_KEEPALIVE_SEC * 1000) {
        uint8_t buf[2];
        int len = mqtt_build_pingreq(buf, sizeof(buf));
        if (len > 0) {
            tcp_write(mqtt_pcb, buf, (uint16_t)len, TCP_WRITE_FLAG_COPY);
            tcp_output(mqtt_pcb);
        }
        mqtt_client.last_ping_ms = now;
    }
}

bool mqtt_is_connected(void) {
    return mqtt_client.state == MQTT_CONNECTED;
}

int mqtt_get_status(char *buf, size_t buflen) {
    const char *state_str;
    switch (mqtt_client.state) {
        case MQTT_DISCONNECTED: state_str = "disconnected"; break;
        case MQTT_CONNECTING:   state_str = "connecting"; break;
        case MQTT_CONNECTED:    state_str = "connected"; break;
        case MQTT_ERROR:        state_str = "error"; break;
        default:                state_str = "unknown"; break;
    }
    return snprintf(buf, buflen,
        "State: %s\r\nBroker: %s:%u\r\nClient ID: %s\r\n"
        "Messages sent: %lu\r\nMessages received: %lu\r\nSubscriptions: %u\r\n",
        state_str, mqtt_client.broker_ip, mqtt_client.port,
        mqtt_client.client_id, (unsigned long)mqtt_client.msgs_sent,
        (unsigned long)mqtt_client.msgs_received, mqtt_client.sub_count);
}

#else
/* --- Stub implementation when no WiFi/lwIP --- */

int mqtt_init(void) {
    memset(&mqtt_client, 0, sizeof(mqtt_client));
    return 0;
}

int mqtt_connect(const char *broker_ip, uint16_t port, const char *client_id) {
    (void)broker_ip; (void)port; (void)client_id;
    return -1;
}

int mqtt_disconnect(void) { return -1; }

int mqtt_publish(const char *topic, const void *payload, uint16_t len,
                 uint8_t qos, bool retain) {
    (void)topic; (void)payload; (void)len; (void)qos; (void)retain;
    return -1;
}

int mqtt_subscribe(const char *topic, uint8_t qos,
                   mqtt_msg_callback_t cb, void *user_data) {
    (void)topic; (void)qos; (void)cb; (void)user_data;
    return -1;
}

int mqtt_unsubscribe(const char *topic) { (void)topic; return -1; }
void mqtt_task(void) {}
bool mqtt_is_connected(void) { return false; }

int mqtt_get_status(char *buf, size_t buflen) {
    return snprintf(buf, buflen, "MQTT not available (no WiFi)\r\n");
}

#endif
