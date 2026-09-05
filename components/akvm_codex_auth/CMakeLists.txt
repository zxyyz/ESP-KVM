idf_component_register(
    SRCS "akvm_codex_auth.c"
    INCLUDE_DIRS "include"
    REQUIRES akvm_auth esp_http_client mbedtls cjson esp_timer
)
