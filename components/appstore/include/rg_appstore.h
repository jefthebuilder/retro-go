/**
 * App Store Client for Retro-Go
 * 
 * Provides functionality to interact with the Retro-Go App Store server
 * for downloading, installing, and managing apps on retro gaming devices.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Configuration
#define RG_APPSTORE_SERVER_URL  "http://192.168.1.236:5000"  // Configure this
#define RG_APPSTORE_TIMEOUT_MS  30000                          // 30 second timeout
#define RG_APPSTORE_MAX_APPS    100                            // Max apps to load
#define RG_APPSTORE_CHUNK_SIZE  4096                           // Download chunk size

typedef struct {
    char id[37];                // UUID
    char name[128];
    char description[256];
    char version[32];
    char author[128];
    char category[64];
    char extensions[128];       // Supported file extensions (space-separated)
    char icon_url[256];
    float price;
    char currency[3];
    float rating;
    int download_count;
    int file_size;
    char file_hash[64];
    bool featured;
} rg_appstore_app_t;

typedef struct {
    char device_id[128];
    char name[128];
    char model[64];
    char firmware_version[32];
    int64_t storage_total;
    int64_t storage_used;
} rg_appstore_device_t;

typedef struct {
    char id[37];
    char app_id[37];
    char device_id[128];
    char license_key[256];
    bool is_valid;
    bool is_trial;
    bool is_free;
} rg_appstore_license_t;

// Initialize app store client
bool rg_appstore_init(const char *server_url);

// Cleanup
void rg_appstore_deinit(void);

// Device registration
bool rg_appstore_register_device(const rg_appstore_device_t *device);

// App listing and discovery
bool rg_appstore_get_apps(const char *category, int *out_count, rg_appstore_app_t **out_apps);
bool rg_appstore_get_featured_apps(int *out_count, rg_appstore_app_t **out_apps);
bool rg_appstore_search_apps(const char *query, int *out_count, rg_appstore_app_t **out_apps);
bool rg_appstore_get_categories(int *out_count, char ***out_categories);

// App details
bool rg_appstore_get_app_details(const char *app_id, rg_appstore_app_t *out_app);

// Download and installation
typedef bool (*rg_appstore_progress_cb_t)(int total_bytes, int downloaded_bytes);
bool rg_appstore_download_app(const char *app_id, const char *dest_path, rg_appstore_progress_cb_t progress_cb);
bool rg_appstore_install_app(const char *app_id, const char *version);
bool rg_appstore_uninstall_app(const char *app_id);

// Licensing
bool rg_appstore_check_license(const char *app_id, rg_appstore_license_t *out_license);
bool rg_appstore_get_trial(const char *app_id, rg_appstore_license_t *out_license);

// Reviews and ratings
bool rg_appstore_submit_review(const char *app_id, int rating, const char *title, const char *comment);

// Device inventory
bool rg_appstore_get_installed_apps(int *out_count, rg_appstore_app_t **out_apps);

// Updates
typedef struct {
    char app_id[37];
    char app_name[128];
    char current_version[32];
    char new_version[32];
    bool update_available;
} rg_appstore_update_t;

bool rg_appstore_check_updates(int *out_count, rg_appstore_update_t **out_updates);

// Stats reporting
bool rg_appstore_report_stats(int64_t uptime, int64_t memory_free, int64_t storage_used);
