#include "mqtt_handler.h"
#include "printer_helper.h"

const char* BROKER_ADDRESS = "mqtt://172.16.217.243:1883";

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:

        PRINTFC_MQTT("MQTT Connected");
        msg_id = esp_mqtt_client_subscribe(client, "/torget", 2);
        break;

    case MQTT_EVENT_DISCONNECTED:
        PRINTFC_MQTT("DISCONNECTED!");

        //TODO: RECOONECT??
        break;

    case MQTT_EVENT_SUBSCRIBED:
        
        const char* temp_data = "Jesper, gick med i torget!";
        PRINTFC_MQTT("MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        //TODO: FIXA ORDENTLIGT HÄR
        
        msg_id = esp_mqtt_client_publish(client, "/torget", temp_data, 0, 0, 0);
        PRINTFC_MQTT("sent publish successful, msg_id=%d", msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        PRINTFC_MQTT("MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:

        PRINTFC_MQTT("MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        PRINTFC_MQTT("MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);

        if (strncmp(event->data, "send binary please", event->data_len) == 0) {
            PRINTFC_MQTT("Sending binary?");
            
        }
        break;
    case MQTT_EVENT_ERROR:
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            PRINTFC_MQTT("Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
            PRINTFC_MQTT("Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
            PRINTFC_MQTT("Last captured errno : %d (%s)",  event->error_handle->esp_transport_sock_errno,
                     strerror(event->error_handle->esp_transport_sock_errno));

        } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
            PRINTFC_MQTT("Connection refused error: 0x%x", event->error_handle->connect_return_code);
        } else {
            PRINTFC_MQTT("Unknown error type: 0x%x", event->error_handle->error_type);
        }
        break;
    default:
        PRINTFC_MQTT("Other event id:%d", event->event_id);
        break;
    }
}

void mqtt_app_start(mqtt_init_param_t* params){

    //TODO: FIXA SÅ VI INTE KÖR FÖRENS VI HAR WIFI    
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = BROKER_ADDRESS,
            //.verification.certificate =,
        },
    };


    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);


    PRINTFC_MQTT("Väntar på wifi");
    xEventGroupWaitBits(params->wifi_event_group, BIT0 | BIT1, pdFALSE, pdTRUE, portMAX_DELAY);
    
    PRINTFC_MQTT("Startar igång mqtt event!");
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    PRINTFC_MQTT("Startar igång mqtt!");
    esp_mqtt_client_start(client);
}