#ifdef RG_ENABLE_NETPLAY

#include "rg_gui.h"
#include "rg_netplay.h"
#include "rg_system.h"
#include "rg_input.h"
#include "rg_settings.h"
#include <string.h>
#include <stdio.h>

#define NS_NETPLAY "netplay"

// Show netplay status overlay
void rg_gui_draw_netplay_status(void)
{
    if (rg_netplay_get_status() == RG_NETPLAY_STATUS_DISCONNECTED)
        return;
    
    rg_netplay_session_t *session = rg_netplay_get_session();
    if (!session)
        return;
    
    char status_text[128];
    const char *status_str = "Unknown";
    rg_color_t status_color = C_WHITE;
    
    switch (rg_netplay_get_status())
    {
        case RG_NETPLAY_STATUS_CONNECTING:
            status_str = "Connecting...";
            status_color = C_YELLOW;
            break;
        case RG_NETPLAY_STATUS_SYNCHRONIZING:
            status_str = "Synchronizing...";
            status_color = C_YELLOW;
            break;
        case RG_NETPLAY_STATUS_CONNECTED:
            status_str = "Connected";
            status_color = C_GREEN;
            break;
        case RG_NETPLAY_STATUS_DESYNCED:
            status_str = "Desynced!";
            status_color = C_RED;
            break;
        case RG_NETPLAY_STATUS_ERROR:
            status_str = "Error";
            status_color = C_RED;
            break;
        default:
            return;
    }
    
    // Draw status box in top-right corner
    snprintf(status_text, sizeof(status_text), "Netplay: %s (P%d/%d)", 
             status_str, rg_netplay_get_local_player_id() + 1, rg_netplay_get_player_count());
    
    int text_width = strlen(status_text) * 8;
    int box_width = text_width + 20;
    int box_height = 24;
    int box_x = rg_display_get_width() - box_width - 10;
    int box_y = 10;
    
    rg_gui_draw_rect(box_x, box_y, box_width, box_height, 2, status_color, C_BLACK);
    rg_gui_draw_text(box_x + 10, box_y + 5, text_width, status_text, C_WHITE, C_BLACK, RG_TEXT_ALIGN_LEFT);
}

// Netplay host menu
static bool netplay_host_menu(const char *game_hash)
{
    char port_str[16];
    char max_players_str[16];
    char input_delay_str[16];
    
    uint16_t port = rg_settings_get_number(NS_NETPLAY, "port", RG_NETPLAY_PORT);
    uint8_t max_players = rg_settings_get_number(NS_NETPLAY, "max_players", 2);
    uint16_t input_delay = rg_settings_get_number(NS_NETPLAY, "input_delay", 2);
    
    snprintf(port_str, sizeof(port_str), "%d", port);
    snprintf(max_players_str, sizeof(max_players_str), "%d", max_players);
    snprintf(input_delay_str, sizeof(input_delay_str), "%d", input_delay);
    
    const rg_gui_option_t options[] = {
        {0, "Port", port_str, RG_DIALOG_FLAG_NORMAL, NULL},
        {1, "Max Players", max_players_str, RG_DIALOG_FLAG_NORMAL, NULL},
        {2, "Input Delay", input_delay_str, RG_DIALOG_FLAG_NORMAL, NULL},
        {3, "Start Hosting", NULL, RG_DIALOG_FLAG_NORMAL, NULL},
        RG_DIALOG_END
    };
    
    while (true)
    {
        int sel = rg_gui_dialog("Host Netplay Game", options, 0);
        
        if (sel == RG_DIALOG_CANCELLED)
            return false;
        
        switch (sel)
        {
            case 0: // Port
            {
                int new_port = rg_gui_number_picker("Port", port, 1024, 65535, 1);
                if (new_port > 0)
                {
                    port = new_port;
                    rg_settings_set_number(NS_NETPLAY, "port", port);
                }
                break;
            }
            
            case 1: // Max Players
            {
                int new_max = rg_gui_number_picker("Max Players", max_players, 2, RG_NETPLAY_MAX_PLAYERS, 1);
                if (new_max > 0)
                {
                    max_players = new_max;
                    rg_settings_set_number(NS_NETPLAY, "max_players", max_players);
                }
                break;
            }
            
            case 2: // Input Delay
            {
                int new_delay = rg_gui_number_picker("Input Delay (frames)", input_delay, 0, 10, 1);
                if (new_delay >= 0)
                {
                    input_delay = new_delay;
                    rg_settings_set_number(NS_NETPLAY, "input_delay", input_delay);
                }
                break;
            }
            
            case 3: // Start hosting
            {
                rg_netplay_session_t *session = rg_netplay_create_session();
                if (!session)
                {
                    rg_gui_alert("Failed", "Could not create netplay session");
                    return false;
                }
                
                if (!rg_netplay_host_game(port, game_hash, max_players))
                {
                    rg_gui_alert("Failed", "Could not start hosting");
                    rg_netplay_destroy_session(session);
                    return false;
                }
                
                // Get IP address
                char ip_address[32] = "Unknown";
                rg_network_wifi_get_ip_address(ip_address, sizeof(ip_address));
                
                char info[128];
                snprintf(info, sizeof(info), "Hosting on %s:%d\nWaiting for players...", ip_address, port);
                rg_gui_alert("Hosting", info);
                
                return true;
            }
        }
    }
    
    return false;
}

// Netplay client menu
static bool netplay_client_menu(void)
{
    char host[64] = {0};
    char player_name[32] = {0};
    
    // Get saved host from settings
    const char *saved_host = rg_settings_get_string(NS_NETPLAY, "last_host", "192.168.1.100");
    const char *saved_name = rg_settings_get_string(NS_NETPLAY, "player_name", "Player");
    
    strncpy(host, saved_host, sizeof(host) - 1);
    strncpy(player_name, saved_name, sizeof(player_name) - 1);
    
    uint16_t port = rg_settings_get_number(NS_NETPLAY, "port", RG_NETPLAY_PORT);
    
    // Simple keyboard input for host
    if (!rg_gui_keyboard("Enter Host IP", host, sizeof(host)))
        return false;
    
    // Simple keyboard input for player name
    if (!rg_gui_keyboard("Enter Player Name", player_name, sizeof(player_name)))
        return false;
    
    // Save settings
    rg_settings_set_string(NS_NETPLAY, "last_host", host);
    rg_settings_set_string(NS_NETPLAY, "player_name", player_name);
    
    // Create session
    rg_netplay_session_t *session = rg_netplay_create_session();
    if (!session)
    {
        rg_gui_alert("Failed", "Could not create netplay session");
        return false;
    }
    
    // Connect
    char status[64];
    snprintf(status, sizeof(status), "Connecting to %s:%d...", host, port);
    rg_gui_alert("Connecting", status);
    
    if (!rg_netplay_connect(host, port, player_name))
    {
        rg_gui_alert("Failed", "Connection failed");
        rg_netplay_destroy_session(session);
        return false;
    }
    
    rg_gui_alert("Success", "Connected to host!");
    return true;
}

// Netplay statistics overlay
void rg_gui_draw_netplay_stats(void)
{
    if (rg_netplay_get_status() == RG_NETPLAY_STATUS_DISCONNECTED)
        return;
    
    uint32_t packets_sent, packets_recv, bytes_sent, bytes_recv, rollbacks;
    float latency;
    
    rg_netplay_get_stats(&packets_sent, &packets_recv, &bytes_sent, &bytes_recv, &rollbacks, &latency);
    
    char stats[256];
    snprintf(stats, sizeof(stats), 
             "Netplay Stats:\n"
             "Packets: %u sent, %u recv\n"
             "Bytes: %u sent, %u recv\n"
             "Rollbacks: %u\n"
             "Latency: %.1f ms",
             packets_sent, packets_recv, bytes_sent, bytes_recv, rollbacks, latency);
    
    rg_gui_draw_rect(10, 50, 300, 120, 2, C_WHITE, C_BLACK);
    rg_gui_draw_text(20, 60, 280, stats, C_WHITE, C_BLACK, RG_TEXT_ALIGN_LEFT | RG_TEXT_MULTILINE);
}

// Player list display
static void draw_player_list(void)
{
    char text[256];
    int y = 60;
    
    snprintf(text, sizeof(text), "Connected Players (%d):", rg_netplay_get_player_count());
    rg_gui_draw_text(20, y, 280, text, C_WHITE, C_BLACK, RG_TEXT_ALIGN_LEFT);
    y += 25;
    
    for (uint8_t i = 0; i < RG_NETPLAY_MAX_PLAYERS; i++)
    {
        const rg_netplay_player_t *player = rg_netplay_get_player_info(i);
        if (!player || !player->connected)
            continue;
        
        rg_color_t color = (i == rg_netplay_get_local_player_id()) ? C_GREEN : C_WHITE;
        snprintf(text, sizeof(text), "P%d: %s (%.0fms)", i + 1, player->name, (float)player->latency_ms);
        rg_gui_draw_text(20, y, 280, text, color, C_BLACK, RG_TEXT_ALIGN_LEFT);
        y += 20;
    }
}

// Main netplay menu
void rg_gui_netplay_menu(void)
{
    rg_app_t *app = rg_system_get_app();
    
    // Verify WiFi is connected
    if (!rg_network_wifi_is_connected())
    {
        rg_gui_alert("Error", "WiFi not connected!\nPlease connect to WiFi first.");
        return;
    }
    
    // Calculate game hash
    char game_hash[32] = {0};
    if (app->romPath && !rg_netplay_verify_game_hash(app->romPath, game_hash))
    {
        rg_gui_alert("Error", "Could not calculate game hash");
        return;
    }
    
    // Check if already in netplay session
    if (rg_netplay_get_status() != RG_NETPLAY_STATUS_DISCONNECTED)
    {
        const rg_gui_option_t active_options[] = {
            {0, "View Players", NULL, RG_DIALOG_FLAG_NORMAL, NULL},
            {1, "View Statistics", NULL, RG_DIALOG_FLAG_NORMAL, NULL},
            {2, "Disconnect", NULL, RG_DIALOG_FLAG_NORMAL, NULL},
            RG_DIALOG_END
        };
        
        int sel = rg_gui_dialog("Netplay Active", active_options, 0);
        
        switch (sel)
        {
            case 0: // View Players
            {
                // Show player list in a dialog
                draw_player_list();
                rg_input_wait_for_key(RG_KEY_ANY, true);
                break;
            }
            case 1: // View Statistics
            {
                rg_gui_draw_netplay_stats();
                rg_input_wait_for_key(RG_KEY_ANY, true);
                break;
            }
            case 2: // Disconnect
            {
                if (rg_gui_confirm("Disconnect from netplay session?", false, false))
                {
                    rg_netplay_disconnect();
                    rg_gui_alert("Disconnected", "Netplay session ended");
                }
                break;
            }
        }
        return;
    }
    
    // Main netplay menu
    const rg_gui_option_t options[] = {
        {0, "Host Game", NULL, RG_DIALOG_FLAG_NORMAL, NULL},
        {1, "Join Game", NULL, RG_DIALOG_FLAG_NORMAL, NULL},
        {2, "Settings", NULL, RG_DIALOG_FLAG_NORMAL, NULL},
        RG_DIALOG_END
    };
    
    int sel = rg_gui_dialog("Netplay", options, 0);
    
    switch (sel)
    {
        case 0: // Host
            netplay_host_menu(game_hash);
            break;
            
        case 1: // Join
            netplay_client_menu();
            break;
            
        case 2: // Settings
        {
            const rg_gui_option_t settings[] = {
                {0, "Sync Method", "Rollback", RG_DIALOG_FLAG_NORMAL, NULL},
                {1, "Show Status", "Yes", RG_DIALOG_FLAG_NORMAL, NULL},
                RG_DIALOG_END
            };
            rg_gui_dialog("Netplay Settings", settings, 0);
            break;
        }
    }
}

// Simple keyboard dialog (placeholder - should be replaced with proper implementation)
bool rg_gui_keyboard(const char *title, char *buffer, size_t buffer_size)
{
    // This is a simplified version - in a real implementation you would have
    // a proper on-screen keyboard with input handling
    rg_gui_alert(title, "Keyboard input not yet implemented.\nUsing default value.");
    return true;
}

// Number picker dialog
int rg_gui_number_picker(const char *title, int initial_value, int min_value, int max_value, int step)
{
    int value = initial_value;
    
    while (true)
    {
        char value_str[32];
        snprintf(value_str, sizeof(value_str), "%d", value);
        
        const rg_gui_option_t options[] = {
            {0, "Value", value_str, RG_DIALOG_FLAG_NORMAL, NULL},
            {1, "Decrease", NULL, RG_DIALOG_FLAG_NORMAL, NULL},
            {2, "Increase", NULL, RG_DIALOG_FLAG_NORMAL, NULL},
            {3, "Confirm", NULL, RG_DIALOG_FLAG_NORMAL, NULL},
            RG_DIALOG_END
        };
        
        int sel = rg_gui_dialog(title, options, 0);
        
        if (sel == RG_DIALOG_CANCELLED)
            return -1;
        
        switch (sel)
        {
            case 1: // Decrease
                value = RG_MAX(min_value, value - step);
                break;
            case 2: // Increase
                value = RG_MIN(max_value, value + step);
                break;
            case 3: // Confirm
                return value;
        }
    }
}

#endif // RG_ENABLE_NETPLAY
