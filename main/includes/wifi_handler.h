#include "stdio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#ifndef WIFI_HANDLER_H
#define WIFI_HANDLER_H

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_HAS_IP_BIT BIT1
#define WIFI_RECONNECT_MAX_ATTEMPT 50

typedef struct wifi_init_param_t
{
    EventGroupHandle_t wifi_event_group;
    char ssid[32];
    char password[64];
    // int wifi_event_group,
} wifi_init_param_t;

void wifi_handler_start(wifi_init_param_t *param);

#endif