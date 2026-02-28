# Integrating App Store into Retro-Go Launcher

This guide explains how to add the Retro-Go App Store integration to the launcher application.

## Overview

The App Store client library allows the launcher (or any retro-go app) to:
- Discover and browse available apps
- Download apps directly to the device
- Track installed apps and their versions
- Submit ratings and reviews
- Manage app licenses and trials

## Step 1: Add App Store Client to Components

Copy the app store client files to the retro-go component:

```bash
cp appstore/client/rg_appstore.h components/retro-go/
cp appstore/client/rg_appstore.c components/retro-go/
```

## Step 2: Update CMakeLists.txt

Add the app store client to your component's CMakeLists.txt:

```cmake
idf_component_register(
    SRCS
        rg_appstore.c
        # ... existing sources
    INCLUDE_DIRS
        .
    REQUIRES
        cjson
        esp_http_client
        # ... existing requirements
)
```

Ensure cJSON is available in your project (needed for JSON parsing).

## Step 3: Initialize App Store in Launcher

In your launcher's main.c or initialization code:

```c
#include "rg_appstore.h"

static void launcher_init_appstore(void)
{
    // Configure app store server URL
    const char *server_url = "http://appstore.local:5000";
    if (!rg_appstore_init(server_url))
    {
        RG_LOGW("Failed to initialize app store");
        return;
    }
    
    // Get device info
    rg_app_t *app = rg_system_get_app();
    rg_appstore_device_t device = {
        .name = "My Device",
        .model = "ODROID-GO",
        .firmware_version = app->version,
        .storage_total = 4000000000,  // Example: 4GB
        .storage_used = 1000000000,   // Example: 1GB used
    };
    
    // Register device with app store
    if (!rg_appstore_register_device(&device))
    {
        RG_LOGW("Failed to register device with app store");
    }
    
    RG_LOGI("App store initialized");
}

void app_main(void)
{
    // ... existing initialization
    launcher_init_appstore();
    // ... rest of launcher
}
```

## Step 4: Create App Store UI Screen

Add a new screen to the launcher to browse and download apps:

```c
#include "rg_appstore.h"

typedef struct {
    rg_appstore_app_t *apps;
    int app_count;
    int selected_index;
    char *selected_category;
} appstore_ui_t;

static appstore_ui_t appstore_ui = {0};

static void appstore_load_apps(const char *category)
{
    // Free previous apps
    if (appstore_ui.apps) {
        free(appstore_ui.apps);
        appstore_ui.apps = NULL;
    }
    
    appstore_ui.app_count = 0;
    appstore_ui.selected_index = 0;
    
    // Load apps from server
    if (!rg_appstore_get_apps(category, &appstore_ui.app_count, &appstore_ui.apps))
    {
        RG_LOGE("Failed to load apps from store");
        return;
    }
    
    RG_LOGI("Loaded %d apps from store", appstore_ui.app_count);
}

static void appstore_draw_apps(void)
{
    rg_surface_t *screen = rg_system_get_app()->screen;
    
    // Draw app list
    for (int i = 0; i < appstore_ui.app_count && i < 5; i++)
    {
        int y = 40 + (i * 32);
        rg_appstore_app_t *app = &appstore_ui.apps[i];
        
        // Highlight selected app
        if (i == appstore_ui.selected_index)
        {
            rg_gui_fill_rect(screen, 0, y - 2, 160, 30, 
                           (rg_color_t){.r = 100, .g = 100, .b = 255});
        }
        
        // Draw app name and info
        char text[128];
        snprintf(text, sizeof(text), "%s (%.2f MB)", 
                app->name, 
                app->file_size / (1024.0 * 1024.0));
        
        rg_gui_draw_text(screen, 5, y + 5, 150, 
                        text, RG_COLOR_WHITE);
    }
}

static void appstore_download_app(int index)
{
    if (index < 0 || index >= appstore_ui.app_count)
        return;
    
    rg_appstore_app_t *app = &appstore_ui.apps[index];
    
    // Show progress dialog
    rg_gui_dialog("Downloading", NULL, 0);
    
    // Download app
    char dest_path[RG_PATH_MAX];
    snprintf(dest_path, sizeof(dest_path), "%s/apps/%s.bin", 
            RG_BASE_PATH, app->name);
    
    if (!rg_appstore_download_app(app->id, dest_path, NULL))
    {
        RG_LOGE("Failed to download app: %s", app->name);
        rg_gui_alert("Error", "Failed to download app");
        return;
    }
    
    // Record installation
    if (!rg_appstore_install_app(app->id, app->version))
    {
        RG_LOGW("Failed to record installation");
    }
    
    rg_gui_alert("Success", "App downloaded successfully!");
}

static void appstore_run(void)
{
    rg_app_t *app = rg_system_get_app();
    
    // Load featured apps initially
    appstore_load_apps(NULL);
    
    while (1)
    {
        // Draw UI
        rg_display_clear(0);
        
        // Draw title
        rg_gui_draw_text(app->screen, 5, 5, 150, 
                        "App Store", RG_COLOR_WHITE);
        
        // Draw apps list
        appstore_draw_apps();
        
        // Draw instructions
        rg_gui_draw_text(app->screen, 5, 150, 150, 
                        "A:Download B:Back", RG_COLOR_GRAY);
        
        rg_display_sync();
        
        // Handle input
        rg_input_event_t event = rg_input_read_event();
        
        if (event.button == RG_BUTTON_MENU)
            break;
        
        if (event.button == RG_BUTTON_UP)
            appstore_ui.selected_index--;
        
        if (event.button == RG_BUTTON_DOWN)
            appstore_ui.selected_index++;
        
        if (event.button == RG_BUTTON_A)
            appstore_download_app(appstore_ui.selected_index);
        
        // Wrap selection
        if (appstore_ui.selected_index < 0)
            appstore_ui.selected_index = appstore_ui.app_count - 1;
        
        if (appstore_ui.selected_index >= appstore_ui.app_count)
            appstore_ui.selected_index = 0;
    }
}
```

## Step 5: Add App Store Menu Item to Launcher

In your launcher's GUI code, add a menu option to access the app store:

```c
static rg_gui_event_t menu_appstore_cb(rg_gui_option_t *option, rg_gui_event_t event)
{
    if (event == RG_DIALOG_ENTER)
    {
        appstore_run();
        return RG_DIALOG_REDRAW;
    }
    return RG_DIALOG_VOID;
}

// Add to menu options
static rg_gui_option_t menu_options[] = {
    {0, "Browse Apps", "...", 1, &menu_appstore_cb},
    // ... other menu items
    RG_DIALOG_END
};
```

## Step 6: Network Configuration

Ensure your device can reach the app store server. Configure WiFi:

1. Create `/retro-go/config/wifi.json`:
```json
{
  "ssid0": "your-network",
  "password0": "your-password"
}
```

2. Or configure in code:
```c
rg_wifi_config_t wifi_config = {
    .ssid = "your-network",
    .password = "your-password",
    .channel = 0,
    .ap_mode = false
};

rg_network_wifi_set_config(&wifi_config);
rg_network_wifi_start();
```

## Step 7: Build and Test

Build your project with the app store integration:

```bash
idf.py build
idf.py flash
```

Test the integration:
1. Power on your device
2. Connect to WiFi network
3. Navigate to the app store menu
4. Browse and download apps

## Troubleshooting

### Device can't connect to app store server
- Verify WiFi is connected: Check network settings
- Verify server URL is correct
- Check firewall allows outbound HTTP on port 5000
- Ensure server is running: `python run.py`

### Apps don't download
- Check free storage space on device
- Verify file path is writable
- Check server logs for errors

### Ratings/reviews not submitting
- Verify device has WiFi connection
- Check device ID is properly set

## Best Practices

1. **Check connectivity before operations**:
```c
rg_network_t net = rg_network_get_info();
if (net.state != RG_NETWORK_CONNECTED)
{
    RG_LOGE("Not connected to network");
    return false;
}
```

2. **Handle errors gracefully**:
```c
if (!rg_appstore_download_app(app_id, path, progress_cb))
{
    rg_gui_alert("Error", "Failed to download app");
}
```

3. **Show progress for long operations**:
```c
static bool progress_callback(int total, int downloaded)
{
    int percent = (downloaded * 100) / total;
    // Update progress display
    return true;
}

rg_appstore_download_app(app_id, path, progress_callback);
```

## Further Reading

- [App Store API Documentation](../README.md)
- [Device Client API](../client/rg_appstore.h)
- [Retro-Go Component Documentation](../../components/retro-go/README.md)
