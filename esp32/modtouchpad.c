/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * Development of the code in this file was sponsored by Microbric Pty Ltd
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Nick Moore
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

#include "esp_spi_flash.h"
#include "driver/touch_pad.h"

#include "py/nlr.h"
#include "py/objlist.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "py/mperrno.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "soc/sens_reg.h"
#include "soc/rtc_cntl_reg.h"

volatile uint32_t touchpad_flags = 0;

void touchpad_intr(void *arg) {
   uint32_t rtc_intr = READ_PERI_REG(RTC_CNTL_INT_ST_REG);
   WRITE_PERI_REG(RTC_CNTL_INT_CLR_REG, rtc_intr);
   SET_PERI_REG_MASK(SENS_SAR_TOUCH_CTRL2_REG, SENS_TOUCH_MEAS_EN_CLR);
   if (rtc_intr & RTC_CNTL_TOUCH_INT_ST) {
       touchpad_flags |= READ_PERI_REG(SENS_SAR_TOUCH_CTRL2_REG) & 0x3ff;
   }
}

STATIC mp_obj_t touchpad_init() {
    static int initialized = 0;
    if (!initialized) {
        ESP_LOGD("modtouchpad", "initializing");
        touch_pad_init();
        touch_pad_isr_handler_register(touchpad_intr, NULL, 0, NULL);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(touchpad_init_obj, touchpad_init);

STATIC mp_obj_t touchpad_config(mp_obj_t num_obj, mp_obj_t threshold_obj) {
    touch_pad_t num = mp_obj_get_int(num_obj);
    uint16_t threshold = mp_obj_get_int(threshold_obj);
    esp_err_t err = touch_pad_config(num, threshold);
    if (err != ESP_OK) ESP_LOGD("touchpad_config", "%d", err);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(touchpad_config_obj, touchpad_config);

STATIC mp_obj_t touchpad_read(mp_obj_t num_obj) {
    touch_pad_t num = mp_obj_get_int(num_obj);
    uint16_t value = 0;
    esp_err_t err = touch_pad_read(num, &value);
    if (err != ESP_OK) {
        ESP_LOGD("touchpad_config", "%d", err);
        return mp_const_none;
    }
    return MP_OBJ_NEW_SMALL_INT(value);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(touchpad_read_obj, touchpad_read);

STATIC mp_obj_t touchpad_poll(mp_obj_t num_obj) {
    touch_pad_t mask = 1 << mp_obj_get_int(num_obj);
    if (touchpad_flags & mask) {
        touchpad_flags &= ~mask;
        return mp_const_true;
    }
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(touchpad_poll_obj, touchpad_poll);

STATIC mp_obj_t touchpad_pollall() {
    uint32_t flags = touchpad_flags;
    touchpad_flags = 0;
    return MP_OBJ_NEW_SMALL_INT(flags);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(touchpad_pollall_obj, touchpad_pollall);

STATIC const mp_rom_map_elem_t touchpad_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_touchpad) },
    { MP_ROM_QSTR(MP_QSTR___init__), MP_ROM_PTR(&touchpad_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_config), MP_ROM_PTR(&touchpad_config_obj) },
    { MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&touchpad_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_poll), MP_ROM_PTR(&touchpad_poll_obj) },
    { MP_ROM_QSTR(MP_QSTR_pollall), MP_ROM_PTR(&touchpad_pollall_obj) },
};

STATIC MP_DEFINE_CONST_DICT(touchpad_module_globals, touchpad_module_globals_table);

const mp_obj_module_t mp_module_touchpad = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&touchpad_module_globals,
};
