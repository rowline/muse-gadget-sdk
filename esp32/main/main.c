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

#include "app.h"
#include "esp_log.h"
#include "diagnostic_log.h"
#if CONFIG_MUSE_ENABLED
#include "muse_glue.h"
#endif

// Optional platform start-up, linked in by a wrapper project that hosts this
// firmware (for example ESP-Mosaico's Vibe Mode services). Runs before Wi-Fi,
// BLE and the UI start; leave it undefined on stand-alone boards.
void muse_gadget_platform_start(void) __attribute__((weak));

void app_main(void) {
#if CONFIG_HOMEHUB_SUPPORT_BUG_REPORT
    if (!diagnostic_log_init()) {
        ESP_LOGW("link.main",
                 "support log capture disabled: PSRAM unavailable");
    }
#endif
    ESP_LOGI("link.main", CONFIG_GADGET_PRODUCT_NAME " starting");
    if (muse_gadget_platform_start) {
        muse_gadget_platform_start();
    }
#if CONFIG_MUSE_ENABLED
    muse_glue_start();
#endif
    app_run();
}
