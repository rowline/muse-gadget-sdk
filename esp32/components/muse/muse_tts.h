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
 * A voice for replies, synthesized on the gadget. Muse replies in text; with
 * no voice registered, or the speaker off, each message is shown at reading
 * pace instead. A host project registers its one before Muse starts, from
 * muse_gadget_platform_start(). All three calls come from the chat session's
 * task, one message at a time. Its stack is in PSRAM, so they must not touch
 * flash (no mmap, NVS or partition writes): anything that freezes the caches
 * asserts. Map data beforehand, when registering.
 */
typedef struct {
    const char *name;
    /* Starts saying text: UTF-8 as Muse wrote it, markdown and emoji and all,
     * maybe cut short mid-character. False if it can't; the message is shown
     * unspoken. */
    bool (*begin)(const char *text);
    /* Up to cap more samples, 16 kHz mono; 0 once it has all been said. */
    size_t (*read)(int16_t *pcm, size_t cap);
    /* Stops, whether or not it has all been said. */
    void (*end)(void);
} muse_tts_t;

void muse_tts_register(const muse_tts_t *tts);

/* The registered voice, or NULL. */
const muse_tts_t *muse_tts_get(void);

#ifdef __cplusplus
}
#endif
