#include "shared.h"
#include <lwip/sockets.h>
#include <cJSON.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>
#define SERVER_IP    "192.168.1.236"
#define VIDEO_PORT   5005
#define CONTROL_PORT 5006

#define FRAME_MAX    (64 * 1024)   // max JPEG size
#define CHUNK_SIZE   1024

static int sock_video = -1;
static int sock_control = -1;



// forward declaration
static void send_control_json(const char *json);

// ------------------ CONTROL ------------------

static void send_button_event(const char *key, bool pressed)
{
    char buf[128];
    snprintf(buf, sizeof(buf),
             "{\"type\":\"%s\",\"key\":\"%s\"}",
             pressed ? "press" : "release", key);
    send_control_json(buf);
}

static void poll_input(void)
{
    static uint32_t last_state = 0;
    uint32_t joystick = rg_input_read_gamepad();

    struct {
        uint32_t mask;
        const char *name;
    } mapping[] = {
        {RG_KEY_UP,     "UP"},
        {RG_KEY_DOWN,   "DOWN"},
        {RG_KEY_LEFT,   "LEFT"},
        {RG_KEY_RIGHT,  "RIGHT"},
        {RG_KEY_A,      "A"},
        {RG_KEY_X,      "X"},
        {RG_KEY_Y,      "Y"},
        {RG_KEY_B,      "B"},
        {RG_KEY_START,  "START"},
        {RG_KEY_SELECT, "SELECT"},
    };

    for (int i = 0; i < sizeof(mapping)/sizeof(mapping[0]); i++) {
        bool now = joystick & mapping[i].mask;
        bool before = last_state & mapping[i].mask;
        if (now && !before) send_button_event(mapping[i].name, true);
        if (!now && before) send_button_event(mapping[i].name, false);
    }

    last_state = joystick;
}

static void send_control_json(const char *json)
{
    if (sock_control < 0) return;
    send(sock_control, json, strlen(json), 0);
}

// ------------------ VIDEO ------------------

// receiver_optimized.c
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>


#define PORT 5005
#define MAX_FRAME_SIZE (320*240*2+8)  // adjust based on max expected size
int recv_all(int sock, void *buf, int len) {
    int total = 0;
    char *p = buf;
    while (total < len) {
        int to_read = len - total;
        if (to_read > 8192) to_read = 8192; // limit per recv
        int r = recv(sock, p + total, to_read, 0);
        if (r <= 0) return r;
        total += r;
    }
    return total;
}
void tcp_image_server_task(void *pvParameters) {
    int listen_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    static uint8_t frame_buf[MAX_FRAME_SIZE];  // persistent buffer

    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0) {
        printf("Socket creation failed\n");
        vTaskDelete(NULL);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    listen(listen_sock, 1);

    printf("TCP server listening on port %d\n", PORT);

    while (1) {
        client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_sock < 0) continue;
        printf("Client connected\n");
        int recv_buf_size = 64 * 1024; // 64 KB, adjust based on RAM
        setsockopt(client_sock, SOL_SOCKET, SO_RCVBUF, &recv_buf_size, sizeof(recv_buf_size));
        int flag = 1;
        setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
        while (1) {
            uint32_t buf_size;
            int received = recv_all(client_sock, &buf_size, sizeof(buf_size));
            if (received <= 0) break;
            buf_size = ntohl(buf_size);
            RG_LOGI("%i",buf_size);
            if (buf_size > MAX_FRAME_SIZE) break;

            if (recv_all(client_sock, frame_buf, buf_size) < 0) break;

            void *new_surface = rg_surface_load_image(frame_buf, buf_size, 0);
            if (new_surface) {

                rg_display_submit(new_surface, 0);
                rg_surface_free(new_surface);
            } else {
                printf("Image decode failed (size %u)\n", buf_size);
            }
        }

        printf("Client disconnected\n");
        close(client_sock);
    }
}
void cast_main(void)
{
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
            RG_LOGI(stderr, "Warning: WiFi not connected, proceeding anyway.\n");
        }
    }
    RG_LOGI("connected to wifi");


    xTaskCreate(tcp_image_server_task, "video_task", 16*1024, NULL, 5, NULL);

    // open control socket
    sock_control = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in addr_ctrl = {
        .sin_family = AF_INET,
        .sin_port = htons(CONTROL_PORT),
        .sin_addr.s_addr = inet_addr(SERVER_IP),
    };
    connect(sock_control, (struct sockaddr *)&addr_ctrl, sizeof(addr_ctrl));

    // announce to server
    send_control_json("{\"type\":\"hello\"}");

    while (1) {
        poll_input();
        vTaskDelay(1);  // yield to FreeRTOS
    }
}
