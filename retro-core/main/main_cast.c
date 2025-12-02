#include "shared.h"
#include <lwip/sockets.h>
#include <cJSON.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>
#define REGISTER_CONTROLLER    "http://192.168.1.236:8000/link_controller/"
#define VIDEO_PORT   5005
#define CONTROL_PORT 5006

#define FRAME_MAX    (64 * 1024)   // max JPEG size
#define CHUNK_SIZE   1024

static int sock_video = -1;
static int sock_control = -1;

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

esp_err_t multiplayer_server_event_get_handler(esp_http_client_event_handle_t evt)
{
    static char *output_buffer = NULL;  // Accumulates response if user_data not set
    static int output_len = 0;

    switch (evt->event_id)
    {
        case HTTP_EVENT_ON_DATA:
            RG_LOGI( "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);

            if (!esp_http_client_is_chunked_response(evt->client)) {
                if (evt->user_data) {
                    // Copy response into user-provided buffer
                    memcpy(evt->user_data + output_len, evt->data, evt->data_len);
                } else {
                    // Allocate buffer if first chunk
                    if (output_buffer == NULL) {
                        int content_length = esp_http_client_get_content_length(evt->client);
                        if (content_length <= 0) {
                            content_length = 1024; // fallback size
                        }
                        output_buffer = (char *) malloc(content_length + 1); // +1 for null terminator
                        if (output_buffer == NULL) {
                            RG_LOGI( "Failed to allocate memory for output buffer");
                            return ESP_FAIL;
                        }
                        output_len = 0;
                    }
                    memcpy(output_buffer + output_len, evt->data, evt->data_len);
                }
                output_len += evt->data_len;
            }
            break;

        case HTTP_EVENT_ON_FINISH:
            RG_LOGI( "HTTP_EVENT_ON_FINISH");
            if (output_buffer != NULL) {
                output_buffer[output_len] = '\0'; // Null-terminate accumulated response
                RG_LOGI( "HTTP response: %s", output_buffer);
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;

        case HTTP_EVENT_DISCONNECTED:
            RG_LOGI( "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error(evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                RG_LOGI( "Last esp error code: 0x%x", err);
                RG_LOGI( "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;

        default:
            break;
    }

    return ESP_OK;
}
char* send_https(const char* url, const char* post_data){
        char response[512];

    esp_http_client_config_t config = {
            .url = url,
            .method = HTTP_METHOD_POST,
            .user_data =response,
            .event_handler = multiplayer_server_event_get_handler
        };
        esp_http_client_handle_t client = esp_http_client_init(&config);

        

        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_post_field(client, post_data, strlen(post_data));

        esp_err_t err = esp_http_client_perform(client);
        if (err != ESP_OK) {
            rg_gui_alert("Error", "Failed to contact server!");
            esp_http_client_cleanup(client);
            return NULL;
        }

        const char *buffer = response;
        if (buffer) {
            RG_LOGI("Server response: %s", buffer);

            return buffer;
        }
        return NULL;
}
int  tank_id = 0;
int asktank(){
    const rg_gui_option_t gamemodes[] = {
        {1, "1", NULL, RG_DIALOG_FLAG_NORMAL, NULL},
        {2, "2", NULL, RG_DIALOG_FLAG_NORMAL, NULL},
        {3, "3", NULL, RG_DIALOG_FLAG_NORMAL, NULL},
        {4, "4", NULL, RG_DIALOG_FLAG_NORMAL, NULL},
        {5, "5", NULL, RG_DIALOG_FLAG_NORMAL, NULL},
        {6, "6", NULL, RG_DIALOG_FLAG_NORMAL, NULL},
        {7, "7", NULL, RG_DIALOG_FLAG_NORMAL, NULL},
        {8, "8", NULL, RG_DIALOG_FLAG_NORMAL, NULL},
        RG_DIALOG_END
    };
    return rg_gui_dialog("Select Tank", gamemodes, 0);
}
void register_to_tank(){
    int tank_number = asktank();
    char post_data[128];
    snprintf(post_data, sizeof(post_data),
             "http://192.168.1.236:8000/link_controller/%d",
             tank_number);
    char* response = send_https(post_data, "{}");
    if (response) {
        tank_id = atoi(response);
        // is just a int response
        RG_LOGI("Registered to tank: %s", response);
    } else {
        rg_gui_alert("Error", "No response from server!");
    }

}
bool sended = false;
void sendkey(char* act){
    char post_data[128];
    sended = true;
    snprintf(post_data, sizeof(post_data),
             "http://192.168.1.236:8000/send_action/%d/%s",
             tank_number, act);
    char* response = send_https(post_data, "{}");
    

}
void cast_main(void)
{
    app = rg_system_reinit(AUDIO_SAMPLE_RATE, &handlers, NULL);
    rg_network_init();
    rg_network_wifi_start();
    if (rg_network_get_info().state != RG_NETWORK_CONNECTED)
    {
        int retry = 0;
        while (rg_network_get_info().state != RG_NETWORK_CONNECTED && retry++ < 50)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        if (rg_network_get_info().state != RG_NETWORK_CONNECTED)
        {
            RG_LOGI("Warning: WiFi not connected, proceeding anyway.\n");
        }
    }
    RG_LOGI("connected to wifi");
    
    register_to_tank();
    uint32_t joystick_old = -1;
    uint32_t joystick = 0;
    while (1)
    {
        joystick = rg_input_read_gamepad();

        if (joystick & (RG_KEY_MENU|RG_KEY_OPTION))
        {
            if (joystick & RG_KEY_MENU)
            {
                if (gnuboy_sram_dirty()) // save in case the user quits
                    gnuboy_save_sram(sramFile, false);
                rg_gui_game_menu();
            }
            else
                rg_gui_options_menu();
        }
        else if (joystick != joystick_old)
        {
            int pad = 0;
            sended = false;
            if (joystick & RG_KEY_UP) sendkey("f");
            if (joystick & RG_KEY_RIGHT) sendkey("r");
            if (joystick & RG_KEY_DOWN) sendkey("b");
            if (joystick & RG_KEY_LEFT)sendkey("l");
            if (joystick & RG_KEY_SELECT) pad |= GB_PAD_SELECT;
            if (joystick & RG_KEY_START) pad |= GB_PAD_START;
            if (joystick & RG_KEY_A) sendkey("S");
            if (joystick & RG_KEY_B) pad |= GB_PAD_B;
            if (!sended) sendkey("N"); // stop
            joystick_old = joystick;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
}
