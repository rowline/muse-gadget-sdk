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

#include "sim_board.h"
#include "sim_platform.h"

#include "src/drivers/sdl/lv_sdl_mouse.h"
#include "src/drivers/sdl/lv_sdl_window.h"

#if MUSE_SIM_BOARD_ESP_MOSAICO
/* ESP-Mosaico: 480 px square; matches projects/muse_gadget's board. */
#define SIM_RESOLUTION 480
#else
#define SIM_RESOLUTION 412     /* SenseCAP Watcher */
#endif

static lv_display_t *s_display;

/* muse_ui.c reads the selected board through this production global. */
const muse_board_t *muse_board;

static esp_err_t sim_init(void)
{
    return ESP_OK;
}

static lv_display_t *sim_display_start(lv_indev_t **touch)
{
    s_display = lv_sdl_window_create(SIM_RESOLUTION, SIM_RESOLUTION);
    if (!s_display) {
        return NULL;
    }
    /* The SDL driver installs SDL_GetTicks. Use the simulator clock instead so
     * scripted runs can advance time without sleeping and render repeatably. */
    lv_tick_set_cb(sim_time_tick_ms);
    lv_sdl_window_set_title(s_display, "Muse Gadget Simulator");
    lv_sdl_window_set_resizeable(s_display, false);
    if (touch) {
        *touch = lv_sdl_mouse_create();
    }
    return s_display;
}

static bool sim_display_lock(int timeout_ms)
{
    (void)timeout_ms;
    return true;
}

static void sim_display_unlock(void)
{
}

static void sim_set_brightness(int pct)
{
    (void)pct;
}

static void sim_panel_sleep(bool sleep)
{
    (void)sleep;
}

static esp_err_t sim_power_off(void)
{
    return ESP_FAIL;
}

static const muse_board_t s_sim_board = {
#if MUSE_SIM_BOARD_ESP_MOSAICO
    .name = "ESP-Mosaico Simulator",
    .width = SIM_RESOLUTION,
    .height = SIM_RESOLUTION,
    .round = false,
    .touch = true,
    .diagonal_in = 1.8f,
    .talk_button = "AI",
    .aux_button = "BOOT",
    /* AI is on the top edge, right of centre; BOOT is on the bottom edge,
     * left of the USB-C port (ON/OFF is to its right). Clear of the rounded
     * corners, the status line and the page dots. */
    .talk_hint = { LV_ALIGN_TOP_MID, 140, 14 },
    .aux_hint = { LV_ALIGN_BOTTOM_MID, -110, -12 },
#else
    .name = "SenseCAP Watcher Simulator",
    .width = SIM_RESOLUTION,
    .height = SIM_RESOLUTION,
    .round = true,
    .touch = true,
    .diagonal_in = 1.45f,
    .talk_button = "wheel",
    .aux_button = "scroll",
    .talk_hint = { LV_ALIGN_CENTER, 100, -143 },
#endif
    .frame_ms = 40,
    .init = sim_init,
    .display_start = sim_display_start,
    .display_lock = sim_display_lock,
    .display_unlock = sim_display_unlock,
    .set_brightness = sim_set_brightness,
    .panel_sleep = sim_panel_sleep,
    .power_off = sim_power_off,
};

const muse_board_t *sim_board_get(void)
{
    return &s_sim_board;
}

lv_display_t *sim_board_display(void)
{
    return s_display;
}
