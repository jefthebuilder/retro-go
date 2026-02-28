# Custom App Template for Retro-Go

This template shows you how to create a custom application that can be installed dynamically via the app store.

**This template works within the retro-go workspace.** For standalone use, see STANDALONE.md.

## Quick Start (Within Retro-Go Workspace)

1. **Source ESP-IDF** (required):
   ```bash
   . ~/esp/esp-idf/export.sh  # or wherever your ESP-IDF is installed
   ```

2. **Copy this template** to create your new app:
   ```bash
   cp -r custom-app-template my-awesome-app
   cd my-awesome-app
   ```

3. **Edit main/main.c** - Implement your app logic

4. **Build using rg_tool** (recommended):
   ```bash
   cd /media/jef/gamesetc/retro-go
   python3 rg_tool.py --target jaf1 build my-awesome-app
   ```

   Or using Makefile (if ESP-IDF is sourced):
   ```bash
   cd my-awesome-app
   make build
   ```

5. **Binary location**: `build/my-awesome-app.bin`

## File Extensions

If your app handles specific file types (like .txt, .dat, .rom), make sure to:
1. Set extensions in the server metadata
2. The launcher will automatically map file extensions to your app

## App Configuration

Your app receives a `configNs` parameter from the launcher. For multi-purpose apps,
use this to determine behavior based on file type.

## Memory Constraints

Dynamic app slots have ~1.56MB each. Keep your binary size under this limit:
- Remove unused features
- Use `-Os` optimization
- Strip debug symbols
- Compress assets

## Integration with Retro-Go

Your app should:
1. Call `rg_system_init()` to initialize the system
2. Handle input via `rg_input_*` functions
3. Use `rg_display_*` for rendering
4. Clean up and return to launcher with `rg_system_switch_app()` or restart

## Testing

After building:
1. Flash launcher: `python rg_tool.py --target YOUR_TARGET flash-img launcher`
2. Install via app store UI on device
3. Check serial output for errors

## Advanced: Custom Graphics

Generate custom assets using the theme tool:
```bash
python tools/gen_images.py themes/my-theme launcher/main/images.c
```

## Troubleshooting

**Binary too large**: Reduce features, enable LTO, remove debug info
**App won't boot**: Check configNs mapping in applications.c
**Files not showing**: Verify extensions metadata in server database
**Crashes on launch**: Check stack size in sdkconfig (CONFIG_ESP_MAIN_TASK_STACK_SIZE)

## Next Steps

- See `prboom-go/` for a complete working example
- See `retro-core/` for multi-emulator app pattern
- Check `launcher/main/applications.c` for file extension to configNs mapping
