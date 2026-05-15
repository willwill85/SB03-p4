#include "board_io.h"

#include "board_config.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "board_io";

static void config_output(gpio_num_t gpio, bool level)
{
    gpio_config_t cfg = {};
    cfg.pin_bit_mask = 1ULL << gpio;
    cfg.mode = GPIO_MODE_OUTPUT;
    cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cfg.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&cfg));
    gpio_set_level(gpio, level ? 1 : 0);
}

esp_err_t board_io_init(void)
{
    config_output(SB03_GPIO_POWER_AMP, true);
    config_output(SB03_GPIO_LED_HEART, false);
    config_output(SB03_GPIO_LED_AI, false);
    config_output(SB03_GPIO_RS485_DE, false);
    config_output(SB03_GPIO_TTL_SNORE, false);
    ESP_LOGI(TAG, "GPIO ready: heart=%d ai=%d rs485_de=%d ttl=%d",
             SB03_GPIO_LED_HEART, SB03_GPIO_LED_AI,
             SB03_GPIO_RS485_DE, SB03_GPIO_TTL_SNORE);
    return ESP_OK;
}

void board_io_set(sb03_io_id_t id, bool level)
{
    gpio_num_t gpio = GPIO_NUM_NC;

    switch (id) {
    case SB03_IO_HEART:
        gpio = SB03_GPIO_LED_HEART;
        break;
    case SB03_IO_AI:
        gpio = SB03_GPIO_LED_AI;
        break;
    case SB03_IO_RS485_DE:
        gpio = SB03_GPIO_RS485_DE;
        break;
    case SB03_IO_TTL_SNORE:
        gpio = SB03_GPIO_TTL_SNORE;
        break;
    default:
        return;
    }

    gpio_set_level(gpio, level ? 1 : 0);
}
