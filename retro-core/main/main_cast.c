#include "shared.h"
#include <lwip/sockets.h>
#include <cJSON.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>
#include "esp_http_client.h"
#include "rg_gui.h"

// Forward declarations
static inline void gui_clear(uint16_t color);
void gui_rect(int x, int y, int w, int h, uint16_t color);
char* send_https(const char* url, const char* post_data);
void minimap_dot(int x, int y, uint16_t color);

// Macro to create RGB565 color from 8-bit R, G, B values
#define RG_RGB565(r, g, b) ((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3))

#define REGISTER_CONTROLLER    "http://192.168.1.236:8000/link_controller/"
#define VIDEO_PORT   5005
#define CONTROL_PORT 5006

#define FRAME_MAX    (64 * 1024)   // max JPEG size
#define CHUNK_SIZE   1024
#define MINIMAP_SIZE 80
#define MINIMAP_X    220
#define MINIMAP_Y    10

#define MINIMAP_RADIUS 50.0f  // View radius around player
#define MINIMAP_SCALE  0.8f   // Pixels per unit

// Player position for centering minimap
static float player_x = 0.0f;
static float player_y = 0.0f;

int map_x(float x)
{
    // Center on player, relative positioning
    float relative_x = x - player_x;
    return MINIMAP_X + MINIMAP_SIZE/2 + (int)(relative_x * MINIMAP_SCALE);
}

int map_y(float y)
{
    // Center on player, relative positioning
    float relative_y = y - player_y;
    return MINIMAP_Y + MINIMAP_SIZE/2 + (int)(relative_y * MINIMAP_SCALE);
}

float distance_to_player(float x, float y)
{
    float dx = x - player_x;
    float dy = y - player_y;
    return sqrt(dx * dx + dy * dy);
}
void minimap_draw_bg(void)
{
    rg_gui_draw_rect(MINIMAP_X, MINIMAP_Y, MINIMAP_SIZE, MINIMAP_SIZE, 1,
                      RG_RGB565(255, 255, 255), RG_RGB565(0, 0, 0));

    
}
typedef struct {
    int id;
    float x;
    float y;
    bool isbot;
} minimap_tank_t;

static minimap_tank_t minimap_tanks[16];
static int minimap_count = 0;

// static int sock_video = -1;
// static int sock_control = -1;
int tank_number = 0;
static rg_app_t *app;
// static rg_surface_t *updates[2];
static rg_surface_t *currentUpdate;
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#define GUI_W 320
#define GUI_H 240

static rg_surface_t gui_surface;
static uint16_t gui_fb[GUI_W * GUI_H];

typedef struct {
    int health;
    int ammo;
    int kills;
    float speed;
    int powerup;
    bool isbot;
    bool connected;
    char message[128];
} tank_stats_t;

static tank_stats_t stats = {
    .health = 100,
    .ammo = 20,
    .kills = 0,
    .speed = 0.0f,
    .powerup = 0,
    .isbot = false,
    .connected = false,
    .message = {0}
};

typedef struct {
    char *buffer;
    int length;
    int max_len;
} http_resp_t;
void fetch_kills(void)
{
    char url[128];
    snprintf(url, sizeof(url),
        "http://192.168.1.236:8000/kills/%d",
        tank_number);

    char *resp = send_https(url, "{}");
    if (resp)
        stats.kills = atoi(resp);
}

void fetch_message(void)
{
    char url[128];
    snprintf(url, sizeof(url),
        "http://192.168.1.236:8000/message/%d",
        tank_number);

    char *resp = send_https(url, "{}");
    if (resp && strlen(resp) > 0) {
        strncpy(stats.message, resp, sizeof(stats.message) - 1);
        stats.message[sizeof(stats.message) - 1] = '\0';
    }
}

static int stats_timer = 0;



void fetch_tank_state(void)
{
    char url[128];
    snprintf(url, sizeof(url),
        "http://192.168.1.236:8000/state/%d",
        tank_number);

    char *resp = send_https(url, "{}");
    if (!resp || strlen(resp) < 3) {
        stats.connected = false;
        return;
    }

    stats.connected = true;

    // Expected: health,powerup,speed,ammo,isbot
    // Example: "87,0,1.2,14,False"

    char isbot_str[8];
    sscanf(resp, "%d,%d,%f,%d,%7s",
           &stats.health,
           &stats.powerup,
           &stats.speed,
           &stats.ammo,
           isbot_str);

    stats.isbot = (isbot_str[0] == 'T' || isbot_str[0] == 't');
}
void update_network_stats(void)
{
    if (++stats_timer >= 20) { // ~500ms
        fetch_tank_state();
        fetch_kills();
        fetch_message();
        stats_timer = 0;
    }
}
void minimap_update(void)
{
    char *json = send_https("http://192.168.1.236:8000/minimap", "{}");
    if (!json) return;

    cJSON *root = cJSON_Parse(json);
    if (!root) return;

    minimap_count = 0;
    cJSON *item;
    cJSON_ArrayForEach(item, root) {
        minimap_tanks[minimap_count].id =
            atoi(cJSON_GetObjectItem(item, "id")->valuestring);

        minimap_tanks[minimap_count].x =
            cJSON_GetObjectItem(item, "x")->valuedouble;

        minimap_tanks[minimap_count].y =
            cJSON_GetObjectItem(item, "y")->valuedouble;

        minimap_tanks[minimap_count].isbot =
            cJSON_GetObjectItem(item, "isbot")->valueint;

        minimap_count++;
        if (minimap_count >= 16) break;
    }

    cJSON_Delete(root);
}
void minimap_dot(int x, int y, uint16_t color)
{
    rg_gui_draw_rect(x,y,3,3,0,color,color);
    // Draw a small dot at (x, y) on the minimap
    // Using a simple 3x3 pixel square
    
}
void minimap_draw(void)
{
    minimap_draw_bg();

    for (int i = 0; i < minimap_count; i++) {
        int x = map_x(minimap_tanks[i].x);
        int y = map_y(minimap_tanks[i].y);

        uint16_t color;
        if (distance_to_player(minimap_tanks[i].x, minimap_tanks[i].y) > MINIMAP_RADIUS)
            continue;   // Out of range
        if (minimap_tanks[i].id == tank_number){
            color = RG_RGB565(0,255,0);      // YOU
            player_x = minimap_tanks[i].x;
            player_y = minimap_tanks[i].y;
        }
        else if (minimap_tanks[i].isbot)
            color = RG_RGB565(255,255,0);    // BOT
        else
            color = RG_RGB565(255,0,0);      // ENEMY

        minimap_dot(x, y, color);
    }
}

void gui_init(void)
{
    memset(gui_fb, 0, sizeof(gui_fb));

    gui_surface.width  = GUI_W;
    gui_surface.height = GUI_H;
    gui_surface.stride = GUI_W;
    gui_surface.offset = 0;
    gui_surface.format = RG_PIXEL_565_LE;
    gui_surface.palette = NULL;
    gui_surface.data = gui_fb;
    gui_surface.free_data = false;
    gui_surface.free_palette = false;

    currentUpdate = &gui_surface;
}
void gui_draw_stats(void)
{
    int y = 30;

    gui_rect(0, 0, GUI_W, 24, RG_RGB565(20, 20, 20));
    rg_gui_draw_text(8, 6, 200, "RETRO-GO TANK STATS", RG_RGB565(255,255,255), 0, 0);

    char line[64];

    // Status line
    rg_gui_draw_text(
        10, y, 200,
        stats.connected ? "Status: CONNECTED" : "Status: DISCONNECTED",
        stats.connected ? RG_RGB565(0,255,0) : RG_RGB565(255,0,0),
        0, 0
    );

    snprintf(line, sizeof(line), "Health: %d", stats.health);
    rg_gui_draw_text(10, y+20, 200, line, RG_RGB565(0,255,0), 0, 0);

    snprintf(line, sizeof(line), "Ammo: %d", stats.ammo);
    rg_gui_draw_text(10, y+40, 200, line, RG_RGB565(255,255,0), 0, 0);

    snprintf(line, sizeof(line), "Speed: %.2f", stats.speed);
    rg_gui_draw_text(10, y+60, 200, line, RG_RGB565(100,200,255), 0, 0);

    snprintf(line, sizeof(line), "Kills: %d", stats.kills);
    rg_gui_draw_text(10, y+80, 200, line, RG_RGB565(255,100,100), 0, 0);

    snprintf(line, sizeof(line), "Powerup: %d", stats.powerup);
    rg_gui_draw_text(10, y+100, 200, line, RG_RGB565(200,100,255), 0, 0);

    // Display message if available
    if (strlen(stats.message) > 0) {
        rg_gui_draw_rect(0, GUI_H - 30, GUI_W, 30,0,0,RG_RGB565(40, 40, 40));
        rg_gui_draw_text(5, GUI_H - 24, 310, stats.message, RG_RGB565(255,255,100), 0, 0);
    }

    minimap_draw();

    rg_display_submit(currentUpdate, 0);
}

static inline void gui_clear(uint16_t color)
{
    rg_gui_draw_rect(
        0,
        0,
        GUI_W,
        GUI_H,
        0,
        0,

        color
    );
    
}
void gui_rect(int x, int y, int w, int h, uint16_t color)
{
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            gui_fb[(y + j) * GUI_W + (x + i)] = color;
}

esp_err_t multiplayer_server_event_get_handler(esp_http_client_event_handle_t evt)
{
    http_resp_t *resp = (http_resp_t *)evt->user_data;

    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (!resp || !evt->data_len) break;

            int copy_len = evt->data_len;
            if (resp->length + copy_len >= resp->max_len) {
                copy_len = resp->max_len - resp->length - 1;
            }

            if (copy_len > 0) {
                memcpy(resp->buffer + resp->length, evt->data, copy_len);
                resp->length += copy_len;
                resp->buffer[resp->length] = '\0';
            }
            break;

        default:
            break;
    }
    return ESP_OK;
}
char* send_https(const char* url, const char* post_data)
{
    static char response[1024];

    http_resp_t resp = {
        .buffer = response,
        .length = 0,
        .max_len = sizeof(response)
    };

    memset(response, 0, sizeof(response));

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .event_handler = multiplayer_server_event_get_handler,
        .user_data = &resp
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        RG_LOGI("HTTP error: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return NULL;
    }

    esp_http_client_cleanup(client);
    RG_LOGI("Server response: %s", response);
    return response;
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
    tank_number = asktank();
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
    snprintf(post_data, sizeof(post_data),
             "http://192.168.1.236:8000/send_action/%d/%s",
             tank_number, act);
    send_https(post_data, "{}");
    sended = true;
    

}
void cast_main(void)
{
    app = rg_system_reinit(AUDIO_SAMPLE_RATE, NULL, NULL);
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
    gui_clear(RG_RGB565(0, 0, 0)); // black background

    while (1)
    {
        joystick = rg_input_read_gamepad();
        update_network_stats();
        
        
        static int minimap_timer = 0;
        if (++minimap_timer > 15) {   // ~500ms
            minimap_update();
            minimap_timer = 0;

        }
        gui_draw_stats();

        if (joystick & (RG_KEY_MENU|RG_KEY_OPTION))
        {
            
            if (joystick & RG_KEY_MENU)
            {
                
                rg_gui_game_menu();
            }
            else
                rg_gui_options_menu();
            gui_clear(RG_RGB565(0, 0, 0)); // black background
            
        }
        else if (joystick != joystick_old)
        {
            sended = false;
            sendkey("N");
            if (joystick & RG_KEY_UP) sendkey("F");
            if (joystick & RG_KEY_RIGHT) sendkey("R");
            if (joystick & RG_KEY_DOWN) sendkey("B");
            if (joystick & RG_KEY_LEFT)sendkey("L");
            if (joystick & RG_KEY_A) sendkey("S");
            if (!sended)  // stop
            joystick_old = joystick;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
}
