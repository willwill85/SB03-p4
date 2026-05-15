#include "snore_engine.h"

#include <stdio.h>
#include <string.h>

#include "audio_capture.h"
#include "board_io.h"
#include "detection_state.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "sb03_snore_lib.h"

static const char *TAG = "snore_engine";

static sb03_detection_state_t s_detection_state = {};
static portMUX_TYPE s_detection_state_lock = portMUX_INITIALIZER_UNLOCKED;

void detection_state_set(const sb03_detection_state_t *state)
{
    portENTER_CRITICAL(&s_detection_state_lock);
    s_detection_state = *state;
    portEXIT_CRITICAL(&s_detection_state_lock);
}

sb03_detection_state_t detection_state_get(void)
{
    sb03_detection_state_t state;
    portENTER_CRITICAL(&s_detection_state_lock);
    state = s_detection_state;
    portEXIT_CRITICAL(&s_detection_state_lock);
    return state;
}

static void detection_state_from_result(const sb03_snore_result_t *result,
                                        sb03_detection_state_t *state)
{
    memset(state, 0, sizeof(*state));
    state->snoresig = result->snoresig;
    state->snore_score = result->snore_score;
    state->snore_detector_score = result->snore_detector_score;
    state->snore_count = result->snore_count;
    state->iferr_count = result->iferr_count;
    state->output_p = result->output_p;
    state->snore_prob = result->snore_prob;
    state->snore_prob_raw = result->snore_prob_raw;
    state->audio_std = result->audio_std;
    state->audio_volume = result->audio_volume;
    state->last_update_ms = result->last_update_ms;
}

static void format_top_scores(const sb03_snore_result_t *result, char *buf, size_t buf_size)
{
    int off = 0;
    for (int i = 0; i < SB03_SNORE_TOP_COUNT && off < (int)buf_size; i++) {
        off += snprintf(buf + off, buf_size - off, "%d=%.3f ",
                        result->top[i].index, result->top[i].score);
    }
}

void snore_engine_task(void *arg)
{
    QueueHandle_t audio_queue = (QueueHandle_t)arg;

    while (true) {
        sb03_audio_frame_t *frame = NULL;
        if (xQueueReceive(audio_queue, &frame, portMAX_DELAY) != pdTRUE || !frame) {
            continue;
        }

        sb03_snore_result_t result = {};
        sb03_snore_status_t status = sb03_snore_infer(frame->samples,
                                                      SB03_AUDIO_SAMPLES,
                                                      frame->timestamp_ms,
                                                      &result);
        if (status != SB03_SNORE_OK) {
            sb03_detection_state_t zero_state = {};
            detection_state_set(&zero_state);
            board_io_set(SB03_IO_AI, true);
            board_io_set(SB03_IO_TTL_SNORE, false);
            ESP_LOGW(TAG, "snore library infer failed: status=%d", status);
            heap_caps_free(frame);
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        sb03_detection_state_t state = {};
        detection_state_from_result(&result, &state);
        detection_state_set(&state);

        board_io_set(SB03_IO_AI, !result.snoresig);
        board_io_set(SB03_IO_TTL_SNORE, result.snoresig);

        ESP_LOGI(TAG, "**************** DREAME CORE OUTPUT ****************");
        ESP_LOGI(TAG, "output_p=%d snore_index=%d sound_level=%.4f snore_count=%d iferr_count=%d",
                 result.output_p, result.dreame_snore_index, result.audio_volume,
                 result.snore_count, result.iferr_count);
        ESP_LOGI(TAG, "****************************************************");

        char top_summary[128] = {};
        format_top_scores(&result, top_summary, sizeof(top_summary));
        ESP_LOGI(TAG,
                 "sn=%.4f raw=%.4f sig=%d cnt=%d iferr=%d out=%d std=%.4f vol=%.4f infer=%dms licensed=%d trial=%lu/%lu",
                 result.snore_prob, result.snore_prob_raw, result.snoresig,
                 result.snore_count, result.iferr_count, result.output_p,
                 result.audio_std, result.audio_volume, result.infer_ms,
                 result.licensed ? 1 : 0, (unsigned long)result.trial_count,
                 (unsigned long)result.trial_limit);
        ESP_LOGI(TAG, "audio peak=%.4f rms=%.4f top=[%s]",
                 result.audio_peak, result.audio_rms, top_summary);

        heap_caps_free(frame);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
