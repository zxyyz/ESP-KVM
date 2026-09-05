#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AKVM_CODEX_MODEL_UPSTREAM_SHA "588b781ab4924ce7352488394028e63d74cf807f"
#define AKVM_CODEX_COMPAT_CLIENT_VERSION "0.153.4"

esp_err_t akvm_codex_model_init(void);
const char *akvm_codex_model_selected(void);
void akvm_codex_model_clear_selection(void);

#ifdef __cplusplus
}
#endif
