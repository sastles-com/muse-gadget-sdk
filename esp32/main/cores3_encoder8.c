/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cores3_encoder8.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// ---- Protocol (host-tested) -------------------------------------------------

// The unit's register map, from M5Stack's Encoder8 (U153) I2C protocol
// document. Absolute counters are int32 little-endian, four bytes per
// channel: channels 0-3 at 0x00, channels 4-7 at 0x10. The increments
// since the last read (reading them clears them) sit the same way at
// 0x20 and 0x30. One byte per button at 0x50 (nonzero = pressed), the
// slide switch at 0x60 (nonzero = on) and the firmware version at 0xFE.
#define ENCODER8_CHANNELS 8
#define ENCODER8_REG_COUNT_CH0 0x00
#define ENCODER8_REG_COUNT_CH4 0x10
#define ENCODER8_REG_INCREMENT_CH0 0x20
#define ENCODER8_REG_INCREMENT_CH4 0x30
#define ENCODER8_REG_BUTTON_CH0 0x50
#define ENCODER8_REG_SLIDE_SWITCH 0x60
#define ENCODER8_REG_FIRMWARE 0xFE

// The first counter or increment register of a channel.
static uint8_t encoder8_value_reg(int channel, bool increment) {
    uint8_t base = channel < 4
        ? (increment ? ENCODER8_REG_INCREMENT_CH0 : ENCODER8_REG_COUNT_CH0)
        : (increment ? ENCODER8_REG_INCREMENT_CH4 : ENCODER8_REG_COUNT_CH4);
    return (uint8_t)(base + (channel % 4) * 4);
}

// One register dump of the whole unit, as the driver read it: the
// counters and increments as eight int32 each in channel order, the
// buttons as one byte each, and the switch and firmware bytes.
typedef struct {
    uint8_t counts[ENCODER8_CHANNELS * 4];
    uint8_t increments[ENCODER8_CHANNELS * 4];
    uint8_t buttons[ENCODER8_CHANNELS];
    uint8_t slide_switch;
    uint8_t firmware_version;
} encoder8_raw_t;

// One sample of the whole unit, decoded.
typedef struct {
    bool valid;
    int32_t count[ENCODER8_CHANNELS];
    int32_t increment[ENCODER8_CHANNELS];
    bool button[ENCODER8_CHANNELS];
    bool slide_switch;
    int firmware_version;  // -1 when unknown
} encoder8_reading_t;

static int32_t encoder8_le32(const uint8_t b[4]) {
    uint32_t v = (uint32_t)b[0] | ((uint32_t)b[1] << 8)
        | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
    return (int32_t)v;
}

static void encoder8_parse(const encoder8_raw_t *raw, encoder8_reading_t *out) {
    for (int ch = 0; ch < ENCODER8_CHANNELS; ch++) {
        out->count[ch] = encoder8_le32(&raw->counts[ch * 4]);
        out->increment[ch] = encoder8_le32(&raw->increments[ch * 4]);
        out->button[ch] = raw->buttons[ch] != 0;
    }
    out->slide_switch = raw->slide_switch != 0;
    out->firmware_version = raw->firmware_version;
}

// encoders.read result: the channels with their counts and increments,
// the buttons and the slide switch; an error when there is no sample.
static cJSON *readings_result(const encoder8_reading_t *r) {
    cJSON *result = cJSON_CreateObject();
    if (!result) return NULL;
    if (!r->valid) {
        cJSON_AddBoolToObject(result, "ok", false);
        cJSON *error = cJSON_AddObjectToObject(result, "error");
        cJSON_AddStringToObject(error, "code", "no_readings");
        cJSON_AddStringToObject(error, "message",
                                "no response from the Encoder8 unit on "
                                "Grove Port A (I2C address 0x41); is it "
                                "plugged in?");
        return result;
    }
    cJSON *payload = cJSON_CreateObject();
    cJSON *channels = cJSON_CreateArray();
    cJSON *buttons = cJSON_CreateArray();
    bool built = payload && channels && buttons;
    for (int ch = 0; built && ch < ENCODER8_CHANNELS; ch++) {
        cJSON *channel = cJSON_CreateObject();
        cJSON *pressed = cJSON_CreateBool(r->button[ch]);
        if (!channel || !pressed
            || !cJSON_AddNumberToObject(channel, "count", (double)r->count[ch])
            || !cJSON_AddNumberToObject(channel, "increment",
                                        (double)r->increment[ch])
            || !cJSON_AddItemToArray(channels, channel)
            || !cJSON_AddItemToArray(buttons, pressed)) {
            cJSON_Delete(channel);
            cJSON_Delete(pressed);
            built = false;
        }
    }
    if (!built) {
        cJSON_Delete(result);
        cJSON_Delete(payload);
        cJSON_Delete(channels);
        cJSON_Delete(buttons);
        return NULL;
    }
    cJSON_AddItemToObject(payload, "channels", channels);
    cJSON_AddItemToObject(payload, "buttons", buttons);
    cJSON_AddBoolToObject(payload, "slide_switch", r->slide_switch);
    if (r->firmware_version >= 0) {
        cJSON_AddNumberToObject(payload, "firmware_version",
                                r->firmware_version);
    }
    cJSON_AddBoolToObject(result, "ok", true);
    cJSON_AddItemToObject(result, "payload", payload);
    return result;
}

// ---- Encoder8 driver ----------------------------------------------------------

static const char *TAG = "link.encoder8";

// Grove Port A is a bus of its own (SDA GPIO2, SCL GPIO1), separate
// from the internal bus the BSP drives (SDA GPIO12, SCL GPIO11) for the
// PMU, touch controller and codecs, and the ESP32-S3's second I2C
// controller is free. Nothing else uses Port A, so this module owns
// the bus; the BSP never initialises it.
#define ENCODER8_I2C_ADDR 0x41
#define ENCODER8_PORTA_SDA_GPIO 2
#define ENCODER8_PORTA_SCL_GPIO 1
#define ENCODER8_IO_TIMEOUT_MS 50

static i2c_master_bus_handle_t s_bus = NULL;
static i2c_master_dev_handle_t s_dev = NULL;
static SemaphoreHandle_t s_lock = NULL;
static portMUX_TYPE s_lock_mux = portMUX_INITIALIZER_UNLOCKED;
static bool s_fail_logged = false;

static SemaphoreHandle_t encoder8_lock(void) {
    if (s_lock) return s_lock;
    taskENTER_CRITICAL(&s_lock_mux);
    if (!s_lock) s_lock = xSemaphoreCreateMutex();
    taskEXIT_CRITICAL(&s_lock_mux);
    return s_lock;
}

// The Port A bus, created on the first call. Caller holds the lock.
static esp_err_t encoder8_bus_up(void) {
    if (s_bus) return ESP_OK;
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = -1,
        .sda_io_num = ENCODER8_PORTA_SDA_GPIO,
        .scl_io_num = ENCODER8_PORTA_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&bus_cfg, &s_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Port A I2C bus (SDA=%d SCL=%d) failed: %s",
                 ENCODER8_PORTA_SDA_GPIO, ENCODER8_PORTA_SCL_GPIO,
                 esp_err_to_name(err));
        s_bus = NULL;
    }
    return err;
}

// Probe for the unit and add it to the bus. Caller holds the lock.
static esp_err_t encoder8_dev_up(void) {
    esp_err_t err = encoder8_bus_up();
    if (err != ESP_OK) return err;
    err = i2c_master_probe(s_bus, ENCODER8_I2C_ADDR, ENCODER8_IO_TIMEOUT_MS);
    if (err != ESP_OK) return err;
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = ENCODER8_I2C_ADDR,
        .scl_speed_hz = 100000,
    };
    return i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev);
}

static esp_err_t encoder8_read_reg(uint8_t reg, uint8_t *buf, size_t len) {
    return i2c_master_transmit_receive(s_dev, &reg, 1, buf, len,
                                       ENCODER8_IO_TIMEOUT_MS);
}

cJSON *cores3_encoder8_command(void) {
    encoder8_raw_t raw;
    memset(&raw, 0, sizeof(raw));
    encoder8_reading_t reading;
    memset(&reading, 0, sizeof(reading));
    reading.firmware_version = -1;
    SemaphoreHandle_t lock = encoder8_lock();
    if (lock) xSemaphoreTake(lock, portMAX_DELAY);
    esp_err_t err = ESP_OK;
    if (!s_dev) {
        err = encoder8_dev_up();
        if (err != ESP_OK && !s_fail_logged) {
            s_fail_logged = true;
            ESP_LOGW(TAG, "Encoder8 did not respond at 0x%02x on Port A "
                          "(SDA=%d SCL=%d); encoders.read keeps retrying",
                     ENCODER8_I2C_ADDR, ENCODER8_PORTA_SDA_GPIO,
                     ENCODER8_PORTA_SCL_GPIO);
        }
    }
    if (err == ESP_OK) {
        // Counts before increments, so the pair is as close to one
        // instant as the bus allows; every transfer is time-boxed, so
        // a unit that stops answering fails the read instead of
        // hanging the command.
        for (int ch = 0; ch < ENCODER8_CHANNELS && err == ESP_OK; ch++) {
            err = encoder8_read_reg(encoder8_value_reg(ch, false),
                                    &raw.counts[ch * 4], 4);
        }
        for (int ch = 0; ch < ENCODER8_CHANNELS && err == ESP_OK; ch++) {
            err = encoder8_read_reg(encoder8_value_reg(ch, true),
                                    &raw.increments[ch * 4], 4);
        }
        for (int ch = 0; ch < ENCODER8_CHANNELS && err == ESP_OK; ch++) {
            err = encoder8_read_reg((uint8_t)(ENCODER8_REG_BUTTON_CH0 + ch),
                                    &raw.buttons[ch], 1);
        }
        if (err == ESP_OK) {
            err = encoder8_read_reg(ENCODER8_REG_SLIDE_SWITCH,
                                    &raw.slide_switch, 1);
        }
        if (err == ESP_OK) {
            err = encoder8_read_reg(ENCODER8_REG_FIRMWARE,
                                    &raw.firmware_version, 1);
        }
    }
    if (err == ESP_OK) {
        encoder8_parse(&raw, &reading);
        reading.valid = true;
    } else if (s_dev) {
        ESP_LOGW(TAG, "Encoder8 read failed: %s", esp_err_to_name(err));
    }
    if (lock) xSemaphoreGive(lock);
    return readings_result(&reading);
}
