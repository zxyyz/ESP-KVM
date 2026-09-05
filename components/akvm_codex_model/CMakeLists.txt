idf_component_register(
    SRCS "akvm_codex_model.c"
    INCLUDE_DIRS "include"
    REQUIRES akvm_ai akvm_auth akvm_sse esp_http_client mbedtls
)
