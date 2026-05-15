#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SB03_SNORE_AUDIO_SAMPLE_RATE_HZ 16000
#define SB03_SNORE_AUDIO_SAMPLES 16000
#define SB03_SNORE_TOP_COUNT 5
#define SB03_SNORE_LICENSE_MAX_LEN 96
#define SB03_SNORE_TRIAL_LIMIT 2000

typedef enum {
    SB03_SNORE_OK = 0,
    SB03_SNORE_ERR_ARG = -1,
    SB03_SNORE_ERR_INIT = -2,
    SB03_SNORE_ERR_MODEL = -3,
    SB03_SNORE_ERR_INVOKE = -4,
    SB03_SNORE_ERR_NO_LICENSE = -5,
    SB03_SNORE_ERR_TRIAL_EXPIRED = -6,
} sb03_snore_status_t;

typedef struct {
    int index;
    float score;
} sb03_snore_class_score_t;

typedef struct {
    int snoresig;
    int snore_score;
    int snore_detector_score;
    int snore_count;
    int iferr_count;
    int output_p;
    int dreame_snore_index;
    float snore_prob;
    float snore_prob_raw;
    float audio_std;
    float audio_volume;
    float audio_peak;
    float audio_rms;
    int infer_ms;
    uint32_t last_update_ms;
    bool licensed;
    uint32_t trial_count;
    uint32_t trial_limit;
    sb03_snore_class_score_t top[SB03_SNORE_TOP_COUNT];
} sb03_snore_result_t;

/*
 * Verify and unlock the library for unlimited inference.
 *
 * The license is bound to the chip eFuse MAC ID. This function should be called
 * once at boot after NVS is initialized. On success, later inference calls do
 * not consume the trial counter.
 */
sb03_snore_status_t sb03_snore_verify_license(const char *license);

/*
 * Run one 16 kHz, 1 second mono PCM frame through the embedded YAMNet snore
 * detector. Without a valid license, the library returns real results for the
 * first SB03_SNORE_TRIAL_LIMIT successful inferences, then returns
 * SB03_SNORE_ERR_TRIAL_EXPIRED and zeroes out the result.
 */
sb03_snore_status_t sb03_snore_infer(const int16_t *samples,
                                     int sample_count,
                                     uint32_t timestamp_ms,
                                     sb03_snore_result_t *result);

#ifdef __cplusplus
}
#endif
