#pragma once

#include <stdint.h>

typedef struct {
    int snoresig;
    int snore_score;
    int snore_detector_score;
    int snore_count;
    int iferr_count;
    int output_p;
    float snore_prob;
    float snore_prob_raw;
    float audio_std;
    float audio_volume;
    uint32_t last_update_ms;
} sb03_detection_state_t;

void detection_state_set(const sb03_detection_state_t *state);
sb03_detection_state_t detection_state_get(void);
