#include "comm_mode.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "comm_mode";
static const char *kNamespace = "sb03";
static const char *kModeKey = "comm_mode";
static sb03_comm_mode_t s_comm_mode = SB03_COMM_MODE_DREAME;

const char *comm_mode_to_string(sb03_comm_mode_t mode)
{
    switch (mode) {
    case SB03_COMM_MODE_DREAME:
        return "dreame";
    case SB03_COMM_MODE_RS485:
        return "rs485";
    case SB03_COMM_MODE_RICHMAT:
        return "richmat";
    case SB03_COMM_MODE_DREAME_GPIO:
        return "dreame_gpio";
    default:
        return "unknown";
    }
}

bool comm_mode_from_string(const char *name, sb03_comm_mode_t *mode)
{
    if (!name || !mode) {
        return false;
    }
    if (strcmp(name, "dreame") == 0) {
        *mode = SB03_COMM_MODE_DREAME;
        return true;
    }
    if (strcmp(name, "rs485") == 0) {
        *mode = SB03_COMM_MODE_RS485;
        return true;
    }
    if (strcmp(name, "richmat") == 0) {
        *mode = SB03_COMM_MODE_RICHMAT;
        return true;
    }
    if (strcmp(name, "dreame_gpio") == 0 || strcmp(name, "dreame-gpio") == 0) {
        *mode = SB03_COMM_MODE_DREAME_GPIO;
        return true;
    }
    return false;
}

static esp_err_t comm_mode_write_nvs(sb03_comm_mode_t mode)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_u8(handle, kModeKey, (uint8_t)mode);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

esp_err_t comm_mode_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(err));
        return err;
    }

    nvs_handle_t handle;
    err = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(err));
        return err;
    }

    uint8_t stored = 0;
    err = nvs_get_u8(handle, kModeKey, &stored);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        s_comm_mode = SB03_COMM_MODE_DREAME;
        esp_err_t write_err = nvs_set_u8(handle, kModeKey, (uint8_t)s_comm_mode);
        if (write_err == ESP_OK) {
            write_err = nvs_commit(handle);
        }
        nvs_close(handle);
        if (write_err != ESP_OK) {
            ESP_LOGE(TAG, "NVS default mode write failed: %s", esp_err_to_name(write_err));
            return write_err;
        }
        ESP_LOGI(TAG, "NVS comm_mode missing, defaulted to %s", comm_mode_to_string(s_comm_mode));
        return ESP_OK;
    }

    nvs_close(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS comm_mode read failed: %s", esp_err_to_name(err));
        return err;
    }

    if (stored > SB03_COMM_MODE_DREAME_GPIO) {
        ESP_LOGW(TAG, "Invalid NVS comm_mode=%u, falling back to dreame", stored);
        s_comm_mode = SB03_COMM_MODE_DREAME;
        return comm_mode_write_nvs(s_comm_mode);
    }

    s_comm_mode = (sb03_comm_mode_t)stored;
    ESP_LOGI(TAG, "NVS comm_mode=%s", comm_mode_to_string(s_comm_mode));
    return ESP_OK;
}

sb03_comm_mode_t comm_mode_get(void)
{
    return s_comm_mode;
}

esp_err_t comm_mode_set(sb03_comm_mode_t mode)
{
    esp_err_t err = comm_mode_write_nvs(mode);
    if (err == ESP_OK) {
        s_comm_mode = mode;
    }
    return err;
}

static void comm_mode_handle_line(char *line)
{
    char *argv[5] = {};
    int argc = 0;
    char *token = strtok(line, " \t\r\n");
    while (token && argc < (int)(sizeof(argv) / sizeof(argv[0]))) {
        argv[argc++] = token;
        token = strtok(NULL, " \t\r\n");
    }

    if (argc == 0) {
        return;
    }
    if (strcmp(argv[0], "comm_mode") != 0) {
        printf("unknown command: %s\n", argv[0]);
        return;
    }

    if (argc == 1 || (argc == 2 && strcmp(argv[1], "get") == 0)) {
        printf("comm_mode=%s\n", comm_mode_to_string(s_comm_mode));
        return;
    }

    if (argc >= 3 && strcmp(argv[1], "set") == 0) {
        sb03_comm_mode_t mode;
        if (!comm_mode_from_string(argv[2], &mode)) {
            printf("invalid mode: %s\n", argv[2]);
            printf("valid modes: dreame rs485 richmat dreame_gpio\n");
            return;
        }

        esp_err_t err = comm_mode_set(mode);
        if (err != ESP_OK) {
            printf("set failed: %s\n", esp_err_to_name(err));
            return;
        }

        printf("comm_mode=%s saved\n", comm_mode_to_string(mode));
        if (argc >= 4 && strcmp(argv[3], "reboot") == 0) {
            printf("rebooting\n");
            fflush(stdout);
            esp_restart();
        }
        return;
    }

    printf("usage: comm_mode [get|set <dreame|rs485|richmat|dreame_gpio> [reboot]]\n");
}

static void comm_mode_stdin_task(void *arg)
{
    (void)arg;

    ESP_LOGI(TAG, "stdin command ready: comm_mode get | comm_mode set <mode> reboot");

    char line[96] = {};
    while (true) {
        if (fgets(line, sizeof(line), stdin) != NULL) {
            comm_mode_handle_line(line);
            continue;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void comm_mode_console_start(void)
{
    xTaskCreate(comm_mode_stdin_task, "comm_mode_stdin", 4096, NULL, 2, NULL);
}
