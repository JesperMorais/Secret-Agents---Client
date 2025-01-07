#include "mqtt_client.h"
#include <stdio.h>
typedef struct mqtt_init_param_t
{
    EventGroupHandle_t wifi_event_group;
    EventGroupHandle_t cert_event_group;
    // int wifi_event_group,
} mqtt_init_param_t;

#define TOPIC_TORGET "/torget"
#define TOPIC_MYNDIGHETEN "/myndigheten"

extern char* TOPIC_DOWNLINK; // ÄNDRAS I KODEN DÅ VI INTE VET CN DIREKT
extern char* TOPIC_UPLINK; // ÄNDRAS I KODEN DÅ VI INTE VET CN DIREKT

void mqtt_app_start(mqtt_init_param_t* gg);

void mqtt_send_to_broker(char* topic, char* data);

/// @brief Hanterar data från myndigheten
/// @param json_payload 
void handle_myndigheten_data(char* json_payload);

