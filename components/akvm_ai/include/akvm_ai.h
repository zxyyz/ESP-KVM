#pragma once

#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AKVM_AI_STOPPED = 0,
    AKVM_AI_IDLE,
    AKVM_AI_RUNNING,
    AKVM_AI_ERROR,
} akvm_ai_state_t;

typedef void (*akvm_ai_text_cb_t)(const char *text, size_t len, void *ctx);

typedef struct {
    esp_err_t (*run_turn)(const char *prompt,
                          akvm_ai_text_cb_t on_text,
                          void *cb_ctx);
} akvm_ai_transport_t;

esp_err_t akvm_ai_init(void);
esp_err_t akvm_ai_bind_transport(const akvm_ai_transport_t *transport);
esp_err_t akvm_ai_run_turn(const char *prompt, akvm_ai_text_cb_t on_text, void *cb_ctx);
akvm_ai_state_t akvm_ai_state(void);

#ifdef __cplusplus
}
#endif
