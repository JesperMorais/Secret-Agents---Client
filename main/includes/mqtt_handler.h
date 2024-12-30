#include "mqtt_client.h"
#include <stdio.h>
typedef struct mqtt_init_param_t
{
    EventGroupHandle_t wifi_event_group;
    EventGroupHandle_t cert_event_group;
    // int wifi_event_group,
} mqtt_init_param_t;

void mqtt_app_start(mqtt_init_param_t* gg);

