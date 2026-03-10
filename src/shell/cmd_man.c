/* cmd_man.c - Manual page system for littleOS */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

typedef struct {
    const char *name;
    const char *short_desc;
    const char *synopsis;
    const char *description;
    const char *examples;
    const char *see_also;
} man_page_t;

static const man_page_t man_pages[] = {
    { "help", "Show available commands",
      "help", "Display a list of all available shell commands with brief descriptions.",
      "help", "man" },
    { "version", "Show OS version",
      "version", "Display littleOS version, SageLang version, and supervisor status.",
      "version", "health, stats" },
    { "clear", "Clear the terminal",
      "clear", "Clear the terminal screen using ANSI escape codes.",
      "clear", "screen" },
    { "reboot", "Reboot the system",
      "reboot", "Perform a system reboot via watchdog timer reset.",
      "reboot", "health, power" },
    { "history", "Show command history",
      "history\n    !!        - Repeat last command\n    !N        - Repeat command N",
      "Display the command history buffer (last 20 commands). Use !! to repeat the last command or !N to repeat command number N.",
      "history\n    !!          Repeat last command\n    !3          Repeat command 3",
      "alias" },
    { "health", "Quick system health check",
      "health", "Show a quick summary of system health including CPU, memory, watchdog, and supervisor status.",
      "health", "stats, top" },
    { "stats", "Detailed system statistics",
      "stats", "Display detailed system statistics including memory segments, task counts, uptime, and peripheral status.",
      "stats", "health, top, profile" },
    { "supervisor", "Core 1 supervisor control",
      "supervisor <start|stop|status|alerts>",
      "Control the Core 1 supervisor that monitors system health. View alerts, start/stop monitoring.",
      "supervisor status\n    supervisor alerts",
      "health, stats" },
    { "dmesg", "Kernel message buffer",
      "dmesg [--level LEVEL] [--clear] [--follow]",
      "Display or control the kernel ring buffer. Filter by log level (info, warn, err, crit). Use --follow for live output.",
      "dmesg\n    dmesg --level err\n    dmesg --clear",
      "health, stats" },
    { "sage", "SageLang interpreter",
      "sage [--help] [eval EXPR | run FILE | repl]",
      "Run the embedded SageLang scripting interpreter. Evaluate expressions, run scripts, or enter interactive REPL mode.",
      "sage eval \"2 + 2\"\n    sage repl\n    sage run myscript",
      "script, pkg" },
    { "script", "Script management",
      "script [list|save|load|delete|run] [args]",
      "Manage stored SageLang scripts. Save, load, list, delete, or run scripts from persistent storage.",
      "script list\n    script save myscript\n    script run myscript",
      "sage, pkg" },
    { "users", "User account management",
      "users [list|info|add|passwd]",
      "Manage user accounts. List users, view info, add accounts, change passwords.",
      "users list\n    users info root",
      "perms" },
    { "perms", "Permission and access control",
      "perms [list|check|grant] [args]",
      "View and manage resource permissions. Check access rights, grant/revoke capabilities.",
      "perms list\n    perms check uart0",
      "users" },
    { "tasks", "Task scheduler management",
      "tasks [list|create|kill|suspend|resume|priority] [args]",
      "Manage scheduled tasks. View running tasks, create new ones, set priorities, kill or suspend tasks.",
      "tasks list\n    tasks kill 3\n    tasks priority 2 high",
      "top, profile" },
    { "memory", "Memory diagnostics",
      "memory [status|map|test|alloc|free] [args]",
      "View memory usage, run diagnostics, allocate/free segments. Shows 264KB SRAM utilization.",
      "memory status\n    memory map\n    memory test",
      "top, stats" },
    { "fs", "Filesystem tools",
      "fs [mount|format|ls|mkdir|write|read|rm|stat|sync|df] [args]",
      "F2FS-inspired filesystem operations. Mount, format, list files, read/write files, check disk usage.",
      "fs ls /\n    fs write /hello.txt \"Hello World\"\n    fs read /hello.txt\n    fs df",
      "cat, dev" },
    { "ipc", "Inter-process communication",
      "ipc [mutex|sem|mbox|pipe|shm] <subcommand> [args]",
      "Manage IPC primitives: mutexes, semaphores, mailboxes, pipes, and shared memory regions.",
      "ipc mutex create mylock\n    ipc sem create mysem 5\n    ipc pipe create mypipe",
      "tasks" },
    { "hw", "Hardware peripherals",
      "hw [i2c|spi|pwm|adc] <subcommand> [args]",
      "Access hardware peripherals. Scan I2C bus, read/write SPI, configure PWM duty cycle, read ADC channels.",
      "hw i2c scan\n    hw adc read 0\n    hw pwm set 0 1000 50",
      "dev, pinout, pio" },
    { "net", "Networking (WiFi/TCP/UDP)",
      "net [wifi|tcp|udp|status|ping] [args]",
      "Network management for Pico W. Connect to WiFi, create TCP/UDP connections, check status.",
      "net wifi connect SSID PASSWORD\n    net status\n    net tcp connect 192.168.1.1 80",
      "mqtt, remote, ota" },
    { "ota", "Over-the-air firmware updates",
      "ota [status|check|download|apply] [args]",
      "Manage firmware updates over the network. Check for updates, download, and apply.",
      "ota status\n    ota check\n    ota apply",
      "net" },
    { "usb", "USB device mode",
      "usb [status|mode|cdc|hid|msc] [args]",
      "Configure USB device modes using TinyUSB. Switch between CDC (serial), HID (keyboard/mouse), and MSC (mass storage).",
      "usb status\n    usb mode cdc\n    usb hid type \"hello\"",
      "dev" },
    { "pio", "Programmable I/O",
      "pio [status|load|unload|config|start|stop|exec|blink|ws2812] [args]",
      "Control RP2040 PIO state machines. Load programs, configure pins, execute instructions, run built-in programs.",
      "pio status\n    pio blink 25\n    pio ws2812 12 8",
      "hw, dma, pinout" },
    { "dma", "DMA engine control",
      "dma [status|claim|release|memcpy|test|sniff] [args]",
      "Manage DMA channels. Perform memory copies, test transfers, use CRC sniffer.",
      "dma status\n    dma claim 0\n    dma memcpy 0 src dst 256",
      "pio, memory" },
    { "remote", "Remote shell over TCP",
      "remote [status|start|stop|kick|send] [args]",
      "Start a TCP server for remote shell access over the network. Supports 2 concurrent clients.",
      "remote start 2323\n    remote status\n    remote kick 0",
      "net, screen" },
    { "sensor", "Sensor framework and logging",
      "sensor [list|add|remove|read|enable|disable|log|alert] [args]",
      "Register and manage sensors. Read values, enable logging, set alert thresholds, export CSV data.",
      "sensor list\n    sensor add temp adc 4\n    sensor read temp\n    sensor log",
      "hw, cron" },
    { "power", "Power management",
      "power [status|sleep|clock|voltage|periph] [args]",
      "Manage power states, clock frequencies, core voltage, and peripheral power gating.",
      "power status\n    power clock 48\n    power sleep light",
      "top, profile" },
    { "profile", "Runtime profiling",
      "profile [status|start|stop|report|tasks|system|reset|section|benchmark] [args]",
      "Profile system performance. Track CPU usage per task, measure code sections, benchmark operations.",
      "profile start\n    profile tasks\n    profile report",
      "top, stats" },
    { "env", "Environment variables",
      "env [list|set|unset|get] [args]",
      "Manage shell environment variables. Set, get, unset, or list all variables.",
      "env list\n    env set MY_VAR hello\n    env get PATH",
      "alias, export, prompt" },
    { "alias", "Command aliases",
      "alias [list|set|unset] [args]",
      "Create command shortcuts. Aliases expand before command execution.",
      "alias set ll \"fs ls -l\"\n    alias list\n    alias unset ll",
      "env" },
    { "export", "Set environment variable",
      "export KEY=VALUE",
      "Shorthand for 'env set'. Sets an environment variable in the current shell session.",
      "export PATH=/bin:/usr/bin\n    export EDITOR=sage",
      "env" },
    { "cat", "Display file contents",
      "cat [-n] FILE [FILE2...]",
      "Concatenate and display file contents. Use -n to show line numbers.",
      "cat /hello.txt\n    cat -n /script.sg",
      "head, tail, grep, hexdump, fs" },
    { "echo", "Print text",
      "echo [-n] [-e] [TEXT...]",
      "Print arguments to stdout. Use -n to omit trailing newline, -e to interpret escape sequences (\\n, \\t, \\\\).",
      "echo Hello World\n    echo -n \"no newline\"\n    echo -e \"line1\\nline2\"",
      "env, tee" },
    { "head", "Show first lines of file",
      "head [-n NUM] FILE",
      "Display the first N lines of a file (default 10).",
      "head /log.txt\n    head -n 5 /log.txt",
      "tail, cat" },
    { "tail", "Show last lines of file",
      "tail [-n NUM] FILE",
      "Display the last N lines of a file (default 10).",
      "tail /log.txt\n    tail -n 20 /log.txt",
      "head, cat" },
    { "wc", "Word count",
      "wc [-l] [-w] [-c] FILE",
      "Count lines, words, and bytes in a file.",
      "wc /hello.txt\n    wc -l /log.txt",
      "cat, grep" },
    { "grep", "Search for patterns",
      "grep [-i] [-n] [-c] [-v] PATTERN [FILE]",
      "Search for substring matches in a file. Use -i for case-insensitive, -n for line numbers, -v to invert match.",
      "grep error /log.txt\n    grep -in \"warning\" /dmesg.txt",
      "cat, wc" },
    { "hexdump", "Hex dump of file",
      "hexdump [-n NUM] FILE",
      "Display file contents in hex + ASCII format, 16 bytes per line.",
      "hexdump /data.bin\n    hexdump -n 64 /firmware.bin",
      "cat, fs" },
    { "tee", "Duplicate output to file",
      "tee [-a] FILE",
      "Read from input and write to both stdout and a file. Use -a for append mode.",
      "tee /log.txt",
      "echo, cat" },
    { "top", "Live system monitor",
      "top [-d SEC] [-n NUM]",
      "Interactive full-screen process monitor. Shows tasks, CPU%, memory usage in real-time. Press 'q' to quit.",
      "top\n    top -d 2        Refresh every 2 seconds\n    top -n 5        Run 5 iterations then exit",
      "tasks, profile, stats" },
    { "pinout", "GPIO pin visualizer",
      "pinout [watch|gpio N|legend]",
      "Display ASCII art of the Pico board with live GPIO pin states. Use 'watch' for continuous updates.",
      "pinout\n    pinout watch\n    pinout gpio 25\n    pinout legend",
      "hw, dev, pio" },
    { "cron", "Scheduled tasks",
      "cron [list|add|remove|enable|disable|status] [args]",
      "Schedule periodic command execution. Add jobs with an interval in seconds.",
      "cron add temp 60 \"sensor read temp\"\n    cron list\n    cron disable temp",
      "tasks, sensor" },
    { "mqtt", "MQTT client",
      "mqtt [status|connect|disconnect|pub|sub|unsub] [args]",
      "MQTT 3.1.1 client for IoT messaging. Connect to brokers, publish/subscribe to topics. Requires Pico W.",
      "mqtt connect 192.168.1.100\n    mqtt pub sensors/temp 23.5\n    mqtt sub alerts/#",
      "net, sensor" },
    { "pkg", "Package manager",
      "pkg [list|installed|install|remove|info|run|search] [args]",
      "Manage SageLang script packages. Install from built-in registry, run scripts, search available packages.",
      "pkg list\n    pkg install hello\n    pkg run hello\n    pkg search temp",
      "sage, script" },
    { "screen", "Terminal multiplexer",
      "screen [list|new|kill|switch|next|prev|rename|status] [args]",
      "Manage virtual terminal windows. Create up to 4 windows, switch between them.",
      "screen new log\n    screen switch 1\n    screen list",
      "remote" },
    { "dev", "Device files",
      "dev [list|read|write|info] [PATH] [VALUE]",
      "Read/write hardware devices via /dev paths. Control GPIO pins, read ADC, access UART.",
      "dev list\n    dev read /dev/gpio25\n    dev write /dev/gpio25 1\n    dev read /dev/adc4",
      "hw, pinout, proc" },
    { "proc", "Process filesystem",
      "proc [read|list] [PATH]",
      "Read virtual files from /proc. Shows system info like cpuinfo, meminfo, uptime, tasks, gpio states.",
      "proc read /proc/cpuinfo\n    proc read /proc/meminfo\n    proc list",
      "dev, stats, top" },
    { "man", "Manual pages",
      "man [-k KEYWORD] [-l] [COMMAND]",
      "Display manual pages for commands. Use -k to search by keyword, -l to list all pages.",
      "man fs\n    man -k network\n    man -l",
      "help" },
    { "fetch", "System info display",
      "fetch",
      "Display system information in a neofetch-style format with ASCII art.",
      "fetch",
      "version, stats" },
};

#define MAN_PAGE_COUNT (sizeof(man_pages) / sizeof(man_pages[0]))

static void print_man_page(const man_page_t *page) {
    printf("\r\n\033[1mMAN(1)                    littleOS Manual                    MAN(1)\033[0m\r\n\r\n");
    printf("\033[1mNAME\033[0m\r\n    %s - %s\r\n\r\n", page->name, page->short_desc);
    printf("\033[1mSYNOPSIS\033[0m\r\n    %s\r\n\r\n", page->synopsis);
    printf("\033[1mDESCRIPTION\033[0m\r\n    %s\r\n\r\n", page->description);
    if (page->examples) {
        printf("\033[1mEXAMPLES\033[0m\r\n    %s\r\n\r\n", page->examples);
    }
    if (page->see_also) {
        printf("\033[1mSEE ALSO\033[0m\r\n    %s\r\n\r\n", page->see_also);
    }
}

int cmd_man(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: man [-k keyword] [-l] <command>\r\n");
        printf("  man <command>   - Show manual page\r\n");
        printf("  man -k KEYWORD  - Search pages by keyword\r\n");
        printf("  man -l          - List all manual pages\r\n");
        return 0;
    }

    /* List all pages */
    if (strcmp(argv[1], "-l") == 0) {
        printf("Available manual pages (%d):\r\n", (int)MAN_PAGE_COUNT);
        for (size_t i = 0; i < MAN_PAGE_COUNT; i++) {
            printf("  %-12s - %s\r\n", man_pages[i].name, man_pages[i].short_desc);
        }
        return 0;
    }

    /* Search by keyword */
    if (strcmp(argv[1], "-k") == 0) {
        if (argc < 3) {
            printf("Usage: man -k <keyword>\r\n");
            return 1;
        }
        const char *kw = argv[2];
        int found = 0;
        /* Case-insensitive substring search */
        for (size_t i = 0; i < MAN_PAGE_COUNT; i++) {
            bool match = false;
            /* Search in name, short_desc, description */
            const char *fields[] = { man_pages[i].name, man_pages[i].short_desc,
                                      man_pages[i].description };
            for (int f = 0; f < 3; f++) {
                const char *haystack = fields[f];
                size_t klen = strlen(kw);
                for (const char *p = haystack; *p; p++) {
                    bool ok = true;
                    for (size_t k = 0; k < klen && ok; k++) {
                        if (tolower((unsigned char)p[k]) != tolower((unsigned char)kw[k]))
                            ok = false;
                    }
                    if (ok) { match = true; break; }
                }
                if (match) break;
            }
            if (match) {
                printf("  %-12s - %s\r\n", man_pages[i].name, man_pages[i].short_desc);
                found++;
            }
        }
        if (found == 0) printf("No matches for '%s'\r\n", kw);
        return 0;
    }

    /* Look up specific page */
    for (size_t i = 0; i < MAN_PAGE_COUNT; i++) {
        if (strcmp(man_pages[i].name, argv[1]) == 0) {
            print_man_page(&man_pages[i]);
            return 0;
        }
    }

    printf("No manual entry for '%s'\r\n", argv[1]);
    printf("Use 'man -l' to list available pages\r\n");
    return 1;
}
