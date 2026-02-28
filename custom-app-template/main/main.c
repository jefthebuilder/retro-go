/**
 * Custom App Template for Retro-Go
 * Compatible with JAF1 and other ESP32 targets
 */

#include <rg_system.h>
#include <string.h>
#include <stdio.h>

// App state
typedef struct {
    bool running;
    int counter;
    char message[128];
} app_state_t;

static app_state_t state = {
    .running = true,
    .counter = 0,
    .message = "Hello from Custom App!",
};

/**
 * Handle user input
 */
static void handle_input(void)
{
    uint32_t joystick = rg_input_read_gamepad();
    
    if (joystick & RG_KEY_MENU) {
        // Return to launcher on MENU press
        state.running = false;
        rg_system_switch_app("launcher", "launcher", "", 0, 0);
    }
    
    if (joystick & RG_KEY_A) {
        state.counter++;
        RG_LOGI("A pressed! Counter: %d", state.counter);
    }
    
    if (joystick & RG_KEY_B) {
        state.counter--;
        RG_LOGI("B pressed! Counter: %d", state.counter);
    }
}

/**
 * Update app logic
 */
static void update(void)
{
    // Update your app state here
    handle_input();
}

/**
 * Render the screen
 */
static void render(void)
{
    rg_display_clear(C_BLACK);
    
    // Draw title
    rg_gui_draw_text(10, 10, 300, state.message, C_WHITE, C_BLACK, 0);
    
    // Draw counter
    char counter_text[64];
    snprintf(counter_text, sizeof(counter_text), "Counter: %d", state.counter);
    rg_gui_draw_text(10, 40, 300, counter_text, C_GREEN, C_BLACK, 0);
    
    // Draw instructions
    rg_gui_draw_text(10, 100, 300, "Press A to increment", C_GRAY, C_BLACK, 0);
    rg_gui_draw_text(10, 120, 300, "Press B to decrement", C_GRAY, C_BLACK, 0);
    rg_gui_draw_text(10, 140, 300, "Press MENU to exit", C_GRAY, C_BLACK, 0);
    
    rg_display_submit(NULL, 0);
}

/**
 * Main application entry point
 */
void app_main(void)
{
    // Initialize the retro-go system
    rg_app_t *app = rg_system_init(0, NULL, NULL);
    
    RG_LOGI("Custom app starting...");
    RG_LOGI("configNs: %s", app->configNs ?: "none");
    RG_LOGI("romPath: %s", app->romPath ?: "none");
    
    // Optional: Check if a file was passed to us
    if (app->romPath && app->romPath[0]) {
        snprintf(state.message, sizeof(state.message), "Loaded file: %s", rg_basename(app->romPath));
        RG_LOGI("File loaded: %s", app->romPath);
        
        // TODO: Open and process the file
        // FILE *fp = fopen(app->romPath, "rb");
        // if (fp) {
        //     // Read and process file...
        //     fclose(fp);
        // }
    }
    
    // Main loop
    while (state.running) {
        update();
        render();
        rg_task_yield();
    }
    
    // Clean up and exit
    RG_LOGI("Custom app exiting...");
    rg_system_shutdown();
}
