#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "sdkconfig.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AKVM_SSE_EVENT_NAME_MAX 64
#define AKVM_SSE_FIELD_NAME_MAX 16
#define AKVM_SSE_DATA_MAX CONFIG_AKVM_MAX_TOOL_PAYLOAD

typedef struct {
    const char *event;
    const char *data;
    size_t data_len;
} akvm_sse_event_t;

typedef esp_err_t (*akvm_sse_event_cb_t)(const akvm_sse_event_t *event, void *ctx);

typedef enum {
    AKVM_SSE_FIELD_NONE = 0,
    AKVM_SSE_FIELD_DATA,
    AKVM_SSE_FIELD_EVENT,
    AKVM_SSE_FIELD_IGNORE,
} akvm_sse_field_kind_t;

typedef struct {
    char event_name[AKVM_SSE_EVENT_NAME_MAX];
    size_t event_name_len;
    char data[AKVM_SSE_DATA_MAX + 1];
    size_t data_len;

    char field_name[AKVM_SSE_FIELD_NAME_MAX];
    size_t field_name_len;
    akvm_sse_field_kind_t field_kind;
    bool field_complete;
    bool skip_single_space;
    bool line_nonempty;
    bool line_had_data;

    akvm_sse_event_cb_t callback;
    void *callback_ctx;
} akvm_sse_parser_t;

esp_err_t akvm_sse_init(akvm_sse_parser_t *parser,
                        akvm_sse_event_cb_t callback,
                        void *callback_ctx);
void akvm_sse_reset(akvm_sse_parser_t *parser);
esp_err_t akvm_sse_feed(akvm_sse_parser_t *parser, const void *data, size_t len);
esp_err_t akvm_sse_finish(akvm_sse_parser_t *parser);

#ifdef __cplusplus
}
#endif
