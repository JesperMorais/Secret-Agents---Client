#include "string.h"
#include "serial_handler.h"
#include "esp_system.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "printer_helper.h"
#include "mqtt_handler.h"
#include "global_params.h"

void init(void)
{
    uart_driver_install(UART_PORT, RX_BUF_SIZE, 0, 0, NULL, 0);
}


void rx_task(void *arg)
{
    uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE + 1);
    while (1) {
        const int rxBytes = uart_read_bytes(UART_PORT, data, RX_BUF_SIZE, 1000 / portTICK_PERIOD_MS);
        if (rxBytes > 0) {
            data[rxBytes] = 0;
            PRINTFC_CLIENT("Read %d bytes: '%s'", rxBytes, data);

            //TODO: SKICKA TILL NÅGOT TYP AV PARSER
            handle_data((char*)data);
        }
    }
    free(data);
}

//Topics:
// /torget
// uplink
// downlink

void handle_data(char* input) {
    PRINTFC_CLIENT("Data: %s", input);
    // EXEMPEL INPUT /TOPIC:DATA

    if (input == NULL || strlen(input) == 0) {
        PRINTFC_CLIENT("Invalid input: NULL or empty");
        return;
    }

    char topic[50];
    char* sep = strchr(input, ':'); //Pekar till kolon, exempel input = /torget:hej då blir sep är ":hej"
    if (sep == NULL) {
        PRINTFC_CLIENT("Invalid input: %s (no colon found)", input);
        return;
    }


    int topic_len = sep - input; //längden av topic
    if (topic_len > sizeof(topic) - 1) {
        PRINTFC_CLIENT("Topic is too long (%d bytes)", topic_len);
        return;
    }

    // Kopiera topic till topic buffer
    strncpy(topic, input, topic_len);
    topic[topic_len] = '\0'; // Avsluta strängen

    // Pekare till meddelandet
    char* msg = sep + 1; // Allt efter ':' Exempel från innan sep är ":hej", nu blir msg bara "hej"
    if (strlen(msg) == 0) {
        PRINTFC_CLIENT("Invalid message, setting it to: empty");
        msg = "empty";
    }


    if (strncmp(topic, "/torget", topic_len) == 0) {
        PRINTFC_CLIENT("Sending to torget: %s", msg);

        //skicka till mqtt
        mqtt_send_to_broker(topic, msg);

    } else if (strncmp(topic, "/uplink", topic_len) == 0) {
        PRINTFC_CLIENT("Sending to uplink: %s", msg);

        // Validera meddelandet
        if (strcmp(msg, "ok") != 0 && strcmp(msg, "neka") != 0 &&
            strcmp(msg, "lyckas") != 0 && strcmp(msg, "sabotera") != 0) {
            PRINTFC_CLIENT("Invalid message for uplink: %s", msg);
            return;
        }
        // Skicka till relevant funktion
        //TOPIC_UPLINK BORDE VARA SATT TILL RÄTT TOPIC

    
        // Skapa JSON-payload med val
        char json_payload[100];
        snprintf(json_payload, sizeof(json_payload), "{\"val\":\"%s\"}", msg);

        // Skicka JSON till relevant uplink topic
        PRINTFC_CLIENT("Formatted JSON for uplink: %s", json_payload);    
        mqtt_send_to_broker(TOPIC_UPLINK, json_payload);

        // Skicka till relevant funktion
    }else {
        PRINTFC_CLIENT("Invalid topic: %s", topic);
    }
}