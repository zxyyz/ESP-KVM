#include "akvm_core.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static SemaphoreHandle_t s_lock;
static akvm_runtime_snapshot_t s_state;

static akvm_ai_policy_t default_policy(void)
{
#if CONFIG_AKVM_AI_POLICY_AUTONOMOUS
    return AKVM_AI_POLICY_AUTONOMOUS;
#elif CONFIG_AKVM_AI_POLICY_ASSIST
    return AKVM_AI_POLICY_ASSIST;
#else
    return AKVM_AI_POLICY_OBSERVE;
#endif
}

esp_err_t akvm_core_init(void)
{
    memset(&s_state, 0, sizeof(s_state));
    s_state.ai_policy = default_policy();
    s_state.generation = 1;
    s_lock = xSemaphoreCreateMutex();
    return s_lock ? ESP_OK : ESP_ERR_NO_MEM;
}

void akvm_core_set_boot_complete(void)
{
    if (!s_lock) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_state.boot_complete = true;
    ++s_state.generation;
    xSemaphoreGive(s_lock);
}

void akvm_core_set_service_ready(akvm_service_t service, bool ready)
{
    if (!s_lock || service < 0 || service >= AKVM_SERVICE_COUNT) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_state.service_ready[service] != ready) {
        s_state.service_ready[service] = ready;
        ++s_state.generation;
    }
    xSemaphoreGive(s_lock);
}

bool akvm_core_is_service_ready(akvm_service_t service)
{
    bool ready = false;
    if (!s_lock || service < 0 || service >= AKVM_SERVICE_COUNT) return false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    ready = s_state.service_ready[service];
    xSemaphoreGive(s_lock);
    return ready;
}

void akvm_core_set_ai_policy(akvm_ai_policy_t policy)
{
    if (!s_lock || policy > AKVM_AI_POLICY_AUTONOMOUS) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_state.ai_policy != policy) {
        s_state.ai_policy = policy;
        ++s_state.generation;
    }
    xSemaphoreGive(s_lock);
}

akvm_ai_policy_t akvm_core_get_ai_policy(void)
{
    akvm_ai_policy_t policy = AKVM_AI_POLICY_OBSERVE;
    if (!s_lock) return policy;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    policy = s_state.ai_policy;
    xSemaphoreGive(s_lock);
    return policy;
}

esp_err_t akvm_core_snapshot(akvm_runtime_snapshot_t *out)
{
    if (!out || !s_lock) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *out = s_state;
    xSemaphoreGive(s_lock);
    return ESP_OK;
}
