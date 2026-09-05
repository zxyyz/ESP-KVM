#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *profile_name;
    uint16_t p4_revision;
    bool pinmap_bound;
    bool c5_link_bound;
    bool hdmi_bridge_bound;
    bool usb_device_bound;
} akvm_board_info_t;

esp_err_t akvm_board_init(void);
const akvm_board_info_t *akvm_board_get_info(void);

#ifdef __cplusplus
}
#endif
