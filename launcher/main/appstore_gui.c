/**
 * App Store UI Module for Retro-Go Launcher
 * Simplified integration with app store functionality
 */

#include <rg_system.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "gui.h"
#include "appstore_gui.h"
#include "rg_appstore.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_app_format.h"

// App store state
static rg_appstore_app_t *apps = NULL;
static int app_count = 0;
static bool initialized = false;

// Partition tracking for installed apps
// Map: app_id -> partition info
static struct {
    char app_id[37];
    char app_name[64];
    char app_type[32];
    char extensions[128];
    uint32_t partition_addr;
} app_partitions[3];
static int app_partition_count = 0;

// Initialize app store
void appstore_gui_init(void)
{
    if (initialized) return;
    
    // Initialize the app store library
    if (rg_appstore_init("http://192.168.1.236:5000"))
    {
        initialized = true;
        RG_LOGI("App Store initialized");
    }
}

// Cleanup
void appstore_gui_deinit(void)
{
    if (apps) free(apps);
    apps = NULL;
    app_count = 0;
    rg_appstore_deinit();
    initialized = false;
}

// Load apps from server
static bool load_apps(void)
{
    if (apps) free(apps);
    apps = NULL;
    app_count = 0;
    
    if (!rg_appstore_get_apps(NULL, &app_count, &apps))
    {
        RG_LOGE("Failed to load apps from server");
        return false;
    }
    
    // Validate loaded apps
    for (int i = 0; i < app_count; i++)
    {
        if (!apps[i].name[0])
            strncpy(apps[i].name, "Unknown App", sizeof(apps[i].name) - 1);
        if (!apps[i].author[0])
            strncpy(apps[i].author, "Unknown Author", sizeof(apps[i].author) - 1);
        if (!apps[i].version[0])
            strncpy(apps[i].version, "1.0", sizeof(apps[i].version) - 1);
        if (!apps[i].description[0])
            strncpy(apps[i].description, "No description available", sizeof(apps[i].description) - 1);
    }
    
    RG_LOGI("Loaded %d apps from server", app_count);
    return true;
}

// Progress callback for downloads
static bool download_progress(int total_bytes, int downloaded_bytes)
{
    int percent = (total_bytes > 0) ? (downloaded_bytes * 100) / total_bytes : 0;
    char msg[64];
    snprintf(msg, sizeof(msg), "Downloading... %d%%", percent);
    rg_gui_draw_message(msg);
    return true;
}

// Find or allocate an OTA partition for the app
// Find an existing partition already holding this app (by metadata name)
static const esp_partition_t* find_existing_partition(const char *app_name)
{
    for (int slot = 1; slot <= 3; slot++)
    {
        char partition_name[32];
        snprintf(partition_name, sizeof(partition_name), "app_slot_%d", slot);

        char metadata_path[256];
        snprintf(metadata_path, sizeof(metadata_path),
                 RG_STORAGE_ROOT "/odroid/appstore/metadata/%s.txt", partition_name);

        FILE *fp = fopen(metadata_path, "r");
        if (!fp) continue;

        char line[256];
        bool match = false;
        while (fgets(line, sizeof(line), fp))
        {
            char *newline = strchr(line, '\n');
            if (newline) *newline = '\0';
            if (strncmp(line, "name=", 5) == 0 && strcmp(line + 5, app_name) == 0)
            {
                match = true;
                break;
            }
        }
        fclose(fp);

        if (match)
        {
            uint32_t ota_subtype = ESP_PARTITION_SUBTYPE_APP_OTA_0 + slot;
            const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ota_subtype, NULL);
            if (p) return p;
            p = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, partition_name);
            if (p) return p;
        }
    }
    return NULL;
}

// Find or allocate an OTA partition for the app
static const esp_partition_t* find_app_partition(const char *app_name)
{
    RG_LOGI("Looking for available app slots...");

    // Reuse existing partition that already has this app
    const esp_partition_t *existing = find_existing_partition(app_name);
    if (existing)
    {
        RG_LOGI("Reusing existing partition %s at 0x%x", existing->label, existing->address);
        return existing;
    }
    
    // Scan app_slot_1 through app_slot_3 for first empty slot
    for (int slot = 1; slot <= 3; slot++)
    {
        char partition_name[32];
        snprintf(partition_name, sizeof(partition_name), "app_slot_%d", slot);
        
        // Try finding by subtype first
        uint32_t ota_subtype = ESP_PARTITION_SUBTYPE_APP_OTA_0 + slot;
        const esp_partition_t *partition = esp_partition_find_first(
            ESP_PARTITION_TYPE_APP, ota_subtype, NULL);
        
        // If not found by subtype, try finding by name
        if (!partition)
        {
            partition = esp_partition_find_first(
                ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, partition_name);
            if (partition)
                RG_LOGI("Found partition %s by name (subtype: 0x%x)", partition_name, partition->subtype);
        }
        else
        {
            RG_LOGI("Found partition %s by subtype 0x%x at 0x%x", partition_name, ota_subtype, partition->address);
        }
        
        if (partition)
        {
            // Check if partition is empty by looking for ESP app header magic (0xe9)
            // Erased flash reads as 0xFF, valid app header starts with 0xe9
            uint8_t first_byte = 0;
            if (esp_partition_read(partition, 0, &first_byte, 1) == ESP_OK)
            {
                // Slot is empty if:
                // - First byte is 0xFF (erased), OR
                // - First byte is NOT 0xe9 (not a valid ESP app header)
                // A valid app has 0xe9 as the magic byte for ESP image header
                if (first_byte == 0xFF || first_byte != 0xe9)
                {
                    RG_LOGI("Found empty app slot %d at 0x%x (size: %d bytes, first_byte: 0x%02x)", 
                            slot, partition->address, partition->size, first_byte);
                    return partition;
                }
                else
                {
                    RG_LOGI("App slot %d appears occupied (magic: 0x%02x)", slot, first_byte);
                }
            }
            else
            {
                RG_LOGW("Failed to read partition %d at 0x%x", slot, partition->address);
            }
        }
        else
        {
            RG_LOGW("Partition %s not found (tried subtype 0x%x)", partition_name, ota_subtype);
        }
    }
    
    RG_LOGE("No available app slots found. Max 3 apps supported.");
    return NULL;
}

// Register app to partition mapping
static void register_app_partition(const char *app_id, const char *app_name, const char *app_category, 
                                   const char *extensions, const esp_partition_t *partition)
{
    for (int i = 0; i < app_partition_count; i++)
    {
        if (strcmp(app_partitions[i].app_id, app_id) == 0)
        {
            // Update existing
            app_partitions[i].partition_addr = partition->address;
            strncpy(app_partitions[i].app_name, app_name, sizeof(app_partitions[i].app_name) - 1);
            strncpy(app_partitions[i].app_type, app_category, sizeof(app_partitions[i].app_type) - 1);
            strncpy(app_partitions[i].extensions, extensions, sizeof(app_partitions[i].extensions) - 1);
            RG_LOGI("Updated app %s (%s) to partition 0x%x", app_name, app_category, partition->address);
            return;
        }
    }
    
    // Add new mapping
    if (app_partition_count < 3)
    {
        strncpy(app_partitions[app_partition_count].app_id, app_id, sizeof(app_partitions[0].app_id) - 1);
        strncpy(app_partitions[app_partition_count].app_name, app_name, sizeof(app_partitions[0].app_name) - 1);
        strncpy(app_partitions[app_partition_count].app_type, app_category, sizeof(app_partitions[0].app_type) - 1);
        strncpy(app_partitions[app_partition_count].extensions, extensions, sizeof(app_partitions[0].extensions) - 1);
        app_partitions[app_partition_count].partition_addr = partition->address;
        app_partition_count++;
        RG_LOGI("Registered app %s (%s) to partition 0x%x", app_name, app_category, partition->address);
    }
}

// Unregister app from partition
static void unregister_app_partition(const char *app_id)
{
    for (int i = 0; i < app_partition_count; i++)
    {
        if (strcmp(app_partitions[i].app_id, app_id) == 0)
        {
            // Shift remaining entries
            for (int j = i; j < app_partition_count - 1; j++)
            {
                app_partitions[j] = app_partitions[j + 1];
            }
            app_partition_count--;
            RG_LOGI("Unregistered app %s", app_id);
            return;
        }
    }
}

// Progress callback for OTA writes
typedef struct {
    esp_ota_handle_t handle;
    const esp_partition_t *partition;
    int total_bytes;
    int written_bytes;
} ota_context_t;

static bool ota_write_progress(int total_bytes, int downloaded_bytes)
{
    int percent = (total_bytes > 0) ? (downloaded_bytes * 100) / total_bytes : 0;
    char msg[64];
    snprintf(msg, sizeof(msg), "Installing... %d%%", percent);
    rg_gui_draw_message(msg);
    return true;
}

// Download and install app directly to OTA partition
static bool download_app(int index, bool defer_reboot)
{
    if (index < 0 || index >= app_count)
        return false;
    
    rg_appstore_app_t *app = &apps[index];
    
    // Find or allocate partition for this app
    const esp_partition_t *partition = find_app_partition(app->name);
    if (!partition)
    {
        rg_gui_alert("Error", "No more app slots available.\nMax 3 apps supported.\nDelete an app to install another.");
        return false;
    }
    
    // Check if partition is large enough for the app
    if (partition->size < app->file_size)
    {
        char msg[256];
        snprintf(msg, sizeof(msg), "App too large!\\nApp size: %.1f MB\\nPartition: %.1f MB",
                 app->file_size / (1024.0 * 1024.0),
                 partition->size / (1024.0 * 1024.0));
        rg_gui_alert("Error", msg);
        return false;
    }
    
    RG_LOGI("Installing %s to partition %s (0x%x)", app->name, partition->label, partition->address);
    rg_gui_draw_message("Preparing installation...");
    
    // Erase partition before writing (important for clean install)
    RG_LOGI("Erasing partition 0x%x", partition->address);
    esp_err_t err = esp_partition_erase_range(partition, 0, partition->size);
    if (err != ESP_OK)
    {
        rg_gui_alert("Error", "Failed to erase partition.");
        return false;
    }
    
    // Begin OTA update
    esp_ota_handle_t ota_handle;
    err = esp_ota_begin(partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK)
    {
        rg_gui_alert("Error", "Failed to begin OTA update.");
        return false;
    }
    
    // Download to temporary file first
    char temp_path[256];
    snprintf(temp_path, sizeof(temp_path), RG_STORAGE_ROOT "/odroid/appstore/%s.tmp", app->name);
    
    // Ensure directory exists
    rg_storage_mkdir(RG_STORAGE_ROOT "/odroid/appstore");
    
    if (!rg_appstore_download_app(app->id, temp_path, download_progress))
    {
        esp_ota_abort(ota_handle);
        remove(temp_path);
        rg_gui_alert("Error", "Download failed.\nCheck network connection.");
        return false;
    }
    
    // Read file and write to partition
    FILE *fp = fopen(temp_path, "rb");
    if (!fp)
    {
        esp_ota_abort(ota_handle);
        remove(temp_path);
        rg_gui_alert("Error", "Failed to open downloaded file.");
        return false;
    }
    
    rg_gui_draw_message("Writing to partition...");
    
    char *buffer = malloc(4096);
    if (!buffer)
    {
        fclose(fp);
        esp_ota_abort(ota_handle);
        remove(temp_path);
        rg_gui_alert("Error", "Out of memory.");
        return false;
    }
    
    size_t bytes_read;
    int total_written = 0;
    bool write_failed = false;
    
    while ((bytes_read = fread(buffer, 1, 4096, fp)) > 0)
    {
        err = esp_ota_write(ota_handle, buffer, bytes_read);
        if (err != ESP_OK)
        {
            write_failed = true;
            break;
        }
        total_written += bytes_read;
        ota_write_progress(app->file_size, total_written);
    }
    
    free(buffer);
    fclose(fp);
    remove(temp_path);
    
    if (write_failed)
    {
        esp_ota_abort(ota_handle);
        rg_gui_alert("Error", "Failed to write to partition.");
        return false;
    }
    
    // Finalize OTA
    err = esp_ota_end(ota_handle);
    if (err != ESP_OK)
    {
        rg_gui_alert("Error", "Failed to finalize OTA.");
        return false;
    }
    
    // Save metadata file for dynamic loading at startup
    char metadata_path[256];
    snprintf(metadata_path, sizeof(metadata_path), 
             RG_STORAGE_ROOT "/odroid/appstore/metadata/%s.txt", partition->label);
    
    rg_storage_mkdir(RG_STORAGE_ROOT "/odroid/appstore/metadata");
    
    FILE *meta_fp = fopen(metadata_path, "w");
    if (meta_fp)
    {
        fprintf(meta_fp, "name=%s\n", app->name);   
        fprintf(meta_fp, "type=%s\n", app->category);
        fprintf(meta_fp, "extensions=%s\n", app->extensions);
        fprintf(meta_fp, "version=%s\n", app->version);
        fprintf(meta_fp, "author=%s\n", app->author);
        fprintf(meta_fp, "partition=%s\n", partition->label);
        fclose(meta_fp);
        RG_LOGI("Saved metadata to %s", metadata_path);
    }
    
    // Register app to partition mapping
    register_app_partition(app->id, app->name, app->category, app->extensions, partition);
    
    // Set as boot partition
    err = esp_ota_set_boot_partition(partition);
    if (err != ESP_OK)
    {
        RG_LOGW("Failed to set boot partition, but app is installed");
    }
    
    // Record installation
    rg_appstore_install_app(app->id, app->version);
    
    // Show success
    char msg[256];
    snprintf(msg, sizeof(msg), "%s installed successfully!\n\nReboot to launcher?", app->name);
    if (!defer_reboot && rg_gui_confirm("Success", msg, true))
    {
        // Set launcher as boot partition before reboot
        const esp_partition_t *launcher = esp_partition_find_first(
            ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
        if (launcher)
        {
            esp_ota_set_boot_partition(launcher);
        }
        rg_system_restart();
    }

    if (defer_reboot)
    {
        rg_gui_alert("Installed", msg);
    }

    return true;
}

// Uninstall/delete an app and erase its partition
static void uninstall_app(int index)
{
    if (index < 0 || index >= app_count)
        return;
    
    rg_appstore_app_t *app = &apps[index];
    
    // Find which partition has this app by checking metadata files
    const esp_partition_t *partition = NULL;
    char found_partition_label[32] = {0};
    
    for (int slot = 1; slot <= 3; slot++)
    {
        char partition_name[32];
        snprintf(partition_name, sizeof(partition_name), "app_slot_%d", slot);
        
        char metadata_path[256];
        snprintf(metadata_path, sizeof(metadata_path), 
                 RG_STORAGE_ROOT "/odroid/appstore/metadata/%s.txt", partition_name);
        
        FILE *fp = fopen(metadata_path, "r");
        if (fp)
        {
            char line[256];
            bool found = false;
            while (fgets(line, sizeof(line), fp))
            {
                char *newline = strchr(line, '\n');
                if (newline) *newline = '\0';
                
                if (strncmp(line, "name=", 5) == 0)
                {
                    if (strcmp(line + 5, app->name) == 0)
                    {
                        found = true;
                        strncpy(found_partition_label, partition_name, sizeof(found_partition_label) - 1);
                        break;
                    }
                }
            }
            fclose(fp);
            
            if (found)
            {
                // Find the actual partition
                uint32_t ota_subtype = ESP_PARTITION_SUBTYPE_APP_OTA_0 + slot;
                partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ota_subtype, NULL);
                if (!partition)
                {
                    partition = esp_partition_find_first(
                        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, partition_name);
                }
                break;
            }
        }
    }
    
    if (!partition)
    {
        rg_gui_alert("Error", "App partition not found.");
        return;
    }
    
    if (!rg_gui_confirm("Delete App", "Remove this app and free space?", false))
        return;
    
    rg_gui_draw_message("Erasing partition...");
    
    // Erase the partition
    esp_err_t err = esp_partition_erase_range(partition, 0, partition->size);
    if (err != ESP_OK)
    {
        rg_gui_alert("Error", "Failed to erase partition.");
        return;
    }
    
    // Delete metadata file
    char metadata_path[256];
    snprintf(metadata_path, sizeof(metadata_path), 
             RG_STORAGE_ROOT "/odroid/appstore/metadata/%s.txt", found_partition_label);
    remove(metadata_path);
    RG_LOGI("Deleted metadata file: %s", metadata_path);
    
    // Unregister from tracking
    unregister_app_partition(app->id);
    
    // Report uninstall to server
    rg_appstore_uninstall_app(app->id);
    
    rg_gui_alert("Success", "App deleted and space freed.");
}

// Check for app updates
static void check_updates(void)
{
    rg_gui_draw_message("Checking for updates...");

    // Load current apps from server
    if (!load_apps())
    {
        rg_gui_alert("Updates", "Failed to check for updates.\nCheck network connection.");
        return;
    }

    // Track updates by server index
    int update_indices[16];
    int update_count = 0;
    char update_list[512] = {0};

    for (int slot = 1; slot <= 3; slot++)
    {
        char partition_name[32];
        snprintf(partition_name, sizeof(partition_name), "app_slot_%d", slot);

        char metadata_path[256];
        snprintf(metadata_path, sizeof(metadata_path),
                 RG_STORAGE_ROOT "/odroid/appstore/metadata/%s.txt", partition_name);

        FILE *fp = fopen(metadata_path, "r");
        if (!fp) continue;

        char installed_name[128] = {0};
        char installed_version[32] = {0};
        char line[256];

        while (fgets(line, sizeof(line), fp))
        {
            char *newline = strchr(line, '\n');
            if (newline) *newline = '\0';

            if (strncmp(line, "name=", 5) == 0)
                strncpy(installed_name, line + 5, sizeof(installed_name) - 1);
            else if (strncmp(line, "version=", 8) == 0)
                strncpy(installed_version, line + 8, sizeof(installed_version) - 1);
        }
        fclose(fp);

        if (!installed_name[0])
            continue;

        // Find matching app on server by name
        for (int i = 0; i < app_count; i++)
        {
            if (strcmp(apps[i].name, installed_name) == 0)
            {
                // Version compare (simple lexicographic; keep versions consistent like 1.2.3)
                if (strcmp(apps[i].version, installed_version) > 0)
                {
                    if (update_count < (int)(sizeof(update_indices)/sizeof(update_indices[0])))
                        update_indices[update_count++] = i;

                    char update_info[128];
                    snprintf(update_info, sizeof(update_info), "%s: %s -> %s\n",
                             installed_name, installed_version, apps[i].version);
                    strncat(update_list, update_info, sizeof(update_list) - strlen(update_list) - 1);
                }
                break;
            }
        }
    }

    if (update_count == 0)
    {
        rg_gui_alert("Updates", "All apps are up to date!");
        return;
    }

    char msg[768];
    snprintf(msg, sizeof(msg), "Updates available:\n\n%s\nInstall now?", update_list);

    if (!rg_gui_confirm("Updates Available", msg, true))
        return;

    // Install updates in place (find_app_partition will reuse existing partition for same app)
    bool installed_any = false;
    for (int u = 0; u < update_count; u++)
    {
        if (download_app(update_indices[u], true))
            installed_any = true;
    }

    if (installed_any)
    {
        if (rg_gui_confirm("Updates Installed", "All updates installed. Reboot now?", true))
        {
            const esp_partition_t *launcher = esp_partition_find_first(
                ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
            if (launcher)
            {
                esp_ota_set_boot_partition(launcher);
            }
            rg_system_restart();
        }
    }
}

// Show app store main menu
static rg_gui_event_t appstore_main_menu(void)
{
    rg_gui_option_t menu[] = {
        {1, "Browse Apps", "", 1, NULL},
        {2, "Check for Updates", "", 1, NULL},
        {3, "Back", "", 1, NULL},
        RG_DIALOG_END,
    };
    
    int choice = rg_gui_dialog("App Store", menu, 0);
    
    if (choice == 1)
        return 1; // Browse apps
    else if (choice == 2)
    {
        check_updates();
        return 0; // Stay in menu
    }
    
    return -1; // Back
}

// Show app store
static rg_gui_event_t appstore_show(rg_gui_option_t *option, rg_gui_event_t event)
{
    if (event != RG_DIALOG_ENTER)
        return RG_DIALOG_VOID;
    
    // Show main menu
    int menu_result = appstore_main_menu();
    if (menu_result != 1)
        return RG_DIALOG_VOID;
    
    // Load apps
    if (!load_apps())
    {
        rg_gui_alert("App Store", "Failed to load apps from server.\nMake sure the server is running.");
        return RG_DIALOG_VOID;
    }
    
    if (app_count == 0)
    {
        rg_gui_alert("App Store", "No apps available on the server.");
        return RG_DIALOG_VOID;
    }
    
    // Build app list dialog options
    rg_gui_option_t *options = malloc((app_count + 1) * sizeof(rg_gui_option_t));
    if (!options)
    {
        rg_gui_alert("App Store", "Memory error - cannot allocate options");
        return RG_DIALOG_VOID;
    }
    
    // Create option for each app
    for (int i = 0; i < app_count; i++)
    {
        options[i].arg = i;
        options[i].label = apps[i].name;
        options[i].value = "";
        options[i].flags = 1;
        options[i].update_cb = NULL;
    }
    options[app_count] = (rg_gui_option_t)RG_DIALOG_END;
    
    // Show app list
    int selected = rg_gui_dialog("App Store", options, 0);
    
    // Show details and download option if user selected an app
    if (selected >= 0 && selected < app_count)
    {
        rg_appstore_app_t *app = &apps[selected];
        
        // Build details message using dynamic buffer to avoid truncation warnings
        const size_t message_len = snprintf(NULL, 0,
            "%s\n"
            "v%s by %s\n\n"
            "Rating: %.1f★  Downloads: %d\n"
            "Size: %.1f MB\n\n"
            "%s",
            app->name,
            app->version,
            app->author,
            app->rating,
            app->download_count,
            app->file_size / (1024.0 * 1024.0),
            app->description);

        char *message = malloc(message_len + 1);
        if (!message)
        {
            rg_gui_alert("App Store", "Memory error - cannot allocate message buffer");
            free(options);
            return RG_DIALOG_VOID;
        }

        snprintf(message, message_len + 1,
            "%s\n"
            "v%s by %s\n\n"
            "Rating: %.1f★  Downloads: %d\n"
            "Size: %.1f MB\n\n"
            "%s",
            app->name,
            app->version,
            app->author,
            app->rating,
            app->download_count,
            app->file_size / (1024.0 * 1024.0),
            app->description);
        
        // Check if app is already installed by checking for metadata file
        bool is_installed = false;
        for (int slot = 1; slot <= 3; slot++)
        {
            char partition_name[32];
            snprintf(partition_name, sizeof(partition_name), "app_slot_%d", slot);
            char metadata_path[256];
            snprintf(metadata_path, sizeof(metadata_path), 
                     RG_STORAGE_ROOT "/odroid/appstore/metadata/%s.txt", partition_name);
            FILE *fp = fopen(metadata_path, "r");
            if (fp)
            {
                char line[256];
                while (fgets(line, sizeof(line), fp))
                {
                    // Remove newline
                    char *newline = strchr(line, '\n');
                    if (newline) *newline = '\0';
                    
                    // Check if line starts with "name=" and matches app name
                    if (strncmp(line, "name=", 5) == 0)
                    {
                        if (strcmp(line + 5, app->name) == 0)
                        {
                            is_installed = true;
                            break;
                        }
                    }
                }
                fclose(fp);
                if (is_installed) break;
            }
        }
        
        // Show action menu with install or uninstall option
        rg_gui_option_t actions[] = {
            {1, is_installed ? "Uninstall" : "Download & Install", "", 1, NULL},
            {2, "Back", "", 1, NULL},
            RG_DIALOG_END,
        };
        
        // Show details
        rg_gui_alert("App Details", message);
        
        // Show action menu
        int action = rg_gui_dialog("Actions", actions, 1);
        
        if (action == 1)
        {
            if (is_installed)
            {
                // Uninstall the app
                uninstall_app(selected);
            }
            else
            {
                // Check if app is free or needs license
                if (apps[selected].price > 0)
                {
                    rg_gui_alert("Notice", "This is a paid app.\nPayment not yet implemented.");
                }
                else
                {
                    download_app(selected, false);
                }
            }
        }

        free(message);
    }
    
    free(options);
    return RG_DIALOG_VOID;
}

// Get menu option for launcher
rg_gui_option_t appstore_get_menu_option(void)
{
    return (rg_gui_option_t){
        .arg = 0,
        .label = "App Store",
        .value = "",
        .flags = 1,
        .update_cb = appstore_show,
    };
}
