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

#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Hooks for a wrapper project that hosts this firmware (for example
 * ESP-Mosaico's Vibe Mode services and expansion modules). All are weak:
 * stand-alone boards leave them undefined.
 */

/* Runs before Wi-Fi, BLE and the UI start. */
void muse_gadget_platform_start(void) __attribute__((weak));

/*
 * Adds the platform's own Home Link commands to the register's commands_v2
 * object, after the firmware's. Each entry is
 * {"description": "...", "required": {name: {"type", "description"}},
 *  "optional": {...}, "timeout_ms": n}; noise_ctrl_add_command() builds one.
 * Called on every registration, so advertise what may be plugged in later and
 * report its absence when invoked.
 */
void muse_gadget_platform_add_commands(cJSON *commands) __attribute__((weak));

/*
 * Runs one of those commands, synchronously: returns the result object
 * ({"ok": true, "payload": {...}} or {"ok": false, "error": {"code",
 * "message"}}), or NULL for a command that isn't the platform's.
 */
cJSON *muse_gadget_platform_command(const char *command, cJSON *params) __attribute__((weak));

#ifdef __cplusplus
}
#endif
