#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool signal_present;
    uint16_t width;
    uint16_t height;
    uint16_t fps_x100;
    uint32_t generation;
} akvm_video_status_t;

esp_err_t akvm_video_init(void);
bool akvm_video_ready(void);
esp_err_t akvm_video_get_status(akvm_video_status_t *out);
esp_err_t akvm_video_get_screen_text(char *out, size_t out_size, size_t *written);

#ifdef __cplusplus
}
#endif
