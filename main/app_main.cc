#include "audio_capture.h"
#include "board_io.h"
#include "comm_mode.h"
#include "dreame_protocol.h"
#include "esp_log.h"
#include "license_console.h"
#include "rs485_protocol.h"
#include "snore_engine.h"

static const char *TAG = "app";

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Starting ESP32-P4 SB03 snore detector");

    ESP_ERROR_CHECK(comm_mode_init());
    sb03_comm_mode_t comm_mode = comm_mode_get();
    ESP_LOGI(TAG, "Communication mode: %s", comm_mode_to_string(comm_mode));

    ESP_ERROR_CHECK(board_io_init());
    ESP_ERROR_CHECK(audio_capture_init());
    license_console_verify_stored();
    license_console_start();

    QueueHandle_t audio_queue = audio_capture_get_queue();

    xTaskCreatePinnedToCore(audio_capture_task, "audio_capture", 8192, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(snore_engine_task, "snore_engine", 16384, audio_queue, 4, NULL, 0);

    switch (comm_mode) {
    case SB03_COMM_MODE_RS485:
        xTaskCreate(rs485_task, "rs485", 4096, NULL, 6, NULL);
        break;
    case SB03_COMM_MODE_DREAME:
        xTaskCreate(dreame_task, "dreame", 4096, NULL, 3, NULL);
        break;
    case SB03_COMM_MODE_RICHMAT:
        ESP_LOGW(TAG, "Richmat mode selected but protocol task is not implemented yet");
        break;
    case SB03_COMM_MODE_DREAME_GPIO:
        ESP_LOGW(TAG, "Dreame GPIO mode selected but protocol task is not implemented yet");
        break;
    default:
        ESP_LOGW(TAG, "Unknown communication mode, no communication task started");
        break;
    }
}
