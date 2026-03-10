/* cmd_mqtt.c - MQTT shell commands */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mqtt.h"

static void mqtt_print_cb(const char *topic, const uint8_t *payload,
                           uint16_t len, void *user_data) {
    (void)user_data;
    printf("[MQTT] %s: ", topic);
    for (uint16_t i = 0; i < len && i < 256; i++)
        putchar(payload[i]);
    printf("\r\n");
}

int cmd_mqtt(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: mqtt <command> [args]\r\n");
        printf("Commands:\r\n");
        printf("  status                      - Show connection status\r\n");
        printf("  connect IP [PORT] [ID]      - Connect to broker\r\n");
        printf("  disconnect                  - Disconnect\r\n");
        printf("  pub TOPIC MESSAGE           - Publish message\r\n");
        printf("  sub TOPIC                   - Subscribe to topic\r\n");
        printf("  unsub TOPIC                 - Unsubscribe\r\n");
        return 0;
    }

    if (strcmp(argv[1], "status") == 0) {
        char buf[256];
        mqtt_get_status(buf, sizeof(buf));
        printf("=== MQTT Status ===\r\n%s", buf);
        return 0;
    }

    if (strcmp(argv[1], "connect") == 0) {
        if (argc < 3) {
            printf("Usage: mqtt connect <ip> [port] [client_id]\r\n");
            return 1;
        }
        uint16_t port = (argc >= 4) ? (uint16_t)atoi(argv[3]) : 1883;
        const char *cid = (argc >= 5) ? argv[4] : "littleos";
        int rc = mqtt_connect(argv[2], port, cid);
        if (rc == 0)
            printf("Connecting to %s:%u...\r\n", argv[2], port);
        else
            printf("Connection failed: %d\r\n", rc);
        return rc;
    }

    if (strcmp(argv[1], "disconnect") == 0) {
        mqtt_disconnect();
        printf("Disconnected\r\n");
        return 0;
    }

    if (strcmp(argv[1], "pub") == 0) {
        if (argc < 4) {
            printf("Usage: mqtt pub <topic> <message>\r\n");
            return 1;
        }
        /* Concatenate remaining args as message */
        char msg[256];
        int pos = 0;
        for (int i = 3; i < argc && pos < 255; i++) {
            if (i > 3) msg[pos++] = ' ';
            int len = (int)strlen(argv[i]);
            if (pos + len > 255) len = 255 - pos;
            memcpy(&msg[pos], argv[i], len);
            pos += len;
        }
        msg[pos] = '\0';

        int rc = mqtt_publish(argv[2], msg, (uint16_t)pos, 0, false);
        if (rc == 0)
            printf("Published to %s\r\n", argv[2]);
        else
            printf("Publish failed: %d\r\n", rc);
        return rc;
    }

    if (strcmp(argv[1], "sub") == 0) {
        if (argc < 3) {
            printf("Usage: mqtt sub <topic>\r\n");
            return 1;
        }
        int rc = mqtt_subscribe(argv[2], 0, mqtt_print_cb, NULL);
        if (rc == 0)
            printf("Subscribed to %s\r\n", argv[2]);
        else
            printf("Subscribe failed: %d\r\n", rc);
        return rc;
    }

    if (strcmp(argv[1], "unsub") == 0) {
        if (argc < 3) {
            printf("Usage: mqtt unsub <topic>\r\n");
            return 1;
        }
        int rc = mqtt_unsubscribe(argv[2]);
        if (rc == 0)
            printf("Unsubscribed from %s\r\n", argv[2]);
        else
            printf("Unsubscribe failed: %d\r\n", rc);
        return rc;
    }

    printf("Unknown subcommand: %s\r\n", argv[1]);
    return 1;
}
