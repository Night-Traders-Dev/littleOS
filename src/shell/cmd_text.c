/* cmd_text.c - Unix-like text utility commands for littleOS shell
 *
 * Commands: cat, echo, head, tail, wc, grep, hexdump, tee
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "fs.h"

extern struct fs *g_fs_ptr;
#define g_fs g_fs_ptr

/* ===== HELPERS ===== */

/* Case-insensitive substring search */
static const char *strcasestr_simple(const char *haystack, const char *needle) {
    if (!needle[0]) return haystack;
    size_t nlen = strlen(needle);
    for (; *haystack; haystack++) {
        if (tolower((unsigned char)*haystack) == tolower((unsigned char)*needle)) {
            size_t i;
            for (i = 1; i < nlen; i++) {
                if (tolower((unsigned char)haystack[i]) != tolower((unsigned char)needle[i]))
                    break;
            }
            if (i == nlen) return haystack;
        }
    }
    return NULL;
}

/* ===== cmd_cat ===== */

int cmd_cat(int argc, char **argv) {
    int show_line_numbers = 0;
    int first_file = 1;

    if (argc >= 2 && strcmp(argv[1], "-n") == 0) {
        show_line_numbers = 1;
        first_file = 2;
    }

    if (first_file >= argc) {
        printf("Usage: cat [-n] FILE [FILE2...]\r\n");
        return -1;
    }

    for (int fi = first_file; fi < argc; fi++) {
        struct fs_file fd;
        int r = fs_open(g_fs, argv[fi], FS_O_RDONLY, &fd);
        if (r != FS_OK) {
            printf("cat: cannot open '%s': error %d\r\n", argv[fi], r);
            return -1;
        }

        uint8_t buf[512];
        int line_num = 1;
        int at_line_start = 1;

        for (;;) {
            int n = fs_read(g_fs, &fd, buf, sizeof(buf));
            if (n < 0) {
                printf("cat: read error: %d\r\n", n);
                fs_close(g_fs, &fd);
                return -1;
            }
            if (n == 0) break;

            if (!show_line_numbers) {
                /* Fast path: just output raw bytes */
                for (int i = 0; i < n; i++) {
                    if (buf[i] == '\n')
                        printf("\r\n");
                    else
                        putchar(buf[i]);
                }
            } else {
                for (int i = 0; i < n; i++) {
                    if (at_line_start) {
                        printf("%6d\t", line_num);
                        at_line_start = 0;
                    }
                    if (buf[i] == '\n') {
                        printf("\r\n");
                        line_num++;
                        at_line_start = 1;
                    } else {
                        putchar(buf[i]);
                    }
                }
            }
        }

        fs_close(g_fs, &fd);
    }

    return 0;
}

/* ===== cmd_echo ===== */

int cmd_echo(int argc, char **argv) {
    int no_newline = 0;
    int interpret_escapes = 0;
    int first_arg = 1;

    /* Parse flags */
    while (first_arg < argc) {
        if (strcmp(argv[first_arg], "-n") == 0) {
            no_newline = 1;
            first_arg++;
        } else if (strcmp(argv[first_arg], "-e") == 0) {
            interpret_escapes = 1;
            first_arg++;
        } else {
            break;
        }
    }

    for (int i = first_arg; i < argc; i++) {
        if (i > first_arg) putchar(' ');

        if (!interpret_escapes) {
            printf("%s", argv[i]);
        } else {
            const char *s = argv[i];
            while (*s) {
                if (*s == '\\' && s[1]) {
                    s++;
                    switch (*s) {
                        case 'n':  printf("\r\n"); break;
                        case 't':  putchar('\t');  break;
                        case '\\': putchar('\\');  break;
                        default:
                            putchar('\\');
                            putchar(*s);
                            break;
                    }
                } else {
                    putchar(*s);
                }
                s++;
            }
        }
    }

    if (!no_newline)
        printf("\r\n");

    return 0;
}

/* ===== cmd_head ===== */

int cmd_head(int argc, char **argv) {
    int num_lines = 10;
    const char *filepath = NULL;
    int i = 1;

    while (i < argc) {
        if (strcmp(argv[i], "-n") == 0) {
            if (i + 1 >= argc) {
                printf("head: option '-n' requires an argument\r\n");
                return -1;
            }
            num_lines = 0;
            for (const char *p = argv[i + 1]; *p; p++) {
                if (*p < '0' || *p > '9') {
                    printf("head: invalid number '%s'\r\n", argv[i + 1]);
                    return -1;
                }
                num_lines = num_lines * 10 + (*p - '0');
            }
            i += 2;
        } else {
            filepath = argv[i];
            i++;
        }
    }

    if (!filepath) {
        printf("Usage: head [-n NUM] FILE\r\n");
        return -1;
    }

    struct fs_file fd;
    int r = fs_open(g_fs, filepath, FS_O_RDONLY, &fd);
    if (r != FS_OK) {
        printf("head: cannot open '%s': error %d\r\n", filepath, r);
        return -1;
    }

    uint8_t buf[512];
    int lines_printed = 0;

    while (lines_printed < num_lines) {
        int n = fs_read(g_fs, &fd, buf, sizeof(buf));
        if (n < 0) {
            printf("head: read error: %d\r\n", n);
            fs_close(g_fs, &fd);
            return -1;
        }
        if (n == 0) break;

        for (int j = 0; j < n && lines_printed < num_lines; j++) {
            if (buf[j] == '\n') {
                printf("\r\n");
                lines_printed++;
            } else {
                putchar(buf[j]);
            }
        }
    }

    fs_close(g_fs, &fd);
    return 0;
}

/* ===== cmd_tail ===== */

int cmd_tail(int argc, char **argv) {
    int num_lines = 10;
    const char *filepath = NULL;
    int i = 1;

    while (i < argc) {
        if (strcmp(argv[i], "-n") == 0) {
            if (i + 1 >= argc) {
                printf("tail: option '-n' requires an argument\r\n");
                return -1;
            }
            num_lines = 0;
            for (const char *p = argv[i + 1]; *p; p++) {
                if (*p < '0' || *p > '9') {
                    printf("tail: invalid number '%s'\r\n", argv[i + 1]);
                    return -1;
                }
                num_lines = num_lines * 10 + (*p - '0');
            }
            i += 2;
        } else {
            filepath = argv[i];
            i++;
        }
    }

    if (!filepath) {
        printf("Usage: tail [-n NUM] FILE\r\n");
        return -1;
    }

    struct fs_file fd;
    int r = fs_open(g_fs, filepath, FS_O_RDONLY, &fd);
    if (r != FS_OK) {
        printf("tail: cannot open '%s': error %d\r\n", filepath, r);
        return -1;
    }

    /* Read entire file into buffer (up to 1024 bytes for memory safety) */
    uint8_t buf[1024];
    int total = 0;

    for (;;) {
        int space = (int)sizeof(buf) - total;
        if (space <= 0) break;
        int n = fs_read(g_fs, &fd, buf + total, (uint32_t)space);
        if (n < 0) {
            printf("tail: read error: %d\r\n", n);
            fs_close(g_fs, &fd);
            return -1;
        }
        if (n == 0) break;
        total += n;
    }

    fs_close(g_fs, &fd);

    if (total == 0) return 0;

    /* Count total newlines */
    int total_lines = 0;
    for (int j = 0; j < total; j++) {
        if (buf[j] == '\n') total_lines++;
    }
    /* If file doesn't end with newline, last partial line counts */
    if (buf[total - 1] != '\n') total_lines++;

    /* Find the starting position: skip (total_lines - num_lines) lines */
    int skip = total_lines - num_lines;
    if (skip < 0) skip = 0;

    int pos = 0;
    int skipped = 0;
    while (skipped < skip && pos < total) {
        if (buf[pos] == '\n') skipped++;
        pos++;
    }

    /* Print from pos to end */
    for (int j = pos; j < total; j++) {
        if (buf[j] == '\n')
            printf("\r\n");
        else
            putchar(buf[j]);
    }

    return 0;
}

/* ===== cmd_wc ===== */

int cmd_wc(int argc, char **argv) {
    int flag_l = 0, flag_w = 0, flag_c = 0;
    const char *filepath = NULL;
    int i = 1;

    while (i < argc) {
        if (strcmp(argv[i], "-l") == 0) {
            flag_l = 1; i++;
        } else if (strcmp(argv[i], "-w") == 0) {
            flag_w = 1; i++;
        } else if (strcmp(argv[i], "-c") == 0) {
            flag_c = 1; i++;
        } else {
            filepath = argv[i]; i++;
        }
    }

    if (!filepath) {
        printf("Usage: wc [-l] [-w] [-c] FILE\r\n");
        return -1;
    }

    /* If no flags, show all */
    int show_all = (!flag_l && !flag_w && !flag_c);

    struct fs_file fd;
    int r = fs_open(g_fs, filepath, FS_O_RDONLY, &fd);
    if (r != FS_OK) {
        printf("wc: cannot open '%s': error %d\r\n", filepath, r);
        return -1;
    }

    uint32_t lines = 0, words = 0, bytes = 0;
    int in_word = 0;
    uint8_t buf[512];

    for (;;) {
        int n = fs_read(g_fs, &fd, buf, sizeof(buf));
        if (n < 0) {
            printf("wc: read error: %d\r\n", n);
            fs_close(g_fs, &fd);
            return -1;
        }
        if (n == 0) break;

        bytes += (uint32_t)n;
        for (int j = 0; j < n; j++) {
            if (buf[j] == '\n') lines++;

            if (buf[j] == ' ' || buf[j] == '\t' || buf[j] == '\n' || buf[j] == '\r') {
                if (in_word) {
                    words++;
                    in_word = 0;
                }
            } else {
                in_word = 1;
            }
        }
    }

    /* Count last word if file doesn't end with whitespace */
    if (in_word) words++;

    fs_close(g_fs, &fd);

    if (show_all) {
        printf("  %u  %u  %u %s\r\n", lines, words, bytes, filepath);
    } else {
        if (flag_l) printf("  %u", lines);
        if (flag_w) printf("  %u", words);
        if (flag_c) printf("  %u", bytes);
        printf(" %s\r\n", filepath);
    }

    return 0;
}

/* ===== cmd_grep ===== */

int cmd_grep(int argc, char **argv) {
    int flag_i = 0, flag_n = 0, flag_c = 0, flag_v = 0;
    const char *pattern = NULL;
    const char *filepath = NULL;
    int i = 1;

    while (i < argc) {
        if (strcmp(argv[i], "-i") == 0) {
            flag_i = 1; i++;
        } else if (strcmp(argv[i], "-n") == 0) {
            flag_n = 1; i++;
        } else if (strcmp(argv[i], "-c") == 0) {
            flag_c = 1; i++;
        } else if (strcmp(argv[i], "-v") == 0) {
            flag_v = 1; i++;
        } else if (!pattern) {
            pattern = argv[i]; i++;
        } else {
            filepath = argv[i]; i++;
        }
    }

    if (!pattern) {
        printf("Usage: grep [-i] [-n] [-c] [-v] PATTERN [FILE]\r\n");
        return -1;
    }

    if (!filepath) {
        printf("grep: FILE argument required\r\n");
        return -1;
    }

    struct fs_file fd;
    int r = fs_open(g_fs, filepath, FS_O_RDONLY, &fd);
    if (r != FS_OK) {
        printf("grep: cannot open '%s': error %d\r\n", filepath, r);
        return -1;
    }

    /* Read file and process line by line */
    uint8_t filebuf[1024];
    int total = 0;

    for (;;) {
        int space = (int)sizeof(filebuf) - total;
        if (space <= 0) break;
        int n = fs_read(g_fs, &fd, filebuf + total, (uint32_t)space);
        if (n < 0) {
            printf("grep: read error: %d\r\n", n);
            fs_close(g_fs, &fd);
            return -1;
        }
        if (n == 0) break;
        total += n;
    }

    fs_close(g_fs, &fd);

    /* Process line by line */
    int line_num = 1;
    uint32_t match_count = 0;
    int line_start = 0;

    for (int j = 0; j <= total; j++) {
        if (j == total || filebuf[j] == '\n') {
            /* We have a line from line_start to j-1 */
            int line_len = j - line_start;
            /* Null-terminate temporarily */
            char saved = 0;
            if (j < total) {
                saved = (char)filebuf[j];
                filebuf[j] = '\0';
            }

            const char *line = (const char *)&filebuf[line_start];
            int matched;

            if (flag_i) {
                matched = (strcasestr_simple(line, pattern) != NULL);
            } else {
                matched = (strstr(line, pattern) != NULL);
            }

            if (flag_v) matched = !matched;

            if (matched) {
                match_count++;
                if (!flag_c) {
                    if (flag_n)
                        printf("%d:", line_num);
                    /* Print the line */
                    for (int k = 0; k < line_len; k++) {
                        char ch = (char)filebuf[line_start + k];
                        if (ch == '\r') continue;
                        putchar(ch);
                    }
                    printf("\r\n");
                }
            }

            /* Restore byte */
            if (j < total) filebuf[j] = (uint8_t)saved;

            line_num++;
            line_start = j + 1;
        }
    }

    if (flag_c) {
        printf("%u\r\n", match_count);
    }

    return (match_count > 0) ? 0 : 1;
}

/* ===== cmd_hexdump ===== */

int cmd_hexdump(int argc, char **argv) {
    int max_bytes = -1;  /* -1 means unlimited */
    const char *filepath = NULL;
    int i = 1;

    while (i < argc) {
        if (strcmp(argv[i], "-n") == 0) {
            if (i + 1 >= argc) {
                printf("hexdump: option '-n' requires an argument\r\n");
                return -1;
            }
            max_bytes = 0;
            for (const char *p = argv[i + 1]; *p; p++) {
                if (*p < '0' || *p > '9') {
                    printf("hexdump: invalid number '%s'\r\n", argv[i + 1]);
                    return -1;
                }
                max_bytes = max_bytes * 10 + (*p - '0');
            }
            i += 2;
        } else {
            filepath = argv[i];
            i++;
        }
    }

    if (!filepath) {
        printf("Usage: hexdump [-n NUM] FILE\r\n");
        return -1;
    }

    struct fs_file fd;
    int r = fs_open(g_fs, filepath, FS_O_RDONLY, &fd);
    if (r != FS_OK) {
        printf("hexdump: cannot open '%s': error %d\r\n", filepath, r);
        return -1;
    }

    uint8_t buf[512];
    uint32_t offset = 0;
    int done = 0;

    while (!done) {
        uint32_t to_read = sizeof(buf);
        if (max_bytes >= 0 && (uint32_t)max_bytes - offset < to_read)
            to_read = (uint32_t)max_bytes - offset;
        if (to_read == 0) break;

        int n = fs_read(g_fs, &fd, buf, to_read);
        if (n < 0) {
            printf("hexdump: read error: %d\r\n", n);
            fs_close(g_fs, &fd);
            return -1;
        }
        if (n == 0) break;

        for (int j = 0; j < n; j += 16) {
            int line_bytes = n - j;
            if (line_bytes > 16) line_bytes = 16;

            /* Offset */
            printf("%08x  ", offset + (uint32_t)j);

            /* Hex bytes */
            for (int k = 0; k < 16; k++) {
                if (k == 8) putchar(' ');
                if (k < line_bytes)
                    printf("%02x ", buf[j + k]);
                else
                    printf("   ");
            }

            /* ASCII */
            printf(" |");
            for (int k = 0; k < line_bytes; k++) {
                uint8_t c = buf[j + k];
                putchar((c >= 0x20 && c <= 0x7e) ? c : '.');
            }
            printf("|\r\n");
        }

        offset += (uint32_t)n;
        if (max_bytes >= 0 && offset >= (uint32_t)max_bytes) done = 1;
    }

    printf("%08x\r\n", offset);

    fs_close(g_fs, &fd);
    return 0;
}

/* ===== cmd_tee ===== */

int cmd_tee(int argc, char **argv) {
    int append_mode = 0;
    const char *filepath = NULL;
    int i = 1;

    while (i < argc) {
        if (strcmp(argv[i], "-a") == 0) {
            append_mode = 1; i++;
        } else {
            filepath = argv[i]; i++;
        }
    }

    if (!filepath) {
        printf("Usage: tee [-a] FILE\r\n");
        return -1;
    }

    uint16_t flags;
    if (append_mode)
        flags = FS_O_WRONLY | FS_O_CREAT | FS_O_APPEND;
    else
        flags = FS_O_WRONLY | FS_O_CREAT | FS_O_TRUNC;

    struct fs_file fd;
    int r = fs_open(g_fs, filepath, flags, &fd);
    if (r != FS_OK) {
        printf("tee: cannot open '%s': error %d\r\n", filepath, r);
        return -1;
    }

    uint8_t buf[512];
    int ch;
    int pos = 0;

    /* Read from stdin character by character, flush on newline or buffer full */
    while ((ch = getchar()) != EOF) {
        buf[pos++] = (uint8_t)ch;

        /* Echo to stdout */
        if (ch == '\n')
            printf("\r\n");
        else
            putchar(ch);

        /* Write to file when buffer full or newline */
        if (pos >= (int)sizeof(buf) || ch == '\n') {
            int n = fs_write(g_fs, &fd, buf, (uint32_t)pos);
            if (n < 0) {
                printf("tee: write error: %d\r\n", n);
                fs_close(g_fs, &fd);
                return -1;
            }
            pos = 0;
        }
    }

    /* Flush remaining data */
    if (pos > 0) {
        int n = fs_write(g_fs, &fd, buf, (uint32_t)pos);
        if (n < 0) {
            printf("tee: write error: %d\r\n", n);
            fs_close(g_fs, &fd);
            return -1;
        }
    }

    fs_close(g_fs, &fd);
    return 0;
}
