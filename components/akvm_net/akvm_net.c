#include "akvm_net.h"

#include "akvm_core.h"
#include "esp_log.h"

static const char *TAG = "akvm_net";
static akvm_net_state_t s_state;

esp_err_t akvm_net_init(void)
{
    s_state = AKVM_NET_OFFLINE;
    akvm_core_set_service_ready(AKVM_SERVICE_NETWORK, false);
    akvm_core_set_service_ready(AKVM_SERVICE_VPN, false);

#if CONFIG_AKVM_NETWORK_ESP_HOSTED_SDIO
    ESP_LOGW(TAG, "ESP-Hosted SDIO selected but board transport is not bound yet");
#elif CONFIG_AKVM_NETWORK_ESP_HOSTED_SPI
    ESP_LOGW(TAG, "ESP-Hosted SPI selected but board transport is not bound yet");
#elif CONFIG_AKVM_NETWORK_ETHERNET
    ESP_LOGW(TAG, "Ethernet selected but PHY/board transport is not bound yet");
#else
    ESP_LOGI(TAG, "offline/stub network backend selected");
#endif
    return ESP_OK;
}

akvm_net_state_t akvm_net_state(void)
{
    return s_state;
}

void akvm_net_set_uplink_ready(bool ready)
{
    if (!ready) {
        s_state = AKVM_NET_OFFLINE;
        akvm_core_set_service_ready(AKVM_SERVICE_NETWORK, false);
        akvm_core_set_service_ready(AKVM_SERVICE_VPN, false);
        return;
    }
    s_state = AKVM_NET_UPLINK;
    akvm_core_set_service_ready(AKVM_SERVICE_NETWORK, true);
}

void akvm_net_set_vpn_ready(bool ready)
{
    if (!ready) {
        if (s_state == AKVM_NET_VPN_READY) s_state = AKVM_NET_UPLINK;
        akvm_core_set_service_ready(AKVM_SERVICE_VPN, false);
        return;
    }
    if (s_state != AKVM_NET_OFFLINE) {
        s_state = AKVM_NET_VPN_READY;
        akvm_core_set_service_ready(AKVM_SERVICE_VPN, true);
    }
}

bool akvm_net_remote_kvm_allowed(void)
{
    /* Remote management is fail-closed. Local-LAN policy belongs in the web
     * listener binding, not in this remote-access predicate. */
    return s_state == AKVM_NET_VPN_READY;
}
