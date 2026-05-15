idf_component_register(
    INCLUDE_DIRS "include"
    REQUIRES
        esp_hw_support
        esp_timer
        esp_psram
        heap
        nvs_flash
        espressif__esp-tflite-micro
)

add_prebuilt_library(sb03_snore_prebuilt_archive "lib/libsb03_snore_lib.a"
    PRIV_REQUIRES
        esp_hw_support
        esp_timer
        esp_psram
        heap
        nvs_flash
        espressif__esp-tflite-micro
)

target_link_libraries(${COMPONENT_LIB} INTERFACE sb03_snore_prebuilt_archive)
