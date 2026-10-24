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

#include "atoms3r_imu.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "atoms3r_i2c.h"
#include "bmi270.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// ---- Protocol (host-tested) -------------------------------------------------

// One BMI270 sample, in the sensor's own frame: acceleration in g and
// rotation rate in degrees per second. (M5Unified presents the AtomS3R's
// axes remapped to the board — X and Y swapped, Y inverted; this command
// reports the frame the chip reports, and the handoff notes the choice.)
typedef struct {
    bool valid;
    float accel_g[3];
    float gyro_dps[3];
    int64_t at_us;
} imu_reading_t;

static double rounded(float value, int decimals) {
    double scale = pow(10, decimals);
    return round(value * scale) / scale;
}

// One axis triplet as {value: {x, y, z}, unit, age_s}: the sensors.read
// reading envelope, with a vector in place of the scalar value.
static cJSON *axis_reading(const float v[3], const char *unit, int decimals,
                           int64_t age_s) {
    cJSON *sensor = cJSON_CreateObject();
    cJSON *value = cJSON_CreateObject();
    if (!sensor || !value) {
        cJSON_Delete(sensor);
        cJSON_Delete(value);
        return NULL;
    }
    cJSON_AddNumberToObject(value, "x", rounded(v[0], decimals));
    cJSON_AddNumberToObject(value, "y", rounded(v[1], decimals));
    cJSON_AddNumberToObject(value, "z", rounded(v[2], decimals));
    cJSON_AddItemToObject(sensor, "value", value);
    cJSON_AddStringToObject(sensor, "unit", unit);
    cJSON_AddNumberToObject(sensor, "age_s", (double)age_s);
    return sensor;
}

// sensors.read result: the accelerometer and gyroscope with their units
// and the sample's age in seconds; an error when there is no sample.
static cJSON *readings_result(const imu_reading_t *r, int64_t now_us) {
    cJSON *result = cJSON_CreateObject();
    if (!result) return NULL;
    if (!r->valid) {
        cJSON_AddBoolToObject(result, "ok", false);
        cJSON *error = cJSON_AddObjectToObject(result, "error");
        cJSON_AddStringToObject(error, "code", "no_readings");
        cJSON_AddStringToObject(error, "message",
                                "no reading from the BMI270 IMU; it did not "
                                "respond on the internal I2C bus");
        return result;
    }
    cJSON *payload = cJSON_CreateObject();
    int64_t age_s = (now_us - r->at_us) / 1000000;
    cJSON *accel = axis_reading(r->accel_g, "g", 3, age_s);
    cJSON *gyro = axis_reading(r->gyro_dps, "dps", 1, age_s);
    if (!payload || !accel || !gyro) {
        cJSON_Delete(result);
        cJSON_Delete(payload);
        cJSON_Delete(accel);
        cJSON_Delete(gyro);
        return NULL;
    }
    cJSON_AddItemToObject(payload, "accelerometer", accel);
    cJSON_AddItemToObject(payload, "gyroscope", gyro);
    cJSON_AddBoolToObject(result, "ok", true);
    cJSON_AddItemToObject(result, "payload", payload);
    return result;
}

// ---- BMI270 driver ------------------------------------------------------------

// The espressif/bmi270 component does the chip-ID check, the soft reset
// and the configuration-file upload in bmi270_create(), and scales the
// raw axes in bmi270_get_acce_data() / bmi270_get_gyro_data().

static const char *TAG = "link.imu";

static bmi270_handle_t *s_imu = NULL;
static SemaphoreHandle_t s_lock = NULL;
static portMUX_TYPE s_lock_mux = portMUX_INITIALIZER_UNLOCKED;
static bool s_fail_logged = false;

static SemaphoreHandle_t imu_lock(void) {
    if (s_lock) return s_lock;
    taskENTER_CRITICAL(&s_lock_mux);
    if (!s_lock) s_lock = xSemaphoreCreateMutex();
    taskEXIT_CRITICAL(&s_lock_mux);
    return s_lock;
}

// Probe the addresses M5Unified tries for its BMI270 class (0x69 first,
// then 0x68) and start sampling at 100 Hz.
static esp_err_t imu_init(i2c_master_bus_handle_t bus) {
    static const uint8_t addrs[] = {0x69, 0x68};
    for (size_t i = 0; i < sizeof(addrs) / sizeof(addrs[0]); i++) {
        bmi270_driver_config_t drv = {
            .addr = addrs[i],
            .interface = BMI270_USE_I2C,
            .i2c_bus = bus,
        };
        bmi270_handle_t *imu = NULL;
        if (bmi270_create(&drv, &imu) != ESP_OK) continue;
        const bmi270_config_t cfg = {
            .acce_odr = BMI270_ACC_ODR_100_HZ,
            .acce_range = BMI270_ACC_RANGE_2_G,
            .gyro_odr = BMI270_GYR_ODR_100_HZ,
            .gyro_range = BMI270_GYR_RANGE_2000_DPS,
        };
        if (bmi270_start(imu, &cfg) == ESP_OK) {
            s_imu = imu;
            ESP_LOGI(TAG, "BMI270 at 0x%02x: accel +/-2 g, gyro +/-2000 dps, 100 Hz",
                     addrs[i]);
            return ESP_OK;
        }
        bmi270_delete(imu);
    }
    return ESP_ERR_NOT_FOUND;
}

cJSON *atoms3r_imu_command(void) {
    imu_reading_t reading = {0};
    SemaphoreHandle_t lock = imu_lock();
    if (lock) xSemaphoreTake(lock, portMAX_DELAY);
    if (!s_imu) {
        i2c_master_bus_handle_t bus = atoms3r_i2c_bus();
        if (!bus || imu_init(bus) != ESP_OK) {
            if (!s_fail_logged) {
                s_fail_logged = true;
                ESP_LOGW(TAG, "BMI270 did not respond; sensors.read keeps retrying");
            }
            if (lock) xSemaphoreGive(lock);
            return readings_result(&reading, esp_timer_get_time());
        }
    }
    float accel[3], gyro[3];
    esp_err_t err = bmi270_get_acce_data(s_imu, &accel[0], &accel[1], &accel[2]);
    if (err == ESP_OK) {
        err = bmi270_get_gyro_data(s_imu, &gyro[0], &gyro[1], &gyro[2]);
    }
    if (err == ESP_OK) {
        reading.valid = true;
        memcpy(reading.accel_g, accel, sizeof(accel));
        memcpy(reading.gyro_dps, gyro, sizeof(gyro));
        reading.at_us = esp_timer_get_time();
    } else {
        ESP_LOGW(TAG, "BMI270 read failed: %s", esp_err_to_name(err));
    }
    if (lock) xSemaphoreGive(lock);
    return readings_result(&reading, esp_timer_get_time());
}
