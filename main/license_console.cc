#include "license_console.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "sb03_snore_lib.h"
#include "comm_mode.h"

static const char *TAG = "license_console";
static const char *kNamespace = "sb03_lic";
static const char *kLicenseKey = "license";

static void print_device_id(void)
{
    uint8_t mac[6] = {};
    if (esp_efuse_mac_get_default(mac) != ESP_OK) {
        printf("device_id_error\n");
        return;
    }
    printf("device_id=%02x%02x%02x%02x%02x%02x\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void handle_license_set(const char *license, bool reboot)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        err = nvs_set_str(handle, kLicenseKey, license);
        if (err == ESP_OK) {
            err = nvs_commit(handle);
        }
        nvs_close(handle);
    }

    if (err != ESP_OK) {
        printf("license_save_failed=%s\n", esp_err_to_name(err));
        return;
    }

    sb03_snore_status_t lic_status = sb03_snore_verify_license(license);
    printf("license_saved verify_status=%d\n", lic_status);
    if (reboot) {
        printf("rebooting\n");
        fflush(stdout);
        esp_restart();
    }
}

static void handle_line(char *line)
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
    if (strcmp(argv[0], "comm_mode") == 0) {
        if (argc == 1 || (argc == 2 && strcmp(argv[1], "get") == 0)) {
            printf("comm_mode=%s\n", comm_mode_to_string(comm_mode_get()));
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
        return;
    }
    if (argc >= 2 && strcmp(argv[0], "device_id") == 0 && strcmp(argv[1], "get") == 0) {
        print_device_id();
        return;
    }
    if (argc >= 3 && strcmp(argv[0], "license") == 0 && strcmp(argv[1], "set") == 0) {
        handle_license_set(argv[2], argc >= 4 && strcmp(argv[3], "reboot") == 0);
        return;
    }
}

static void license_stdin_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "stdin commands ready: comm_mode get/set | device_id get | license set <lic> reboot");
    char line[128] = {};
    while (true) {
        if (fgets(line, sizeof(line), stdin) != NULL) {
            handle_line(line);
            continue;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void license_console_start(void)
{
    xTaskCreate(license_stdin_task, "license_stdin", 4096, NULL, 2, NULL);
}

void license_console_verify_stored(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(kNamespace, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "no stored license");
        return;
    }

    char license[SB03_SNORE_LICENSE_MAX_LEN] = {};
    size_t len = sizeof(license);
    err = nvs_get_str(handle, kLicenseKey, license, &len);
    nvs_close(handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "stored license not found");
        return;
    }

    sb03_snore_status_t status = sb03_snore_verify_license(license);
    ESP_LOGI(TAG, "stored license verify_status=%d", status);
}
