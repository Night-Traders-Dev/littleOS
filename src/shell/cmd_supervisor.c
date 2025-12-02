// cmd_supervisor.c
// Shell commands for supervisor monitoring
#include <stdio.h>
#include <string.h>
#include "supervisor.h"
#include "../shell/shell_internal.h"

void cmd_health(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    if (!supervisor_is_running()) {
        printf("Supervisor is not running\r\n");
        return;
    }
    
    system_metrics_t metrics;
    if (!supervisor_get_metrics(&metrics)) {
        printf("Failed to get metrics\r\n");
        return;
    }
    
    // Quick health status
    printf("\r\nSystem Health: %s\r\n", supervisor_health_string(metrics.health_status));
    printf("Uptime: %lu.%03lu seconds\r\n", metrics.uptime_ms / 1000, metrics.uptime_ms % 1000);
    printf("\r\n");
    
    // Health flags
    if (metrics.health_flags != 0) {
        printf("Active Warnings:\r\n");
        if (metrics.health_flags & HEALTH_FLAG_WATCHDOG) 
            printf("  - Watchdog not being fed regularly\r\n");
        if (metrics.health_flags & HEALTH_FLAG_MEMORY_HIGH) 
            printf("  - Memory usage high\r\n");
        if (metrics.health_flags & HEALTH_FLAG_MEMORY_LEAK) 
            printf("  - Possible memory leak detected\r\n");
        if (metrics.health_flags & HEALTH_FLAG_TEMP_HIGH) 
            printf("  - Temperature high\r\n");
        if (metrics.health_flags & HEALTH_FLAG_TEMP_CRITICAL) 
            printf("  - Temperature critical!\r\n");
        if (metrics.health_flags & HEALTH_FLAG_STACK_OVERFLOW) 
            printf("  - Stack overflow detected\r\n");
        if (metrics.health_flags & HEALTH_FLAG_CORE0_HUNG) 
            printf("  - Core 0 appears hung!\r\n");
        if (metrics.health_flags & HEALTH_FLAG_FIFO_OVERFLOW) 
            printf("  - FIFO overflow\r\n");
        printf("\r\n");
    }
    
    // Quick stats
    printf("Temperature: %.1f°C (peak: %.1f°C)\r\n", 
           metrics.temp_celsius, metrics.temp_peak_celsius);
    printf("Memory: %lu bytes (%.1f%%) - peak: %lu bytes\r\n",
           metrics.heap_used_bytes, metrics.memory_usage_percent, metrics.heap_peak_bytes);
    printf("Core 0: %s (heartbeat %lu ms ago)\r\n",
           metrics.core0_responsive ? "Responsive" : "NOT RESPONDING",
           metrics.uptime_ms - metrics.core0_last_heartbeat);
    printf("\r\n");
}

void cmd_stats(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    if (!supervisor_is_running()) {
        printf("Supervisor is not running\r\n");
        return;
    }
    
    char buffer[1024];
    supervisor_get_stats_string(buffer, sizeof(buffer));
    printf("\r\n%s\r\n", buffer);
}

void cmd_supervisor(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: supervisor [start|stop|status|alerts]\r\n");
        return;
    }
    
    if (strcmp(argv[1], "start") == 0) {
        if (supervisor_is_running()) {
            printf("Supervisor already running\r\n");
        } else {
            supervisor_init();
            printf("Supervisor started\r\n");
        }
    }
    else if (strcmp(argv[1], "stop") == 0) {
        if (!supervisor_is_running()) {
            printf("Supervisor not running\r\n");
        } else {
            supervisor_stop();
            printf("Supervisor stopped\r\n");
        }
    }
    else if (strcmp(argv[1], "status") == 0) {
        printf("Supervisor: %s\r\n", supervisor_is_running() ? "Running" : "Stopped");
        if (supervisor_is_running()) {
            system_health_t health = supervisor_get_health();
            printf("Health: %s\r\n", supervisor_health_string(health));
        }
    }
    else if (strcmp(argv[1], "alerts") == 0) {
        if (argc < 3) {
            printf("Usage: supervisor alerts [on|off]\r\n");
            return;
        }
        if (strcmp(argv[2], "on") == 0) {
            supervisor_set_alerts(true);
            printf("Supervisor alerts enabled\r\n");
        } else if (strcmp(argv[2], "off") == 0) {
            supervisor_set_alerts(false);
            printf("Supervisor alerts disabled\r\n");
        } else {
            printf("Usage: supervisor alerts [on|off]\r\n");
        }
    }
    else {
        printf("Unknown command: %s\r\n", argv[1]);
        printf("Usage: supervisor [start|stop|status|alerts]\r\n");
    }
}
