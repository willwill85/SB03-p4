#pragma once

#include "esp_err.h"

typedef enum {
    SB03_COMM_MODE_DREAME = 0,
    SB03_COMM_MODE_RS485 = 1,
    SB03_COMM_MODE_RICHMAT = 2,
    SB03_COMM_MODE_DREAME_GPIO = 3,
} sb03_comm_mode_t;

esp_err_t comm_mode_init(void);
sb03_comm_mode_t comm_mode_get(void);
esp_err_t comm_mode_set(sb03_comm_mode_t mode);
const char *comm_mode_to_string(sb03_comm_mode_t mode);
bool comm_mode_from_string(const char *name, sb03_comm_mode_t *mode);
void comm_mode_console_start(void);
