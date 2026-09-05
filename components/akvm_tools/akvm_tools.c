#include "akvm_tools.h"

#include <stdio.h>
#include <string.h>
#include "akvm_core.h"
#include "akvm_hid.h"
#include "akvm_video.h"

esp_err_t akvm_tools_init(void)
{
    return ESP_OK;
}

bool akvm_tools_is_destructive(akvm_tool_t tool)
{
    return tool == AKVM_TOOL_POWER_PRESS ||
           tool == AKVM_TOOL_RESET_PRESS ||
           tool == AKVM_TOOL_MOUNT_MEDIA;
}

bool akvm_tools_is_mutating(akvm_tool_t tool)
{
    return tool != AKVM_TOOL_GET_STATUS && tool != AKVM_TOOL_GET_SCREEN_TEXT;
}

esp_err_t akvm_tools_authorize(const akvm_tool_context_t *ctx, akvm_tool_t tool)
{
    if (!ctx) return ESP_ERR_INVALID_ARG;

    if (ctx->actor == AKVM_ACTOR_LOCAL_USER) return ESP_OK;
    if (ctx->actor == AKVM_ACTOR_REMOTE_USER) {
        return ctx->trusted_session ? ESP_OK : ESP_ERR_INVALID_STATE;
    }

    if (ctx->actor != AKVM_ACTOR_AI) return ESP_ERR_INVALID_ARG;

    if (!akvm_tools_is_mutating(tool)) return ESP_OK;

    akvm_ai_policy_t policy = akvm_core_get_ai_policy();
    if (policy == AKVM_AI_POLICY_OBSERVE) return ESP_ERR_INVALID_STATE;

    /* Power/reset/media remain approval-gated even inside an authorized
     * autonomous workflow. */
    if (akvm_tools_is_destructive(tool)) {
        return ctx->interactive_approval ? ESP_OK : ESP_ERR_INVALID_STATE;
    }

    if (policy == AKVM_AI_POLICY_ASSIST) {
        return ctx->interactive_approval ? ESP_OK : ESP_ERR_INVALID_STATE;
    }

    return ctx->workflow_authorized ? ESP_OK : ESP_ERR_INVALID_STATE;
}

esp_err_t akvm_tools_execute(const akvm_tool_context_t *ctx,
                             const akvm_tool_request_t *request,
                             akvm_tool_result_t *result)
{
    if (!ctx || !request || !result) return ESP_ERR_INVALID_ARG;
    memset(result, 0, sizeof(*result));

    result->status = akvm_tools_authorize(ctx, request->tool);
    if (result->status != ESP_OK) return result->status;

    switch (request->tool) {
    case AKVM_TOOL_GET_STATUS: {
        akvm_runtime_snapshot_t snapshot;
        result->status = akvm_core_snapshot(&snapshot);
        if (result->status == ESP_OK) {
            int n = snprintf(result->text, sizeof(result->text),
                             "boot=%d net=%d vpn=%d hid=%d video=%d auth=%d ai=%d policy=%d gen=%lu",
                             snapshot.boot_complete,
                             snapshot.service_ready[AKVM_SERVICE_NETWORK],
                             snapshot.service_ready[AKVM_SERVICE_VPN],
                             snapshot.service_ready[AKVM_SERVICE_HID],
                             snapshot.service_ready[AKVM_SERVICE_VIDEO],
                             snapshot.service_ready[AKVM_SERVICE_AUTH],
                             snapshot.service_ready[AKVM_SERVICE_AI],
                             (int)snapshot.ai_policy,
                             (unsigned long)snapshot.generation);
            if (n > 0) result->text_len = (size_t)n < sizeof(result->text) ? (size_t)n : sizeof(result->text) - 1;
        }
        break;
    }
    case AKVM_TOOL_GET_SCREEN_TEXT:
        result->status = akvm_video_get_screen_text(result->text, sizeof(result->text), &result->text_len);
        break;
    case AKVM_TOOL_SEND_KEYBOARD_REPORT: {
        akvm_keyboard_report_t report = {
            .modifiers = request->args.keyboard.modifiers,
        };
        memcpy(report.keycodes, request->args.keyboard.keycodes, sizeof(report.keycodes));
        result->status = akvm_hid_send_keyboard(&report);
        break;
    }
    case AKVM_TOOL_SEND_MOUSE_REPORT: {
        akvm_mouse_report_t report = {
            .buttons = request->args.mouse.buttons,
            .x = request->args.mouse.x,
            .y = request->args.mouse.y,
            .wheel = request->args.mouse.wheel,
        };
        result->status = akvm_hid_send_mouse(&report);
        break;
    }
    case AKVM_TOOL_RELEASE_INPUT:
        result->status = akvm_hid_release_all();
        break;
    case AKVM_TOOL_POWER_PRESS:
    case AKVM_TOOL_RESET_PRESS:
    case AKVM_TOOL_MOUNT_MEDIA:
        result->status = ESP_ERR_NOT_SUPPORTED;
        break;
    default:
        result->status = ESP_ERR_INVALID_ARG;
        break;
    }

    return result->status;
}
