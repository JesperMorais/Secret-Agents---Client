#include "string.h"
#include "serial_handler.h"
#include "esp_system.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "printer_helper.h"

void init(void)
{
    uart_driver_install(UART_PORT, RX_BUF_SIZE, 0, 0, NULL, 0);
}

int sendData(const char* data)
{
    const int len = strlen(data);
    const int txBytes = uart_write_bytes(UART_PORT, data, len);
    PRINTFC_MAIN("wrote %d bytes", txBytes);
    return txBytes;
}

void tx_task(void *arg)
{
    while (1) {
        sendData("Hello world");
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

void rx_task(void *arg)
{
    uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE + 1);
    while (1) {
        const int rxBytes = uart_read_bytes(UART_PORT, data, RX_BUF_SIZE, 1000 / portTICK_PERIOD_MS);
        if (rxBytes > 0) {
            data[rxBytes] = 0;
            PRINTFC_CLIENT("Read %d bytes: '%s'", rxBytes, data);
        }
    }
    free(data);
}
