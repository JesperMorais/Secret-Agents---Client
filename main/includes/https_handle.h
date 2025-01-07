#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_http_client.h"

#define CERT_READY_BIT BIT7


typedef struct https_init_param_t
{
    EventGroupHandle_t wifi_event_group;
    EventGroupHandle_t cert_event_group;
    // int wifi_event_group,
} https_init_param_t;

//Struct för att spara responsen från servern i HTTP_EVENT_ON_DATA
typedef struct {
    char* body_buf;
    int total_size;
}https_resp_t;

//Sruct för att skicka params in i eventhandler
typedef struct {
    https_resp_t resp;
    https_init_param_t* init_param;
}event_ctx_t;

void cert_manage_task(void *pvParameters);

/// @brief HTTP event handler
static esp_err_t _http_event_handler(esp_http_client_event_t *event);

/// @brief Hjälp funktion för att convertera inkommande JSON till CN
/// @param go_str GO string
/// @return char* CN string
char* convert_to_cn(char* go_str);

char* createCSR(char* CommonName);

esp_err_t send_csr_request(const char* csr);

void handle_json_response(char* json);

esp_err_t send_ready_check();

