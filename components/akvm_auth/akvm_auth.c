#include "akvm_auth.h"

#include <string.h>
#include "akvm_core.h"
#include "akvm_net.h"
#include "esp_log.h"

static const char *TAG = "akvm_auth";
static akvm_auth_state_t s_state;
static akvm_auth_device_challenge_t s_challenge;
static akvm_auth_token_set_t s_tokens;
static const akvm_auth_transport_t *s_transport;

static void zero_sensitive_state(void)
{
    volatile unsigned char *p = (volatile unsigned char *)&s_tokens;
    for (size_t i = 0; i < sizeof(s_tokens); ++i) p[i] = 0;
    memset(&s_challenge, 0, sizeof(s_challenge));
}

esp_err_t akvm_auth_init(void)
{
    zero_sensitive_state();
    s_state = AKVM_AUTH_SIGNED_OUT;
    s_transport = NULL;
    akvm_core_set_service_ready(AKVM_SERVICE_AUTH, false);
    ESP_LOGI(TAG, "auth manager ready; Codex transport not bound");
    return ESP_OK;
}

esp_err_t akvm_auth_bind_transport(const akvm_auth_transport_t *transport)
{
    if (!transport || !transport->request_device_code || !transport->poll_device_code || !transport->refresh_tokens) {
        return ESP_ERR_INVALID_ARG;
    }
    s_transport = transport;
    return ESP_OK;
}

esp_err_t akvm_auth_begin_device_login(akvm_auth_device_challenge_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    if (!akvm_core_is_service_ready(AKVM_SERVICE_NETWORK)) return ESP_ERR_INVALID_STATE;
    if (!s_transport) return ESP_ERR_NOT_SUPPORTED;

    zero_sensitive_state();
    esp_err_t err = s_transport->request_device_code(&s_challenge);
    if (err != ESP_OK) {
        s_state = AKVM_AUTH_ERROR;
        return err;
    }

    s_state = AKVM_AUTH_DEVICE_PENDING;
    *out = s_challenge;
    return ESP_OK;
}

esp_err_t akvm_auth_poll_device_login(bool *still_pending)
{
    if (!still_pending) return ESP_ERR_INVALID_ARG;
    *still_pending = false;
    if (s_state != AKVM_AUTH_DEVICE_PENDING) return ESP_ERR_INVALID_STATE;
    if (!s_transport) return ESP_ERR_NOT_SUPPORTED;

    bool pending = false;
    akvm_auth_token_set_t next = {0};
    esp_err_t err = s_transport->poll_device_code(&s_challenge, &next, &pending);
    if (pending) {
        *still_pending = true;
        return ESP_OK;
    }
    if (err != ESP_OK) {
        s_state = AKVM_AUTH_ERROR;
        return err;
    }

    s_tokens = next;
    s_state = AKVM_AUTH_READY;
    akvm_core_set_service_ready(AKVM_SERVICE_AUTH, true);
    memset(&s_challenge, 0, sizeof(s_challenge));
    return ESP_OK;
}

esp_err_t akvm_auth_sign_out(void)
{
    zero_sensitive_state();
    s_state = AKVM_AUTH_SIGNED_OUT;
    akvm_core_set_service_ready(AKVM_SERVICE_AUTH, false);
    return ESP_OK;
}

akvm_auth_state_t akvm_auth_state(void)
{
    return s_state;
}

const akvm_auth_token_set_t *akvm_auth_tokens(void)
{
    return s_state == AKVM_AUTH_READY ? &s_tokens : NULL;
}
