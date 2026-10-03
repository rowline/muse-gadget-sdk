// SPDX-License-Identifier: Apache-2.0
#include "uplink.h"

#include "esp_log.h"
#include "esp_netif_net_stack.h"
#include "esp_timer.h"
#include "lwip/ip4_addr.h"
#include "lwip_hooks/uplink_gateway_hook.h"
#include "sdkconfig.h"

// DHCP renewals put the router's DNS server back; check this often.
#define UPLINK_DNS_CHECK_US (30LL * 1000000LL)

static const char *TAG = "link.uplink";

// Written by the event task before or between connections, read by the lwIP
// task in the hook. Each is a single aligned 32-bit word.
static struct netif *s_sta_lwip;
static esp_netif_t *s_sta;
static esp_ip4_addr_t s_configured_gw;  // 0 when the override is off
static esp_ip4_addr_t s_dns;            // 0 when DHCP's DNS stays
static ip4_addr_t s_active_gw;          // 0 while the hook defers to DHCP
static esp_timer_handle_t s_dns_timer;

const ip4_addr_t *muse_uplink_gateway_hook(struct netif *netif,
                                           const ip4_addr_t *dest) {
    (void)dest;
    if (netif == NULL || netif != s_sta_lwip || s_active_gw.addr == 0) {
        return NULL;
    }
    return &s_active_gw;
}

static bool parse_ip4(const char *name, const char *text, esp_ip4_addr_t *out) {
    if (!text[0]) return false;
    if (esp_netif_str_to_ip4(text, out) != ESP_OK || out->addr == 0) {
        ESP_LOGE(TAG, "%s \"%s\" is not an IPv4 address; ignoring it", name, text);
        out->addr = 0;
        return false;
    }
    return true;
}

static void apply_dns(void) {
    if (s_dns.addr == 0 || s_active_gw.addr == 0) return;
    esp_netif_dns_info_t current = {0};
    if (esp_netif_get_dns_info(s_sta, ESP_NETIF_DNS_MAIN, &current) == ESP_OK
        && current.ip.type == ESP_IPADDR_TYPE_V4
        && current.ip.u_addr.ip4.addr == s_dns.addr) {
        return;
    }
    esp_netif_dns_info_t dns = {0};
    dns.ip.type = ESP_IPADDR_TYPE_V4;
    dns.ip.u_addr.ip4 = s_dns;
    esp_err_t err = esp_netif_set_dns_info(s_sta, ESP_NETIF_DNS_MAIN, &dns);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "setting DNS " IPSTR " failed: %s", IP2STR(&s_dns),
                 esp_err_to_name(err));
    }
}

static void dns_timer_cb(void *arg) {
    (void)arg;
    apply_dns();
}

void uplink_init(esp_netif_t *sta) {
    if (!parse_ip4("gateway override", CONFIG_HOMEHUB_UPLINK_GATEWAY,
                   &s_configured_gw)) {
        return;
    }
    s_sta = sta;
    s_sta_lwip = esp_netif_get_netif_impl(sta);
    if (!parse_ip4("DNS override", CONFIG_HOMEHUB_UPLINK_DNS, &s_dns)) return;
    const esp_timer_create_args_t args = {
        .callback = dns_timer_cb,
        .name = "uplink_dns",
    };
    esp_err_t err = esp_timer_create(&args, &s_dns_timer);
    if (err == ESP_OK) err = esp_timer_start_periodic(s_dns_timer, UPLINK_DNS_CHECK_US);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "DNS re-check timer failed: %s; renewals may restore DHCP DNS",
                 esp_err_to_name(err));
    }
}

void uplink_on_got_ip(const esp_netif_ip_info_t *info) {
    if (s_configured_gw.addr == 0 || info == NULL) return;
    const uint32_t mask = info->netmask.addr;
    if ((s_configured_gw.addr & mask) != (info->ip.addr & mask)) {
        s_active_gw.addr = 0;
        ESP_LOGW(TAG, "gateway override " IPSTR " is outside this network; "
                 "using the DHCP router " IPSTR,
                 IP2STR(&s_configured_gw), IP2STR(&info->gw));
        return;
    }
    s_active_gw.addr = s_configured_gw.addr;
    apply_dns();
    if (s_dns.addr) {
        ESP_LOGI(TAG, "internet via " IPSTR " (DHCP router " IPSTR "), DNS " IPSTR,
                 IP2STR(&s_configured_gw), IP2STR(&info->gw), IP2STR(&s_dns));
    } else {
        ESP_LOGI(TAG, "internet via " IPSTR " (DHCP router " IPSTR ")",
                 IP2STR(&s_configured_gw), IP2STR(&info->gw));
    }
}
