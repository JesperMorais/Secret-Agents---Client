#include "mqtt_handler.h"
#include "printer_helper.h"
#include "https_handle.h"
#include "nvs.h"
#include "global_params.h"
#include "cJSON.h"


const char* BROKER_ADDRESS = "mqtts://192.168.10.199:8883";

esp_mqtt_client_handle_t client;

char* root_ca_pem;
char* client_cert_pem;
char* client_key_pem;

char* TOPIC_DOWNLINK; // ÄNDRAS I KODEN DÅ VI INTE VET CN DIREKT
char* TOPIC_UPLINK; // ÄNDRAS I KODEN DÅ VI INTE VET CN DIREKT

//TODO: KOLLA SÅ ATT ALLT STÄMMER I PARTITIONEN / EOL
int load_certs(){ 
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open_from_partition("eol", "certs", NVS_READONLY, &my_handle);
    if(err != ESP_OK){
        PRINTFC_MQTT("Failed to open NVS");
        return -1;
    }
    else{
        //hämta rootCA
        size_t size = 0;
        nvs_get_str(my_handle, "rootCA", NULL, &size);
        if(size > 0){
            //Allocera minne till root_ca_pem
            root_ca_pem = malloc(size);
            nvs_get_str(my_handle, "rootCA", root_ca_pem, &size);
        }else{
            PRINTFC_MQTT("Failed to get root_ca_pem");
            return -1;
        }

        //hämta client_cert som blivit skickat från servern
        size = 0;
        nvs_get_str(my_handle, "ClientCert", NULL, &size);
        if(size > 0){
            //Allocera minne till client_cert_pem
            client_cert_pem = malloc(size);
            nvs_get_str(my_handle, "ClientCert", client_cert_pem, &size);
        }else{
            PRINTFC_MQTT("Failed to get client_cert_pem");
            return -1;
        }

        //hämta client_key
        size = 0;
        nvs_get_str(my_handle, "ClientKey", NULL, &size);
        if(size > 0){
            //Allocera minne till client_key_pem
            client_key_pem = malloc(size);
            nvs_get_str(my_handle, "ClientKey", client_key_pem, &size);
        }
        else{
            PRINTFC_MQTT("Failed to get client_key_pem");
            return -1;
        }

        nvs_close(my_handle);
        return 0;
    }
}


static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        PRINTFC_MQTT("MQTT Connected");
        esp_mqtt_client_subscribe(client, "/torget", 2);
        esp_mqtt_client_subscribe(client, "/myndigheten", 2);
        esp_mqtt_client_subscribe(client, TOPIC_DOWNLINK, 2);
        break;

    case MQTT_EVENT_DISCONNECTED:
        PRINTFC_MQTT("DISCONNECTED!");
        //VERKAR SOM MQTT HANTERAR DET SJÄLV
        //TYP DELAY PÅ 7 sec eller något
        break;

    case MQTT_EVENT_SUBSCRIBED:
        //PRINTA TOPIC VI SUBSCRIBAT TILL
        PRINTFC_MQTT("MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        PRINTFC_MQTT("MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:

        PRINTFC_MQTT("MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
    {
        char topic[100];
        char data[500]; // Anpassa storleken efter max förväntad data

        // Kopiera och null-terminera topic
        snprintf(topic, sizeof(topic), "%.*s", event->topic_len, event->topic);

        // Kopiera och null-terminera data
        snprintf(data, sizeof(data), "%.*s", event->data_len, event->data);

        if (strcmp(topic, TOPIC_TORGET) == 0)
        {
            PRINTFC_MQTT("Received data from topic '%s': '%s'", topic, data);
        }
        else if (strcmp(topic, TOPIC_MYNDIGHETEN) == 0)
        {
            PRINTFC_MQTT("Received data from topic '%s': '%s'", topic, data);
            handle_myndigheten_data(data); // FIXA
        }
        else if (strcmp(topic, TOPIC_DOWNLINK) == 0)
        {
            PRINTFC_MQTT("Received data from topic '%s': '%s'", topic, data);
        }
        else
        {
            PRINTFC_MQTT("Unknown topic '%s': '%s'", topic, data);
        }
        break;
    }
    case MQTT_EVENT_ERROR:
        PRINTFC_MQTT("MQTT_EVENT_ERROR %d", event->error_handle->error_type);
        break;
    default:
        PRINTFC_MQTT("Other event id:%d", event->event_id);
        break;
    }
}

void mqtt_app_start(mqtt_init_param_t* params){
    //HÄR VÄNTAR PÅ PÅ CERTIFIKAT FÖR ATT KUNNA KÖRA MQTT
    //SAMT LADDAR IN CERTIFIKATEN
    PRINTFC_MQTT("Väntar på certifikat");
    xEventGroupWaitBits(params->cert_event_group, CERT_READY_BIT, pdTRUE, pdTRUE, portMAX_DELAY);

    TOPIC_DOWNLINK = malloc(25); // Anpassa storlek
    snprintf(TOPIC_DOWNLINK, 25, "/spelare/%d/downlink", playerID);

    TOPIC_UPLINK = malloc(25); // Anpassa storlek
    snprintf(TOPIC_UPLINK, 25, "/spelare/%d/uplink", playerID); 

    if(load_certs() < 0){
        PRINTFC_MQTT("Failed to load certs");
        PRINTFC_MQTT("MQTT will not start");
        //vTaskDelete(NULL);
        return;
    }
    //TODO GLÖM INTE FREEA ALLT MINNE SOM ALLOKERAS

    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = BROKER_ADDRESS,
            .verification = {
                .certificate = (const char*)root_ca_pem,
                .skip_cert_common_name_check = true,
            },
        },

        .credentials = {
            .authentication = {
                .certificate = (const char*)client_cert_pem,
                .key = (const char*)client_key_pem,
            }
        },
    };


    client = esp_mqtt_client_init(&mqtt_cfg);

    
    PRINTFC_MQTT("Startar igång mqtt event!");
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    PRINTFC_MQTT("Startar igång mqtt!");
    esp_mqtt_client_start(client);

}

void mqtt_send_to_broker(char* topic, char* data){
    if (esp_mqtt_client_publish(client, topic, data, strlen(data), 2, 0) < 0) {
    PRINTFC_MQTT("Failed to send data to broker");
    } else {
        PRINTFC_MQTT("Data sent to broker");
    }   
}

void handle_myndigheten_data(char* json_payload) {
    PRINTFC_MQTT("Handling data from /myndigheten: %s", json_payload);

    // Parsar JSON-data
    cJSON* root = cJSON_Parse(json_payload);
    if (root == NULL) {
        PRINTFC_MQTT("Failed to parse JSON payload");
        return;
    }

    // Läs fältet "typ"
    cJSON* typ = cJSON_GetObjectItem(root, "typ");
    if (!cJSON_IsString(typ)) {
        PRINTFC_MQTT("Invalid or missing 'typ' in JSON");
        cJSON_Delete(root);
        return;
    }

    // Läs fältet "data"
    cJSON* data = cJSON_GetObjectItem(root, "data");

    // Baserat på typ, skriv ut instruktioner till spelaren
    if (strcmp(typ->valuestring, "val av ledare") == 0) {
        PRINTFC_MQTT("Myndighet: Val av ledare! Data: %s", cJSON_Print(data));
        PRINTFC_CLIENT("Skriv /uplink:{\"val\":\"ok\"} för att godkänna, eller /uplink:{\"val\":\"neka\"} för att neka ledaren.");
    } else if (strcmp(typ->valuestring, "ny runda") == 0) {
        PRINTFC_MQTT("Myndighet: Ny runda startad! Data: %s", cJSON_Print(data));
        PRINTFC_CLIENT("Skriv /uplink:{\"val\":\"lyckas\"} för att bidra till uppdraget, eller /uplink:{\"val\":\"sabotera\"} för att sabotera.");
    } else if (strcmp(typ->valuestring, "val att sparka") == 0) {
        PRINTFC_MQTT("Myndighet: Val att sparka ledaren! Data: %s", cJSON_Print(data));
        PRINTFC_CLIENT("Skriv /uplink:{\"val\":\"ok\"} för att sparka, eller /uplink:{\"val\":\"neka\"} för att behålla ledaren.");
    } else if (strcmp(typ->valuestring, "uppdrag saboterat") == 0) {
        PRINTFC_MQTT("Myndighet: Uppdrag saboterat! Data: %s", cJSON_Print(data));
    } else if (strcmp(typ->valuestring, "uppdrag lyckades") == 0) {
        PRINTFC_MQTT("Myndighet: Uppdrag lyckades! Data: %s", cJSON_Print(data));
    } else if (strcmp(typ->valuestring, "spelare") == 0) {
        PRINTFC_MQTT("Myndighet: Spelare i spelet! Data: %s", cJSON_Print(data));
    } else if (strcmp(typ->valuestring, "tillit") == 0) {
        PRINTFC_MQTT("Myndighet: Uppdaterad tillit! Data: %s", cJSON_Print(data));
    } else if (strcmp(typ->valuestring, "lyckade uppdrag") == 0) {
        PRINTFC_MQTT("Myndighet: Antal lyckade uppdrag! Data: %s", cJSON_Print(data));
    } else if (strcmp(typ->valuestring, "sparka spelare") == 0) {
        PRINTFC_MQTT("Myndighet: Spelare har sparkats! Data: %s", cJSON_Print(data));
    } else {
        PRINTFC_MQTT("Myndighet: Okänt meddelande 'typ': %s, Data: %s", typ->valuestring, cJSON_Print(data));
    }

    // Rensa upp
    cJSON_Delete(root);
}