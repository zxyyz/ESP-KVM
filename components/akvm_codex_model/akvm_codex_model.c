#include "akvm_codex_model.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sdkconfig.h"
#include "akvm_ai.h"
#include "akvm_auth.h"
#include "akvm_sse.h"
#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "mbedtls/base64.h"

/* All ChatGPT/Codex wire constants live in this component and are tied to the
 * reviewed upstream revision. This is intentionally an upstream-coupled
 * compatibility layer, not a claim that these routes are a generic stable API. */
#define CODEX_BASE_URL "https://chatgpt.com/backend-api/codex"
#define CODEX_MODELS_URL CODEX_BASE_URL "/models?client_version=" AKVM_CODEX_COMPAT_CLIENT_VERSION
#define CODEX_RESPONSES_URL CODEX_BASE_URL "/responses"
#define CODEX_MODEL_MAX 128
#define CODEX_ACCOUNT_ID_MAX 192
#define CODEX_HTTP_TIMEOUT_MS 300000
#define CODEX_ORIGINATOR "p4_c5_ai_kvm"
#define CODEX_USER_AGENT "p4-c5-ai-kvm/0.1 codex-compat/" AKVM_CODEX_COMPAT_CLIENT_VERSION

static const char *TAG = "akvm_codex_model";
static char s_model[CODEX_MODEL_MAX];

typedef struct {
    char account_id[CODEX_ACCOUNT_ID_MAX];
    bool fedramp;
} codex_claims_t;

typedef struct {
    char *buffer;
    size_t capacity;
    size_t length;
    bool overflow;
} response_collector_t;

typedef struct {
    akvm_sse_parser_t parser;
    akvm_ai_text_cb_t on_text;
    void *callback_ctx;
    esp_err_t stream_error;
    bool completed;
    bool saw_text;
} codex_stream_ctx_t;

static void secure_zero(void *ptr, size_t len)
{
    volatile unsigned char *p = (volatile unsigned char *)ptr;
    while (len--) *p++ = 0;
}

static esp_err_t decode_id_token_claims(const char *jwt, codex_claims_t *out)
{
    if (!jwt || !out) return ESP_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));

    const char *dot1 = strchr(jwt, '.');
    if (!dot1) return ESP_ERR_INVALID_RESPONSE;
    const char *payload = dot1 + 1;
    const char *dot2 = strchr(payload, '.');
    if (!dot2 || dot2 == payload) return ESP_ERR_INVALID_RESPONSE;

    size_t payload_len = (size_t)(dot2 - payload);
    size_t padded_len = (payload_len + 3U) & ~3U;
    char *encoded = calloc(1, padded_len + 1);
    if (!encoded) return ESP_ERR_NO_MEM;

    for (size_t i = 0; i < payload_len; ++i) {
        char c = payload[i];
        encoded[i] = c == '-' ? '+' : (c == '_' ? '/' : c);
    }
    for (size_t i = payload_len; i < padded_len; ++i) encoded[i] = '=';

    size_t decoded_cap = (padded_len / 4U) * 3U + 1U;
    unsigned char *decoded = calloc(1, decoded_cap);
    if (!decoded) {
        secure_zero(encoded, padded_len + 1);
        free(encoded);
        return ESP_ERR_NO_MEM;
    }

    size_t decoded_len = 0;
    int rc = mbedtls_base64_decode(decoded,
                                   decoded_cap - 1,
                                   &decoded_len,
                                   (const unsigned char *)encoded,
                                   padded_len);
    secure_zero(encoded, padded_len + 1);
    free(encoded);
    if (rc != 0 || decoded_len == 0 || decoded_len >= decoded_cap) {
        secure_zero(decoded, decoded_cap);
        free(decoded);
        return ESP_ERR_INVALID_RESPONSE;
    }
    decoded[decoded_len] = '\0';

    cJSON *root = cJSON_Parse((const char *)decoded);
    if (!root) {
        secure_zero(decoded, decoded_cap);
        free(decoded);
        return ESP_ERR_INVALID_RESPONSE;
    }

    const cJSON *auth = cJSON_GetObjectItemCaseSensitive(root, "https://api.openai.com/auth");
    const cJSON *account = cJSON_IsObject(auth)
        ? cJSON_GetObjectItemCaseSensitive(auth, "chatgpt_account_id")
        : cJSON_GetObjectItemCaseSensitive(root, "chatgpt_account_id");
    const cJSON *fedramp = cJSON_IsObject(auth)
        ? cJSON_GetObjectItemCaseSensitive(auth, "chatgpt_account_is_fedramp")
        : cJSON_GetObjectItemCaseSensitive(root, "chatgpt_account_is_fedramp");

    esp_err_t err = ESP_OK;
    if (!cJSON_IsString(account) || !account->valuestring || account->valuestring[0] == '\0') {
        err = ESP_ERR_INVALID_RESPONSE;
    } else if (strlen(account->valuestring) >= sizeof(out->account_id)) {
        err = ESP_ERR_INVALID_SIZE;
    } else {
        snprintf(out->account_id, sizeof(out->account_id), "%s", account->valuestring);
        out->fedramp = cJSON_IsTrue(fedramp);
    }

    cJSON_Delete(root);
    secure_zero(decoded, decoded_cap);
    free(decoded);
    return err;
}

static esp_err_t set_auth_headers(esp_http_client_handle_t client,
                                  const akvm_auth_token_set_t *tokens,
                                  const codex_claims_t *claims)
{
    if (!client || !tokens || !claims || tokens->access_token[0] == '\0' || claims->account_id[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    size_t bearer_len = strlen(tokens->access_token) + sizeof("Bearer ");
    char *bearer = calloc(1, bearer_len);
    if (!bearer) return ESP_ERR_NO_MEM;
    snprintf(bearer, bearer_len, "Bearer %s", tokens->access_token);

    esp_err_t err = esp_http_client_set_header(client, "Authorization", bearer);
    secure_zero(bearer, bearer_len);
    free(bearer);
    if (err == ESP_OK) err = esp_http_client_set_header(client, "ChatGPT-Account-ID", claims->account_id);
    if (err == ESP_OK && claims->fedramp) err = esp_http_client_set_header(client, "X-OpenAI-Fedramp", "true");
    if (err == ESP_OK) err = esp_http_client_set_header(client, "originator", CODEX_ORIGINATOR);
    if (err == ESP_OK) err = esp_http_client_set_header(client, "User-Agent", CODEX_USER_AGENT);
    return err;
}

static esp_err_t collector_event_handler(esp_http_client_event_t *evt)
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

static esp_err_t fetch_models_once(char *response, size_t response_capacity, int *status_out)
{
    if (!response || response_capacity < 2 || !status_out) return ESP_ERR_INVALID_ARG;
    const akvm_auth_token_set_t *tokens = akvm_auth_tokens();
    if (!tokens) return ESP_ERR_INVALID_STATE;

    codex_claims_t claims;
    esp_err_t err = decode_id_token_claims(tokens->id_token, &claims);
    if (err != ESP_OK) return err;

    response_collector_t collector = {
        .buffer = response,
        .capacity = response_capacity,
    };
    response[0] = '\0';

    esp_http_client_config_t config = {
        .url = CODEX_MODELS_URL,
        .timeout_ms = 30000,
        .event_handler = collector_event_handler,
        .user_data = &collector,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return ESP_ERR_NO_MEM;

    err = esp_http_client_set_method(client, HTTP_METHOD_GET);
    if (err == ESP_OK) err = esp_http_client_set_header(client, "Accept", "application/json");
    if (err == ESP_OK) err = set_auth_headers(client, tokens, &claims);
    if (err == ESP_OK) err = esp_http_client_perform(client);
    *status_out = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    secure_zero(&claims, sizeof(claims));
    if (collector.overflow) return ESP_ERR_INVALID_SIZE;
    return err;
}

static bool model_supported(const cJSON *model, bool require_list_visibility)
{
    if (!cJSON_IsObject(model)) return false;
    const cJSON *supported = cJSON_GetObjectItemCaseSensitive(model, "supported_in_api");
    if (!cJSON_IsTrue(supported)) return false;
    if (!require_list_visibility) return true;
    const cJSON *visibility = cJSON_GetObjectItemCaseSensitive(model, "visibility");
    return cJSON_IsString(visibility) && visibility->valuestring && strcmp(visibility->valuestring, "list") == 0;
}

static esp_err_t choose_model_from_catalog(const char *response)
{
    cJSON *root = cJSON_Parse(response);
    if (!root) return ESP_ERR_INVALID_RESPONSE;
    const cJSON *models = cJSON_GetObjectItemCaseSensitive(root, "models");
    if (!cJSON_IsArray(models)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }

    esp_err_t err = ESP_ERR_NOT_FOUND;
    for (int pass = 0; pass < 2 && err != ESP_OK; ++pass) {
        bool require_list = pass == 0;
        int best_priority = INT_MAX;
        const char *best_slug = NULL;
        const cJSON *model = NULL;
        cJSON_ArrayForEach(model, models) {
            if (!model_supported(model, require_list)) continue;
            const cJSON *slug = cJSON_GetObjectItemCaseSensitive(model, "slug");
            const cJSON *priority = cJSON_GetObjectItemCaseSensitive(model, "priority");
            if (!cJSON_IsString(slug) || !slug->valuestring || slug->valuestring[0] == '\0') continue;
            int value = cJSON_IsNumber(priority) ? priority->valueint : INT_MAX - 1;
            if (!best_slug || value < best_priority) {
                best_slug = slug->valuestring;
                best_priority = value;
            }
        }
        if (best_slug) {
            size_t len = strlen(best_slug);
            if (len >= sizeof(s_model)) err = ESP_ERR_INVALID_SIZE;
            else {
                memcpy(s_model, best_slug, len + 1);
                err = ESP_OK;
            }
        }
    }

    cJSON_Delete(root);
    return err;
}

static esp_err_t ensure_model_selected(void)
{
    if (s_model[0] != '\0') return ESP_OK;
    if (CONFIG_AKVM_CODEX_MODEL_OVERRIDE[0] != '\0') {
        if (strlen(CONFIG_AKVM_CODEX_MODEL_OVERRIDE) >= sizeof(s_model)) return ESP_ERR_INVALID_SIZE;
        snprintf(s_model, sizeof(s_model), "%s", CONFIG_AKVM_CODEX_MODEL_OVERRIDE);
        return ESP_OK;
    }

    char *response = calloc(1, CONFIG_AKVM_CODEX_MODEL_CATALOG_MAX + 1U);
    if (!response) return ESP_ERR_NO_MEM;

    esp_err_t err = ESP_FAIL;
    for (int attempt = 0; attempt < 2; ++attempt) {
        int status = 0;
        err = fetch_models_once(response, CONFIG_AKVM_CODEX_MODEL_CATALOG_MAX + 1U, &status);
        if (err != ESP_OK) break;
        if (status == 401 && attempt == 0) {
            err = akvm_auth_refresh();
            if (err != ESP_OK) break;
            memset(response, 0, CONFIG_AKVM_CODEX_MODEL_CATALOG_MAX + 1U);
            continue;
        }
        if (status < 200 || status >= 300) {
            ESP_LOGW(TAG, "Codex model catalog returned HTTP %d", status);
            err = ESP_FAIL;
            break;
        }
        err = choose_model_from_catalog(response);
        break;
    }

    if (err == ESP_OK) ESP_LOGI(TAG, "selected account model: %s", s_model);
    secure_zero(response, CONFIG_AKVM_CODEX_MODEL_CATALOG_MAX + 1U);
    free(response);
    return err;
}

static char *build_text_request(const char *prompt)
{
    if (!prompt || strlen(prompt) > CONFIG_AKVM_MAX_TOOL_PAYLOAD) return NULL;

    cJSON *root = cJSON_CreateObject();
    cJSON *input = cJSON_CreateArray();
    cJSON *message = cJSON_CreateObject();
    cJSON *content = cJSON_CreateArray();
    cJSON *text = cJSON_CreateObject();
    cJSON *include = cJSON_CreateArray();
    if (!root || !input || !message || !content || !text || !include) goto fail;

    if (!cJSON_AddStringToObject(root, "model", s_model)) goto fail;
    if (!cJSON_AddStringToObject(root, "instructions",
        "You are the reasoning component of a constrained embedded IP-KVM. "
        "Reply concisely. In this compatibility stage you have no control tools, "
        "so never claim that you changed the target computer.")) goto fail;

    if (!cJSON_AddStringToObject(message, "type", "message")) goto fail;
    if (!cJSON_AddStringToObject(message, "role", "user")) goto fail;
    if (!cJSON_AddStringToObject(text, "type", "input_text")) goto fail;
    if (!cJSON_AddStringToObject(text, "text", prompt)) goto fail;
    cJSON_AddItemToArray(content, text); text = NULL;
    cJSON_AddItemToObject(message, "content", content); content = NULL;
    cJSON_AddItemToArray(input, message); message = NULL;
    cJSON_AddItemToObject(root, "input", input); input = NULL;

    if (!cJSON_AddStringToObject(root, "tool_choice", "auto")) goto fail;
    if (!cJSON_AddBoolToObject(root, "parallel_tool_calls", false)) goto fail;
    cJSON_AddItemToObject(root, "reasoning", cJSON_CreateNull());
    if (!cJSON_AddBoolToObject(root, "store", false)) goto fail;
    if (!cJSON_AddBoolToObject(root, "stream", true)) goto fail;
    cJSON_AddItemToObject(root, "include", include); include = NULL;

    char *serialized = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return serialized;

fail:
    cJSON_Delete(text);
    cJSON_Delete(content);
    cJSON_Delete(message);
    cJSON_Delete(input);
    cJSON_Delete(include);
    cJSON_Delete(root);
    return NULL;
}

static esp_err_t model_sse_event(const akvm_sse_event_t *event, void *opaque)
{
    codex_stream_ctx_t *ctx = (codex_stream_ctx_t *)opaque;
    if (!event || !ctx) return ESP_ERR_INVALID_ARG;
    if (event->data_len == 0 || strcmp(event->data, "[DONE]") == 0) return ESP_OK;

    cJSON *root = cJSON_Parse(event->data);
    if (!root) {
        ctx->stream_error = ESP_ERR_INVALID_RESPONSE;
        return ctx->stream_error;
    }
    const cJSON *type = cJSON_GetObjectItemCaseSensitive(root, "type");
    const char *kind = cJSON_IsString(type) ? type->valuestring : NULL;

    if (kind && strcmp(kind, "response.output_text.delta") == 0) {
        const cJSON *delta = cJSON_GetObjectItemCaseSensitive(root, "delta");
        if (cJSON_IsString(delta) && delta->valuestring) {
            ctx->saw_text = true;
            if (ctx->on_text) ctx->on_text(delta->valuestring, strlen(delta->valuestring), ctx->callback_ctx);
        }
    } else if (kind && strcmp(kind, "response.completed") == 0) {
        ctx->completed = true;
    } else if (kind && (strcmp(kind, "response.failed") == 0 || strcmp(kind, "response.incomplete") == 0 || strcmp(kind, "error") == 0)) {
        const cJSON *response = cJSON_GetObjectItemCaseSensitive(root, "response");
        const cJSON *error = cJSON_IsObject(response) ? cJSON_GetObjectItemCaseSensitive(response, "error") : cJSON_GetObjectItemCaseSensitive(root, "error");
        const cJSON *code = cJSON_IsObject(error) ? cJSON_GetObjectItemCaseSensitive(error, "code") : NULL;
        ESP_LOGW(TAG, "Codex stream failed%s%s",
                 cJSON_IsString(code) && code->valuestring ? ": " : "",
                 cJSON_IsString(code) && code->valuestring ? code->valuestring : "");
        ctx->stream_error = ESP_FAIL;
    }

    cJSON_Delete(root);
    return ctx->stream_error;
}

static esp_err_t stream_event_handler(esp_http_client_event_t *evt)
{
    if (!evt || evt->event_id != HTTP_EVENT_ON_DATA || evt->data_len <= 0) return ESP_OK;
    codex_stream_ctx_t *ctx = (codex_stream_ctx_t *)evt->user_data;
    if (!ctx) return ESP_ERR_INVALID_ARG;
    esp_err_t err = akvm_sse_feed(&ctx->parser, evt->data, (size_t)evt->data_len);
    if (err != ESP_OK) ctx->stream_error = err;
    return err;
}

static esp_err_t stream_turn_once(const char *request,
                                  akvm_ai_text_cb_t on_text,
                                  void *callback_ctx,
                                  int *status_out)
{
    if (!request || !status_out) return ESP_ERR_INVALID_ARG;
    const akvm_auth_token_set_t *tokens = akvm_auth_tokens();
    if (!tokens) return ESP_ERR_INVALID_STATE;

    codex_claims_t claims;
    esp_err_t err = decode_id_token_claims(tokens->id_token, &claims);
    if (err != ESP_OK) return err;

    codex_stream_ctx_t *stream = calloc(1, sizeof(*stream));
    if (!stream) {
        secure_zero(&claims, sizeof(claims));
        return ESP_ERR_NO_MEM;
    }
    stream->on_text = on_text;
    stream->callback_ctx = callback_ctx;
    err = akvm_sse_init(&stream->parser, model_sse_event, stream);
    if (err != ESP_OK) goto done;

    esp_http_client_config_t config = {
        .url = CODEX_RESPONSES_URL,
        .timeout_ms = CODEX_HTTP_TIMEOUT_MS,
        .event_handler = stream_event_handler,
        .user_data = stream,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        err = ESP_ERR_NO_MEM;
        goto done;
    }

    err = esp_http_client_set_method(client, HTTP_METHOD_POST);
    if (err == ESP_OK) err = esp_http_client_set_header(client, "Content-Type", "application/json");
    if (err == ESP_OK) err = esp_http_client_set_header(client, "Accept", "text/event-stream");
    if (err == ESP_OK) err = set_auth_headers(client, tokens, &claims);
    if (err == ESP_OK) err = esp_http_client_set_post_field(client, request, (int)strlen(request));
    if (err == ESP_OK) err = esp_http_client_perform(client);
    *status_out = esp_http_client_get_status_code(client);

    if (err == ESP_OK && *status_out >= 200 && *status_out < 300) {
        esp_err_t finish_err = akvm_sse_finish(&stream->parser);
        if (finish_err != ESP_OK) err = finish_err;
        else if (stream->stream_error != ESP_OK) err = stream->stream_error;
        else if (!stream->completed) err = ESP_ERR_INVALID_RESPONSE;
    }
    esp_http_client_cleanup(client);

done:
    secure_zero(&claims, sizeof(claims));
    if (stream) {
        secure_zero(stream, sizeof(*stream));
        free(stream);
    }
    return err;
}

static esp_err_t run_turn(const char *prompt, akvm_ai_text_cb_t on_text, void *callback_ctx)
{
    esp_err_t err = ensure_model_selected();
    if (err != ESP_OK) return err;

    char *request = build_text_request(prompt);
    if (!request) return ESP_ERR_NO_MEM;

    for (int attempt = 0; attempt < 2; ++attempt) {
        int status = 0;
        err = stream_turn_once(request, on_text, callback_ctx, &status);
        if (status == 401 && attempt == 0) {
            err = akvm_auth_refresh();
            if (err == ESP_OK) continue;
        }
        if (err == ESP_OK && (status < 200 || status >= 300)) {
            ESP_LOGW(TAG, "Codex responses returned HTTP %d", status);
            err = ESP_FAIL;
        }
        break;
    }

    secure_zero(request, strlen(request));
    cJSON_free(request);
    return err;
}

static const akvm_ai_transport_t s_transport = {
    .run_turn = run_turn,
};

esp_err_t akvm_codex_model_init(void)
{
    s_model[0] = '\0';
    ESP_LOGI(TAG, "binding Codex text transport (upstream %.12s, client %s)",
             AKVM_CODEX_MODEL_UPSTREAM_SHA,
             AKVM_CODEX_COMPAT_CLIENT_VERSION);
    return akvm_ai_bind_transport(&s_transport);
}

const char *akvm_codex_model_selected(void)
{
    return s_model[0] ? s_model : NULL;
}

void akvm_codex_model_clear_selection(void)
{
    memset(s_model, 0, sizeof(s_model));
}
