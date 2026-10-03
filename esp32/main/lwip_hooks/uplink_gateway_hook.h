// SPDX-License-Identifier: Apache-2.0
#pragma once
/* Compiled into lwIP through ESP_IDF_LWIP_HOOK_FILENAME (main/CMakeLists.txt)
 * only when CONFIG_HOMEHUB_UPLINK_GATEWAY is set. etharp_output() asks this
 * hook for the next hop of every off-subnet packet; NULL keeps the DHCP
 * router. Implemented in main/uplink.c. */
#include "lwip/ip4_addr.h"

struct netif;

const ip4_addr_t *muse_uplink_gateway_hook(struct netif *netif,
                                           const ip4_addr_t *dest);

#define LWIP_HOOK_ETHARP_GET_GW(netif, dest) \
    muse_uplink_gateway_hook((netif), (dest))
