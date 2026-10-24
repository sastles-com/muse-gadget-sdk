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

#include "display_text.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "led_status.h"
#include "pixel_font.h"

// ---- Layout (host-tested) ---------------------------------------------------

// Glyph geometry: the pixel font is 5x8 with a one-column gap, and lines
// get two glyph rows of leading.
#define TEXT_ADV   (PIXEL_FONT_WIDTH + 1)
#define TEXT_PITCH (PIXEL_FONT_HEIGHT + 2)
#define TEXT_MAX_LINES 16
#define TEXT_MAX_SCALE 4

typedef struct {
    const char *start;  // into the caller's text
    int len;            // characters on the line (no trailing spaces)
    int px;             // line width in pixels at the layout's scale
} text_line_t;

typedef struct {
    int scale;
    int lines;
    int block_px;  // height of the whole text block in pixels
    text_line_t line[TEXT_MAX_LINES];
} text_layout_t;

// Wrap `text` at `max_chars` columns. '\n' breaks a line; a word longer
// than a line is split. Returns the line count, capped at max_lines.
static int wrap_text(const char *text, int max_chars, text_line_t *lines,
                     int max_lines) {
    int n = 0;
    const char *p = text;
    while (*p && n < max_lines) {
        const char *line_start = p;
        const char *line_end = p;  // end of the last word that fit
        const char *q = p;
        for (;;) {
            const char *w = q;
            while (*w == ' ') w++;
            if (*w == '\n') {
                q = w + 1;
                break;
            }
            if (*w == '\0') {
                q = w;
                break;
            }
            const char *we = w;
            while (*we && *we != ' ' && *we != '\n') we++;
            if (line_end == line_start && line_start == p && q == p) {
                // Nothing on this line yet.
                if ((int)(we - w) <= max_chars) {
                    line_start = w;
                    line_end = we;
                    q = we;
                    continue;
                }
                // A single word longer than the line: split it.
                line_start = w;
                line_end = w + max_chars;
                q = line_end;
                break;
            }
            if ((int)(we - line_start) <= max_chars) {
                line_end = we;
                q = we;
                continue;
            }
            break;  // the word starts the next line; q is at its spaces
        }
        lines[n].start = line_start;
        lines[n].len = (int)(line_end - line_start);
        n++;
        p = q;
    }
    return n;
}

// Lay `text` out for a width x height screen. want_scale is 1..4, or 0
// for the largest scale (up to 3) whose wrapped text fits the height.
// The layout borrows `text`; it stays valid while `text` does.
static void text_layout(const char *text, int width, int height,
                        int want_scale, text_layout_t *out) {
    int first = want_scale > 0 ? want_scale : 3;
    int last = want_scale > 0 ? want_scale : 1;
    for (int scale = first; scale >= last; scale--) {
        int max_chars = width / (TEXT_ADV * scale);
        if (max_chars < 1) max_chars = 1;
        out->scale = scale;
        out->lines = wrap_text(text, max_chars, out->line, TEXT_MAX_LINES);
        out->block_px = out->lines > 0
                            ? (out->lines * TEXT_PITCH - (TEXT_PITCH - PIXEL_FONT_HEIGHT)) * scale
                            : 0;
        for (int i = 0; i < out->lines; i++) {
            int len = out->line[i].len;
            out->line[i].px = len > 0 ? len * TEXT_ADV * scale - scale : 0;
        }
        if (want_scale > 0 || out->block_px <= height) return;
    }
}

// ---- Rendering ----------------------------------------------------------------

static const char *TAG = "link.text";

#define TEXT_MAX_CHARS 512
// Rows rendered per led_status_draw_rect() call; 8 rows of a 128-wide
// screen stays under every backend's per-call pixel limit.
#define TEXT_STRIP_ROWS 8

static cJSON *text_error(const char *code, const char *message) {
    cJSON *result = cJSON_CreateObject();
    if (!result) return NULL;
    cJSON_AddBoolToObject(result, "ok", false);
    cJSON *error = cJSON_AddObjectToObject(result, "error");
    cJSON_AddStringToObject(error, "code", code);
    cJSON_AddStringToObject(error, "message", message);
    return result;
}

// "#rrggbb" (the '#' is optional) to the byte-swapped RGB565 that
// led_status_draw_rect() takes. Returns false on a malformed colour.
static bool parse_color(const char *s, uint16_t *out) {
    if (*s == '#') s++;
    uint32_t rgb = 0;
    for (int i = 0; i < 6; i++) {
        char c = s[i];
        unsigned digit;
        if (c >= '0' && c <= '9') digit = (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f') digit = (unsigned)(c - 'a') + 10;
        else if (c >= 'A' && c <= 'F') digit = (unsigned)(c - 'A') + 10;
        else return false;
        rgb = (rgb << 4) | digit;
    }
    if (s[6] != '\0') return false;
    unsigned r = (rgb >> 16) & 0xFF, g = (rgb >> 8) & 0xFF, b = rgb & 0xFF;
    uint16_t px = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    *out = (uint16_t)((px >> 8) | (px << 8));  // high byte first
    return true;
}

// Is the glyph pixel at (fx, fy) of line `li` set? fx/fy are pixel
// offsets within the text block.
static bool glyph_on(const text_layout_t *layout, int li, int fx, int fy) {
    const text_line_t *line = &layout->line[li];
    int scale = layout->scale;
    int col = fx / scale;
    int gx = col % TEXT_ADV;
    if (gx >= PIXEL_FONT_WIDTH) return false;
    unsigned char ch = (unsigned char)line->start[col / TEXT_ADV];
    if (ch < PIXEL_FONT_FIRST || ch > PIXEL_FONT_LAST) ch = '?';
    return (pixel_font[ch - PIXEL_FONT_FIRST][gx] >> (fy / scale)) & 1;
}

cJSON *display_text_command(cJSON *params) {
    cJSON *text_item = cJSON_GetObjectItem(params, "text");
    if (!cJSON_IsString(text_item) || !text_item->valuestring
        || !text_item->valuestring[0]) {
        return text_error("missing_param", "text is required");
    }
    const char *text = text_item->valuestring;
    if (strlen(text) > TEXT_MAX_CHARS) {
        return text_error("invalid_params", "text is too long (512 characters at most)");
    }
    uint16_t fg = 0xFFFF;  // white
    cJSON *color_item = cJSON_GetObjectItem(params, "color");
    if (color_item) {
        if (!cJSON_IsString(color_item) || !color_item->valuestring
            || !parse_color(color_item->valuestring, &fg)) {
            return text_error("invalid_params", "color must be a hex colour like #ffffff");
        }
    }
    int want_scale = 0;
    cJSON *size_item = cJSON_GetObjectItem(params, "size");
    if (size_item) {
        if (!cJSON_IsNumber(size_item) || size_item->valueint < 1
            || size_item->valueint > TEXT_MAX_SCALE) {
            return text_error("invalid_params", "size must be an integer from 1 to 4");
        }
        want_scale = size_item->valueint;
    }

    int width, height;
    if (!led_status_display_info(&width, &height)) {
        return text_error("unavailable", "this device has no display");
    }
    text_layout_t layout;
    text_layout(text, width, height, want_scale, &layout);
    int block_y = layout.block_px < height ? (height - layout.block_px) / 2 : 0;

    uint16_t *strip = heap_caps_malloc(
        (size_t)width * TEXT_STRIP_ROWS * sizeof(uint16_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!strip) {
        strip = heap_caps_malloc((size_t)width * TEXT_STRIP_ROWS * sizeof(uint16_t),
                                 MALLOC_CAP_8BIT);
    }
    if (!strip) return text_error("out_of_memory", "failed to allocate a draw buffer");

    const uint16_t bg = 0;  // black
    for (int y0 = 0; y0 < height; y0 += TEXT_STRIP_ROWS) {
        int rows = height - y0;
        if (rows > TEXT_STRIP_ROWS) rows = TEXT_STRIP_ROWS;
        for (int r = 0; r < rows; r++) {
            int y = y0 + r;
            uint16_t *row = strip + r * width;
            int by = y - block_y;
            int li = by >= 0 ? by / (TEXT_PITCH * layout.scale) : -1;
            int fy = li >= 0 ? by - li * TEXT_PITCH * layout.scale : 0;
            bool in_glyph = li >= 0 && li < layout.lines
                            && fy < PIXEL_FONT_HEIGHT * layout.scale;
            for (int x = 0; x < width; x++) {
                bool on = false;
                if (in_glyph) {
                    const text_line_t *line = &layout.line[li];
                    int fx = x - (width - line->px) / 2;
                    if (fx >= 0 && fx < line->px) on = glyph_on(&layout, li, fx, fy);
                }
                row[x] = on ? fg : bg;
            }
        }
        if (!led_status_draw_rect(0, y0, width, rows, strip)) {
            free(strip);
            return text_error("draw_failed", "the display rejected the text");
        }
    }
    free(strip);
    led_status_draw_done();

    ESP_LOGI(TAG, "drew %d line(s) of text at scale %d", layout.lines, layout.scale);
    cJSON *result = cJSON_CreateObject();
    if (!result) return NULL;
    cJSON_AddBoolToObject(result, "ok", true);
    cJSON *payload = cJSON_AddObjectToObject(result, "payload");
    cJSON_AddNumberToObject(payload, "width", width);
    cJSON_AddNumberToObject(payload, "height", height);
    cJSON_AddNumberToObject(payload, "lines", layout.lines);
    cJSON_AddNumberToObject(payload, "scale", layout.scale);
    return result;
}
