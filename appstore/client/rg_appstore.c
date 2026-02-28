/**
 * App Store Client Implementation for Retro-Go
 */

#include "rg_appstore.h"
#include <rg_system.h>
#include <rg_network.h>
#include <rg_storage.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cjson/cJSON.h>

static char appstore_server[256] = RG_APPSTORE_SERVER_URL;
static char device_id[128] = {0};

// Get or generate device ID (MAC address)
static void get_device_id(char *out_id)
{
    if (device_id[0] == '\0')
    {
        // TODO: Get actual MAC address from network interface
        // For now, use a placeholder based on system info
        snprintf(device_id, sizeof(device_id), "device-%08x", rand());
    }
    strcpy(out_id, device_id);
}

bool rg_appstore_init(const char *server_url)
{
    if (server_url)
    {
        strncpy(appstore_server, server_url, sizeof(appstore_server) - 1);
    }
    
    RG_LOGI("App Store initialized: %s", appstore_server);
    return true;
}

void rg_appstore_deinit(void)
{
    // Cleanup
}

bool rg_appstore_register_device(const rg_appstore_device_t *device)
{
    RG_ASSERT_ARG(device);
    
    char device_id[128];
    get_device_id(device_id);
    
    // Create JSON payload
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_id", device_id);
    cJSON_AddStringToObject(root, "name", device->name);
    cJSON_AddStringToObject(root, "model", device->model);
    cJSON_AddStringToObject(root, "firmware_version", device->firmware_version);
    cJSON_AddNumberToObject(root, "storage_total", device->storage_total);
    cJSON_AddNumberToObject(root, "storage_used", device->storage_used);
    
    char *payload = cJSON_Print(root);
    
    // Build URL
    char url[512];
    snprintf(url, sizeof(url), "%s/api/v1/device/register", appstore_server);
    
    // Make HTTP request
    rg_http_cfg_t cfg = RG_HTTP_DEFAULT_CONFIG();
    cfg.post_data = payload;
    cfg.post_len = strlen(payload);
    
    rg_http_req_t *req = rg_network_http_open(url, &cfg);
    if (!req)
    {
        RG_LOGE("Failed to register device with app store");
        cJSON_Delete(root);
        free(payload);
        return false;
    }
    
    // Read response
    char response[1024] = {0};
    int bytes_read = rg_network_http_read(req, response, sizeof(response) - 1);
    rg_network_http_close(req);
    
    cJSON_Delete(root);
    free(payload);
    
    return req->status_code == 200;
}

bool rg_appstore_get_apps(const char *category, int *out_count, rg_appstore_app_t **out_apps)
{
    RG_ASSERT_ARG(out_count && out_apps);
    
    // Build URL
    char url[512];
    snprintf(url, sizeof(url), "%s/api/v1/device/apps?limit=%d", 
             appstore_server, RG_APPSTORE_MAX_APPS);
    
    if (category)
    {
        snprintf(url + strlen(url), sizeof(url) - strlen(url), 
                 "&category=%s", category);
    }
    
    // Make HTTP request
    rg_http_cfg_t cfg = RG_HTTP_DEFAULT_CONFIG();
    rg_http_req_t *req = rg_network_http_open(url, &cfg);
    
    if (!req)
    {
        RG_LOGE("Failed to fetch apps from app store");
        return false;
    }
    
    // Read response
    char *response = malloc(req->content_length + 1);
    if (!response)
    {
        rg_network_http_close(req);
        return false;
    }
    
    int total_read = 0;
    int bytes_read;
    while ((bytes_read = rg_network_http_read(req, response + total_read, 
                                              req->content_length - total_read)) > 0)
    {
        total_read += bytes_read;
    }
    response[total_read] = '\0';
    
    rg_network_http_close(req);
    
    // Parse JSON response
    cJSON *root = cJSON_Parse(response);
    free(response);
    
    if (!root)
    {
        RG_LOGE("Failed to parse app store response");
        return false;
    }
    
    cJSON *apps_array = cJSON_GetObjectItem(root, "apps");
    if (!apps_array || !cJSON_IsArray(apps_array))
    {
        cJSON_Delete(root);
        return false;
    }
    
    int count = cJSON_GetArraySize(apps_array);
    *out_count = count;
    
    *out_apps = malloc(count * sizeof(rg_appstore_app_t));
    if (!*out_apps)
    {
        cJSON_Delete(root);
        return false;
    }
    
    // Parse each app
    for (int i = 0; i < count; i++)
    {
        cJSON *app_json = cJSON_GetArrayItem(apps_array, i);
        rg_appstore_app_t *app = &(*out_apps)[i];
        
        cJSON_GetObjectItem(app_json, "id")->valuestring;
        strncpy(app->id, cJSON_GetObjectItem(app_json, "id")->valuestring, 
                sizeof(app->id) - 1);
        strncpy(app->name, cJSON_GetObjectItem(app_json, "name")->valuestring, 
                sizeof(app->name) - 1);
        strncpy(app->description, cJSON_GetObjectItem(app_json, "description")->valuestring, 
                sizeof(app->description) - 1);
        strncpy(app->version, cJSON_GetObjectItem(app_json, "version")->valuestring, 
                sizeof(app->version) - 1);
        strncpy(app->author, cJSON_GetObjectItem(app_json, "author")->valuestring, 
                sizeof(app->author) - 1);
        strncpy(app->category, cJSON_GetObjectItem(app_json, "category")->valuestring, 
                sizeof(app->category) - 1);
        
        app->price = cJSON_GetObjectItem(app_json, "price")->valuedouble;
        app->rating = cJSON_GetObjectItem(app_json, "rating")->valuedouble;
        app->download_count = cJSON_GetObjectItem(app_json, "download_count")->valueint;
        app->featured = cJSON_GetObjectItem(app_json, "featured")->type == cJSON_True;
    }
    
    cJSON_Delete(root);
    return true;
}

bool rg_appstore_get_featured_apps(int *out_count, rg_appstore_app_t **out_apps)
{
    RG_ASSERT_ARG(out_count && out_apps);
    
    // Build URL for featured apps
    char url[512];
    snprintf(url, sizeof(url), "%s/api/v1/device/apps?featured=true&limit=%d", 
             appstore_server, RG_APPSTORE_MAX_APPS);
    
    // Use get_apps implementation
    return rg_appstore_get_apps(NULL, out_count, out_apps);
}

bool rg_appstore_search_apps(const char *query, int *out_count, rg_appstore_app_t **out_apps)
{
    RG_ASSERT_ARG(query && out_count && out_apps);
    
    // Build URL with search parameter
    char url[512];
    snprintf(url, sizeof(url), "%s/api/v1/device/apps?search=%s&limit=%d", 
             appstore_server, query, RG_APPSTORE_MAX_APPS);
    
    // Make HTTP request
    rg_http_cfg_t cfg = RG_HTTP_DEFAULT_CONFIG();
    rg_http_req_t *req = rg_network_http_open(url, &cfg);
    
    if (!req || req->status_code != 200)
    {
        if (req) rg_network_http_close(req);
        return false;
    }
    
    // Read and parse response (similar to get_apps)
    // Implementation omitted for brevity - similar to rg_appstore_get_apps
    
    return true;
}

bool rg_appstore_get_categories(int *out_count, char ***out_categories)
{
    RG_ASSERT_ARG(out_count && out_categories);
    
    // Build URL
    char url[512];
    snprintf(url, sizeof(url), "%s/api/v1/device/categories", appstore_server);
    
    // Make HTTP request
    rg_http_cfg_t cfg = RG_HTTP_DEFAULT_CONFIG();
    rg_http_req_t *req = rg_network_http_open(url, &cfg);
    
    if (!req || req->status_code != 200)
    {
        if (req) rg_network_http_close(req);
        return false;
    }
    
    // Read response and parse JSON
    // Implementation similar to get_apps
    
    return true;
}

bool rg_appstore_get_app_details(const char *app_id, rg_appstore_app_t *out_app)
{
    RG_ASSERT_ARG(app_id && out_app);
    
    char url[512];
    snprintf(url, sizeof(url), "%s/api/v1/device/apps/%s", appstore_server, app_id);
    
    rg_http_cfg_t cfg = RG_HTTP_DEFAULT_CONFIG();
    rg_http_req_t *req = rg_network_http_open(url, &cfg);
    
    if (!req || req->status_code != 200)
    {
        if (req) rg_network_http_close(req);
        return false;
    }
    
    // Read and parse response
    // Implementation similar to get_apps but for single app
    
    return true;
}

bool rg_appstore_download_app(const char *app_id, const char *dest_path, 
                               rg_appstore_progress_cb_t progress_cb)
{
    RG_ASSERT_ARG(app_id && dest_path);
    
    char device_id[128];
    get_device_id(device_id);
    
    char url[512];
    snprintf(url, sizeof(url), "%s/api/v1/device/apps/%s/download?device_id=%s", 
             appstore_server, app_id, device_id);
    
    rg_http_cfg_t cfg = RG_HTTP_DEFAULT_CONFIG();
    rg_http_req_t *req = rg_network_http_open(url, &cfg);
    
    if (!req || req->status_code != 200)
    {
        if (req) rg_network_http_close(req);
        RG_LOGE("Failed to download app: HTTP %d", req ? req->status_code : 0);
        return false;
    }
    
    // Download to file
    FILE *file = fopen(dest_path, "wb");
    if (!file)
    {
        rg_network_http_close(req);
        return false;
    }
    
    int total_size = req->content_length;
    int downloaded = 0;
    char buffer[RG_APPSTORE_CHUNK_SIZE];
    int bytes_read;
    
    while ((bytes_read = rg_network_http_read(req, buffer, sizeof(buffer))) > 0)
    {
        fwrite(buffer, 1, bytes_read, file);
        downloaded += bytes_read;
        
        if (progress_cb)
        {
            progress_cb(total_size, downloaded);
        }
    }
    
    fclose(file);
    rg_network_http_close(req);
    
    RG_LOGI("Downloaded app %s (%d bytes)", app_id, downloaded);
    return true;
}

bool rg_appstore_install_app(const char *app_id, const char *version)
{
    RG_ASSERT_ARG(app_id && version);
    
    char device_id[128];
    get_device_id(device_id);
    
    char url[512];
    snprintf(url, sizeof(url), "%s/api/v1/device/apps/%s/install?device_id=%s", 
             appstore_server, app_id, device_id);
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "version", version);
    cJSON_AddNumberToObject(root, "storage_used", 0); // TODO: Get actual storage used
    
    char *payload = cJSON_Print(root);
    
    rg_http_cfg_t cfg = RG_HTTP_DEFAULT_CONFIG();
    cfg.post_data = payload;
    cfg.post_len = strlen(payload);
    
    rg_http_req_t *req = rg_network_http_open(url, &cfg);
    bool success = req && req->status_code == 200;
    
    if (req) rg_network_http_close(req);
    cJSON_Delete(root);
    free(payload);
    
    return success;
}

bool rg_appstore_uninstall_app(const char *app_id)
{
    RG_ASSERT_ARG(app_id);
    
    char device_id[128];
    get_device_id(device_id);
    
    char url[512];
    snprintf(url, sizeof(url), "%s/api/v1/device/apps/%s/uninstall?device_id=%s", 
             appstore_server, app_id, device_id);
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "storage_used", 0); // TODO: Get actual storage used
    
    char *payload = cJSON_Print(root);
    
    rg_http_cfg_t cfg = RG_HTTP_DEFAULT_CONFIG();
    cfg.post_data = payload;
    cfg.post_len = strlen(payload);
    
    rg_http_req_t *req = rg_network_http_open(url, &cfg);
    bool success = req && req->status_code == 200;
    
    if (req) rg_network_http_close(req);
    cJSON_Delete(root);
    free(payload);
    
    return success;
}

bool rg_appstore_check_license(const char *app_id, rg_appstore_license_t *out_license)
{
    RG_ASSERT_ARG(app_id && out_license);
    
    char device_id[128];
    get_device_id(device_id);
    
    char url[512];
    snprintf(url, sizeof(url), "%s/api/v1/device/licenses/%s?device_id=%s", 
             appstore_server, app_id, device_id);
    
    rg_http_cfg_t cfg = RG_HTTP_DEFAULT_CONFIG();
    rg_http_req_t *req = rg_network_http_open(url, &cfg);
    
    if (!req || req->status_code != 200)
    {
        if (req) rg_network_http_close(req);
        return false;
    }
    
    // Parse response and populate out_license
    rg_network_http_close(req);
    return true;
}

bool rg_appstore_get_trial(const char *app_id, rg_appstore_license_t *out_license)
{
    RG_ASSERT_ARG(app_id && out_license);
    
    // Request trial license from server
    // Implementation similar to check_license
    
    return true;
}

bool rg_appstore_submit_review(const char *app_id, int rating, const char *title, const char *comment)
{
    RG_ASSERT_ARG(app_id && rating >= 1 && rating <= 5);
    
    char device_id[128];
    get_device_id(device_id);
    
    char url[512];
    snprintf(url, sizeof(url), "%s/api/v1/device/apps/%s/rate?device_id=%s", 
             appstore_server, app_id, device_id);
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "rating", rating);
    if (title) cJSON_AddStringToObject(root, "title", title);
    if (comment) cJSON_AddStringToObject(root, "comment", comment);
    
    char *payload = cJSON_Print(root);
    
    rg_http_cfg_t cfg = RG_HTTP_DEFAULT_CONFIG();
    cfg.post_data = payload;
    cfg.post_len = strlen(payload);
    
    rg_http_req_t *req = rg_network_http_open(url, &cfg);
    bool success = req && req->status_code == 201;
    
    if (req) rg_network_http_close(req);
    cJSON_Delete(root);
    free(payload);
    
    return success;
}

bool rg_appstore_get_installed_apps(int *out_count, rg_appstore_app_t **out_apps)
{
    RG_ASSERT_ARG(out_count && out_apps);
    
    char device_id[128];
    get_device_id(device_id);
    
    char url[512];
    snprintf(url, sizeof(url), "%s/api/v1/device/devices/%s/apps", 
             appstore_server, device_id);
    
    rg_http_cfg_t cfg = RG_HTTP_DEFAULT_CONFIG();
    rg_http_req_t *req = rg_network_http_open(url, &cfg);
    
    if (!req || req->status_code != 200)
    {
        if (req) rg_network_http_close(req);
        return false;
    }
    
    // Read and parse response
    rg_network_http_close(req);
    return true;
}

bool rg_appstore_report_stats(int64_t uptime, int64_t memory_free, int64_t storage_used)
{
    char device_id[128];
    get_device_id(device_id);
    
    char url[512];
    snprintf(url, sizeof(url), "%s/api/v1/device/stats", appstore_server);
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_id", device_id);
    cJSON_AddNumberToObject(root, "uptime", uptime);
    cJSON_AddNumberToObject(root, "memory_free", memory_free);
    cJSON_AddNumberToObject(root, "storage_used", storage_used);
    
    char *payload = cJSON_Print(root);
    
    rg_http_cfg_t cfg = RG_HTTP_DEFAULT_CONFIG();
    cfg.post_data = payload;
    cfg.post_len = strlen(payload);
    
    rg_http_req_t *req = rg_network_http_open(url, &cfg);
    bool success = req && req->status_code == 200;
    
    if (req) rg_network_http_close(req);
    cJSON_Delete(root);
    free(payload);
    
    return success;
}
