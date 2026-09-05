#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "akvm_ai.h"
#include "akvm_auth.h"
#include "akvm_board.h"
#include "akvm_codex_auth.h"
#include "akvm_codex_model.h"
#include "akvm_core.h"
#include "akvm_hid.h"
#include "akvm_net.h"
#include "akvm_storage.h"
#include "akvm_tools.h"
#include "akvm_video.h"

static const char *TAG = "akvm_main";

static esp_err_t init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "erase NVS");
        err = nvs_flash_init();
    }
    return err;
}

void app_main(void)
{
    ESP_ERROR_CHECK(init_nvs());
    ESP_ERROR_CHECK(akvm_core_init());
    ESP_ERROR_CHECK(akvm_board_init());
    ESP_ERROR_CHECK(akvm_storage_init());

    /* Each service owns its failure state. Missing optional hardware must not
     * prevent the management plane from booting. */
    ESP_ERROR_CHECK(akvm_net_init());
    ESP_ERROR_CHECK(akvm_hid_init());
    ESP_ERROR_CHECK(akvm_video_init());
    ESP_ERROR_CHECK(akvm_tools_init());
    ESP_ERROR_CHECK(akvm_auth_init());
#if CONFIG_AKVM_ENABLE_AI
    ESP_ERROR_CHECK(akvm_codex_auth_init());
#endif
    ESP_ERROR_CHECK(akvm_ai_init());
#if CONFIG_AKVM_ENABLE_AI
    ESP_ERROR_CHECK(akvm_codex_model_init());
#endif

    akvm_core_set_boot_complete();
    ESP_LOGI(TAG, "P4+C5 AI KVM architecture scaffold started");
}
