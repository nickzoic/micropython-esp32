/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * Development of the code in this file was sponsored by Microbric Pty Ltd
 * and Mnemote Pty Ltd
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2016, 2017 Nick Moore @mnemote
 * Copyright (c) 2017 "Eric Poulsen" <eric@zyxod.com>
 *
 * Based on esp8266/modnetwork.c which is Copyright (c) 2015 Paul Sokolovsky
 * And the ESP IDF example code which is Public Domain / CC0
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "py/nlr.h"
#include "py/objlist.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "py/mperrno.h"
#include "netutils.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_log.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "lwip/dns.h"
#include "tcpip_adapter.h"

#include "network_lan.h"
#include "network_wlan.h"

#define MODNETWORK_INCLUDE_CONSTANTS (1)

MP_DECLARE_CONST_FUN_OBJ_KW(get_lan_obj);
MP_DECLARE_CONST_FUN_OBJ_KW(get_wlan_obj);

// This function is called by the system-event task and so runs in a different
// thread to the main MicroPython task.  It must not raise any Python exceptions.
static esp_err_t event_handler(void *ctx, system_event_t *event) {
   switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
    case SYSTEM_EVENT_STA_GOT_IP:
    case SYSTEM_EVENT_STA_DISCONNECTED:
	return wlan_event_handler(ctx, event);
    default:
        ESP_LOGI("network", "event %d", event->event_id);
    }
    return ESP_OK;
}

STATIC mp_obj_t esp_initialize() {
    static int initialized = 0;
    if (!initialized) {
        ESP_LOGD("modnetwork", "Initializing TCP/IP");
        tcpip_adapter_init();
        ESP_LOGD("modnetwork", "Initializing Event Loop");
	// XXX exception
        esp_event_loop_init(event_handler, NULL);
        initialized = 1;
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(esp_initialize_obj, esp_initialize);

#if (WIFI_MODE_STA & WIFI_MODE_AP != WIFI_MODE_NULL || WIFI_MODE_STA | WIFI_MODE_AP != WIFI_MODE_APSTA)
#error WIFI_MODE_STA and WIFI_MODE_AP are supposed to be bitfields!
#endif

STATIC mp_obj_t esp_phy_mode(size_t n_args, const mp_obj_t *args) {
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(esp_phy_mode_obj, 0, 1, esp_phy_mode);


STATIC const mp_map_elem_t mp_module_network_globals_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__), MP_OBJ_NEW_QSTR(MP_QSTR_network) },
    { MP_OBJ_NEW_QSTR(MP_QSTR___init__), (mp_obj_t)&esp_initialize_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_WLAN), (mp_obj_t)&get_wlan_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_LAN), (mp_obj_t)&get_lan_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_phy_mode), (mp_obj_t)&esp_phy_mode_obj },

#if MODNETWORK_INCLUDE_CONSTANTS
    { MP_OBJ_NEW_QSTR(MP_QSTR_STA_IF),
        MP_OBJ_NEW_SMALL_INT(WIFI_IF_STA)},
    { MP_OBJ_NEW_QSTR(MP_QSTR_AP_IF),
        MP_OBJ_NEW_SMALL_INT(WIFI_IF_AP)},

    { MP_OBJ_NEW_QSTR(MP_QSTR_MODE_11B),
        MP_OBJ_NEW_SMALL_INT(WIFI_PROTOCOL_11B) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_MODE_11G),
        MP_OBJ_NEW_SMALL_INT(WIFI_PROTOCOL_11G) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_MODE_11N),
        MP_OBJ_NEW_SMALL_INT(WIFI_PROTOCOL_11N) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_AUTH_OPEN),
        MP_OBJ_NEW_SMALL_INT(WIFI_AUTH_OPEN) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_AUTH_WEP),
        MP_OBJ_NEW_SMALL_INT(WIFI_AUTH_WEP) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_AUTH_WPA_PSK),
        MP_OBJ_NEW_SMALL_INT(WIFI_AUTH_WPA_PSK) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_AUTH_WPA2_PSK),
        MP_OBJ_NEW_SMALL_INT(WIFI_AUTH_WPA2_PSK) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_AUTH_WPA_WPA2_PSK),
        MP_OBJ_NEW_SMALL_INT(WIFI_AUTH_WPA_WPA2_PSK) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_AUTH_MAX),
        MP_OBJ_NEW_SMALL_INT(WIFI_AUTH_MAX) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_PHY_LAN8720),
        MP_OBJ_NEW_SMALL_INT(PHY_LAN8720) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PHY_TLK110),
        MP_OBJ_NEW_SMALL_INT(PHY_TLK110) },
#endif
};

STATIC MP_DEFINE_CONST_DICT(mp_module_network_globals, mp_module_network_globals_table);

const mp_obj_module_t mp_module_network = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mp_module_network_globals,
};
