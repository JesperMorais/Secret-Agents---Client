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
#include "https_handle.h"

//skapa event group för hallå koll på om certifikatet är klart

EventGroupHandle_t cert_event_group; //håller koll på om certifikatet är signat och klart för att kunna connecta med Brokern
EventGroupHandle_t wifi_event_group;    //Håller koll på om vi är connectade till wifi samt om vi har en IP adress


wifi_init_param_t w_param = {
    .ssid = CONFIG_WIFI_SSID,
    .password = CONFIG_WIFI_PASSWORD,
};

// Om du vill använda MQTT senare, definiera param
mqtt_init_param_t mqtt_param;
https_init_param_t https_param;

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

    err = nvs_flash_init_partition("eol");
    ESP_ERROR_CHECK(err);

    // Initiera event loop och netif HÄR istället för i wifi_handler
    esp_netif_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    PRINTFC_MAIN("Creating event groups");
    wifi_event_group = xEventGroupCreate();
    cert_event_group = xEventGroupCreate();

    w_param.wifi_event_group = wifi_event_group;
    uart_driver_install(UART_NUM_0, 2048, 0, 0, NULL, 0);


    wifi_handler_start(&w_param); // Starta WiFi denna gör vi inte i loop eller task för den styrs av eventhandler och skall dras igång en gång

    // Starta MQTT senare när WiFi fungerar
    mqtt_param.wifi_event_group = wifi_event_group;
    mqtt_param.cert_event_group = cert_event_group;
    
    https_param.wifi_event_group = wifi_event_group;
    https_param.cert_event_group = cert_event_group;

    xTaskCreate(cert_manage_task, "send_csr_task", 16384, &https_param, configMAX_PRIORITIES - 1, NULL);

    xTaskCreate(rx_task, "uart_rx_task", 1024 * 2, &https_param, configMAX_PRIORITIES - 1, NULL);

    //hoppar in i mqtt_handler.c och kollar om vi har wifi och cert klart
    //TODO: FIXA SÅ VI INTE KÖR FÖRENS VI HAR CERT
    mqtt_app_start(&mqtt_param);

    //Hanterar CERT, dvs skapar key, csr och skickar csr till servern för att få det signat
    //När den är klar, skickar vi notis till mqtt_handler att vi är klara

}
