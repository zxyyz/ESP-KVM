#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AKVM_AUTH_URL_MAX 256
#define AKVM_AUTH_USER_CODE_MAX 40
#define AKVM_AUTH_DEVICE_CODE_MAX 512
#define AKVM_AUTH_TOKEN_MAX 4096

typedef enum {
    AKVM_AUTH_SIGNED_OUT = 0,
    AKVM_AUTH_DEVICE_PENDING,
    AKVM_AUTH_READY,
    AKVM_AUTH_ERROR,
} akvm_auth_state_t;

typedef struct {
    char verification_url[AKVM_AUTH_URL_MAX];
    char user_code[AKVM_AUTH_USER_CODE_MAX];
    char device_code[AKVM_AUTH_DEVICE_CODE_MAX];
    uint32_t interval_seconds;
    uint64_t expires_at_unix;
} akvm_auth_device_challenge_t;

typedef struct {
    char access_token[AKVM_AUTH_TOKEN_MAX];
    char refresh_token[AKVM_AUTH_TOKEN_MAX];
    uint64_t access_expires_at_unix;
} akvm_auth_token_set_t;

typedef struct {
    esp_err_t (*request_device_code)(akvm_auth_device_challenge_t *out);
    esp_err_t (*poll_device_code)(const akvm_auth_device_challenge_t *challenge,
                                  akvm_auth_token_set_t *out_tokens,
                                  bool *authorization_pending);
    esp_err_t (*refresh_tokens)(const akvm_auth_token_set_t *current,
                                akvm_auth_token_set_t *out_tokens);
} akvm_auth_transport_t;

esp_err_t akvm_auth_init(void);
esp_err_t akvm_auth_bind_transport(const akvm_auth_transport_t *transport);
esp_err_t akvm_auth_begin_device_login(akvm_auth_device_challenge_t *out);
esp_err_t akvm_auth_poll_device_login(bool *still_pending);
esp_err_t akvm_auth_sign_out(void);
akvm_auth_state_t akvm_auth_state(void);
const akvm_auth_token_set_t *akvm_auth_tokens(void);

#ifdef __cplusplus
}
#endif
