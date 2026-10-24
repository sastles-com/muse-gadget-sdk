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

// The M5Stack Encoder8 unit (U153) on the CoreS3's Grove Port A: eight
// incremental encoders with push buttons and a slide switch, behind an
// onboard STM32F030 at I2C address 0x41. It is probed lazily on the
// first encoders.read, so a CoreS3 without the unit plugged in boots
// and pairs exactly like one with it.

#pragma once

#include "cJSON.h"

// encoders.read: every channel's absolute count and increment since
// the last read (reading clears the increments), the eight button
// states and the slide switch. An error when the unit does not
// respond.
cJSON *cores3_encoder8_command(void);

// encoders.scan: probe every address on the Port A bus and report
// which ones answer, as a wiring/power/address diagnostic for when
// encoders.read finds no unit at 0x41.
cJSON *cores3_encoder8_scan_command(void);
