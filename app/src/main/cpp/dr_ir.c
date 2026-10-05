#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "debug_log.h"
#include "ir/ir.h"
#include "picowalker_structures.h"
#include "timer.h"

#include "pw_android.h"

/*
 * IR driver: a byte-stream TCP bridge.
 *
 * The phone has no IrDA hardware, so IR traffic is tunneled to an external
 * bridge (e.g. a PC attached to real IR hardware, or an emulator plug-in)
 * over TCP. The wire format is the raw Pokewalker IR byte stream, exactly
 * what pw_ir_read()/pw_ir_write() would push through the UART.
 *
 * The socket is only ever touched from the core loop thread; the UI thread
 * communicates through the atomic config below.
 */

#define IR_CONNECT_TIMEOUT_MS 500
#define IR_READ_TIMEOUT_MS    200 /* PW_IR_READ_TIMEOUT_US */

static pthread_mutex_t cfg_lock = PTHREAD_MUTEX_INITIALIZER;
static char cfg_host[256] = {0};
static int cfg_port = 0;
static bool cfg_enabled = false;

static int sock = -1;          /* runner thread only */
static char conn_host[256];    /* host of the live connection */
static int conn_port = 0;

static int ir_status = 0; /* 0=off 1=configured 2=connected, read by UI */

static void set_status(int s) {
    __atomic_store_n(&ir_status, s, __ATOMIC_SEQ_CST);
}

int pw_and_ir_status(void) {
    return __atomic_load_n(&ir_status, __ATOMIC_SEQ_CST);
}

static void sock_close(void) {
    if (sock >= 0) {
        close(sock);
        sock = -1;
    }
}

void pw_ir_init() {
    set_status(cfg_enabled ? 1 : 0);
}

void pw_ir_deinit() {
    sock_close();
    set_status(cfg_enabled ? 1 : 0);
}

void pw_ir_sleep() {}

void pw_ir_wake() {
    /* Best effort pre-connect when a comms session starts. */
    if (sock < 0 && cfg_enabled && cfg_host[0] != '\0') {
        (void)pw_ir_write(NULL, 0); /* triggers connect via ensure_connected below */
    }
}

/* Read config; returns false when bridging is disabled. */
static bool config_snapshot(char *host, size_t host_len, int *port) {
    bool ok;
    pthread_mutex_lock(&cfg_lock);
    ok = cfg_enabled && cfg_host[0] != '\0';
    if (ok) {
        strncpy(host, cfg_host, host_len - 1);
        host[host_len - 1] = '\0';
        *port = cfg_port;
    }
    pthread_mutex_unlock(&cfg_lock);
    return ok;
}

static int ensure_connected(void) {
    char host[256];
    int port = 0;

    if (sock >= 0) {
        /* Reconnect if the target changed while connected. */
        if (config_snapshot(host, sizeof(host), &port) && (port != conn_port || strcmp(host, conn_host) != 0)) {
            sock_close();
        } else {
            return 0;
        }
    }

    if (!config_snapshot(host, sizeof(host), &port)) {
        set_status(0);
        return -1;
    }

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *res = NULL;
    if (getaddrinfo(host, port_str, &hints, &res) != 0 || res == NULL) {
        set_status(1);
        return -1;
    }

    int fd = -1;
    for (struct addrinfo *ai = res; ai != NULL; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) continue;

        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        int rc = connect(fd, ai->ai_addr, ai->ai_addrlen);
        if (rc == 0) {
            fcntl(fd, F_SETFL, flags);
            break;
        }
        if (errno == EINPROGRESS) {
            struct pollfd pfd = {.fd = fd, .events = POLLOUT};
            rc = poll(&pfd, 1, IR_CONNECT_TIMEOUT_MS);
            if (rc == 1) {
                int soerr = 0;
                socklen_t slen = sizeof(soerr);
                getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &slen);
                if (soerr == 0) {
                    fcntl(fd, F_SETFL, flags);
                    break;
                }
            }
        }
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);

    if (fd < 0) {
        set_status(1);
        return -1;
    }

    /* Keep reads from stalling the loop for minutes on a dead peer. */
    struct timeval tv = {.tv_sec = 0, .tv_usec = 500000};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    sock = fd;
    strncpy(conn_host, host, sizeof(conn_host) - 1);
    conn_host[sizeof(conn_host) - 1] = '\0';
    conn_port = port;
    set_status(2);
    pw_log_info("IR bridge connected to %s:%d\n", host, port);
    return 0;
}

int pw_ir_write(uint8_t *buf, size_t len) {
    if (len == 0) {
        /* used as a connect probe */
        return (ensure_connected() == 0) ? 0 : -1;
    }
    if (buf == NULL) return -1;
    if (ensure_connected() < 0) return 0;

    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(sock, buf + sent, len - sent, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) continue;
            pw_log_warn("IR bridge write failed: %s\n", strerror(errno));
            sock_close();
            set_status(cfg_enabled ? 1 : 0);
            return (int)sent;
        }
        sent += (size_t)n;
    }
    return (int)sent;
}

int pw_ir_read(uint8_t *buf, size_t len) {
    if (buf == NULL || len == 0) return 0;

    if (sock < 0) {
        if (ensure_connected() < 0) {
            /* Nothing listening: behave like an idle IR receiver. */
            return 0;
        }
    }

    size_t got = 0;

    /* First byte waits the full IR read timeout; bytes inside a packet get a
     * short inter-byte window (hardware uses PW_IR_BYTE_TIMEOUT_US). */
    for (int iter = 0; iter < 256 && got < len; iter++) {
        int timeout = (got == 0) ? IR_READ_TIMEOUT_MS : 10;

        struct pollfd pfd = {.fd = sock, .events = POLLIN};
        int rc = poll(&pfd, 1, timeout);
        if (rc == 0) break; /* timeout */
        if (rc < 0) {
            if (errno == EINTR) continue;
            sock_close();
            set_status(cfg_enabled ? 1 : 0);
            return (int)got;
        }
        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            pw_log_warn("IR bridge closed by peer\n");
            sock_close();
            set_status(cfg_enabled ? 1 : 0);
            return (int)got;
        }

        ssize_t n = recv(sock, buf + got, len - got, 0);
        if (n < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
            sock_close();
            set_status(cfg_enabled ? 1 : 0);
            return (int)got;
        }
        if (n == 0) {
            sock_close();
            set_status(cfg_enabled ? 1 : 0);
            return (int)got;
        }
        got += (size_t)n;
    }

    return (int)got;
}

void pw_and_ir_set_config(const char *host, int port, bool enabled) {
    pthread_mutex_lock(&cfg_lock);
    if (host != NULL && host[0] != '\0') {
        strncpy(cfg_host, host, sizeof(cfg_host) - 1);
        cfg_host[sizeof(cfg_host) - 1] = '\0';
        cfg_port = port;
    } else {
        cfg_host[0] = '\0';
        cfg_port = 0;
        enabled = false;
    }
    cfg_enabled = enabled;
    pthread_mutex_unlock(&cfg_lock);

    if (!enabled) set_status(0);
    else if (pw_and_ir_status() == 0) set_status(1);
}

void pw_and_ir_shutdown(void) {
    sock_close();
}
