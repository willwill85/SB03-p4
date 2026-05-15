#include "rs485_protocol.h"

#include <string.h>

#include "board_config.h"
#include "board_io.h"
#include "detection_state.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"

static const char *TAG = "rs485";

#define RS485_FRAME_MAX 32
#define RS485_FIFO_SIZE 256

typedef struct __attribute__((packed)) {
    uint8_t sof;
    uint8_t status;
    uint8_t frame_id;
    uint8_t src_id;
    uint8_t dst_id;
    uint16_t data_len;
    uint16_t head_cksum;
} rs485_head_t;

typedef struct __attribute__((packed)) {
    rs485_head_t head;
    uint8_t data[RS485_FRAME_MAX - sizeof(rs485_head_t) - 2];
    uint16_t data_cksum;
} rs485_frame_t;

static struct {
    uint8_t buf[RS485_FIFO_SIZE];
    uint16_t head;
    uint16_t tail;
} s_fifo;

static uint16_t fifo_next(uint16_t x)
{
    return (x + 1) & (RS485_FIFO_SIZE - 1);
}

static void fifo_init(void)
{
    memset(&s_fifo, 0, sizeof(s_fifo));
}

static bool fifo_put(uint8_t byte)
{
    uint16_t n = fifo_next(s_fifo.head);
    if (n == s_fifo.tail) {
        s_fifo.head = 0;
        s_fifo.tail = 0;
        return false;
    }
    s_fifo.buf[s_fifo.head] = byte;
    s_fifo.head = n;
    return true;
}

static uint16_t crc16_modbus(const uint8_t *d, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    while (len--) {
        crc ^= *d++;
        for (int i = 0; i < 8; i++) {
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
        }
    }
    return crc;
}

static uint16_t fifo_find_frame(uint8_t *outbuf)
{
    uint16_t idx = s_fifo.tail;
    while (idx != s_fifo.head) {
        if (s_fifo.buf[idx] == 0xAA) {
            uint16_t avail = (s_fifo.head - idx) & (RS485_FIFO_SIZE - 1);
            if (avail < 12) {
                return 0;
            }

            uint8_t hdr[9];
            for (int i = 0; i < 9; i++) {
                hdr[i] = s_fifo.buf[(idx + i) & (RS485_FIFO_SIZE - 1)];
            }

            if ((hdr[1] != 0x01 && hdr[1] != 0xFF) || hdr[4] != SB03_RS485_ADDR) {
                idx = fifo_next(idx);
                continue;
            }

            uint16_t hcrc = crc16_modbus(hdr, 7);
            if (hcrc != (uint16_t)(hdr[7] | (hdr[8] << 8))) {
                idx = fifo_next(idx);
                continue;
            }

            uint16_t dlen = hdr[5] | (hdr[6] << 8);
            if (dlen > RS485_FRAME_MAX - sizeof(rs485_head_t) - 2) {
                idx = fifo_next(idx);
                continue;
            }

            uint16_t total = 9 + dlen + 2;
            if (avail < total) {
                return 0;
            }

            for (int i = 0; i < total; i++) {
                outbuf[i] = s_fifo.buf[(idx + i) & (RS485_FIFO_SIZE - 1)];
            }

            uint16_t dcrc = crc16_modbus(&outbuf[9], dlen);
            if (dcrc == (uint16_t)(outbuf[9 + dlen] | (outbuf[9 + dlen + 1] << 8))) {
                s_fifo.tail = (idx + total) & (RS485_FIFO_SIZE - 1);
                return total;
            }
        }
        idx = fifo_next(idx);
    }

    return 0;
}

static int rs485_send(const uint8_t *buf, uint16_t len)
{
    board_io_set(SB03_IO_RS485_DE, true);
    ets_delay_us(50);
    int ret = uart_write_bytes(SB03_RS485_UART_NUM, buf, len);
    uart_wait_tx_done(SB03_RS485_UART_NUM, pdMS_TO_TICKS(100));
    ets_delay_us(50);
    board_io_set(SB03_IO_RS485_DE, false);
    return ret;
}

static void send_response(uint8_t req_id, const void *data, uint16_t len)
{
    rs485_frame_t rsp = {};
    rsp.head.sof = 0xAA;
    rsp.head.frame_id = req_id;
    rsp.head.src_id = SB03_RS485_ADDR;
    rsp.head.dst_id = 0x01;
    rsp.head.data_len = len;
    if (len) {
        memcpy(rsp.data, data, len);
    }

    rsp.head.head_cksum = crc16_modbus((uint8_t *)&rsp.head, 7);
    rsp.data_cksum = crc16_modbus(rsp.data, len);

    uint8_t tx[RS485_FRAME_MAX];
    memcpy(tx, &rsp.head, sizeof(rsp.head));
    if (len) {
        memcpy(tx + sizeof(rsp.head), rsp.data, len);
    }
    memcpy(tx + sizeof(rsp.head) + len, &rsp.data_cksum, 2);

    rs485_send(tx, sizeof(rsp.head) + len + 2);
}

static void slave_poll(const rs485_frame_t *f)
{
    uint8_t rsp[32] = {};
    uint16_t rsp_len = 0;

    if (f->head.data_len >= 1) {
        switch (f->data[0]) {
        case 0x00:
            if (f->head.data_len == 2) {
                rsp[0] = 0x00;
                rsp[1] = f->data[1];
                rsp_len = 2;
            }
            break;
        case 0x01:
            rsp[0] = 0x01;
            rsp[1] = 1;
            rsp_len = 2;
            break;
        case 0x02: {
            sb03_detection_state_t state = detection_state_get();
            int output_p = state.output_p;
            if (output_p < 0) {
                output_p = 0;
            }
            if (output_p > 25) {
                output_p = 25;
            }

            rsp[0] = 0x02;
            rsp[1] = 1;
            rsp[2] = 21;
            rsp[3] = (uint8_t)(output_p * 10);
            rsp_len = 4;

            if (state.snore_count > 0 || state.iferr_count > 0) {
                ESP_LOGI(TAG, "485 output=%d positive=%d negative=%d",
                         output_p * 10, state.snore_count,
                         state.iferr_count);
            }
            break;
        }
        case 0xF0:
            rsp[0] = 0xF0;
            rsp[1] = SB03_FW_VERSION_MAJOR;
            rsp[2] = SB03_FW_VERSION_MINOR;
            rsp[3] = SB03_FW_VERSION_PATCH;
            rsp_len = 4;
            break;
        default:
            break;
        }
    }

    send_response(f->head.frame_id, rsp, rsp_len);
}

static void uart_init(void)
{
    uart_config_t cfg = {};
    cfg.baud_rate = SB03_RS485_BAUDRATE;
    cfg.data_bits = UART_DATA_8_BITS;
    cfg.parity = UART_PARITY_DISABLE;
    cfg.stop_bits = UART_STOP_BITS_1;
    cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    cfg.rx_flow_ctrl_thresh = 122;
    cfg.source_clk = UART_SCLK_DEFAULT;

    ESP_ERROR_CHECK(uart_param_config(SB03_RS485_UART_NUM, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(SB03_RS485_UART_NUM,
                                 SB03_RS485_TX_GPIO,
                                 SB03_RS485_RX_GPIO,
                                 UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(SB03_RS485_UART_NUM, 1024, 0, 0, NULL, 0));
}

void rs485_task(void *arg)
{
    (void)arg;

    fifo_init();
    uart_init();
    board_io_set(SB03_IO_RS485_DE, false);
    ESP_LOGI(TAG, "RS485 ready: uart=%d tx=%d rx=%d de=%d addr=0x%02x",
             SB03_RS485_UART_NUM, SB03_RS485_TX_GPIO, SB03_RS485_RX_GPIO,
             SB03_GPIO_RS485_DE, SB03_RS485_ADDR);

    uint8_t raw[RS485_FRAME_MAX];
    uint8_t rx[64];

    while (true) {
        int n = uart_read_bytes(SB03_RS485_UART_NUM, rx, sizeof(rx), pdMS_TO_TICKS(5));
        for (int i = 0; i < n; i++) {
            fifo_put(rx[i]);
        }

        while (fifo_find_frame(raw) > 0) {
            slave_poll((const rs485_frame_t *)raw);
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
