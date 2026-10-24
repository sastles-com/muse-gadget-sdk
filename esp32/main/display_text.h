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

// display.text for the status screens: draw short text with the
// built-in pixel font, word-wrapped and centred. The text replaces
// the animation until display.show_animation, like an image does.

#pragma once

#include "cJSON.h"

// display.text: params are the command's (text, and optionally color
// and size); the result is the usual command result.
cJSON *display_text_command(cJSON *params);
