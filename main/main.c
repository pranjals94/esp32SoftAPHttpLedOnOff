/*  WiFi softAP with Http Server

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.

    Read me (pranjal)

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    // this delay is necessary after abovecode for stations to get connected other wise shows auth error
    vTaskDelay(5 / portTICK_PERIOD_MS);

    For some android devices the security must be Open_type/.authmode = WIFI_AUTH_WPA2_PSK, else it wont show the Wifi
    leaving the password of ssid makes the connection open

    to set the ssdid/password idf.py menuconfig; >example configuration

*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include <esp_system.h>
#include <sys/param.h>
#include "esp_netif.h"
#include "esp_eth.h"

#include "esp_tls.h"
#include "sdkconfig.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "protocol_examples_utils.h"
#include "esp_tls_crypto.h"
#include <esp_http_server.h>
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_tls.h"
#include "cJSON.h"
#include "esp_vfs.h"

#if !CONFIG_IDF_TARGET_LINUX
#include <esp_wifi.h>
#include <esp_system.h>
#include "nvs_flash.h"
#include "esp_eth.h"
#endif

#include "driver/gpio.h"

/* The examples use WiFi configuration that you can set via project configuration menu.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
// #define EXAMPLE_ESP_WIFI_SSID CONFIG_ESP_WIFI_SSID
// #define EXAMPLE_ESP_WIFI_PASS CONFIG_ESP_WIFI_PASSWORD
// #define EXAMPLE_ESP_WIFI_CHANNEL CONFIG_ESP_WIFI_CHANNEL
// #define EXAMPLE_MAX_STA_CONN CONFIG_ESP_MAX_STA_CONN

#define EXAMPLE_ESP_WIFI_SSID "Esp32-AP"
#define EXAMPLE_ESP_WIFI_PASS "12345678"
#define EXAMPLE_ESP_WIFI_CHANNEL 1
#define EXAMPLE_MAX_STA_CONN 2

#define Led GPIO_NUM_2

#define EXAMPLE_HTTP_QUERY_KEY_MAX_LEN (64)

static const char *TAG = "wifi softAP with Http server";

#define SCRATCH_BUFSIZE (10240)
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
static const char *REST_TAG = "esp-rest";
typedef struct rest_server_context
{
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;

// below codes from httpServer example -----start-----

//------------An http post handler-------------
static esp_err_t ledBlink_handler(httpd_req_t *req)
{
    //------------------------test code start-----------------------
    // ESP_LOGI(TAG, "LED Blinking");
    // gpio_set_level(Led, 1);
    // vTaskDelay(500 / portTICK_PERIOD_MS);
    // gpio_set_level(Led, 0);
    // vTaskDelay(500 / portTICK_PERIOD_MS);
    // gpio_set_level(Led, 1);
    // vTaskDelay(500 / portTICK_PERIOD_MS);
    // gpio_set_level(Led, 0);
    // vTaskDelay(500 / portTICK_PERIOD_MS);

    int total_len = req->content_len;
    ESP_LOGI(TAG, "content length is'%d'", total_len);
    char buf[64]; // length of the content
    ESP_LOGI(TAG, "size of Buffer'%d'", sizeof(buf));
    if (total_len > sizeof(buf))
    {
        /* Respond with Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Error: cant handle long contents.");
        return ESP_FAIL;
    }
    int recv_size = MIN(total_len, sizeof(buf)); // turncate if content length larger tha buffer
    int ret = httpd_req_recv(req, buf, recv_size);
    if (ret <= 0) // 0 indicates connection is closed
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Connection Closed");
        return ESP_FAIL;
    }

    httpd_resp_sendstr(req, "Post control done");

    ESP_LOGI(TAG, "content is '%s'", buf);
    // cJSON *root = cJSON_Parse(buf);
    // int red = cJSON_GetObjectItem(root, "red")->valueint;
    // int green = cJSON_GetObjectItem(root, "green")->valueint;
    // int blue = cJSON_GetObjectItem(root, "blue")->valueint;
    // ESP_LOGI(REST_TAG, "Light control: red = %d, green = %d, blue = %d", red, green, blue);
    // cJSON_Delete(root);
    return ESP_OK;
}

/* An HTTP GET handler */

static esp_err_t ledOn_handler(httpd_req_t *req)
{
    esp_err_t error;
    ESP_LOGI(TAG, "LED Turned ON");
    gpio_set_level(Led, 1);
    const char *response = (const char *)req->user_ctx;
    error = httpd_resp_send(req, response, strlen(response));
    if (error != ESP_OK)
    {
        ESP_LOGI(TAG, "Error %d while sending Response", error);
    }
    else
        ESP_LOGI(TAG, "Response Sent Successfully");
    return error;
}

static esp_err_t ledOff_handler(httpd_req_t *req)
{
    esp_err_t error;
    ESP_LOGI(TAG, "LED Turned OFF");
    gpio_set_level(Led, 0);
    const char *response = (const char *)req->user_ctx;
    error = httpd_resp_send(req, response, strlen(response));
    if (error != ESP_OK)
    {
        ESP_LOGI(TAG, "Error %d while sending Response", error);
    }
    else
        ESP_LOGI(TAG, "Response Sent Successfully");
    return error;
}

static const httpd_uri_t ledOn = {
    .uri = "/ledOn",
    .method = HTTP_GET,
    .handler = ledOn_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx = "<!DOCTYPE html>\
				<html>\
				\
				<body>\
				\
				<h1>Esp32 Http Control: Led Is ON.</h1>\
				\
				<button type=\"button\" onclick=\"window.location.href='/ledOff'\">Led Off</button>\
				</body>\
				</html>"};

static const httpd_uri_t ledOff = {
    .uri = "/ledOff",
    .method = HTTP_GET,
    .handler = ledOff_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx = "<!DOCTYPE html>\
				<html>\
				\
				<body>\
				\
				<h1>Esp32 Http Control: Led is OFF.</h1>\
				\
				<button type=\"button\" onclick=\"window.location.href='/ledOn'\">Led On</button>\
				</body>\
				</html>"};
//--------post Request------
static const httpd_uri_t test_post_uri = {
    .uri = "/testPost",
    .method = HTTP_POST,
    .handler = ledBlink_handler,
    // .user_ctx = NULL,
    .user_ctx = "Post Request Successful"};
//-------post request end----

static esp_err_t ajax_request_handler(httpd_req_t *req)
{
    const char *response = (const char *)req->user_ctx;
    esp_err_t error = httpd_resp_send(req, response, strlen(response));
    return error;
}

static const httpd_uri_t ajax_request_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = ajax_request_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx = "<!DOCTYPE html>\
				<html>\
				\
                <script>\
                    function makeRequestWithFetch(){fetch('/testPost', {method : 'POST', headers: {\
                                              'Content-Type' : 'application/json'\
                                          },\
                                          body : JSON.stringify({key : 'value'})\
                                      })\
                                          .then(response => {\
                                              if (!response.ok)\
                                              {\
                                                  throw new Error('Network response was not ok ' + response.statusText);\
                                              }\
                                              return response.json();\
                                          })\
                                          .then(data => {\
                                              console.log(data);\
                                          })\
                                          .catch(error => {\
                                              console.error('There has been a problem with your fetch operation:', error);\
                                        });\
                                    }\
                </script >\
                <body>\
                <h1>\
                <button onclick = \"makeRequestWithFetch()\"> Make AJAX Request with Fetch</ button>\
				</body>\
				</html>"};

esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "URI not available");
    return ESP_FAIL;
}

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
#if CONFIG_IDF_TARGET_LINUX
    // Setting port as 8001 when building for Linux. Port 80 can be used only by a priviliged user in linux.
    // So when a unpriviliged user tries to run the application, it throws bind error and the server is not started.
    // Port 8001 can be used by an unpriviliged user as well. So the application will not throw bind error and the
    // server will be started.
    config.server_port = 8001;
#endif // !CONFIG_IDF_TARGET_LINUX
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK)
    {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &ledOff);
        httpd_register_uri_handler(server, &ledOn);
        httpd_register_uri_handler(server, &test_post_uri);
        httpd_register_uri_handler(server, &ajax_request_uri);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

#if !CONFIG_IDF_TARGET_LINUX

static void connect_handler(void *arg, esp_event_base_t event_base,
                            int32_t event_id, void *event_data)
{
    httpd_handle_t *server = (httpd_handle_t *)arg;
    if (*server == NULL)
    {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}
#endif // !CONFIG_IDF_TARGET_LINUX

// above codes from httpServer example -----end-----

static void configure_Led(void)
{

    gpio_reset_pin(Led);
    gpio_set_direction(Led, GPIO_MODE_OUTPUT);
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d",
                 MAC2STR(event->mac), event->aid);
        gpio_set_level(Led, 1);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        gpio_set_level(Led, 0);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
        gpio_set_level(Led, 1);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        gpio_set_level(Led, 0);
    }
}

void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
            // #ifdef CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT
            //             .authmode = WIFI_AUTH_WPA3_PSK,
            //             .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
            // #else /* CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT */
            //             .authmode = WIFI_AUTH_WPA2_PSK,
            // #endif
            .pmf_cfg = {
                .required = true,
            },
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    vTaskDelay(5 / portTICK_PERIOD_MS); // this delay is necessary for stations to get connected
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS, EXAMPLE_ESP_WIFI_CHANNEL);
}

extern void app_main(void)
{
    configure_Led();
    static httpd_handle_t server = NULL;
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_init_softap();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &connect_handler, &server));
    //    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));
}
