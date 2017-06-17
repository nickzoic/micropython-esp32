#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "tcpip_adapter.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/ip4.h"

#include "esp_log.h"

int rrepl_fd = -1;
int rrepl_rx_state = 0;

void rrepl_init() {
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
	    lwip_fcntl_r(fd, F_SETFL, O_NONBLOCK);
	    if (rrepl_fd >= 0) lwip_close_r(rrepl_fd);
	    rrepl_fd = fd;
	    rrepl_rx_state = 0;
	    lwip_write_r(rrepl_fd, "\xFF\xFB\x01\xFF\xFE\x01", 6); // TELNET IAC WILL ECHO IAC DON'T ECHO
	}
    }
}

void rrepl_tx(char c) {
    if (rrepl_fd >= 0) {
        lwip_write_r(rrepl_fd, &c, 1);
	if (c == 0xFF) lwip_write_r(rrepl_fd, &c, 1);
    }
}


int rrepl_rx() {
    char c;
    if (rrepl_fd < 0) return -1;

    for (;;) {
        int r = lwip_read_r(rrepl_fd, &c, 1);
        if (r < 0 && errno != EWOULDBLOCK) {
            lwip_close_r(rrepl_fd);
	    rrepl_fd = -1;
	    return -1;
        } 
        if (r <= 0) return -1;

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
	            default: rrepl_rx_state = 0; break;   // DUNNO
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
    return -1;
}
