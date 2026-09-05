#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AKVM_CODEX_UPSTREAM_SHA "588b781ab4924ce7352488394028e63d74cf807f"

esp_err_t akvm_codex_auth_init(void);
const char *akvm_codex_auth_upstream_sha(void);

#ifdef __cplusplus
}
#endif
