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

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The screen's fonts are LVGL's builds of unscii and Montserrat: ASCII, and
 * Montserrat's symbols. Text from elsewhere (replies, network names) gets
 * ASCII stand-ins for what they lack: curly quotes become straight ones, an em
 * dash "--", accented letters their plain ones. Emoji go; anything else stays,
 * for a board's wide font (muse_board.h) to show in captions and replies.
 */

/* The code point of the UTF-8 character at s, and its length in *len; -1 if
 * it's broken. */
int32_t muse_text_decode(const char *s, size_t *len);

/* Whether cp is from a script written without spaces between words (Chinese,
 * Japanese), so a line may break on either side of it. One unscii_16 column,
 * like any other character. */
bool muse_text_ideographic(int32_t cp);

/* The stand-in for the UTF-8 character at s into out, and its length (0 drops
 * the character); -1 to keep it. *len is the character's length in bytes. */
int muse_text_ascii(const char *s, size_t *len, char out[4]);

/* Puts the stand-ins into s, which has room for cap bytes. If one doesn't fit,
 * the text ends there. */
void muse_text_to_ascii(char *s, size_t cap);

/* text, or if it needs stand-ins and fits in cap bytes, a copy with them in buf. */
const char *muse_text_showable(const char *text, char *buf, size_t cap);

/* What the caption's font shows itself (a board's wide font has curly quotes,
 * dashes and the ellipsis), which captions keep instead of a stand-in. Set by
 * the UI; called from any task. */
void muse_text_caption_font(bool (*shows)(int32_t cp));

/* muse_text_ascii() and muse_text_to_ascii() for captions. */
int muse_text_caption_ascii(const char *s, size_t *len, char out[4]);
void muse_text_caption_to_ascii(char *s, size_t cap);

#ifdef __cplusplus
}
#endif
