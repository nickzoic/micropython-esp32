#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "tcpip_adapter.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/ip4.h"

#include "esp_log.h"

int rrepl_fd = -1;
int rrepl_rx_state = 0;

SemaphoreHandle_t rrepl_mutex_handle;
StaticSemaphore_t rrepl_mutex_buffer;

void rrepl_init() {
    rrepl_mutex_handle = xSemaphoreCreateMutexStatic(&rrepl_mutex_buffer);
}

int rrepl_mutex_begin() {
    return (pdTRUE == xSemaphoreTake(rrepl_mutex_handle, portMAX_DELAY));
}

void rrepl_mutex_end() {
    xSemaphoreGive(rrepl_mutex_handle);
}

void rrepl_tx_telnet() {
    /* Introduce ourselves with IAC WILL ECHO IAC DON'T ECHO */
    lwip_write_r(rrepl_fd, "\xFF\xFB\x01\xFF\xFE\x01", 6);
}

void rrepl_task(void *pvParameter) {
    struct addrinfo *repl_ai;
    struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_STREAM };
    lwip_getaddrinfo("0.0.0.0", "23", &hints, &repl_ai);
    int bound_fd = lwip_socket(repl_ai->ai_family, repl_ai->ai_socktype, repl_ai->ai_protocol);
    lwip_bind_r(bound_fd, repl_ai->ai_addr, repl_ai->ai_addrlen);
    lwip_freeaddrinfo(repl_ai);

    lwip_listen_r(bound_fd, 1);
    struct sockaddr addr;
    socklen_t addr_len = sizeof(addr);

    for (;;) {
        int fd = lwip_accept_r(bound_fd, &addr, &addr_len);
	if (fd >= 0) {
	    if (rrepl_mutex_begin()) {
	        lwip_fcntl_r(fd, F_SETFL, O_NONBLOCK);
	        if (rrepl_fd >= 0) lwip_close_r(rrepl_fd);
	        rrepl_fd = fd;
	        rrepl_rx_state = 0;
	        rrepl_tx_telnet();
		rrepl_mutex_end();
	    }
	}
    }
}

void rrepl_tx(char c) {
    if (rrepl_mutex_begin()) {
        if (rrepl_fd >= 0) {
            lwip_write_r(rrepl_fd, &c, 1);
	    if (c == 0xFF) lwip_write_r(rrepl_fd, &c, 1);
        }
	rrepl_mutex_end();
    }
}

int rrepl_rx_raw() {
    if (rrepl_fd < 0) return -1;
    char c;
    int r = lwip_read_r(rrepl_fd, &c, 1);
    if (r > 0) return c;
    if (r < 0 && errno != EWOULDBLOCK) {
        lwip_close_r(rrepl_fd);
        rrepl_fd = -1;
    }
    return -1;
}

int rrepl_rx_telnet() {
    /* This is a half-arsed implementation of a small part of the Telnet protocol,
     * as defined in RFC854 and friends.  A "\xFF" escape character introduces 
     * commands which change the terminal behaviour.  We don't know how to respond
     * to most of these so ignore them.
     */

    for (;;) {
	int c = rrepl_rx_raw();
	if (c < 0) return -1;

        switch (rrepl_rx_state) {
	    case 0:
	        if (c != 0xFF) return c;
	        rrepl_rx_state = 1;
	        break;
	    case 1:
		switch (c) {
		    case 0xFF: return c;
                    case 0xFD: rrepl_rx_state = 2; break; // DO
                    case 0xFE: rrepl_rx_state = 3; break; // DON'T
                    case 0xFB: rrepl_rx_state = 3; break; // WILL
                    case 0xFC: rrepl_rx_state = 3; break; // WON'T
	            default: rrepl_rx_state = 0; break;
                }
		break;
	    case 2: 
		if (c == 0x01) lwip_write_r(rrepl_fd, "\xFF\xFB\x01", 3); // WILL ECHO
	        if (c == 0x03) lwip_write_r(rrepl_fd, "\xFF\xFB\x03", 3); // WILL SUPPRESS-GO-AHEAD
		if (c == 0x05) lwip_write_r(rrepl_fd, "\xFF\xFC\x05", 3); // WON'T STATUS
		// nobreak
	    default:
		rrepl_rx_state = 0;
		break;
	}
    }
}

int rrepl_rx() {
    if (rrepl_mutex_begin()) {
        int c = rrepl_rx_telnet();
	rrepl_mutex_end();
	return c;
    }
    return -1;
}
