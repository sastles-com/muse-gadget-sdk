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

#include "atoms3r_i2c.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Pins from M5Unified's AtomS3R board table (internal bus: SDA 45, SCL 0).
// SCL is the ESP32-S3 BOOT strapping pin; the button is on GPIO41 instead.
#define ATOMS3R_I2C_SDA_GPIO 45
#define ATOMS3R_I2C_SCL_GPIO 0

static const char *TAG = "link.atoms3r_i2c";

static i2c_master_bus_handle_t s_bus = NULL;
static SemaphoreHandle_t s_lock = NULL;
static portMUX_TYPE s_lock_mux = portMUX_INITIALIZER_UNLOCKED;

i2c_master_bus_handle_t atoms3r_i2c_bus(void) {
    if (s_bus) return s_bus;
    // Create the guard mutex once; the critical section only covers that.
    taskENTER_CRITICAL(&s_lock_mux);
    if (!s_lock) s_lock = xSemaphoreCreateMutex();
    taskEXIT_CRITICAL(&s_lock_mux);
    if (!s_lock) return NULL;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (!s_bus) {
        i2c_master_bus_config_t bus_cfg = {
            .i2c_port = -1,
            .sda_io_num = ATOMS3R_I2C_SDA_GPIO,
            .scl_io_num = ATOMS3R_I2C_SCL_GPIO,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .flags.enable_internal_pullup = true,
        };
        esp_err_t err = i2c_new_master_bus(&bus_cfg, &s_bus);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "internal I2C bus (SDA=%d SCL=%d) failed: %s",
                     ATOMS3R_I2C_SDA_GPIO, ATOMS3R_I2C_SCL_GPIO,
                     esp_err_to_name(err));
            s_bus = NULL;
        }
    }
    i2c_master_bus_handle_t bus = s_bus;
    xSemaphoreGive(s_lock);
    return bus;
}
