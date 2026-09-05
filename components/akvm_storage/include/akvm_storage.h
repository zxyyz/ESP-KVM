#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AKVM_STORE_SECURE = 0,
    AKVM_STORE_BULK,
} akvm_store_class_t;

typedef struct {
    bool ready;
    uint64_t capacity_bytes;
    uint32_t erase_size;
    uint32_t program_size;
    const char *backend_name;
} akvm_storage_info_t;

esp_err_t akvm_storage_init(void);
esp_err_t akvm_storage_get_info(akvm_store_class_t store, akvm_storage_info_t *out);
bool akvm_storage_secret_allowed(akvm_store_class_t store);

#ifdef __cplusplus
}
#endif
