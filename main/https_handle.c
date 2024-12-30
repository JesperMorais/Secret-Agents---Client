#include <stdio.h>
#include "https_handle.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_client.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"  // om du vill använda inbyggt CA-bundle

#define SERVER_PORT 443
#define SERVER_ADDR localhost


void send_csr_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Starting send_csr_task...");

    // 1) Förbered en HTTP(S) POST
    esp_http_client_config_t config = {
        .url = "https://192.168.1.50:9191/spelare/csr", // t.ex. Go-serverns IP
        .method = HTTP_METHOD_POST,

        // Om servern har ett “riktigt” cert signerat av en känd CA,
        // kan du använda .crt_bundle_attach = esp_crt_bundle_attach
        // Har du en egen CA måste du ange .cert_pem i stället
        .crt_bundle_attach = esp_crt_bundle_attach,

        // Om du vill verifiera serverns cert med din egen Root CA,
        // då sätter du:
        // .cert_pem = (char *) root_ca_pem_start,
        // .transport_type = HTTP_TRANSPORT_OVER_SSL,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to init http client");
        vTaskDelete(NULL);
        return;
    }

    // 2) Sätt header: "Content-Type: application/x-pem-file" (som du gjorde i Go)
    esp_http_client_set_header(client, "Content-Type", "application/x-pem-file");

    // 3) Sätt vår CSR som POST-body
    // (vi använder en stor buffert, men i praktiken kan du streama etc.)
    esp_err_t err = esp_http_client_set_post_field(client, CSR_PEM, strlen(CSR_PEM));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set_post_field failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        vTaskDelete(NULL);
        return;
    }

    // 4) Skicka requesten
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP POST Status = %d", status);

        // 5) Läs svaret, som ska innehålla signerade certet i PEM
        int content_len = esp_http_client_get_content_length(client);
        ESP_LOGI(TAG, "Content len = %d", content_len);

        if (status == 200) {
            // Allokera en buffer
            char *buffer = malloc(content_len + 1);
            if (!buffer) {
                ESP_LOGE(TAG, "malloc failed");
            } else {
                int read_len = esp_http_client_read(client, buffer, content_len);
                if (read_len >= 0) {
                    buffer[read_len] = 0; // Nollterminera
                    ESP_LOGI(TAG, "Received cert:\n%s", buffer);

                    // Här kan du spara buffer i NVS (flash) eller i RAM
                    // ...
                } else {
                    ESP_LOGE(TAG, "read failed");
                }
                free(buffer);
            }
        }
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    // 6) Stäng ner HTTP
    esp_http_client_cleanup(client);

    // 7) Avsluta task
    vTaskDelete(NULL);
}