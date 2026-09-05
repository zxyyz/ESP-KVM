#include "akvm_board.h"

#include "esp_chip_info.h"
#include "esp_log.h"

static const char *TAG = "akvm_board";
static akvm_board_info_t s_info = {
    .profile_name = "unbound-p4-c5",
};

esp_err_t akvm_board_init(void)
{
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    s_info.p4_revision = chip.revision;
    ESP_LOGW(TAG, "board profile is not bound yet; P4 revision=%u", (unsigned)s_info.p4_revision);
    return ESP_OK;
}

const akvm_board_info_t *akvm_board_get_info(void)
{
    return &s_info;
}
