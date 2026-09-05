#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t modifiers;
    uint8_t keycodes[6];
} akvm_keyboard_report_t;

typedef struct {
    uint8_t buttons;
    int16_t x;
    int16_t y;
    int8_t wheel;
} akvm_mouse_report_t;

esp_err_t akvm_hid_init(void);
bool akvm_hid_ready(void);
esp_err_t akvm_hid_send_keyboard(const akvm_keyboard_report_t *report);
esp_err_t akvm_hid_send_mouse(const akvm_mouse_report_t *report);
esp_err_t akvm_hid_release_all(void);

#ifdef __cplusplus
}
#endif
