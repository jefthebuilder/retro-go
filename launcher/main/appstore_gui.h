/**
 * App Store GUI Module Header
 */

#pragma once

#include <rg_system.h>

// Initialize app store
void appstore_gui_init(void);

// Cleanup app store
void appstore_gui_deinit(void);

// Get menu option for launcher
rg_gui_option_t appstore_get_menu_option(void);
