#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "board_config.h"

typedef struct {
    int16_t samples[SB03_AUDIO_SAMPLES];
    uint32_t timestamp_ms;
} sb03_audio_frame_t;

esp_err_t audio_capture_init(void);
QueueHandle_t audio_capture_get_queue(void);
void audio_capture_task(void *arg);
