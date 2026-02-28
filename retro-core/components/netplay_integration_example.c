// Netplay integration example for retro-core emulators (NES, GB, GBC, SMS, GG, PCE, etc.)
// This file demonstrates how to add netplay support to retro-core based emulators

#ifdef RG_ENABLE_NETPLAY

#include "rg_system.h"
#include "rg_netplay_emu.h"
#include <string.h>

// Example for NES emulator
// Similar pattern can be used for other retro-core emulators

// External emulator functions (these exist in the emulator cores)
extern bool nes_save_state_to_buffer(void **buffer, size_t *size);
extern bool nes_load_state_from_buffer(const void *buffer, size_t size);
extern void nes_set_input(uint8_t player, uint32_t buttons);
extern uint32_t nes_get_input(uint8_t player);

// Serialize emulator state for netplay
static bool netplay_serialize_state(void **data, size_t *size)
{
    // Save emulator state to memory buffer
    return nes_save_state_to_buffer(data, size);
}

// Deserialize emulator state for netplay
static bool netplay_deserialize_state(const void *data, size_t size)
{
    // Load emulator state from memory buffer
    return nes_load_state_from_buffer(data, size);
}

// Apply input from netplay
static void netplay_apply_input(uint8_t player, uint32_t buttons, int16_t analog_x, int16_t analog_y)
{
    // Set input for specific player
    nes_set_input(player, buttons);
}

// Get local input
static uint32_t netplay_get_input(uint8_t player, int16_t *analog_x, int16_t *analog_y)
{
    // Get input from local player
    if (analog_x) *analog_x = 0;
    if (analog_y) *analog_y = 0;
    return nes_get_input(player);
}

// Called when desync is detected
static void netplay_on_desync(uint32_t frame)
{
    RG_LOGE("netplay", "Desync detected at frame %u!", frame);
    // Could show a warning to the user
}

// Called when a player joins
static void netplay_on_player_join(uint8_t player_id, const char *name)
{
    RG_LOGI("netplay", "Player %d (%s) joined", player_id, name);
}

// Called when a player leaves
static void netplay_on_player_leave(uint8_t player_id)
{
    RG_LOGI("netplay", "Player %d left", player_id);
}

// Initialize netplay for the emulator
bool netplay_init_for_emulator(void)
{
    rg_netplay_emu_config_t config = {
        .serialize_state = netplay_serialize_state,
        .deserialize_state = netplay_deserialize_state,
        .apply_input = netplay_apply_input,
        .get_input = netplay_get_input,
        .on_desync = netplay_on_desync,
        .on_player_join = netplay_on_player_join,
        .on_player_leave = netplay_on_player_leave,
        .input_delay_frames = 2,  // 2 frames of input delay (adjustable)
        .use_rollback = true,     // Use rollback netcode
    };
    
    return rg_netplay_emu_init(&config);
}

// INTEGRATION STEPS:
// 
// 1. In your emulator's main() function, after rg_system_init():
//    if (app->bootFlags & RG_BOOT_NETPLAY)
//    {
//        netplay_init_for_emulator();
//    }
//
// 2. In your emulator's main loop, before running a frame:
//    rg_netplay_emu_begin_frame();
//
// 3. In your emulator's main loop, after running a frame:
//    rg_netplay_emu_end_frame();
//
// 4. When reading input, use:
//    uint32_t buttons;
//    if (rg_netplay_emu_is_active())
//    {
//        rg_netplay_emu_get_input(player_id, &buttons, NULL, NULL);
//    }
//    else
//    {
//        buttons = normal_input_reading();
//    }
//
// 5. When the local player provides input:
//    rg_netplay_emu_set_input(buttons, 0, 0);
//
// 6. In your cleanup code:
//    rg_netplay_emu_deinit();

#endif // RG_ENABLE_NETPLAY
