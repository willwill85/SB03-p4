#pragma once

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/i2s_std.h"
#include "driver/uart.h"

#define SB03_SAMPLE_RATE_HZ          (16000)
#define SB03_AUDIO_SECONDS           (1)
#define SB03_AUDIO_SAMPLES           (SB03_SAMPLE_RATE_HZ * SB03_AUDIO_SECONDS)
#define SB03_AUDIO_BYTES             (SB03_AUDIO_SAMPLES * sizeof(int16_t))
#define SB03_AUDIO_QUEUE_DEPTH       (2)

#define SB03_I2C_NUM                 (I2C_NUM_0)
#define SB03_I2C_SCL_GPIO            (GPIO_NUM_8)
#define SB03_I2C_SDA_GPIO            (GPIO_NUM_7)

#define SB03_I2S_NUM                 (I2S_NUM_0)
#define SB03_I2S_MCLK_GPIO           (GPIO_NUM_13)
#define SB03_I2S_BCLK_GPIO           (GPIO_NUM_12)
#define SB03_I2S_WS_GPIO             (GPIO_NUM_10)
#define SB03_I2S_DOUT_GPIO           (GPIO_NUM_9)
#define SB03_I2S_DIN_GPIO            (GPIO_NUM_11)
#define SB03_I2S_MCLK_MULTIPLE       (384)

/*
 * Default GPIOs come from the Waveshare ESP32-P4 I2SCodec example and the
 * Linux application's logical outputs. Adjust these three lines to the SB03
 * schematic if the board uses different pins.
 */
#define SB03_GPIO_POWER_AMP          (GPIO_NUM_53)
#define SB03_GPIO_LED_HEART          (GPIO_NUM_48)
#define SB03_GPIO_LED_AI             (GPIO_NUM_49)
#define SB03_GPIO_RS485_DE           (GPIO_NUM_50)
#define SB03_GPIO_TTL_SNORE          (GPIO_NUM_51)

#define SB03_RS485_UART_NUM          (UART_NUM_1)
#define SB03_RS485_TX_GPIO           (GPIO_NUM_43)
#define SB03_RS485_RX_GPIO           (GPIO_NUM_44)
#define SB03_RS485_BAUDRATE          (115200)
#define SB03_RS485_ADDR              (0x20u)

#define SB03_FW_VERSION_MAJOR        (1)
#define SB03_FW_VERSION_MINOR        (3)
#define SB03_FW_VERSION_PATCH        (0)
