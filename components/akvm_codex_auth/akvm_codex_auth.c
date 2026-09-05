#include "akvm_codex_auth.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "akvm_auth.h"
#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"

/* These constants are intentionally isolated here and pinned to the upstream
 * Codex revision in AKVM_CODEX_UPSTREAM_SHA. Re-verify them when updating
 * Codex compatibility. */
#define CODEX_AUTH_BASE_URL "https://auth.openai.com"
#define CODEX_CLIENT_ID "app_EMoamEEZ73f0CkXaXp7hrann"
#define CODEX_DEVICE_USERCODE_URL CODEX_AUTH_BASE_URL "/api/accounts/deviceauth/usercode"
#define CODEX_DEVICE_TOKEN_URL CODEX_AUTH_BASE_URL "/api/accounts/deviceauth/token"
#define CODEX_OAUTH_TOKEN_URL CODEX_AUTH_BASE_URL "/oauth/token"
#define CODEX_DEVICE_VERIFY_URL CODEX_AUTH_BASE_URL "/codex/device"
#define CODEX_DEVICE_REDIRECT_URI CODEX_AUTH_BASE_URL "/deviceauth/callback"

#define HTTP_RESPONSE_MAX 16384
#define HTTP_TIMEOUT_MS 20000
#define DEVICE_LOGIN_TIMEOUT_US (15LL * 60LL * 1000000LL)

static const char *TAG = "akvm_codex_auth";
static int64_t s_device_login_started_us;

typedef struct {
    char *buffer;
    size_t capacity;
    size_t length;
    bool overflow;
} response_collector_t;

static void secure_zero(void *ptr, size_t len)
{
    volatile unsigned char *p = (volatile unsigned char *)ptr;
    while (len--) *p++ = 0;
}

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    if (!evt || evt->event_id != HTTP_EVENT_ON_DATA || evt->data_len <= 0) return ESP_OK;
    response_collector_t *collector = (response_collector_t *)evt->user_data;
    if (!collector || !collector->buffer) return ESP_OK;

    size_t incoming = (size_t)evt->data_len;
    if (collector->length + incoming + 1 > collector->capacity) {
        collector->overflow = true;
        return ESP_ERR_NO_MEM;
    }

    memcpy(collector->buffer + collector->length, evt->data, incoming);
    collector->length += incoming;
    collector->buffer[collector->length] = '\0';
    return ESP_OK;
}

static esp_err_t http_post(const char *url,
                           const char *content_type,
                           const char *body,
                           char *response,
                           size_t response_capacity,
                           int *status_out)
{
    if (!url || !content_type || !body || !response || response_capacity < 2 || !status_out) {
        return ESP_ERR_INVALID_ARG;
    }

    response_collector_t collector = {
        .buffer = response,
        .capacity = response_capacity,
    };
    response[0] = '\0';

    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .event_handler = http_event_handler,
        .user_data = &collector,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return ESP_ERR_NO_MEM;

    esp_err_t err = esp_http_client_set_method(client, HTTP_METHOD_POST);
    if (err == ESP_OK) err = esp_http_client_set_header(client, "Content-Type", content_type);
    if (err == ESP_OK) err = esp_http_client_set_header(client, "Accept", "application/json");
    if (err == ESP_OK) err = esp_http_client_set_post_field(client, body, (int)strlen(body));
    if (err == ESP_OK) err = esp_http_client_perform(client);

    *status_out = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (collector.overflow) return ESP_ERR_INVALID_SIZE;
    return err;
}

static esp_err_t copy_json_string(const cJSON *root, const char *name, char *out, size_t out_size)
{
    if (!root || !name || !out || out_size == 0) return ESP_ERR_INVALID_ARG;
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, name);
    if (!cJSON_IsString(item) || !item->valuestring) return ESP_ERR_INVALID_RESPONSE;
    size_t len = strlen(item->valuestring);
    if (len >= out_size) return ESP_ERR_INVALID_SIZE;
    memcpy(out, item->valuestring, len + 1);
    return ESP_OK;
}

static esp_err_t parse_interval(const cJSON *root, uint32_t *out)
{
    if (!root || !out) return ESP_ERR_INVALID_ARG;
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, "interval");
    if (cJSON_IsNumber(item) && item->valuedouble > 0) {
        *out = (uint32_t)item->valuedouble;
        return ESP_OK;
    }
    if (cJSON_IsString(item) && item->valuestring) {
        char *end = NULL;
        unsigned long value = strtoul(item->valuestring, &end, 10);
        if (end != item->valuestring && value > 0 && value <= UINT32_MAX) {
            *out = (uint32_t)value;
            return ESP_OK;
        }
    }
    return ESP_ERR_INVALID_RESPONSE;
}

static bool form_unreserved(unsigned char c)
{
    return isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~';
}

static esp_err_t form_encode(const char *input, char *out, size_t out_size)
{
    static const char hex[] = "0123456789ABCDEF";
    if (!input || !out || out_size == 0) return ESP_ERR_INVALID_ARG;
    size_t pos = 0;
    for (const unsigned char *p = (const unsigned char *)input; *p; ++p) {
        if (form_unreserved(*p)) {
            if (pos + 1 >= out_size) return ESP_ERR_INVALID_SIZE;
            out[pos++] = (char)*p;
        } else {
            if (pos + 3 >= out_size) return ESP_ERR_INVALID_SIZE;
            out[pos++] = '%';
            out[pos++] = hex[*p >> 4];
            out[pos++] = hex[*p & 0x0f];
        }
    }
    out[pos] = '\0';
    return ESP_OK;
}

static esp_err_t request_device_code(akvm_auth_device_challenge_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;

    char *response = calloc(1, HTTP_RESPONSE_MAX);
    if (!response) return ESP_ERR_NO_MEM;

    const char request_body[] = "{\"client_id\":\"" CODEX_CLIENT_ID "\"}";
    int status = 0;
    esp_err_t err = http_post(CODEX_DEVICE_USERCODE_URL,
                              "application/json",
                              request_body,
                              response,
                              HTTP_RESPONSE_MAX,
                              &status);
    if (err != ESP_OK) goto done;
    if (status < 200 || status >= 300) {
        ESP_LOGW(TAG, "device-code request failed with HTTP %d", status);
        err = status == 404 ? ESP_ERR_NOT_SUPPORTED : ESP_FAIL;
        goto done;
    }

    cJSON *root = cJSON_Parse(response);
    if (!root) {
        err = ESP_ERR_INVALID_RESPONSE;
        goto done;
    }

    memset(out, 0, sizeof(*out));
    err = copy_json_string(root, "device_auth_id", out->device_code, sizeof(out->device_code));
    if (err == ESP_OK) err = copy_json_string(root, "user_code", out->user_code, sizeof(out->user_code));
    if (err != ESP_OK) {
        /* Upstream currently accepts `usercode` as an alias too. */
        if (out->user_code[0] == '\0') err = copy_json_string(root, "usercode", out->user_code, sizeof(out->user_code));
    }
    if (err == ESP_OK) err = parse_interval(root, &out->interval_seconds);
    if (err == ESP_OK) {
        snprintf(out->verification_url, sizeof(out->verification_url), "%s", CODEX_DEVICE_VERIFY_URL);
        s_device_login_started_us = esp_timer_get_time();
    }
    cJSON_Delete(root);

done:
    secure_zero(response, HTTP_RESPONSE_MAX);
    free(response);
    return err;
}

static esp_err_t exchange_authorization_code(const char *authorization_code,
                                             const char *code_verifier,
                                             akvm_auth_token_set_t *out_tokens)
{
    if (!authorization_code || !code_verifier || !out_tokens) return ESP_ERR_INVALID_ARG;

    size_t code_cap = strlen(authorization_code) * 3 + 1;
    size_t verifier_cap = strlen(code_verifier) * 3 + 1;
    size_t redirect_cap = strlen(CODEX_DEVICE_REDIRECT_URI) * 3 + 1;
    size_t client_cap = strlen(CODEX_CLIENT_ID) * 3 + 1;
    char *code = calloc(1, code_cap);
    char *verifier = calloc(1, verifier_cap);
    char *redirect = calloc(1, redirect_cap);
    char *client = calloc(1, client_cap);
    char *response = calloc(1, HTTP_RESPONSE_MAX);
    char *body = calloc(1, code_cap + verifier_cap + redirect_cap + client_cap + 128);
    if (!code || !verifier || !redirect || !client || !response || !body) {
        free(code); free(verifier); free(redirect); free(client); free(response); free(body);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = form_encode(authorization_code, code, code_cap);
    if (err == ESP_OK) err = form_encode(code_verifier, verifier, verifier_cap);
    if (err == ESP_OK) err = form_encode(CODEX_DEVICE_REDIRECT_URI, redirect, redirect_cap);
    if (err == ESP_OK) err = form_encode(CODEX_CLIENT_ID, client, client_cap);
    if (err != ESP_OK) goto done;

    snprintf(body,
             code_cap + verifier_cap + redirect_cap + client_cap + 128,
             "grant_type=authorization_code&code=%s&redirect_uri=%s&client_id=%s&code_verifier=%s",
             code, redirect, client, verifier);

    int status = 0;
    err = http_post(CODEX_OAUTH_TOKEN_URL,
                    "application/x-www-form-urlencoded",
                    body,
                    response,
                    HTTP_RESPONSE_MAX,
                    &status);
    if (err != ESP_OK) goto done;
    if (status < 200 || status >= 300) {
        ESP_LOGW(TAG, "OAuth token exchange failed with HTTP %d", status);
        err = ESP_FAIL;
        goto done;
    }

    cJSON *root = cJSON_Parse(response);
    if (!root) {
        err = ESP_ERR_INVALID_RESPONSE;
        goto done;
    }
    memset(out_tokens, 0, sizeof(*out_tokens));
    err = copy_json_string(root, "id_token", out_tokens->id_token, sizeof(out_tokens->id_token));
    if (err == ESP_OK) err = copy_json_string(root, "access_token", out_tokens->access_token, sizeof(out_tokens->access_token));
    if (err == ESP_OK) err = copy_json_string(root, "refresh_token", out_tokens->refresh_token, sizeof(out_tokens->refresh_token));
    cJSON_Delete(root);

done:
    if (body) secure_zero(body, code_cap + verifier_cap + redirect_cap + client_cap + 128);
    if (response) secure_zero(response, HTTP_RESPONSE_MAX);
    if (code) secure_zero(code, code_cap);
    if (verifier) secure_zero(verifier, verifier_cap);
    free(code); free(verifier); free(redirect); free(client); free(response); free(body);
    return err;
}

static esp_err_t poll_device_code(const akvm_auth_device_challenge_t *challenge,
                                  akvm_auth_token_set_t *out_tokens,
                                  bool *authorization_pending)
{
    if (!challenge || !out_tokens || !authorization_pending) return ESP_ERR_INVALID_ARG;
    *authorization_pending = false;

    if (s_device_login_started_us > 0 &&
        esp_timer_get_time() - s_device_login_started_us >= DEVICE_LOGIN_TIMEOUT_US) {
        return ESP_ERR_TIMEOUT;
    }

    char *response = calloc(1, HTTP_RESPONSE_MAX);
    char *request = calloc(1, AKVM_AUTH_DEVICE_CODE_MAX + AKVM_AUTH_USER_CODE_MAX + 96);
    if (!response || !request) {
        free(response); free(request);
        return ESP_ERR_NO_MEM;
    }

    snprintf(request,
             AKVM_AUTH_DEVICE_CODE_MAX + AKVM_AUTH_USER_CODE_MAX + 96,
             "{\"device_auth_id\":\"%s\",\"user_code\":\"%s\"}",
             challenge->device_code,
             challenge->user_code);

    int status = 0;
    esp_err_t err = http_post(CODEX_DEVICE_TOKEN_URL,
                              "application/json",
                              request,
                              response,
                              HTTP_RESPONSE_MAX,
                              &status);
    if (err != ESP_OK) goto done;

    if (status == 403 || status == 404) {
        *authorization_pending = true;
        err = ESP_OK;
        goto done;
    }
    if (status < 200 || status >= 300) {
        ESP_LOGW(TAG, "device-code poll failed with HTTP %d", status);
        err = ESP_FAIL;
        goto done;
    }

    cJSON *root = cJSON_Parse(response);
    if (!root) {
        err = ESP_ERR_INVALID_RESPONSE;
        goto done;
    }

    char authorization_code[1024] = {0};
    char code_verifier[1024] = {0};
    char code_challenge[1024] = {0};
    err = copy_json_string(root, "authorization_code", authorization_code, sizeof(authorization_code));
    if (err == ESP_OK) err = copy_json_string(root, "code_verifier", code_verifier, sizeof(code_verifier));
    if (err == ESP_OK) err = copy_json_string(root, "code_challenge", code_challenge, sizeof(code_challenge));
    cJSON_Delete(root);

    if (err == ESP_OK) {
        /* `code_challenge` is validated by the server-side flow; retaining the
         * parse here detects an incompatible response shape. */
        err = exchange_authorization_code(authorization_code, code_verifier, out_tokens);
    }
    secure_zero(authorization_code, sizeof(authorization_code));
    secure_zero(code_verifier, sizeof(code_verifier));
    secure_zero(code_challenge, sizeof(code_challenge));

done:
    secure_zero(request, AKVM_AUTH_DEVICE_CODE_MAX + AKVM_AUTH_USER_CODE_MAX + 96);
    secure_zero(response, HTTP_RESPONSE_MAX);
    free(request);
    free(response);
    return err;
}

static esp_err_t refresh_tokens(const akvm_auth_token_set_t *current,
                                akvm_auth_token_set_t *out_tokens)
{
    if (!current || !out_tokens || current->refresh_token[0] == '\0') return ESP_ERR_INVALID_ARG;

    cJSON *request_json = cJSON_CreateObject();
    if (!request_json) return ESP_ERR_NO_MEM;
    cJSON_AddStringToObject(request_json, "client_id", CODEX_CLIENT_ID);
    cJSON_AddStringToObject(request_json, "grant_type", "refresh_token");
    cJSON_AddStringToObject(request_json, "refresh_token", current->refresh_token);
    char *request = cJSON_PrintUnformatted(request_json);
    cJSON_Delete(request_json);
    if (!request) return ESP_ERR_NO_MEM;

    char *response = calloc(1, HTTP_RESPONSE_MAX);
    if (!response) {
        secure_zero(request, strlen(request));
        cJSON_free(request);
        return ESP_ERR_NO_MEM;
    }

    int status = 0;
    esp_err_t err = http_post(CODEX_OAUTH_TOKEN_URL,
                              "application/json",
                              request,
                              response,
                              HTTP_RESPONSE_MAX,
                              &status);
    if (err != ESP_OK) goto done;
    if (status < 200 || status >= 300) {
        ESP_LOGW(TAG, "token refresh failed with HTTP %d", status);
        err = status == 400 || status == 401 ? ESP_ERR_INVALID_STATE : ESP_FAIL;
        goto done;
    }

    cJSON *root = cJSON_Parse(response);
    if (!root) {
        err = ESP_ERR_INVALID_RESPONSE;
        goto done;
    }

    *out_tokens = *current;
    const cJSON *id_token = cJSON_GetObjectItemCaseSensitive(root, "id_token");
    const cJSON *access_token = cJSON_GetObjectItemCaseSensitive(root, "access_token");
    const cJSON *refresh_token = cJSON_GetObjectItemCaseSensitive(root, "refresh_token");

    if (cJSON_IsString(id_token) && id_token->valuestring) {
        if (strlen(id_token->valuestring) >= sizeof(out_tokens->id_token)) err = ESP_ERR_INVALID_SIZE;
        else snprintf(out_tokens->id_token, sizeof(out_tokens->id_token), "%s", id_token->valuestring);
    }
    if (err == ESP_OK && cJSON_IsString(access_token) && access_token->valuestring) {
        if (strlen(access_token->valuestring) >= sizeof(out_tokens->access_token)) err = ESP_ERR_INVALID_SIZE;
        else snprintf(out_tokens->access_token, sizeof(out_tokens->access_token), "%s", access_token->valuestring);
    }
    if (err == ESP_OK && cJSON_IsString(refresh_token) && refresh_token->valuestring) {
        if (strlen(refresh_token->valuestring) >= sizeof(out_tokens->refresh_token)) err = ESP_ERR_INVALID_SIZE;
        else snprintf(out_tokens->refresh_token, sizeof(out_tokens->refresh_token), "%s", refresh_token->valuestring);
    }
    cJSON_Delete(root);

done:
    secure_zero(request, strlen(request));
    cJSON_free(request);
    secure_zero(response, HTTP_RESPONSE_MAX);
    free(response);
    return err;
}

static const akvm_auth_transport_t s_transport = {
    .request_device_code = request_device_code,
    .poll_device_code = poll_device_code,
    .refresh_tokens = refresh_tokens,
};

esp_err_t akvm_codex_auth_init(void)
{
    ESP_LOGI(TAG, "binding Codex device auth transport (upstream %.12s)", AKVM_CODEX_UPSTREAM_SHA);
    return akvm_auth_bind_transport(&s_transport);
}

const char *akvm_codex_auth_upstream_sha(void)
{
    return AKVM_CODEX_UPSTREAM_SHA;
}
