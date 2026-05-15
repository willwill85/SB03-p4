#pragma once

#include <stdbool.h>

#include "esp_err.h"

typedef enum {
    SB03_IO_HEART = 1,
    SB03_IO_AI = 2,
    SB03_IO_RS485_DE = 3,
    SB03_IO_TTL_SNORE = 4,
} sb03_io_id_t;

esp_err_t board_io_init(void);
void board_io_set(sb03_io_id_t id, bool level);
