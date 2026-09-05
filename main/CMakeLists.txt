idf_component_register(
    SRCS "app_main.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash akvm_core akvm_board akvm_storage akvm_net akvm_hid akvm_video akvm_auth akvm_codex_auth akvm_ai akvm_tools
)
