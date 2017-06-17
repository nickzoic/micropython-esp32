#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "tcpip_adapter.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/ip4.h"

#include "esp_log.h"

QueueHandle_t rrepl_tx_queue, rrepl_rx_queue;
int rrepl_active = 0;

void rrepl_init() {
    rrepl_tx_queue = xQueueCreate(100,1);
    rrepl_rx_queue = xQueueCreate(100,1);
}

void rrepl_task(void *pvParameter) {
    struct addrinfo *repl_ai;
    struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_STREAM };
    lwip_getaddrinfo("0.0.0.0", "2323", &hints, &repl_ai);
    int repl_fd = lwip_socket(repl_ai->ai_family, repl_ai->ai_socktype, repl_ai->ai_protocol);
    lwip_bind_r(repl_fd, repl_ai->ai_addr, repl_ai->ai_addrlen);
    lwip_freeaddrinfo(repl_ai);

    lwip_listen_r(repl_fd, 1);
    struct sockaddr addr;
    socklen_t addr_len = sizeof(addr);

    for (;;) {
        int fd = lwip_accept_r(repl_fd, &addr, &addr_len);
	lwip_fcntl_r(fd, F_SETFL, O_NONBLOCK);
        for (;;) {
	    int c;
	    if (xQueueReceive(rrepl_tx_queue, &c, 1) == pdTRUE) {
		int r = lwip_write_r(fd, &c, 1);
                if (r <= 0) break;
	    }
	    char buf[100];
	    int r = lwip_read_r(fd, &buf, sizeof(buf));
            if (r > 0) {
	        for (int i=0; i<r; i++) {
		   ESP_LOGI("rrepl_task", "RECV %d %02x", i, buf[i]);
		   xQueueSend(rrepl_rx_queue, buf+i, 1);
	        }
	    }
	    else if (r < 0 && errno != EWOULDBLOCK) { 
		    ESP_LOGI("rrepl_task", "recvfrom failed! %d", errno); 
		    break;
	    }
        }
        ESP_LOGI("repl_task", "Close! %d", fd);
        lwip_close_r(fd);
    }
}

void rrepl_tx(char c) {
    if (rrepl_active) xQueueSend(rrepl_tx_queue, &c, 0);
}

int rrepl_rx() {
    if (rrepl_active) return -1;
    char c;
    return xQueueReceive(rrepl_rx_queue, &c, 0) == pdTRUE ? c : -1;
}
