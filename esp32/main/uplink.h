// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "esp_netif.h"

// Optional internet gateway and DNS override for the Wi-Fi station
// (CONFIG_HOMEHUB_UPLINK_GATEWAY / CONFIG_HOMEHUB_UPLINK_DNS). Both calls do
// nothing when the gateway override is empty.

// Call once after the station netif exists and before it connects.
void uplink_init(esp_netif_t *sta);

// Call on every IP_EVENT_STA_GOT_IP. Turns the override on while the gateway
// is inside the new subnet, off otherwise, and applies the DNS server.
void uplink_on_got_ip(const esp_netif_ip_info_t *info);
