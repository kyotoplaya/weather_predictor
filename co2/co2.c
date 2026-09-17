#include "co2.h"

#include <stdio.h>
#include <string.h>

#include <furi.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>

#define TAG "CO2"

#define CO2_BAUD 9600

#define CO2_RX_BUF_SIZE 64
#define CO2_FRAME_SIZE    16
#define CO2_TIMEOUT_MS    1400 /* датчик сам шлёт кадр ~раз в секунду */

/* Поле [6:7] кадра растёт с концентрацией CO2. Шкала линейная (1 ед. = 1 ppm):
   в свежем воздухе (~420 ppm) [6:7] держится ~3700 -> смещение 3700-420 = 3280.
   Калибровка предположительная; при наличии известного газа/эталона поправить. */
#define CO2_RAW_OFFSET 3280

#define CO2_HDR0 0x42
#define CO2_HDR1 0x4D

typedef struct {
    uint8_t buf[CO2_RX_BUF_SIZE];
    volatile uint16_t head; /* written by ISR */
    volatile uint16_t tail; /* written by reader   */
} Co2RxRing;

static FuriHalSerialHandle* co2_serial = NULL;

static Co2RxRing co2_rx;

static void co2_rx_push(uint8_t byte) {
    co2_rx.buf[co2_rx.head] = byte;
    co2_rx.head = (co2_rx.head + 1) % CO2_RX_BUF_SIZE;

    /* overflow: drop the oldest byte */
    if(co2_rx.head == co2_rx.tail) {
        co2_rx.tail = (co2_rx.tail + 1) % CO2_RX_BUF_SIZE;
    }
}

static void co2_rx_flush(void) {
    co2_rx.head = 0;
    co2_rx.tail = 0;
}

static bool co2_rx_pop(uint8_t* byte) {
    if(co2_rx.head == co2_rx.tail) {
        return false;
    }

    *byte = co2_rx.buf[co2_rx.tail];
    co2_rx.tail = (co2_rx.tail + 1) % CO2_RX_BUF_SIZE;

    return true;
}

/* ISR context: only push bytes, no parsing here. */
static void co2_uart_rx_cb(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    UNUSED(handle);
    UNUSED(context);

    if(event & FuriHalSerialRxEventData) {
        while(furi_hal_serial_async_rx_available(handle)) {
            co2_rx_push(furi_hal_serial_async_rx(handle));
        }
    }
}

/* Ищем в буфере валидный auto-report кадр 42 4D ... ; возвращаем сырое поле [6:7]
   (растёт с концентрацией CO2) или -1. */
static int co2_parse_frame(const uint8_t* buf, size_t len) {
    for(size_t i = 0; i + CO2_FRAME_SIZE <= len; i++) {
        if(buf[i] != CO2_HDR0 || buf[i + 1] != CO2_HDR1) {
            continue;
        }

        uint8_t sum = 0;
        for(size_t j = 0; j < CO2_FRAME_SIZE - 1; j++) {
            sum += buf[i + j];
        }

        if(buf[i + CO2_FRAME_SIZE - 1] != sum) {
            continue;
        }

        return (buf[i + 6] << 8) | buf[i + 7];
    }

    return -1;
}

static void co2_log_raw(const uint8_t* buf, size_t len) {
    if(len == 0) {
        FURI_LOG_E(TAG, "no response");
        return;
    }

    char hex[192] = "";
    for(size_t i = 0; i < len && i < 32; i++) {
        snprintf(hex + i * 3, sizeof(hex) - i * 3, "%02X ", buf[i]);
    }
    FURI_LOG_E(TAG, "invalid frame [%d]: %s", (int)len, hex);
}

bool co2_init(void) {
    if(co2_serial) {
        return true;
    }

    co2_serial = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    if(!co2_serial) {
        FURI_LOG_E(TAG, "serial USART is busy");
        return false;
    }

    furi_hal_serial_init(co2_serial, CO2_BAUD);

    furi_hal_serial_configure_framing(
        co2_serial,
        FuriHalSerialDataBits8,
        FuriHalSerialParityNone,
        FuriHalSerialStopBits1);

    furi_hal_serial_async_rx_start(co2_serial, co2_uart_rx_cb, NULL, false);

    FURI_LOG_I(TAG, "UART init ok (9600)");

    return true;
}

void co2_deinit(void) {
    if(!co2_serial) {
        return;
    }

    furi_hal_serial_async_rx_stop(co2_serial);
    furi_hal_serial_deinit(co2_serial);
    furi_hal_serial_control_release(co2_serial);

    co2_serial = NULL;
}

/* Пассивное чтение: ждём очередной auto-report кадр, команд не шлём. */
bool co2_read(int* ppm) {
    if(!co2_serial || !ppm) {
        return false;
    }

    co2_rx_flush();

    const uint32_t deadline = furi_get_tick() + furi_ms_to_ticks(CO2_TIMEOUT_MS);

    uint8_t buf[CO2_RX_BUF_SIZE];
    size_t len = 0;

    while(furi_get_tick() < deadline) {
        uint8_t byte;

        while(co2_rx_pop(&byte)) {
            if(len == sizeof(buf)) {
                memmove(buf, buf + 1, len - 1);
                len--;
            }
            buf[len++] = byte;

            int parsed = co2_parse_frame(buf, len);
            if(parsed >= 0) {
                int value = parsed - CO2_RAW_OFFSET;
                if(value < 0) {
                    value = 0;
                }
                *ppm = value;
                FURI_LOG_I(TAG, "ppm=%d", value);
                return true;
            }
        }

        furi_delay_ms(2);
    }

    co2_log_raw(buf, len);

    return false;
}