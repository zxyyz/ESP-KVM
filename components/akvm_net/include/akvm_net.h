#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AKVM_NET_OFFLINE = 0,
    AKVM_NET_UPLINK,
    AKVM_NET_VPN_READY,
} akvm_net_state_t;

esp_err_t akvm_net_init(void);
akvm_net_state_t akvm_net_state(void);
void akvm_net_set_uplink_ready(bool ready);
void akvm_net_set_vpn_ready(bool ready);
bool akvm_net_remote_kvm_allowed(void);

#ifdef __cplusplus
}
#endif
