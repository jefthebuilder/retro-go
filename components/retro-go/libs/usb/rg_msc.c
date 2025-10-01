/* DESCRIPTION:
 * This example contains code to make ESP32-S3 based device recognizable by USB-hosts as a USB Mass Storage Device.
 * It either allows the embedded application i.e. example to access the partition or Host PC accesses the partition over USB MSC.
 * They can't be allowed to access the partition at the same time.
 * For different scenarios and behaviour, Refer to README of this example.
 */

#include "rg_system.h"

#include <errno.h>
#include <dirent.h>
#include <class/msc/msc_device.h>
#include "tinyusb.h"
#include "tusb_msc_storage.h"
#include "rg_msc.h"
/* TinyUSB descriptors
   ********************************************************************* */


#define EPNUM_MSC       1
#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN)

enum {
    ITF_NUM_MSC = 0,
    ITF_NUM_TOTAL
};

enum {
    EDPT_CTRL_OUT = 0x00,
    EDPT_CTRL_IN  = 0x80,

    EDPT_MSC_OUT  = 0x01,
    EDPT_MSC_IN   = 0x81,
};

static uint8_t const desc_configuration[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // Interface number, string index, EP Out & EP In address, EP size
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 0, EDPT_MSC_OUT, EDPT_MSC_IN, TUD_OPT_HIGH_SPEED ? 512 : 64),
};

static tusb_desc_device_t descriptor_config = {
    .bLength = sizeof(descriptor_config),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x303A, // This is Espressif VID. This needs to be changed according to Users / Customers
    .idProduct = 0x4002,
    .bcdDevice = 0x100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01
};

static char const *string_desc_arr[] = {
    (const char[]) { 0x09, 0x04 },  // 0: is supported language is English (0x0409)
    "TinyUSB",                      // 1: Manufacturer
    "TinyUSB Device",               // 2: Product
    "123456",                       // 3: Serials
    "Example MSC",                  // 4. MSC
};

#define BASE_PATH "/" // base path to mount the partition

// callback that is delivered when storage is mounted/unmounted by application.
static void storage_mount_changed_cb(tinyusb_msc_event_t *event)
{
    RG_LOGI("Storage mounted to application: %s", event->mount_changed_data.is_mounted ? "Yes" : "No");
}

// mount the partition and show all the files in BASE_PATH
static void _mount(void)
{
    RG_LOGI("Mount storage...");
    esp_err_t err = tinyusb_msc_storage_mount(BASE_PATH);
    if (err != ESP_OK) {
        RG_LOGE("Storage mounting failed. err=0x%x", err);
        return;
    }

    // // List all the files in this directory
    // RG_LOGI("\nls command output:");
    // struct dirent *d;
    // DIR *dh = opendir(BASE_PATH);
    // if (!dh) {
    //     if (errno == ENOENT) {
    //         //If the directory is not found
    //         RG_LOGE("Directory doesn't exist %s", BASE_PATH);
    //     } else {
    //         //If the directory is not readable then throw error and exit
    //         RG_LOGE("Unable to read directory %s", BASE_PATH);
    //     }
    //     return;
    // }
    // //While the next entry is not readable we will print directory files
    // while ((d = readdir(dh)) != NULL) {
    //     printf("%s\n", d->d_name);
    // }
    return;
}

// unmount storage
static int _unmount(int argc, char **argv)
{
    // if (tinyusb_msc_storage_in_use_by_usb_host()) {
    //     RG_LOGE("storage is already exposed");
    //     return -1;
    // }
    RG_LOGI("Unmount storage...");
    esp_err_t err = tinyusb_msc_storage_unmount();
    if (err != ESP_OK) {
        RG_LOGE("Storage unmounting failed. err=0x%x", err);
        return -1;
    }
    return 0;
}

int link_usb_msc(sdmmc_card_t *card)
{
    RG_LOGI("Initializing storage...");

    const tinyusb_msc_sdmmc_config_t config_sdmmc = {
        .card = card,
        .callback_mount_changed = storage_mount_changed_cb,
    };
    esp_err_t err = tinyusb_msc_storage_init_sdmmc(&config_sdmmc);
    if(err != ESP_OK) {
        RG_LOGE("Failed to initialize storage: %d", err);
        return -1;
    }
    err = tinyusb_msc_register_callback(TINYUSB_MSC_EVENT_MOUNT_CHANGED, storage_mount_changed_cb);
    if(err != ESP_OK) {
        RG_LOGE("Failed to register callback: %d", err);
        return -1;
    }

    RG_LOGI("USB MSC initialization");
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = &descriptor_config,
        .string_descriptor = string_desc_arr,
        .string_descriptor_count = sizeof(string_desc_arr) / sizeof(string_desc_arr[0]),
        .external_phy = false,
        .configuration_descriptor = desc_configuration,
    };
    err = tinyusb_driver_install(&tusb_cfg);
    if(err != ESP_OK) {
        RG_LOGE("Failed to install driver: %d", err);
        return -1;
    }
    RG_LOGI("USB MSC initialization DONE");

    return 0;
}
