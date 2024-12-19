#include <stdio.h>
#include "wifi_handler.h"
#include "printer_helper.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "driver/uart.h"
#include "string.h"
#include "serial_handler.h"
#include "esp_wifi.h"
#include "mqtt_handler.h" // Avkommentera när du vill använda MQTT

EventGroupHandle_t wifi_event_group;
wifi_init_param_t w_param = {
    .ssid = CONFIG_WIFI_SSID,
    .password = CONFIG_WIFI_PASSWORD,
};

// Om du vill använda MQTT senare, definiera param
mqtt_init_param_t mqtt_param;

void app_main(void)
{
    PRINTFC_MAIN("Main is starting");

    PRINTFC_MAIN("NVS Initialize");
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Initiera event loop och netif HÄR istället för i wifi_handler
    esp_netif_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    PRINTFC_MAIN("Creating event group");
    wifi_event_group = xEventGroupCreate();
    w_param.wifi_event_group = wifi_event_group;
    uart_driver_install(UART_NUM_0, 2048, 0, 0, NULL, 0);


    wifi_handler_start(&w_param);

    // Starta MQTT senare när WiFi fungerar
    mqtt_param.wifi_event_group = wifi_event_group;
    mqtt_app_start(&mqtt_param);

    xTaskCreate(rx_task, "uart_rx_task", 1024 * 2, NULL, configMAX_PRIORITIES - 1, NULL);
    xTaskCreate(tx_task, "uart_tx_task", 1024 * 2, NULL, configMAX_PRIORITIES - 2, NULL);
}
