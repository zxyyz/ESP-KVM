#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AKVM_AI_POLICY_OBSERVE = 0,
    AKVM_AI_POLICY_ASSIST,
    AKVM_AI_POLICY_AUTONOMOUS,
} akvm_ai_policy_t;

typedef enum {
    AKVM_SERVICE_NETWORK = 0,
    AKVM_SERVICE_VPN,
    AKVM_SERVICE_HID,
    AKVM_SERVICE_VIDEO,
    AKVM_SERVICE_AUTH,
    AKVM_SERVICE_AI,
    AKVM_SERVICE_COUNT,
} akvm_service_t;

typedef struct {
    bool boot_complete;
    bool service_ready[AKVM_SERVICE_COUNT];
    akvm_ai_policy_t ai_policy;
    uint32_t generation;
} akvm_runtime_snapshot_t;

esp_err_t akvm_core_init(void);
void akvm_core_set_boot_complete(void);
void akvm_core_set_service_ready(akvm_service_t service, bool ready);
bool akvm_core_is_service_ready(akvm_service_t service);
void akvm_core_set_ai_policy(akvm_ai_policy_t policy);
akvm_ai_policy_t akvm_core_get_ai_policy(void);
esp_err_t akvm_core_snapshot(akvm_runtime_snapshot_t *out);

#ifdef __cplusplus
}
#endif
