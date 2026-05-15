#include "dreame_protocol.h"

#include "detection_state.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "dreame";

void dreame_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "Dreame mode active: core output is produced by snore_engine");

    while (true) {
        sb03_detection_state_t state = detection_state_get();
        if (state.snore_count > 0 || state.iferr_count > 0 || state.output_p > 0) {
            ESP_LOGI(TAG, "state output_p=%d snore_index=%d sound_level=%.4f snore_count=%d iferr_count=%d",
                     state.output_p, state.output_p * 10, state.audio_volume,
                     state.snore_count, state.iferr_count);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
