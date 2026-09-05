#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AKVM_ACTOR_LOCAL_USER = 0,
    AKVM_ACTOR_REMOTE_USER,
    AKVM_ACTOR_AI,
} akvm_actor_t;

typedef enum {
    AKVM_TOOL_GET_STATUS = 0,
    AKVM_TOOL_GET_SCREEN_TEXT,
    AKVM_TOOL_SEND_KEYBOARD_REPORT,
    AKVM_TOOL_SEND_MOUSE_REPORT,
    AKVM_TOOL_RELEASE_INPUT,
    AKVM_TOOL_POWER_PRESS,
    AKVM_TOOL_RESET_PRESS,
    AKVM_TOOL_MOUNT_MEDIA,
} akvm_tool_t;

typedef struct {
    akvm_actor_t actor;
    bool trusted_session;
    bool interactive_approval;
    bool workflow_authorized;
} akvm_tool_context_t;

typedef struct {
    akvm_tool_t tool;
    union {
        struct {
            uint8_t modifiers;
            uint8_t keycodes[6];
        } keyboard;
        struct {
            uint8_t buttons;
            int16_t x;
            int16_t y;
            int8_t wheel;
        } mouse;
    } args;
} akvm_tool_request_t;

typedef struct {
    esp_err_t status;
    char text[CONFIG_AKVM_MAX_SCREEN_TEXT];
    size_t text_len;
} akvm_tool_result_t;

esp_err_t akvm_tools_init(void);
bool akvm_tools_is_destructive(akvm_tool_t tool);
bool akvm_tools_is_mutating(akvm_tool_t tool);
esp_err_t akvm_tools_authorize(const akvm_tool_context_t *ctx, akvm_tool_t tool);
esp_err_t akvm_tools_execute(const akvm_tool_context_t *ctx,
                             const akvm_tool_request_t *request,
                             akvm_tool_result_t *result);

#ifdef __cplusplus
}
#endif
