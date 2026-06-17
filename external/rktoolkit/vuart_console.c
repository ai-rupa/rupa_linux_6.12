/*
 * Copyright 2025 Rockchip Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <time.h>

#define VENDOR_ID                        0x1d87

/* Driver version info */
#define SHM_VUART_MAGIC_NUMBER      0x524B5655
#define SHM_VUART_DRV_VERSION       0x10000000

/* Small buffer size for simplified design */
#define VUART_BUFFER_SIZE                256

/* Shared memory structure definition - simplified design */
struct vuart_shm {
    volatile uint32_t  magic;                                              /* Magic number for validation */
    volatile uint32_t  version;                                            /* Version number */
    volatile uint32_t  master_to_slave_flag;                               /* Master to Slave ready flag */
    volatile uint32_t  slave_to_master_flag;                               /* Slave to Master ready flag */
    volatile uint32_t  master_to_slave_len;                                /* Master to Slave data length */
    volatile uint32_t  slave_to_master_len;                                /* Slave to Master data length */
    volatile uint32_t  master_to_slave_buf[VUART_BUFFER_SIZE / sizeof(uint32_t)]; /* Master to Slave data buffer */
    volatile uint32_t  slave_to_master_buf[VUART_BUFFER_SIZE / sizeof(uint32_t)]; /* Slave to Master data buffer */
    volatile uint32_t  master_rx_enabled;                                  /* Master is ready to recieve data */
};

/* Device context structure */
struct vuart_context {
    void *bar_addr;                       /* Virtual address of mapped BAR */
    size_t bar_size;                      /* Size of the BAR */
    struct vuart_shm *shm;           /* Shared memory pointer */
    int memfd;
    pthread_t rx_thread;                  /* RX polling thread */
    int running;                          /* Thread control flag */
    struct termios orig_termios;          /* Original terminal settings */
    int raw_mode;                         /* Terminal raw mode flag */
    __attribute__((aligned(sizeof(uint32_t)))) char input_buffer[VUART_BUFFER_SIZE]; /* Input line buffer */
    int buffer_pos;                       /* Current position in buffer */

    /* Oneshot mode support */
    int is_oneshot;
    int debug_enabled;
    int command_sent;
    struct timespec last_rx_time;
    char oneshot_cmd[VUART_BUFFER_SIZE];
};

#define VUART_DEBUG(fmt, ...) \
    do { if (g_ctx.debug_enabled) printf(fmt, ##__VA_ARGS__); } while (0)


/* Global context */
static struct vuart_context g_ctx;

/* Function prototypes */
static int setup_vuart_device(uint32_t addr);
static int setup_terminal_raw_mode(void);
static void restore_terminal_settings(void);
static void *rx_polling_thread(void *arg);
static void send_string_to_slave(const char *str, size_t len);
static void process_input_char(unsigned char c);
static void cleanup_and_exit(int status);

/* Signal handler for clean exit */
static void signal_handler(int sig)
{
    printf("\r\nReceived signal %d, exiting...\r\n", sig);
    cleanup_and_exit(0);
}

/* Initialize the device and map space */
static int setup_vuart_device(uint32_t addr)
{
    g_ctx.memfd = open("/dev/mem", O_RDWR | O_SYNC);
    if (g_ctx.memfd < 0) {
        perror("open /dev/mem failed");
        exit(EXIT_FAILURE);
    }

    g_ctx.bar_addr = mmap(NULL, 0x280 * 2, PROT_READ | PROT_WRITE, MAP_SHARED, g_ctx.memfd, addr);
    if (g_ctx.bar_addr == MAP_FAILED) {
        perror("mmap failed");
        close(g_ctx.memfd);
        exit(EXIT_FAILURE);
    }

    g_ctx.bar_size = sizeof(struct vuart_shm);
    g_ctx.shm = (struct vuart_shm *)(g_ctx.bar_addr);

    /* 校验magic和版本号 */
    if (g_ctx.shm->magic != SHM_VUART_MAGIC_NUMBER || g_ctx.shm->version != SHM_VUART_DRV_VERSION) {
        printf("magic=%x or version=%x error, addr=0x%x\n", g_ctx.shm->magic, g_ctx.shm->version, addr);
        return -1;
    }

    return 0;
}

/* Set terminal to raw mode for character-by-character input */
static int setup_terminal_raw_mode(void)
{
    struct termios new_termios;

    /* Save original terminal settings */
    if (tcgetattr(STDIN_FILENO, &g_ctx.orig_termios) < 0) {
        perror("Failed to get terminal attributes\r\n");
        return -1;
    }

    /* Copy original settings and modify for raw mode */
    new_termios = g_ctx.orig_termios;

    /* Input modes: no break, no CR to NL, no parity check, no strip char, no start/stop */
    new_termios.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

    /* Output modes: disable post-processing */
    new_termios.c_oflag &= ~OPOST;
    new_termios.c_oflag |= ONLCR;

    /* Control modes: set 8-bit chars */
    new_termios.c_cflag |= (CS8);

    /* Local modes: echoing off, canonical off, no extended functions, no signal chars */
    new_termios.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    /* Control chars: set return condition (min number of bytes and timer) */
    new_termios.c_cc[VMIN] = 1;  /* Read returns when one char is available */
    new_termios.c_cc[VTIME] = 0; /* No timeout */

    /* Apply new settings */
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_termios) < 0) {
        perror("Failed to set terminal attributes\r\n");
        return -1;
    }

    g_ctx.raw_mode = 1;
    return 0;
}

/* Restore original terminal settings */
static void restore_terminal_settings(void)
{
    if (g_ctx.raw_mode) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_ctx.orig_termios);
        g_ctx.raw_mode = 0;
    }
}

typedef uint64_t phys_addr_t;
static void safe_memcpy(void* dest, volatile const void* src, uint64_t size)
{
    const volatile uint8_t* src_ptr = (const volatile uint8_t*)src;
    uint8_t* dest_ptr = (uint8_t*)dest;

    if (size == 0) {
        return;
    }

    uint64_t unaligned_start = (phys_addr_t)src % 4;
    if (unaligned_start) {
        uint64_t aligned_start = 4 - unaligned_start;
        uint64_t i;
        for (i = 0; i < aligned_start && i < size; i++) {
            dest_ptr[i] = src_ptr[i];
        }
        src_ptr += i;
        dest_ptr += i;
        size -= i;
    }

    uint64_t aligned_size = size / 4 * 4;
    if (aligned_size) {
        const volatile uint32_t* src_u32_ptr = (const volatile uint32_t*)src_ptr;
        uint32_t* dest_u32_ptr = (uint32_t*)dest_ptr;
        uint64_t count = aligned_size / 4;
        for (uint64_t i = 0; i < count; i++) {
            dest_u32_ptr[i] = src_u32_ptr[i];
        }
        src_ptr += aligned_size;
        dest_ptr += aligned_size;
        size -= aligned_size;
    }

    if (size) {
        for (uint64_t i = 0; i < size; i++) {
            dest_ptr[i] = src_ptr[i];
        }
    }
}

/* Thread to poll for incoming data from Slave */
static void *rx_polling_thread(void *arg)
{
    uint32_t len;
    __attribute__((aligned(sizeof(uint32_t)))) uint8_t szBuf[VUART_BUFFER_SIZE + 1];
    uint32_t *src, *dst;

    printf("RX polling thread started\r\n");
    g_ctx.shm->master_rx_enabled = 1;
    while (g_ctx.running) {
        /* Check if there's data from Slave */
        if (g_ctx.shm->slave_to_master_flag) {
            /* Round up to 4 bytes */
            len = g_ctx.shm->slave_to_master_len;

            /* Copy data from shared memory to local buffer */
            src = (uint32_t *)g_ctx.shm->slave_to_master_buf;
            dst = (uint32_t *)szBuf;
            safe_memcpy(dst, src, len);
            /* Print received byte */
            len = g_ctx.shm->slave_to_master_len;
            for (int i = 0; i < len; i++) {
                putchar(szBuf[i]);

                if (g_ctx.is_oneshot && g_ctx.command_sent) {
                    /* Update timestamp for timeout detection */
                    clock_gettime(CLOCK_MONOTONIC, &g_ctx.last_rx_time);

                    /* Simple state machine to match "msh >" prompt */
                    static int match_idx = 0;
                    const char *prompt = "msh >";
                    if (szBuf[i] == prompt[match_idx]) {
                        match_idx++;
                        if (prompt[match_idx] == '\0') {
                            VUART_DEBUG("\n[DEBUG] Found prompt '%s', signaling exit...\n", prompt);
                            g_ctx.running = 0; /* Found prompt, signal exit */
                        }
                    } else {
                        /* Reset if no match, check if current char is the start of prompt */
                        if (szBuf[i] == prompt[0]) {
                            match_idx = 1;
                        } else {
                            match_idx = 0;
                        }
                    }
                }
            }
            /* Clear flag to acknowledge receipt */
            g_ctx.shm->slave_to_master_len = 0;
            g_ctx.shm->slave_to_master_flag = 0;
        }

        /* Short delay to avoid CPU hogging */
        if (!g_ctx.shm->slave_to_master_flag) {
            fflush(stdout);
            usleep(10000); /* 10ms */
        }
    }

    printf("RX polling thread exiting\r\n");
    return NULL;
}

/* Send a string to the Slave */
static void send_string_to_slave(const char *str, size_t len)
{
    int timeout = 1000; /* 10 seconds total timeout (1000 * 10ms) */
    uint32_t *src, *dst;
    uint8_t szLast[sizeof(uint32_t)];

    if ((uintptr_t)str % sizeof(uint32_t) != 0) {
        perror("String address not aligned to 4 bytes\r\n");
        return;
    }

    /* Wait for the previous byte to be processed (master_to_slave_flag cleared) */
    while (g_ctx.shm->master_to_slave_flag && --timeout > 0) {
        usleep(10000); /* 10ms */
    }

    if (timeout <= 0) {
        /* Timeout waiting for Slave to be ready */
        perror("Timeout waiting for Slave to be ready\r\n");
        return;
    }

    /* Copy string to shared memory buffer */
    src = (uint32_t *)str;
    dst = (uint32_t *)g_ctx.shm->master_to_slave_buf;
    for (int i = 0; i < len / sizeof(uint32_t); i++)
        dst[i] = src[i];
    if (len % sizeof(uint32_t) != 0) {
        /* Copy remaining bytes */
        memset(szLast, 0, sizeof(uint32_t));
        memcpy(szLast, str + len - (len % sizeof(uint32_t)), len % sizeof(uint32_t));
        dst[len / sizeof(uint32_t)] = *(uint32_t *)szLast;
    }

    /* Set flag to indicate data is ready to be sent */
    g_ctx.shm->master_to_slave_len = len;
    g_ctx.shm->master_to_slave_flag = 1;
}

/* Process an input character - buffer until newline */
static void process_input_char(unsigned char c)
{
    /* Echo the character locally */
    putchar(c);
    fflush(stdout);

    /* Check for special characters */
    if (c == 24) { /* Ctrl+X - exit */
        printf("\r\nCtrl+X pressed, exiting...\r\n");
        g_ctx.running = 0;
        g_ctx.shm->master_rx_enabled = 0;
        return;
    }

    /* Handle backspace */
    if (c == 127 || c == 8) {
        if (g_ctx.buffer_pos > 0) {
            g_ctx.buffer_pos--;
            /* Optional: echo backspace properly with " \b" */
            putchar('\b');
            putchar(' ');
            putchar('\b');
            fflush(stdout);
        }
        return;
    }

    /* Add character to buffer */
    if (g_ctx.buffer_pos < VUART_BUFFER_SIZE - 1) {
        g_ctx.input_buffer[g_ctx.buffer_pos++] = c;
    }

    /* If newline or buffer full, send to Slave */
    if (c == '\n' || c == '\r' || g_ctx.buffer_pos >= VUART_BUFFER_SIZE - 1) {
        /* Ensure newline at end */
        if (g_ctx.buffer_pos > 0 && g_ctx.input_buffer[g_ctx.buffer_pos - 1] != '\n' &&
            g_ctx.input_buffer[g_ctx.buffer_pos - 1] != '\r') {
            g_ctx.input_buffer[g_ctx.buffer_pos++] = '\n';
        }

        /* Send buffer to Slave */
        send_string_to_slave(g_ctx.input_buffer, g_ctx.buffer_pos);

        /* Reset buffer */
        g_ctx.buffer_pos = 0;
    }
}

/* Clean up resources and exit */
static void cleanup_and_exit(int status)
{
    /* Stop the polling thread */
    if (g_ctx.running || g_ctx.is_oneshot) {
        g_ctx.running = 0;
        g_ctx.shm->master_rx_enabled = 0;
        pthread_join(g_ctx.rx_thread, NULL);
    }

    /* Restore terminal settings */
    restore_terminal_settings();

    if (g_ctx.memfd >= 0)
        close(g_ctx.memfd);

    if (!g_ctx.is_oneshot)
        printf("VUART Master application terminated\r\n");
    exit(status);
}

static int vuart_main(uint32_t addr)
{
    /* Initialize context - Preserve fields set in parse_variable */
    g_ctx.running = 1;
    g_ctx.memfd = -1;
    g_ctx.buffer_pos = 0;
    g_ctx.command_sent = 0;

    if (g_ctx.is_oneshot) {
        VUART_DEBUG("[DEBUG] Oneshot mode: sending command '%s'\n", g_ctx.oneshot_cmd);
    }

    /* Setup signal handlers for clean exit */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Setup vuart device */
    if (setup_vuart_device(addr) != 0) {
        perror("Failed to setup vuart device\r\n");
        cleanup_and_exit(1);
    }

    /* Setup terminal for raw input */
    if (!g_ctx.is_oneshot) {
        if (setup_terminal_raw_mode() != 0) {
            perror("Failed to setup terminal\r\n");
            cleanup_and_exit(1);
        }
    }

    /* Start RX polling thread */
    if (pthread_create(&g_ctx.rx_thread, NULL, rx_polling_thread, NULL) != 0) {
        perror("Failed to create RX polling thread\r\n");
        cleanup_and_exit(1);
    }

    if (!g_ctx.is_oneshot) {
        printf("VUART Master (Version 0x%x) started\r\n", SHM_VUART_DRV_VERSION);
        printf("Line buffered mode: Input will be sent to Slave on newline\r\n");
        printf("Press Ctrl+X to exit\r\n");

        /* Main loop for user input */
        unsigned char byte;
        while (g_ctx.running) {
            /* Read a character from stdin */
            int n = read(STDIN_FILENO, &byte, 1);
            if (n > 0) {
                /* Process the input character */
                process_input_char(byte);
            } else if (n < 0 && errno != EINTR) {
                perror("Error reading from stdin\r\n");
                break;
            }
        }
    } else {
        /* Oneshot mode: send command and wait */
        int cmd_len = strlen(g_ctx.oneshot_cmd);
        if (cmd_len > 0 && g_ctx.oneshot_cmd[cmd_len - 1] != '\n' && g_ctx.oneshot_cmd[cmd_len - 1] != '\r') {
            strcat(g_ctx.oneshot_cmd, "\n");
            cmd_len++;
        }

        /* Initial timestamp */
        clock_gettime(CLOCK_MONOTONIC, &g_ctx.last_rx_time);
        send_string_to_slave(g_ctx.oneshot_cmd, cmd_len);
        g_ctx.command_sent = 1;

        /* Wait loop for prompt or timeout */
        while (g_ctx.running) {
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            double elapsed = (now.tv_sec - g_ctx.last_rx_time.tv_sec) +
                             (now.tv_nsec - g_ctx.last_rx_time.tv_nsec) / 1e9;
            if (elapsed > 0.8) { /* Extend to 800ms timeout for stability */
                VUART_DEBUG("\n[DEBUG] Oneshot timeout reached.\n");
                break;
            }
            usleep(10000); /* 10ms */
        }
        fflush(stdout);
    }

    /* Clean up and exit */
    cleanup_and_exit(0);
    return 0; /* Never reached */
}

static void usage(void)
{
    printf("VUART Shared Memory Communication Tool\n");
    printf("Usage: vuart_console <physical_address> [command] [--debug]\n");
    printf("\n");
    printf("Arguments:\n");
    printf("  <physical_address>  Physical address of the VUART shared memory region (hexadecimal)\n");
    printf("  [command]           Optional: Command to run on the Slave. If provided, tool will run\n");
    printf("                      the command, print output, and exit (oneshot mode).\n");
    printf("  --debug             Optional: Enable debug prints.\n");
    printf("\n");
    printf("Description:\n");
    printf("  This tool establishes a virtual UART communication channel using shared memory.\n");
    printf("  It maps the specified physical address and provides bidirectional communication\n");
    printf("  between the Master and Slave.\n");
    printf("\n");
    printf("Example:\n");
    printf("  Interactive mode: vuart_console 0x47c00000\n");
    printf("  Oneshot mode:     vuart_console 0x47c00000 \"ls /\"\n");
    printf("\n");
}

static int parse_variable(int argc, char *argv[], uint32_t *addr)
{
    int pos_argc = 0;
    char *pos_argv[3];

    /* Check for help and debug flags */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage();
            return 1;
        }
        if (strcmp(argv[i], "--debug") == 0) {
            g_ctx.debug_enabled = 1;
        } else {
            if (pos_argc < 3) {
                pos_argv[pos_argc++] = (char *)argv[i];
            }
        }
    }

    /* Parse positional arguments */
    if (pos_argc >= 1) {
        /* Parse hexadecimal or decimal address */
        if (strncmp(pos_argv[0], "0x", 2) == 0 || strncmp(pos_argv[0], "0X", 2) == 0) {
            if (sscanf(pos_argv[0], "%x", addr) != 1) {
                fprintf(stderr, "Error: Invalid hexadecimal address format: %s\n", pos_argv[0]);
                usage();
                return -1;
            }
        } else {
            if (sscanf(pos_argv[0], "%u", addr) != 1) {
                fprintf(stderr, "Error: Invalid address format: %s\n", pos_argv[0]);
                usage();
                return -1;
            }
        }

        /* Check for optional command */
        if (pos_argc == 2) {
            g_ctx.is_oneshot = 1;
            strncpy(g_ctx.oneshot_cmd, pos_argv[1], VUART_BUFFER_SIZE - 2);
            g_ctx.oneshot_cmd[VUART_BUFFER_SIZE - 2] = '\0';
        }

        if (pos_argc > 2) {
            fprintf(stderr, "Error: Too many positional arguments\n");
            usage();
            return -1;
        }
    } else {
        printf("No address specified\n");
        usage();
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    uint32_t addr;
    int ret;

    ret = parse_variable(argc, argv, &addr);
    if (ret) {
        return ret;
    }

    return vuart_main(addr);
}

