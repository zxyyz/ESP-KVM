#include "akvm_storage.h"

#include <string.h>

static akvm_storage_info_t s_secure = {
    .ready = false,
    .backend_name = "main-flash-protected",
};

static akvm_storage_info_t s_bulk = {
    .ready = false,
    .capacity_bytes = 256ULL * 1024ULL * 1024ULL,
    .backend_name = "external-256mb-unbound",
};

esp_err_t akvm_storage_init(void)
{
    /* The secure backend will be bound to encrypted NVS/main-flash storage.
     * The 256 MB backend stays unavailable until the exact flash device and bus
     * geometry are known. */
    s_secure.ready = false;
    s_bulk.ready = false;
    return ESP_OK;
}

esp_err_t akvm_storage_get_info(akvm_store_class_t store, akvm_storage_info_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    switch (store) {
    case AKVM_STORE_SECURE:
        *out = s_secure;
        return ESP_OK;
    case AKVM_STORE_BULK:
        *out = s_bulk;
        return ESP_OK;
    default:
        memset(out, 0, sizeof(*out));
        return ESP_ERR_INVALID_ARG;
    }
}

bool akvm_storage_secret_allowed(akvm_store_class_t store)
{
    /* External bulk flash must never become the accidental default destination
     * for OAuth, VPN, Wi-Fi or device identity secrets. */
    return store == AKVM_STORE_SECURE;
}
