#pragma once

#include <stdbool.h>
#include <stdint.h>

#if defined(RG_STORAGE_SDSPI_HOST)
#include <driver/sdspi_host.h>
#define SDCARD_DO_TRANSACTION sdspi_host_do_transaction
#elif defined(RG_STORAGE_SDMMC_HOST)
#include <driver/sdmmc_host.h>
#define SDCARD_DO_TRANSACTION sdmmc_host_do_transaction
#endif

int link_usb_msc(sdmmc_card_t *card);
