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

// The M5Stack AtomS3R's onboard motion sensor: a Bosch BMI270
// accelerometer and gyroscope on the internal I2C bus (SDA GPIO45,
// SCL GPIO0, shared with the backlight controller through
// atoms3r_i2c_bus()). It is initialised lazily on the first
// sensors.read, so a board with a missing or unresponsive IMU boots
// and pairs exactly like one without this feature.

#pragma once

#include "cJSON.h"

// sensors.read: a fresh accelerometer and gyroscope sample, each as
// x, y and z with its unit and age. An error when the BMI270 does not
// respond.
cJSON *atoms3r_imu_command(void);
