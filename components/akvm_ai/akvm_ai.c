#include "akvm_ai.h"

#include "akvm_auth.h"
#include "akvm_core.h"
#include "akvm_net.h"
#include "esp_log.h"

static const char *TAG = "akvm_ai";
static const akvm_ai_transport_t *s_transport;
static akvm_ai_state_t s_state;

esp_err_t akvm_ai_init(void)
{
    s_transport = NULL;
    s_state = AKVM_AI_IDLE;
    akvm_core_set_service_ready(AKVM_SERVICE_AI, false);
#if CONFIG_AKVM_ENABLE_AI
    ESP_LOGI(TAG, "AI manager ready; model transport intentionally unbound");
#else
    ESP_LOGI(TAG, "AI feature disabled");
#endif
    return ESP_OK;
}

esp_err_t akvm_ai_bind_transport(const akvm_ai_transport_t *transport)
{
    if (!transport || !transport->run_turn) return ESP_ERR_INVALID_ARG;
    s_transport = transport;
    return ESP_OK;
}

esp_err_t akvm_ai_run_turn(const char *prompt, akvm_ai_text_cb_t on_text, void *cb_ctx)
{
    if (!prompt || !prompt[0]) return ESP_ERR_INVALID_ARG;
    if (!akvm_core_is_service_ready(AKVM_SERVICE_NETWORK)) return ESP_ERR_INVALID_STATE;
    if (akvm_auth_state() != AKVM_AUTH_READY) return ESP_ERR_INVALID_STATE;
    if (!s_transport) return ESP_ERR_NOT_SUPPORTED;
    if (s_state == AKVM_AI_RUNNING) return ESP_ERR_INVALID_STATE;

    s_state = AKVM_AI_RUNNING;
    esp_err_t err = s_transport->run_turn(prompt, on_text, cb_ctx);
    s_state = err == ESP_OK ? AKVM_AI_IDLE : AKVM_AI_ERROR;
    akvm_core_set_service_ready(AKVM_SERVICE_AI, err == ESP_OK);
    return err;
}

akvm_ai_state_t akvm_ai_state(void)
{
    return s_state;
}
