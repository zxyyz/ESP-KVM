#include "akvm_video.h"

#include <string.h>
#include "akvm_core.h"
#include "esp_log.h"

static const char *TAG = "akvm_video";
static akvm_video_status_t s_status;

esp_err_t akvm_video_init(void)
{
    memset(&s_status, 0, sizeof(s_status));
    akvm_core_set_service_ready(AKVM_SERVICE_VIDEO, false);
#if CONFIG_AKVM_ENABLE_VIDEO
    ESP_LOGW(TAG, "video enabled but HDMI/CSI backend is not bound yet");
#else
    ESP_LOGI(TAG, "video disabled until HDMI bridge is confirmed");
#endif
    return ESP_OK;
}

bool akvm_video_ready(void)
{
    return akvm_core_is_service_ready(AKVM_SERVICE_VIDEO);
}

esp_err_t akvm_video_get_status(akvm_video_status_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    *out = s_status;
    return ESP_OK;
}

esp_err_t akvm_video_get_screen_text(char *out, size_t out_size, size_t *written)
{
    if (!out || out_size == 0) return ESP_ERR_INVALID_ARG;
    out[0] = '\0';
    if (written) *written = 0;
    if (!akvm_video_ready()) return ESP_ERR_INVALID_STATE;
    return ESP_ERR_NOT_SUPPORTED;
}
