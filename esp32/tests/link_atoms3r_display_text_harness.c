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

// Host AtomS3R display.text harness: text in, wrapped layout out.
// The runner extracts the production layout code into
// atoms3r_display_text_layout.inc.
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pixel_font.h"
#include "atoms3r_display_text_layout.inc"

static void check_line(const text_layout_t *layout, int i, const char *want) {
    const text_line_t *line = &layout->line[i];
    assert(line->len == (int)strlen(want));
    assert(!strncmp(line->start, want, line->len));
}

static void check_layout(void) {
    text_layout_t layout;

    // A short word auto-fits at the largest scale: 5 glyphs of 6 px at
    // 3x, minus the trailing gap, and one 8-row line with 2 leading.
    text_layout("Hello", 128, 128, 0, &layout);
    assert(layout.scale == 3);
    assert(layout.lines == 1);
    check_line(&layout, 0, "Hello");
    assert(layout.line[0].px == 5 * 6 * 3 - 3);
    assert(layout.block_px == (10 - 2) * 3);

    // Word wrap at 3x (7 columns): no line mixes words that overflow.
    text_layout("the quick brown fox", 128, 128, 0, &layout);
    assert(layout.scale == 3);
    assert(layout.lines == 4);
    check_line(&layout, 0, "the");
    check_line(&layout, 1, "quick");
    check_line(&layout, 2, "brown");
    check_line(&layout, 3, "fox");
    assert(layout.block_px == (4 * 10 - 2) * 3);

    // Newlines break lines, and an explicit size is honoured.
    text_layout("ab\ncd", 128, 128, 2, &layout);
    assert(layout.scale == 2);
    assert(layout.lines == 2);
    check_line(&layout, 0, "ab");
    check_line(&layout, 1, "cd");

    // Blank lines survive.
    text_layout("a\n\nb", 128, 128, 1, &layout);
    assert(layout.lines == 3);
    assert(layout.line[1].len == 0);

    // A word longer than the line (21 columns at 1x) is split.
    char word[31];
    memset(word, 'a', 30);
    word[30] = '\0';
    text_layout(word, 128, 128, 1, &layout);
    assert(layout.scale == 1);
    assert(layout.lines == 2);
    assert(layout.line[0].len == 21);
    assert(layout.line[1].len == 9);

    // Text that fits no scale still lays out at 1x, capped at
    // TEXT_MAX_LINES lines.
    char tall[128];
    tall[0] = '\0';
    for (int i = 0; i < 20; i++) strcat(tall, i ? "\nx" : "x");
    text_layout(tall, 128, 128, 0, &layout);
    assert(layout.scale == 1);
    assert(layout.lines == TEXT_MAX_LINES);
    assert(layout.block_px > 128);
}

int main(void) {
    check_layout();
    puts("PASS atoms3r display.text: auto-fit, word wrap, newlines, hard split, line cap");
    return 0;
}
