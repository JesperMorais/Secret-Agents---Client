#include <stdio.h>
#include "https_handle.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_client.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"  // om du vill använda inbyggt CA-bundle
#include "esp_log.h"
#include "printer_helper.h"
#include "mbedtls/pk.h"
#include "mbedtls/x509_csr.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "nvs.h"
#include "wifi_handler.h"
#include "global_params.h"

#define READ_RETRY_MAX  10
#define READ_RETRY_DELAY_MS 100
#define CHUNK_SIZE 512
#define SERVER_PORT 443
#define SERVER_POST_PLAYER_CERT "https://192.168.10.199:9191/spelare/csr"
#define SERVER_POST_PLAYER "https://192.168.10.199:9191/spelare"
#define SERVER_POST_START "https://192.168.10.199:9191/start"

extern char* root_ca_pem;

#define max_attempts 5

https_init_param_t* globalParam;

void cert_manage_task(void *pvParameters)
{
    PRINTFC_CERT("Cert manage task started");
    https_init_param_t *param = (https_init_param_t *)pvParameters;

    globalParam = param;
    if (globalParam == NULL) {
        PRINTFC_CERT("Error: globalParam is NULL");
        vTaskDelete(NULL);
    }
    PRINTFC_CERT("waiting for wifi connection");
    xEventGroupWaitBits(param->wifi_event_group, WIFI_CONNECTED_BIT | WIFI_HAS_IP_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

    PRINTFC_CERT("Got wifi, Cert manage task started");

    //SKICKA FÖRST FRÅGA OM ATT V´´ARA MED I SPELET FÅ TBX CN

    nvs_handle_t my_handle;
    int ret;
    ret = nvs_open_from_partition("eol", "certs", NVS_READONLY, &my_handle);
    if (ret != ESP_OK) {
        PRINTFC_CERT("Failed to open NVS partition eol/cert");
        vTaskDelete(NULL);
    }

    PRINTFC_CERT("Reading rootCA from NVS");
    size_t required_size = 0;
    ret = nvs_get_str(my_handle, "rootCA", NULL, &required_size);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        PRINTFC_CERT("ESP_ERR_NVS_NOT_FOUND?");
        nvs_close(my_handle);
        vTaskDelete(NULL);
    }else if(ret == ESP_ERR_NVS_INVALID_HANDLE){
        PRINTFC_CERT("ESP_ERR_NVS_INVALID_HANDLE?");
        nvs_close(my_handle);
        vTaskDelete(NULL);
    }else if(ret == ESP_ERR_NVS_INVALID_NAME){
        PRINTFC_CERT("ESP_ERR_NVS_INVALID_NAME?");
        nvs_close(my_handle);
        vTaskDelete(NULL);
    }

    PRINTFC_CERT("malloc for root_ca_pem");
    root_ca_pem = malloc(required_size);
    if (!root_ca_pem) {
        PRINTFC_CERT("malloc for root_ca_pem failed");
        nvs_close(my_handle);
        vTaskDelete(NULL);
    }

    ret = nvs_get_str(my_handle, "rootCA", root_ca_pem, &required_size);
    nvs_close(my_handle);
    if (ret != ESP_OK) {
        PRINTFC_CERT("Failed to read rootCA from NVS: %d", ret);
        free(root_ca_pem);
        vTaskDelete(NULL);
    }

    nvs_close(my_handle);

     PRINTFC_CERT("creating response struct");
    https_resp_t resp = {
        .body_buf = NULL,
        .total_size = 0,
    };

    //för att skicka in params i event
    event_ctx_t* ctx = malloc(sizeof(event_ctx_t));
    if (ctx == NULL) {
        PRINTFC_CERT("Error: Failed to allocate memory for ctx");
        vTaskDelete(NULL);
    }

    ctx->init_param = param;
    ctx->resp = resp;

    PRINTFC_CERT("creating config for http client");
    // 1. FRÅGA OM VI FÅR VARA MED I SPELET
    esp_http_client_config_t config = {
        .url = SERVER_POST_PLAYER, 
        .method = HTTP_METHOD_POST,
        .cert_pem = root_ca_pem, 
        .skip_cert_common_name_check = true,
        .user_data = ctx,  //Skicka in params i event
        .event_handler = _http_event_handler,
    };
    PRINTFC_CERT("conf init");
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        PRINTFC_CERT("Failed to init http client");
        vTaskDelete(NULL);
    }

    //MAKE FAKE JSON WITH NO DATA
    char* json = "{}";
    size_t json_len = strlen(json);
    
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json, json_len);


    uint8_t attempts = 0;
    int status = 0;

    PRINTFC_CERT("Trying to connect to server");
    do {
        PRINTFC_CERT("Attempt %d of %d", attempts + 1, max_attempts);
        esp_err_t err = esp_http_client_perform(client);

        if (err == ESP_OK) {
            status = esp_http_client_get_status_code(client);
            PRINTFC_CERT("HTTP POST Status = %d", status);
            if (status == 200) {
                PRINTFC_CERT("Successfully connected to server");
                break;
            }
        } else {
            PRINTFC_CERT("Failed to perform HTTP request: %s", esp_err_to_name(err));
        }

        attempts++;
        if (attempts < max_attempts) {
            vTaskDelay(pdMS_TO_TICKS(3000)); // Vänta 3 sekunder innan nästa försök
        }
    } while (status != 200 && attempts < max_attempts);

    //IFALL VI HAR NÅTT MAX ATTEMPTS OCH INTE FÅTT 200
    if (status != 200) {
        PRINTFC_CERT("Failed to connect to server after %d attempts", max_attempts);
        esp_http_client_cleanup(client);
        free(root_ca_pem);
        vTaskDelete(NULL);
    }
    
    PRINTFC_CERT("Cleaning up HTTP client");
    esp_http_client_cleanup(client);
    free(root_ca_pem);

    PRINTFC_CERT("Cert manage task completed");
    vTaskDelete(NULL);
}

static esp_err_t _http_event_handler(esp_http_client_event_t *event){
    event_ctx_t* ctx = (event_ctx_t*)event->user_data; //Hämta ut params

    switch(event->event_id) {
        case HTTP_EVENT_ON_DATA:
            PRINTFC_CERT("Received data, len=%d", event->data_len);
            if(event->data_len > 0){
                int old_size = ctx->resp.total_size;
                int new_size = old_size + event->data_len;

                char* new_buf = realloc(ctx->resp.body_buf, new_size + 1);
                if(new_buf == NULL){
                    PRINTFC_CERT("Failed to realloc buffer");
                    free(ctx->resp.body_buf);
                    ctx->resp.body_buf = NULL;
                    return ESP_FAIL;
                }

                ctx->resp.body_buf = new_buf;
                memcpy(ctx->resp.body_buf + old_size, event->data, event->data_len);
                ctx->resp.total_size = new_size;
                ctx->resp.body_buf[new_size] = '\0';
            }
            break;

        case HTTP_EVENT_ON_FINISH: {
            PRINTFC_CERT("I FINISHED");
            char* content_type = ""; 
            esp_http_client_get_header(event->client, "Content-Type", &content_type);
            PRINTFC_CERT("Content-Type: %s", content_type);

            // Om vi får JSON APPLICATION, skapa CSR
            if (content_type != NULL && strstr(content_type, "application/json") != NULL) {
                // Hantera JSON-svaret (ID)
                handle_json_response(ctx->resp.body_buf);
                break;
            } 
            // Om vi får PEM, spara certifikatet
            else if (content_type != NULL && strstr(content_type, "application/x-pem-file") != NULL) {
                // Hantera certifikatet
                PRINTFC_CERT("Received certificate, saving to NVS");
                nvs_handle_t my_handle;
                esp_err_t ret = nvs_open_from_partition("eol", "certs", NVS_READWRITE, &my_handle);
                if (ret == ESP_OK) {
                    ret = nvs_set_str(my_handle, "ClientCert", ctx->resp.body_buf);
                    nvs_commit(my_handle);
                    nvs_close(my_handle);
                    if (ret == ESP_OK) {
                        PRINTFC_CERT("Certificate saved in NVS");
                        xEventGroupSetBits(globalParam->cert_event_group, CERT_READY_BIT);   
                        PRINTFC_CERT("Cert ready bit set");
                    }
                }
            }else{
                PRINTFC_CERT("Did not match any content type"); 
                PRINTFC_CERT("Received data: %s", ctx->resp.body_buf);
            }
            
            PRINTFC_CLIENT("Freeing buffer");
            // Städa upp bufferten
            if (ctx->resp.body_buf) {
                PRINTFC_CERT("Freeing buffer");
                free(ctx->resp.body_buf);
                ctx->resp.body_buf = NULL; // Sätt pekaren till NULL för att undvika dubbla frigörningar
                ctx->resp.total_size = 0;
            }else{
                PRINTFC_CERT("Buffer was NULL");
            }
            break;
        }
        default:
            break;
    }
    return ESP_OK;
}

//SPARA KEY I NVS
//RETURNA CSR
char* createCSR(char* CommonName){

    mbedtls_pk_context key;
    mbedtls_x509write_csr csr;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    int ret;

    mbedtls_pk_init(&key);
    mbedtls_x509write_csr_init(&csr);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);

    
    char* key_buf = malloc(4096); 
    size_t key_buf_len = 4096;
    char* csr_buf = malloc(8192); // buffer för CSR i PEM
    size_t csr_buf_len = 8192;

    // 1) Seed the random number generator
    if((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0)) != 0){
        PRINTFC_CERT("Failed to seed random number generator: %d", ret);
        vTaskDelete(NULL);
    }

    // 2) Generate an RSA key pair
    if((ret = mbedtls_pk_setup(&key, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA))) != 0){
        PRINTFC_CERT("Failed to setup pk context: %d", ret);
        vTaskDelete(NULL);
    }

    // Generera RSA-nyckel
    if((ret = mbedtls_rsa_gen_key(mbedtls_pk_rsa(key), mbedtls_ctr_drbg_random, &ctr_drbg, 2048, 65537)) != 0){
        PRINTFC_CERT("Failed to generate RSA key: %d", ret);
        vTaskDelete(NULL);
    }


    // Exportera privat nyckel i PEM
    if ((ret = mbedtls_pk_write_key_pem(&key, (unsigned char *)key_buf, key_buf_len)) != 0) {
        PRINTFC_CERT("Failed pk_write_key_pem: %d", ret);
    }


    mbedtls_x509write_csr_set_md_alg(&csr, MBEDTLS_MD_SHA256);
    mbedtls_x509write_csr_set_key(&csr, &key);
    // Ange "CN=MyServer" eller nåt
    mbedtls_x509write_csr_set_subject_name(&csr, CommonName);


    // PEM-encoda CSR
    if ((ret = mbedtls_x509write_csr_pem(&csr, (unsigned char *)csr_buf, csr_buf_len, mbedtls_ctr_drbg_random, &ctr_drbg)) != 0) {
        PRINTFC_CERT("Failed x509write_csr_pem: %d", ret);
        vTaskDelete(NULL);
    }

    // städa
    mbedtls_x509write_csr_free(&csr);
    mbedtls_pk_free(&key);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);

    //Lagra key och csr i NVS

    nvs_handle_t my_handle;
    ret = nvs_open_from_partition("eol", "certs", NVS_READWRITE, &my_handle);
    if(ret != ESP_OK){
        PRINTFC_CERT("Failed to open NVS partition eol/cert");
        vTaskDelete(NULL);
    }

    //PRINTFC_CERT("Key: %s", key_buf);
    nvs_set_str(my_handle, "ClientKey", key_buf);
    nvs_commit(my_handle);
    free(key_buf);

    return csr_buf;
}

char* convert_to_cn(char* go_str) {
    printf("STARTING TO CONVERT %s\n", go_str);

    const char* id_start = strstr(go_str, "\"id\":"); // Leta efter exakt "id":
    if (id_start == NULL) {
        printf("Error: id not found\n");
        return NULL;
    }

    // Hoppa framåt för att hitta det faktiska värdet efter "id":
    id_start += 5; // Hoppar förbi "id": (5 tecken)
    while (*id_start == ' ' || *id_start == '\"') {
        id_start++; // Hoppar över mellanslag eller citattecken
    }

    // Läs ut värdet
    const char* id_end = id_start;
    while (*id_end >= '0' && *id_end <= '9') {
        id_end++; // Flyttar framåt till slutet av siffrorna
    }

    if (id_start == id_end) {
        printf("Error: id value not found\n");
        return NULL;
    }

    char id_buffer[16]; // Tillräckligt stor för alla rimliga ID-värden
    size_t id_len = id_end - id_start;
    if (id_len >= sizeof(id_buffer)) {
        printf("Error: id value too long\n");
        return NULL;
    }

    strncpy(id_buffer, id_start, id_len);
    id_buffer[id_len] = '\0'; // Null-terminera strängen

    // Konvertera strängen till ett heltal
    int id_value = atoi(id_buffer);
    playerID = id_value; // Sätt till global variabel
    printf("Extracted playerID: %d\n", playerID);

    char* cn_string = malloc(strlen("CN=") + id_len + 1); // Allokera minne för "CN=" + id + null-terminator
    if (cn_string == NULL) {
        printf("Error: Memory allocation failed\n");
        return NULL;
    }

    snprintf(cn_string, strlen("CN=") + id_len + 1, "CN=%.*s", (int)id_len, id_start); // Skriv "CN=" följt av id-värdet
    PRINTFC_CERT("Converted to CN: %s", cn_string);
    return cn_string;
}

esp_err_t send_csr_request(const char* csr) {
    
    PRINTFC_CERT("Sending CSR to server");
    https_resp_t resp = {
        .body_buf = NULL,
        .total_size = 0,
    };

    if(root_ca_pem == NULL){
        PRINTFC_CERT("Root CA not INIT YET");
        return ESP_FAIL;
    }

    PRINTFC_CERT("Creating HTTP CFG with URL: %s", SERVER_POST_PLAYER_CERT);
    esp_http_client_config_t config = {
        .url = SERVER_POST_PLAYER_CERT,
        .method = HTTP_METHOD_POST,
        .cert_pem = root_ca_pem,
        .skip_cert_common_name_check = true,
        .user_data = &resp,
        .event_handler = _http_event_handler,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Content-Type", "application/x-pem-file");
    esp_http_client_set_post_field(client, csr, strlen(csr));

    PRINTFC_CERT("sening CSR to server");
    esp_err_t err = esp_http_client_perform(client);
    esp_http_client_cleanup(client);

    return err;
}

void handle_json_response(char* json){
    PRINTFC_CERT("Received JSON response: %s", json);

    // Convert to CN
    //KOLLA SÅ DET HAR ID I JSON
    if(strstr(json, "id") == NULL){
        PRINTFC_CERT("No ID in JSON");
        PRINTFC_CERT("JSON: %s", json);
        return;
    }
    char* cn = convert_to_cn(json);

    if(cn == NULL){
        PRINTFC_CERT("Failed to convert to CN");
        return;
    }

    // Create CSR
    char* csr = createCSR(cn);
    if(csr == NULL){
        PRINTFC_CERT("Failed to create CSR");
        free(cn);
        return;
    }    
    free(cn);

    // Send CSR
    esp_err_t err = send_csr_request(csr);
    if(err != ESP_OK){
        PRINTFC_CERT("Failed to send CSR request");
        free(csr);
        return;
    }
    free(csr);
}

esp_err_t send_ready_check() {
    PRINTFC_CERT("Sending ready check to server");

    // Skapa JSON body
    const char* json_body = "{\"val\": \"nu kör vi\"}";

    // Skapa response strukt
    https_resp_t resp = {
        .body_buf = NULL,
        .total_size = 0,
    };

    PRINTFC_CERT("Reading client cert and key from NVS");
    char* client_cert = NULL;
    char* client_key = NULL;
    size_t cert_size = 0, key_size = 0;

    PRINTFC_CERT("4");
    nvs_handle_t nvs_handle;
        if (nvs_open_from_partition("eol", "certs", NVS_READONLY, &nvs_handle) == ESP_OK) {
            nvs_get_str(nvs_handle, "ClientCert", NULL, &cert_size);
            nvs_get_str(nvs_handle, "ClientKey", NULL, &key_size);

            client_cert = malloc(cert_size);
            client_key = malloc(key_size);

            if (client_cert && client_key) {
                nvs_get_str(nvs_handle, "ClientCert", client_cert, &cert_size);
                nvs_get_str(nvs_handle, "ClientKey", client_key, &key_size);
            }
            nvs_close(nvs_handle);
    }

    PRINTFC_CERT("Client cert and key read from NVS");
    if(root_ca_pem == NULL){
        PRINTFC_CERT("Root CA not INIT YET ( send_ready_check )");
        return ESP_FAIL;
    }

    // Skapa HTTP-konfiguration
    esp_http_client_config_t config = {
        .url = SERVER_POST_START,
        .method = HTTP_METHOD_POST,
        .cert_pem = root_ca_pem, // Använd samma certifikat om HTTPS används
        .skip_cert_common_name_check = true,
        .client_cert_pem = client_cert,
        .client_key_pem = client_key,
        .user_data = &resp,
        .event_handler = _http_event_handler, // Kan återanvända samma handler om nödvändigt
    };

     PRINTFC_CERT("3");
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        PRINTFC_CERT("Failed to init HTTP client for ready check");
        return ESP_FAIL;
    }
    PRINTFC_CERT("1");
    // Ställ in header och body
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_body, strlen(json_body));

    PRINTFC_CERT("5");
    // Skicka HTTP-förfrågan
    if (client_cert == NULL || client_key == NULL) {
    PRINTFC_CERT("Client cert or key is NULL, cannot proceed with ready check");
    if (client_cert) free(client_cert);
    if (client_key) free(client_key);
    return ESP_FAIL;
}

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        PRINTFC_CERT("Failed to perform HTTP request for ready check: %s", esp_err_to_name(err));
    } else {
        int status = esp_http_client_get_status_code(client);
        PRINTFC_CERT("Ready check POST Status = %d", status);
    }

    PRINTFC_CERT("6");
    // Städa upp klienten
    esp_http_client_cleanup(client);

    // Frigör response-buffer om den används
    if (resp.body_buf) {
        free(resp.body_buf);
    }
    free(client_cert);
    free(client_key);

    return err;
}