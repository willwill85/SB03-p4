#include "audio_capture.h"

#include <algorithm>
#include <cmath>
#include <string.h>

#include "board_config.h"
#include "board_io.h"
#include "driver/i2c.h"
#include "driver/i2s_std.h"
#include "es8311.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"

static const char *TAG = "audio_capture";

static i2s_chan_handle_t s_tx_handle = NULL;
static i2s_chan_handle_t s_rx_handle = NULL;
static QueueHandle_t s_audio_queue = NULL;

static esp_err_t i2c_init(void)
{
    i2c_config_t cfg = {};
    cfg.mode = I2C_MODE_MASTER;
    cfg.sda_io_num = SB03_I2C_SDA_GPIO;
    cfg.scl_io_num = SB03_I2C_SCL_GPIO;
    cfg.sda_pullup_en = GPIO_PULLUP_ENABLE;
    cfg.scl_pullup_en = GPIO_PULLUP_ENABLE;
    cfg.master.clk_speed = 100000;
    cfg.clk_flags = 0;

    ESP_RETURN_ON_ERROR(i2c_param_config(SB03_I2C_NUM, &cfg), TAG, "config i2c");
    return i2c_driver_install(SB03_I2C_NUM, I2C_MODE_MASTER, 0, 0, 0);
}

static esp_err_t codec_init(void)
{
    ESP_RETURN_ON_ERROR(i2c_init(), TAG, "i2c init");

    es8311_handle_t es = es8311_create(SB03_I2C_NUM, ES8311_ADDRRES_0);
    ESP_RETURN_ON_FALSE(es, ESP_FAIL, TAG, "es8311 create failed");

    es8311_clock_config_t clk_cfg = {};
    clk_cfg.mclk_inverted = false;
    clk_cfg.sclk_inverted = false;
    clk_cfg.mclk_from_mclk_pin = true;
    clk_cfg.mclk_frequency = SB03_SAMPLE_RATE_HZ * SB03_I2S_MCLK_MULTIPLE;
    clk_cfg.sample_frequency = SB03_SAMPLE_RATE_HZ;

    ESP_RETURN_ON_ERROR(es8311_init(es, &clk_cfg, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16),
                        TAG, "es8311 init");
    ESP_RETURN_ON_ERROR(es8311_sample_frequency_config(es,
                                                       SB03_SAMPLE_RATE_HZ * SB03_I2S_MCLK_MULTIPLE,
                                                       SB03_SAMPLE_RATE_HZ),
                        TAG, "es8311 sample rate");
    ESP_RETURN_ON_ERROR(es8311_voice_volume_set(es, CONFIG_SB03_CODEC_VOLUME, NULL),
                        TAG, "es8311 volume");
    ESP_RETURN_ON_ERROR(es8311_microphone_config(es, false), TAG, "es8311 mic config");
    ESP_RETURN_ON_ERROR(es8311_microphone_gain_set(es, (es8311_mic_gain_t)CONFIG_SB03_MIC_GAIN),
                        TAG, "es8311 mic gain");

    return ESP_OK;
}

static esp_err_t i2s_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(SB03_I2S_NUM, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, &s_tx_handle, &s_rx_handle), TAG, "new i2s channel");

    i2s_std_config_t std_cfg = {};
    std_cfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SB03_SAMPLE_RATE_HZ);
    std_cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                           I2S_SLOT_MODE_STEREO);
    std_cfg.gpio_cfg.mclk = SB03_I2S_MCLK_GPIO;
    std_cfg.gpio_cfg.bclk = SB03_I2S_BCLK_GPIO;
    std_cfg.gpio_cfg.ws = SB03_I2S_WS_GPIO;
    std_cfg.gpio_cfg.dout = SB03_I2S_DOUT_GPIO;
    std_cfg.gpio_cfg.din = SB03_I2S_DIN_GPIO;
    std_cfg.gpio_cfg.invert_flags.mclk_inv = false;
    std_cfg.gpio_cfg.invert_flags.bclk_inv = false;
    std_cfg.gpio_cfg.invert_flags.ws_inv = false;
    std_cfg.clk_cfg.mclk_multiple = (i2s_mclk_multiple_t)SB03_I2S_MCLK_MULTIPLE;

    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(s_tx_handle, &std_cfg), TAG, "init tx");
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(s_rx_handle, &std_cfg), TAG, "init rx");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(s_tx_handle), TAG, "enable tx");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(s_rx_handle), TAG, "enable rx");
    return ESP_OK;
}

static void calc_stereo_level(const int16_t *stereo, float *left_peak, float *left_rms,
                              float *right_peak, float *right_rms)
{
    int left_max = 0;
    int right_max = 0;
    double left_sum_sq = 0.0;
    double right_sum_sq = 0.0;

    for (int i = 0; i < SB03_AUDIO_SAMPLES; i++) {
        int left = stereo[i * 2];
        int right = stereo[i * 2 + 1];
        int left_abs = left < 0 ? -left : left;
        int right_abs = right < 0 ? -right : right;
        left_max = std::max(left_max, left_abs);
        right_max = std::max(right_max, right_abs);

        float lf = left / 32768.0f;
        float rf = right / 32768.0f;
        left_sum_sq += (double)lf * lf;
        right_sum_sq += (double)rf * rf;
    }

    *left_peak = left_max / 32768.0f;
    *right_peak = right_max / 32768.0f;
    *left_rms = sqrtf((float)(left_sum_sq / SB03_AUDIO_SAMPLES));
    *right_rms = sqrtf((float)(right_sum_sq / SB03_AUDIO_SAMPLES));
}

esp_err_t audio_capture_init(void)
{
    s_audio_queue = xQueueCreate(SB03_AUDIO_QUEUE_DEPTH, sizeof(sb03_audio_frame_t *));
    ESP_RETURN_ON_FALSE(s_audio_queue, ESP_ERR_NO_MEM, TAG, "create audio queue");

    ESP_RETURN_ON_ERROR(i2s_init(), TAG, "i2s init");
    ESP_RETURN_ON_ERROR(codec_init(), TAG, "codec init");
    ESP_LOGI(TAG, "audio ready: %d Hz mono frames from I2S stereo", SB03_SAMPLE_RATE_HZ);
    return ESP_OK;
}

QueueHandle_t audio_capture_get_queue(void)
{
    return s_audio_queue;
}

void audio_capture_task(void *arg)
{
    (void)arg;

    const size_t stereo_samples = SB03_AUDIO_SAMPLES * 2;
    int16_t *stereo = (int16_t *)heap_caps_malloc(stereo_samples * sizeof(int16_t),
                                                  MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_ERROR_CHECK(stereo ? ESP_OK : ESP_ERR_NO_MEM);

    bool led = false;
    while (true) {
        sb03_audio_frame_t *frame = (sb03_audio_frame_t *)heap_caps_malloc(
            sizeof(sb03_audio_frame_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!frame) {
            ESP_LOGE(TAG, "no memory for audio frame");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        board_io_set(SB03_IO_HEART, led);
        led = !led;

        size_t filled = 0;
        const int64_t start_us = esp_timer_get_time();
        while (filled < stereo_samples * sizeof(int16_t)) {
            size_t bytes_read = 0;
            esp_err_t ret = i2s_channel_read(s_rx_handle,
                                             ((uint8_t *)stereo) + filled,
                                             stereo_samples * sizeof(int16_t) - filled,
                                             &bytes_read,
                                             pdMS_TO_TICKS(1000));
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "i2s read failed: %s", esp_err_to_name(ret));
                break;
            }
            filled += bytes_read;
        }

        if (filled < stereo_samples * sizeof(int16_t)) {
            heap_caps_free(frame);
            continue;
        }

        float left_peak = 0.0f;
        float left_rms = 0.0f;
        float right_peak = 0.0f;
        float right_rms = 0.0f;
        calc_stereo_level(stereo, &left_peak, &left_rms, &right_peak, &right_rms);

        const int channel_offset = right_rms > left_rms ? 1 : 0;
        for (int i = 0; i < SB03_AUDIO_SAMPLES; i++) {
            frame->samples[i] = stereo[i * 2 + channel_offset];
        }
        frame->timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);

        if (xQueueSend(s_audio_queue, &frame, 0) != pdTRUE) {
            sb03_audio_frame_t *old = NULL;
            if (xQueueReceive(s_audio_queue, &old, 0) == pdTRUE && old) {
                heap_caps_free(old);
            }
            (void)xQueueSend(s_audio_queue, &frame, 0);
        }

        ESP_LOGI(TAG, "record took %lld ms ch=%c L(pk=%.4f rms=%.4f) R(pk=%.4f rms=%.4f)",
                 (esp_timer_get_time() - start_us) / 1000,
                 channel_offset == 0 ? 'L' : 'R',
                 left_peak, left_rms, right_peak, right_rms);
    }
}
