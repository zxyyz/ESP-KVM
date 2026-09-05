#include "akvm_hid.h"

#include "akvm_core.h"
#include "esp_log.h"

static const char *TAG = "akvm_hid";
static bool s_ready;

esp_err_t akvm_hid_init(void)
{
    s_ready = false;
    akvm_core_set_service_ready(AKVM_SERVICE_HID, false);
#if CONFIG_AKVM_ENABLE_HID
    ESP_LOGW(TAG, "HID feature enabled but TinyUSB board binding is not implemented yet");
#else
    ESP_LOGI(TAG, "HID disabled until USB-OTG wiring is confirmed");
#endif
    return ESP_OK;
}

bool akvm_hid_ready(void)
{
    return s_ready;
}

esp_err_t akvm_hid_send_keyboard(const akvm_keyboard_report_t *report)
{
    if (!report) return ESP_ERR_INVALID_ARG;
    if (!s_ready) return ESP_ERR_INVALID_STATE;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t akvm_hid_send_mouse(const akvm_mouse_report_t *report)
{
    if (!report) return ESP_ERR_INVALID_ARG;
    if (!s_ready) return ESP_ERR_INVALID_STATE;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t akvm_hid_release_all(void)
{
    if (!s_ready) return ESP_ERR_INVALID_STATE;
    return ESP_ERR_NOT_SUPPORTED;
}
