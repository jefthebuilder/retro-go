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
static void udp_image_server_task(void *pvParameters) {
    int sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    static uint8_t frame_buf[MAX_FRAME_SIZE];
    static uint8_t reassembly_buf[MAX_FRAME_SIZE];
    int expected_chunks = 0;
    int received_chunks = 0;
    int frame_size = 0;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        printf("Socket creation failed\n");
        vTaskDelete(NULL);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("Bind failed\n");
        close(sock);
        vTaskDelete(NULL);
    }

    printf("UDP server listening on port %d\n", PORT);

    while (1) {
        uint8_t packet[1500];
        int len = recvfrom(sock, packet, sizeof(packet), 0,
                           (struct sockaddr *)&client_addr, &client_addr_len);
        if (len < 0) continue;

        // First 2 bytes = chunk index
        if (len < 2) continue;
        uint16_t chunk_idx = (packet[0] << 8) | packet[1];
        const uint8_t *payload = packet + 2;
        int payload_len = len - 2;

        // Store in reassembly buffer
        int offset = chunk_idx * 1398; // CHUNK_SIZE - header (1400-2)
        if (offset + payload_len <= MAX_FRAME_SIZE) {
            memcpy(reassembly_buf + offset, payload, payload_len);
        }

        // (Optional) track received_chunks vs expected
        // For simplicity, assume new frame starts with chunk_idx==0
        if (chunk_idx == 0) {
            received_chunks = 0;
            expected_chunks = 0; // unknown until we detect frame end
        }

        received_chunks++;

        // Heuristic: if packet smaller than full payload, assume end of frame
        if (payload_len < 1398) {
            frame_size = offset + payload_len;

            // Frame is complete → decode
            const uint16_t *data16 = (const uint16_t *)reassembly_buf;
            uint16_t w = data16[0];
            uint16_t h = data16[1];
            rg_surface_t new_surface = {
                .width = w,
                .height = h,
                .stride = w * 2,
                .format = RG_PIXEL_565_LE,
                .data = (void *)(data16 + 2),   // <-- skip header correctly
            };
        }
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


    xTaskCreate(udp_image_server_task, "video_task", 16*1024, NULL, 5, NULL);

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
