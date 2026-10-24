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

// Host AtomS3R IMU harness: a sample in, the sensors.read result out.
// The runner extracts the production protocol code into
// atoms3r_imu_protocol.inc.
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "cJSON.h"

#include "atoms3r_imu_protocol.inc"

#define S(x) (x * 1000000LL)

static void check_result(void) {
    imu_reading_t r = {0};

    // No sample: an error, not an all-null payload.
    cJSON *result = readings_result(&r, S(10));
    assert(cJSON_IsFalse(cJSON_GetObjectItem(result, "ok")));
    cJSON *error = cJSON_GetObjectItem(result, "error");
    assert(!strcmp(cJSON_GetObjectItem(error, "code")->valuestring, "no_readings"));
    assert(!cJSON_GetObjectItem(result, "payload"));
    cJSON_Delete(result);

    // A fresh sample: accel rounds to 3 decimals, gyro to 1, and the
    // age rides along.
    r.valid = true;
    r.accel_g[0] = 0.12349f;
    r.accel_g[1] = -0.5f;
    r.accel_g[2] = 1.0f;
    r.gyro_dps[0] = 10.04f;
    r.gyro_dps[1] = -3.26f;
    r.gyro_dps[2] = 0.5f;
    r.at_us = S(8);
    result = readings_result(&r, S(10));
    assert(cJSON_IsTrue(cJSON_GetObjectItem(result, "ok")));
    char *json = cJSON_PrintUnformatted(cJSON_GetObjectItem(result, "payload"));
    assert(strstr(json, "\"accelerometer\":{\"value\":{\"x\":0.123,\"y\":-0.5,"
                        "\"z\":1},\"unit\":\"g\",\"age_s\":2}"));
    assert(strstr(json, "\"gyroscope\":{\"value\":{\"x\":10,\"y\":-3.3,"
                        "\"z\":0.5},\"unit\":\"dps\",\"age_s\":2}"));
    cJSON_free(json);
    cJSON_Delete(result);
}

int main(void) {
    check_result();
    puts("PASS atoms3r imu: result envelope, rounding, age, no-reading error");
    return 0;
}
