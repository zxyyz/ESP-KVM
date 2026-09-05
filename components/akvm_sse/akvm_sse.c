#include "akvm_sse.h"

#include <string.h>

static void reset_line(akvm_sse_parser_t *p)
{
    memset(p->field_name, 0, sizeof(p->field_name));
    p->field_name_len = 0;
    p->field_kind = AKVM_SSE_FIELD_NONE;
    p->field_complete = false;
    p->skip_single_space = false;
    p->line_nonempty = false;
    p->line_had_data = false;
}

static void classify_field(akvm_sse_parser_t *p)
{
    p->field_name[p->field_name_len] = '\0';
    if (strcmp(p->field_name, "data") == 0) {
        p->field_kind = AKVM_SSE_FIELD_DATA;
    } else if (strcmp(p->field_name, "event") == 0) {
        p->field_kind = AKVM_SSE_FIELD_EVENT;
    } else {
        p->field_kind = AKVM_SSE_FIELD_IGNORE;
    }
    p->field_complete = true;
    p->skip_single_space = true;
}

static esp_err_t dispatch_event(akvm_sse_parser_t *p)
{
    if (p->data_len == 0) {
        p->event_name_len = 0;
        p->event_name[0] = '\0';
        return ESP_OK;
    }

    if (p->data_len > 0 && p->data[p->data_len - 1] == '\n') {
        --p->data_len;
    }
    p->data[p->data_len] = '\0';
    p->event_name[p->event_name_len] = '\0';

    akvm_sse_event_t event = {
        .event = p->event_name,
        .data = p->data,
        .data_len = p->data_len,
    };

    esp_err_t err = p->callback ? p->callback(&event, p->callback_ctx) : ESP_OK;

    p->data_len = 0;
    p->data[0] = '\0';
    p->event_name_len = 0;
    p->event_name[0] = '\0';
    return err;
}

static esp_err_t end_line(akvm_sse_parser_t *p)
{
    if (!p->line_nonempty) {
        reset_line(p);
        return dispatch_event(p);
    }

    if (p->line_had_data) {
        if (p->data_len >= AKVM_SSE_DATA_MAX) return ESP_ERR_INVALID_SIZE;
        p->data[p->data_len++] = '\n';
    }
    reset_line(p);
    return ESP_OK;
}

static esp_err_t consume_char(akvm_sse_parser_t *p, char c)
{
    if (c == '\r') return ESP_OK;
    if (c == '\n') return end_line(p);

    p->line_nonempty = true;

    if (!p->field_complete) {
        if (p->field_name_len == 0 && c == ':') {
            p->field_kind = AKVM_SSE_FIELD_IGNORE;
            p->field_complete = true;
            return ESP_OK;
        }
        if (c == ':') {
            classify_field(p);
            return ESP_OK;
        }
        if (p->field_name_len + 1 >= sizeof(p->field_name)) {
            p->field_kind = AKVM_SSE_FIELD_IGNORE;
            p->field_complete = true;
            return ESP_OK;
        }
        p->field_name[p->field_name_len++] = c;
        return ESP_OK;
    }

    if (p->skip_single_space) {
        p->skip_single_space = false;
        if (c == ' ') return ESP_OK;
    }

    if (p->field_kind == AKVM_SSE_FIELD_DATA) {
        if (p->data_len >= AKVM_SSE_DATA_MAX) return ESP_ERR_INVALID_SIZE;
        p->data[p->data_len++] = c;
        p->line_had_data = true;
    } else if (p->field_kind == AKVM_SSE_FIELD_EVENT) {
        if (p->event_name_len + 1 < sizeof(p->event_name)) {
            p->event_name[p->event_name_len++] = c;
        }
    }
    return ESP_OK;
}

esp_err_t akvm_sse_init(akvm_sse_parser_t *parser,
                        akvm_sse_event_cb_t callback,
                        void *callback_ctx)
{
    if (!parser || !callback) return ESP_ERR_INVALID_ARG;
    memset(parser, 0, sizeof(*parser));
    parser->callback = callback;
    parser->callback_ctx = callback_ctx;
    reset_line(parser);
    return ESP_OK;
}

void akvm_sse_reset(akvm_sse_parser_t *parser)
{
    if (!parser) return;
    akvm_sse_event_cb_t callback = parser->callback;
    void *callback_ctx = parser->callback_ctx;
    memset(parser, 0, sizeof(*parser));
    parser->callback = callback;
    parser->callback_ctx = callback_ctx;
    reset_line(parser);
}

esp_err_t akvm_sse_feed(akvm_sse_parser_t *parser, const void *data, size_t len)
{
    if (!parser || (!data && len != 0)) return ESP_ERR_INVALID_ARG;
    const unsigned char *bytes = (const unsigned char *)data;
    for (size_t i = 0; i < len; ++i) {
        esp_err_t err = consume_char(parser, (char)bytes[i]);
        if (err != ESP_OK) return err;
    }
    return ESP_OK;
}

esp_err_t akvm_sse_finish(akvm_sse_parser_t *parser)
{
    if (!parser) return ESP_ERR_INVALID_ARG;
    if (parser->line_nonempty) {
        esp_err_t err = end_line(parser);
        if (err != ESP_OK) return err;
    }
    return dispatch_event(parser);
}
