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

// Host CoreS3 Encoder8 harness: register bytes in, the encoders.read
// result out, plus the encoders.scan result for a fake set of probed
// addresses. The runner extracts the production protocol code into
// cores3_encoder8_protocol.inc.
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "cJSON.h"

#include "cores3_encoder8_protocol.inc"

static void put_le32(uint8_t *p, int32_t v) {
    uint32_t u = (uint32_t)v;
    p[0] = (uint8_t)u;
    p[1] = (uint8_t)(u >> 8);
    p[2] = (uint8_t)(u >> 16);
    p[3] = (uint8_t)(u >> 24);
}

static void check_registers(void) {
    // The two counter segments and the two increment segments.
    assert(encoder8_value_reg(0, false) == 0x00);
    assert(encoder8_value_reg(3, false) == 0x0c);
    assert(encoder8_value_reg(4, false) == 0x10);
    assert(encoder8_value_reg(7, false) == 0x1c);
    assert(encoder8_value_reg(0, true) == 0x20);
    assert(encoder8_value_reg(3, true) == 0x2c);
    assert(encoder8_value_reg(4, true) == 0x30);
    assert(encoder8_value_reg(7, true) == 0x3c);
}

static void check_result(void) {
    encoder8_reading_t r;
    memset(&r, 0, sizeof(r));
    r.firmware_version = -1;

    // No sample: an error, not an all-null payload.
    cJSON *result = readings_result(&r);
    assert(cJSON_IsFalse(cJSON_GetObjectItem(result, "ok")));
    cJSON *error = cJSON_GetObjectItem(result, "error");
    assert(!strcmp(cJSON_GetObjectItem(error, "code")->valuestring, "no_readings"));
    assert(!cJSON_GetObjectItem(result, "payload"));
    cJSON_Delete(result);

    // A register dump: little-endian counts and increments in channel
    // order, nonzero button bytes pressed, the switch and firmware.
    encoder8_raw_t raw;
    memset(&raw, 0, sizeof(raw));
    put_le32(&raw.counts[0 * 4], 1);
    put_le32(&raw.counts[1 * 4], -2);
    put_le32(&raw.counts[7 * 4], 0x12345678);
    put_le32(&raw.increments[0 * 4], -1);
    put_le32(&raw.increments[3 * 4], 42);
    raw.buttons[0] = 1;
    raw.buttons[5] = 1;
    raw.slide_switch = 1;
    raw.firmware_version = 2;
    encoder8_parse(&raw, &r);
    r.valid = true;
    assert(r.count[0] == 1 && r.count[1] == -2 && r.count[7] == 0x12345678);
    assert(r.increment[0] == -1 && r.increment[3] == 42);
    assert(r.button[0] && !r.button[1] && r.button[5] && !r.button[7]);
    assert(r.slide_switch && r.firmware_version == 2);

    result = readings_result(&r);
    assert(cJSON_IsTrue(cJSON_GetObjectItem(result, "ok")));
    char *json = cJSON_PrintUnformatted(cJSON_GetObjectItem(result, "payload"));
    assert(strstr(json, "\"channels\":[{\"count\":1,\"increment\":-1},"
                        "{\"count\":-2,\"increment\":0},"));
    assert(strstr(json, "{\"count\":305419896,\"increment\":0}]"));
    assert(strstr(json, "\"buttons\":[true,false,false,false,false,true,"
                        "false,false]"));
    assert(strstr(json, "\"slide_switch\":true"));
    assert(strstr(json, "\"firmware_version\":2"));
    cJSON_free(json);
    cJSON_Delete(result);
}

static void check_scan(void) {
    // Addresses spell as two lowercase hex digits after "0x".
    char text[5];
    encoder8_format_addr(0x08, text);
    assert(!strcmp(text, "0x08"));
    encoder8_format_addr(0x41, text);
    assert(!strcmp(text, "0x41"));
    encoder8_format_addr(0x77, text);
    assert(!strcmp(text, "0x77"));
    // The walked range is the standard probe range.
    assert(ENCODER8_SCAN_FIRST == 0x08 && ENCODER8_SCAN_LAST == 0x77);
    assert(ENCODER8_SCAN_MAX == 0x77 - 0x08 + 1);

    encoder8_scan_t scan;
    memset(&scan, 0, sizeof(scan));

    // No bus: an error, not an empty found list.
    cJSON *result = scan_result(&scan);
    assert(cJSON_IsFalse(cJSON_GetObjectItem(result, "ok")));
    cJSON *error = cJSON_GetObjectItem(result, "error");
    assert(!strcmp(cJSON_GetObjectItem(error, "code")->valuestring, "no_bus"));
    assert(!cJSON_GetObjectItem(result, "payload"));
    cJSON_Delete(result);

    // A bus with nothing on it: an empty list, not an error.
    scan.valid = true;
    result = scan_result(&scan);
    assert(cJSON_IsTrue(cJSON_GetObjectItem(result, "ok")));
    char *json = cJSON_PrintUnformatted(cJSON_GetObjectItem(result, "payload"));
    assert(!strcmp(json, "{\"found\":[]}"));
    cJSON_free(json);
    cJSON_Delete(result);

    // What answered, in probe order.
    scan.found[0] = 0x41;
    scan.found[1] = 0x68;
    scan.found_count = 2;
    result = scan_result(&scan);
    assert(cJSON_IsTrue(cJSON_GetObjectItem(result, "ok")));
    json = cJSON_PrintUnformatted(cJSON_GetObjectItem(result, "payload"));
    assert(!strcmp(json, "{\"found\":[\"0x41\",\"0x68\"]}"));
    cJSON_free(json);
    cJSON_Delete(result);
}

int main(void) {
    check_registers();
    check_result();
    check_scan();
    puts("PASS cores3 encoder8: register map, decode, result envelope, "
         "no-reading error, scan result");
    return 0;
}
