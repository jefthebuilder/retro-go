/*
 * i_network_espidf.c
 *
 * ESP-IDF (lwIP sockets) replacement for SDL_net based networking for ESP32-S3.
 * Keeps original function names and behavior from the SDL_net version.
 *
 * IMPORTANT: This module assumes the network interface (Wi-Fi / Ethernet)
 * has already been initialized elsewhere in your application (esp_netif_init(),
 * wifi_start(), connected to AP, etc).
 *
 * Author: Converted to ESP-IDF by ChatGPT
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <stdint.h>
#include <stdbool.h>
#include "i_network.h"
#include "esp_log.h"
#include <netdb.h>
static const char *TAG = "i_network_espidf";

/* --- Types to match original SDL_net naming used by your code --- */

/* Basic aliases used in your original code */


UDP_SOCKET  udp_socket   = -1;
UDP_CHANNEL sentfrom     = -1;
IPaddress   sentfrom_addr;
size_t      sentbytes    = 0;
size_t      recvdbytes   = 0;

/* --- Globals that mirror the original file --- */



/* single reusable packet (original used a global udp_packet) */
static UDP_PACKET *udp_packet = NULL;

/* Channel map: maps UDP_CHANNEL -> sockaddr_in (remote peer) */
#define MAX_CHANNELS 32
static bool channel_used[MAX_CHANNELS];
static struct sockaddr_in channel_addr[MAX_CHANNELS];
static int next_free_channel = 0;

/* Helper: convert IPaddress -> sockaddr_in */
static void ipaddress_to_sockaddr(const IPaddress *ip, struct sockaddr_in *sa)
{
    memset(sa, 0, sizeof(*sa));
    sa->sin_family = AF_INET;
    sa->sin_addr.s_addr = ip->host;
    sa->sin_port = ip->port;
}

/* Helper: convert sockaddr_in -> IPaddress */
static void sockaddr_to_ipaddress(const struct sockaddr_in *sa, IPaddress *ip)
{
    ip->host = sa->sin_addr.s_addr;
    ip->port = sa->sin_port;
}

/* Allocate and initialize global structures (called from I_InitNetwork) */
void I_InitNetwork(void)
{
    /* This does NOT bring up Wi-Fi / DHCP. That must be done by application code. */
    ESP_LOGI(TAG, "I_InitNetwork: initializing UDP packet and channel table");

    /* allocate a reusable UDP_PACKET buffer with a large default size (like original) */
    if (udp_packet == NULL) {
        udp_packet = (UDP_PACKET *)malloc(sizeof(UDP_PACKET));
        if (!udp_packet) {
            ESP_LOGE(TAG, "Failed to allocate udp_packet struct");
            return;
        }
        udp_packet->maxlen = 10000;
        udp_packet->data = (byte *)malloc(udp_packet->maxlen);
        if (!udp_packet->data) {
            ESP_LOGE(TAG, "Failed to allocate udp_packet->data");
            free(udp_packet);
            udp_packet = NULL;
            return;
        }
        udp_packet->len = 0;
    }

    /* initialize channel map */
    for (int i = 0; i < MAX_CHANNELS; ++i) {
        channel_used[i] = false;
        memset(&channel_addr[i], 0, sizeof(channel_addr[i]));
    }
    next_free_channel = 0;

    /* leave UDP socket closed until I_Socket is called by the app */
}

/* Clean up network-related resources */
void I_ShutdownNetwork(void)
{
    ESP_LOGI(TAG, "I_ShutdownNetwork: cleaning up");

    if (udp_packet) {
        if (udp_packet->data) {
            free(udp_packet->data);
            udp_packet->data = NULL;
        }
        free(udp_packet);
        udp_packet = NULL;
    }

    if (udp_socket >= 0) {
        close(udp_socket);
        udp_socket = -1;
    }
}

/* Allocate an SDL-like UDP_PACKET with a given size */
UDP_PACKET *I_AllocPacket(int size)
{
    UDP_PACKET *p = (UDP_PACKET *)malloc(sizeof(UDP_PACKET));
    if (!p) return NULL;
    p->data = (byte *)malloc(size);
    if (!p->data) { free(p); return NULL; }
    p->maxlen = size;
    p->len = 0;
    p->channel = -1;
    memset(&p->address, 0, sizeof(p->address));
    return p;
}

void I_FreePacket(UDP_PACKET *packet)
{
    if (!packet) return;
    if (packet->data) free(packet->data);
    free(packet);
}

/* Make socket non-blocking helper */
static int make_socket_nonblocking(int s)
{
    int flags = fcntl(s, F_GETFL, 0);
    if (flags == -1) flags = 0;
    return fcntl(s, F_SETFL, flags | O_NONBLOCK);
}

/* I_WaitForPacket - wait up to ms milliseconds for data on the UDP socket */
void I_WaitForPacket(int ms)
{
    if (udp_socket < 0) return;

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(udp_socket, &readfds);

    struct timeval tv;
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;

    int rv = select(udp_socket + 1, &readfds, NULL, NULL, &tv);
    if (rv < 0) {
        if (errno != EINTR) {
            ESP_LOGW(TAG, "select() returned error: %d", errno);
        }
    }
    /* when select returns, I_GetPacket will grab the packet when called */
}

/* Server IP (for client connect behavior) stored in channel 0 to match SDL behavior */
static bool server_connected = false;

/* I_ConnectToServer: parse "host:port" (or hostname) and store it in channel 0 */
IPaddress serverIP;
int I_ConnectToServer(const char *serv)
{
    char hostbuf[512];
    char *p;
    Uint16 port;

    if (!serv) return -1;
    if (strlen(serv) >= sizeof(hostbuf)) return -1;
    strcpy(hostbuf, serv);

    p = strchr(hostbuf, ':');
    if (p) {
        *p++ = '\0';
        port = (Uint16)atoi(p);
    } else {
        port = 5030; /* default */
    }

    struct in_addr inaddr;
    if (inet_aton(hostbuf, &inaddr) == 0) {
        /* try DNS resolution with gethostbyname */
        struct addrinfo hints;
        struct addrinfo *res = NULL;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET; /* IPv4 only */
        hints.ai_socktype = SOCK_DGRAM;

        int err = getaddrinfo(hostbuf, NULL, &hints, &res);
        if (err != 0 || !res) {
            ESP_LOGE(TAG, "DNS lookup failed: %s", hostbuf);
            return -1;
        }

        struct sockaddr_in *sin = (struct sockaddr_in *)res->ai_addr;
        inaddr = sin->sin_addr;

        freeaddrinfo(res);
    }

    serverIP.host = inaddr.s_addr;
    serverIP.port = htons(port);

    /* store server in channel 0 */
    channel_used[0] = true;
    channel_addr[0].sin_family = AF_INET;
    channel_addr[0].sin_addr.s_addr = serverIP.host;
    channel_addr[0].sin_port = serverIP.port;
    if (next_free_channel <= 0) next_free_channel = 1;
    server_connected = true;

    return 0;
}

/* I_Disconnect: unbind server in channel 0 */
void I_Disconnect(void)
{
    if (channel_used[0]) {
        channel_used[0] = false;
        memset(&channel_addr[0], 0, sizeof(channel_addr[0]));
    }
    server_connected = false;
}

/* I_Socket: open a UDP socket and bind to port (0 for ephemeral) */
UDP_SOCKET I_Socket(Uint16 port)
{
    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (s < 0) {
        ESP_LOGE(TAG, "I_Socket: socket() failed: %d", errno);
        return -1;
    }

    /* allow reuse (not exactly same as SDL_IPPORT_RESERVED logic but okay) */
    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    sa.sin_port = htons(port);

    if (bind(s, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        /* if bind failed for nonzero port, return error */
        if (port != 0) {
            ESP_LOGE(TAG, "I_Socket: bind() to port %u failed: %d", (unsigned)port, errno);
            close(s);
            return -1;
        } else {
            /* if port 0 requested and bind failed, try increasing ports like original did */
            close(s);
            Uint16 tryport = 1024;
            for (; tryport < 65535; ++tryport) {
                s = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
                if (s < 0) break;
                sa.sin_port = htons(tryport);
                if (bind(s, (struct sockaddr *)&sa, sizeof(sa)) == 0) break;
                close(s);
            }
            if (s < 0) {
                ESP_LOGE(TAG, "I_Socket: failed to open any UDP port");
                return -1;
            }
        }
    }

    make_socket_nonblocking(s);
    udp_socket = s;
    ESP_LOGI(TAG, "I_Socket: udp socket opened (fd=%d)", udp_socket);
    return udp_socket;
}

/* I_CloseSocket: close an opened socket */
void I_CloseSocket(UDP_SOCKET sock)
{
    if (sock >= 0) {
        close(sock);
        if (udp_socket == sock) udp_socket = -1;
    }
}

/* I_RegisterPlayer: store IPaddress into channel map and return channel number */
UDP_CHANNEL I_RegisterPlayer(IPaddress *ipaddr)
{
    /* find a free channel slot */
    int c;
    for (c = 1; c < MAX_CHANNELS; ++c) { /* reserve 0 for serverIP if present */
        if (!channel_used[c]) break;
    }
    if (c >= MAX_CHANNELS) {
        ESP_LOGW(TAG, "I_RegisterPlayer: no free channels");
        return -1;
    }

    channel_used[c] = true;
    channel_addr[c].sin_family = AF_INET;
    channel_addr[c].sin_addr.s_addr = ipaddr->host;
    channel_addr[c].sin_port = ipaddr->port;
    if (c >= next_free_channel) next_free_channel = c + 1;

    return c;
}

/* I_UnRegisterPlayer: free channel mapping */
void I_UnRegisterPlayer(UDP_CHANNEL channel)
{
    if (channel >= 0 && channel < MAX_CHANNELS) {
        channel_used[channel] = false;
        memset(&channel_addr[channel], 0, sizeof(channel_addr[channel]));
        if (channel == next_free_channel - 1) {
            /* adjust next_free_channel downwards */
            int nf = 0;
            for (int i = 0; i < MAX_CHANNELS; ++i) if (channel_used[i]) nf = i + 1;
            next_free_channel = nf;
        }
    }
}

/* ChecksumPacket - same algorithm as original file */
static byte ChecksumPacket(const packet_header_t* buffer, size_t len)
{
    const byte* p = (const void*)buffer;
    byte sum = 0;

    if (len == 0) return 0;
    while (p++, --len) sum += *p;
    return sum;
}

/* I_GetPacket: receive a packet into the provided packet_header_t buffer
 * Returns length of data copied (0 if no valid packet / checksum fail) */
size_t I_GetPacket(packet_header_t* buffer, size_t buflen)
{
    if (udp_socket < 0) return 0;

    struct sockaddr_in src;
    socklen_t srclen = sizeof(src);
    ssize_t r = recvfrom(udp_socket, udp_packet->data, udp_packet->maxlen, 0,
                         (struct sockaddr *)&src, &srclen);
    if (r <= 0) {
        /* no data or error */
        return 0;
    }

    udp_packet->len = (int)r;
    recvdbytes += (size_t)r;

    /* copy up to buflen to caller buffer */
    size_t len = (size_t)udp_packet->len;
    if (buflen < len) len = buflen;
    memcpy(buffer, udp_packet->data, len);

    /* store channel and address info: find if this src matches a registered channel */
    int matched_channel = -1;
    for (int i = 0; i < MAX_CHANNELS; ++i) {
        if (!channel_used[i]) continue;
        if (channel_addr[i].sin_addr.s_addr == src.sin_addr.s_addr &&
            channel_addr[i].sin_port == src.sin_port) {
            matched_channel = i;
            break;
        }
    }
    /* If not matched, we still set sentfrom to -1 and store src address */
    sentfrom = (matched_channel >= 0) ? matched_channel : -1;

    sockaddr_to_ipaddress(&src, &sentfrom_addr);

    /* Validate checksum, similar to original */
    int checksum = 0;
    /* the packet_header_t must contain checksum at offset 'checksum' field which original accessed.
       We'll assume the caller-provided struct layout is identical and that the first bytes of udp_packet->data
       correspond to packet_header_t; we read 'checksum' from that struct in a safe way. */
    /* We copy into a temporary to examine checksum without modifying original buffer in udp_packet->data */
    byte saved_checksum = 0;
    if (len >= sizeof(packet_header_t)) {
        packet_header_t *ph = (packet_header_t *)buffer;
        saved_checksum = ph->checksum; /* requires packet_header_t to have 'checksum' member */
        ph->checksum = 0;
        byte psum = ChecksumPacket((const packet_header_t *)buffer, (size_t)udp_packet->len);
        if (psum == saved_checksum) {
            /* good packet */
            ph->checksum = saved_checksum;
            return len;
        } else {
            /* checksum mismatch */
            ph->checksum = saved_checksum;
            ESP_LOGW(TAG, "I_GetPacket: checksum mismatch (got %u, expected %u)", psum, saved_checksum);
            return 0;
        }
    } else {
        /* can't check checksum safely - return 0 to indicate invalid */
        ESP_LOGW(TAG, "I_GetPacket: packet too small for header (len=%u)", (unsigned)len);
        return 0;
    }
}

/* I_SendPacket: send packet to the default channel (0) to mirror SDL behavior */
void I_SendPacket(packet_header_t* packet, size_t len)
{
    if (udp_socket < 0) return;

    packet->checksum = ChecksumPacket(packet, len);

    /* ensure channel 0 exists (server) */
    if (!channel_used[0]) {
        ESP_LOGW(TAG, "I_SendPacket: no default channel (0) registered; packet not sent");
        return;
    }

    ssize_t s = sendto(udp_socket, (const void*)packet, len, 0,
                       (struct sockaddr *)&channel_addr[0], sizeof(channel_addr[0]));
    if (s > 0) {
        sentbytes += (size_t)s;
    } else {
        ESP_LOGW(TAG, "I_SendPacket: sendto failed: %d", errno);
    }
}

/* I_SendPacketTo: send packet to a specific registered channel */
void I_SendPacketTo(packet_header_t* packet, size_t len, UDP_CHANNEL *to)
{
    if (udp_socket < 0 || !to) return;

    packet->checksum = ChecksumPacket(packet, len);

    int ch = *to;
    if (ch < 0 || ch >= MAX_CHANNELS || !channel_used[ch]) {
        ESP_LOGW(TAG, "I_SendPacketTo: invalid channel %d", ch);
        return;
    }

    ssize_t s = sendto(udp_socket, (const void*)packet, len, 0,
                       (struct sockaddr *)&channel_addr[ch], sizeof(channel_addr[ch]));
    if (s > 0) {
        sentbytes += (size_t)s;
    } else {
        ESP_LOGW(TAG, "I_SendPacketTo: sendto failed: %d", errno);
    }
}

/* I_PrintAddress - placeholder to print the address for a channel (keeps same signature) */
void I_PrintAddress(FILE* fp, UDP_CHANNEL *addr)
{
    if (!fp || !addr) return;
    int ch = *addr;
    if (ch < 0 || ch >= MAX_CHANNELS || !channel_used[ch]) {
        fprintf(fp, "unregistered");
        return;
    }
    char ipbuf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &channel_addr[ch].sin_addr, ipbuf, sizeof(ipbuf));
    uint16_t port = ntohs(channel_addr[ch].sin_port);
    fprintf(fp, "%s:%u", ipbuf, (unsigned)port);
}
