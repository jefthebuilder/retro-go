# Building Your Custom App

## Prerequisites

1. ESP-IDF 5.0+ installed and configured (`idf.py` in PATH)
2. For standalone builds: Path to retro-go components
3. For workspace builds: Located in retro-go workspace

## Building Methods

### Method 1: Using Makefile (Recommended)

```bash
# Build
make build

# Flash to device
make flash

# Monitor serial output
make monitor

# Flash and monitor
make flash-monitor

# Configure
make menuconfig

# Set target chip
make set-target TARGET=esp32s3

# Clean
make clean
```

### Method 2: Using idf.py directly

```bash
# Build
idf.py build

# Flash
idf.py -p /dev/ttyUSB0 flash

# Monitor
idf.py monitor
```

### Method 3: Using retro-go rg_tool (if in workspace)

```bash
python ../rg_tool.py --target YOUR_TARGET build my-custom-app
```

## Step 1: Configure (First Time Only)

### Option A: Copy existing config
```bash
cp ../prboom-go/sdkconfig ./sdkconfig
```

### Option B: Configure from scratch
```bash
make menuconfig
# or
idf.py menuconfig
```

## Step 2: Build the App

```bash
make build
```

The binary will be at: `build/my-custom-app.bin`

## Standalone Build (Outside Workspace)

If you've copied this template outside the retro-go workspace:

1. **Clone retro-go** (if you don't have it):
   ```bash
   git clone https://github.com/ducalex/retro-go.git
   ```

2. **Set the path** in CMakeLists.txt:
   ```cmake
   set(RETRO_GO_PATH "/absolute/path/to/retro-go" CACHE PATH "Path to retro-go root")
   ```

3. **Or set via environment**:
   ```bash
   export RETRO_GO_PATH=/path/to/retro-go
   make build
   ```

4. **Or pass to make**:
   ```bash
   make build RETRO_GO_PATH=/path/to/retro-go
   ```

## Step 3: Test Locally (Optional)

Flash directly to test before adding to app store:
```bash
python ../rg_tool.py --target YOUR_TARGET flash my-custom-app
```

## Step 4: Add to App Store

### Manual Method:
1. Copy binary to server:
   ```bash
   cp build/my-custom-app.bin ../appstore/server/apps/
   ```

2. Add to database:
   ```bash
   cd ../appstore/server
   python manage.py add-app
   ```
   
   Enter details:
   - Name: My Custom App
   - Description: Does amazing things
   - Version: 1.0.0
   - Author: Your Name
   - Category: utility (or game/tool/emulator)
   - Price: 0
   - File path: apps/my-custom-app.bin
   - Extensions: txt dat (space-separated, if your app handles files)

### Automatic Method:
Edit `appstore/server/manage.py` and add your app to the build map:
```python
app_build_map = {
    # ... existing apps ...
    "My Custom App": os.path.join(project_root, "my-custom-app", "build"),
}

extension_map = {
    # ... existing apps ...
    "My Custom App": "txt dat custom",
}
```

Then run:
```bash
cd ../appstore/server
python manage.py update-apps --app "My Custom App"
```

## Step 5: Install on Device

1. Make sure launcher is flashed with latest version
2. Connect device to WiFi (if not already configured)
3. Go to App Store in launcher
4. Browse and install your app

## Size Optimization

If your binary exceeds ~1.5MB:

### Compiler Flags (in CMakeLists.txt):
```cmake
target_compile_options(${COMPONENT_LIB} PRIVATE 
    -Os                    # Optimize for size
    -ffunction-sections    # Enable garbage collection
    -fdata-sections
)
target_link_options(${COMPONENT_LIB} PRIVATE
    -Wl,--gc-sections      # Remove unused sections
)
```

### Config Options (sdkconfig):
- Disable logging: `CONFIG_LOG_DEFAULT_LEVEL_NONE=y`
- Reduce task stacks: Lower `CONFIG_ESP_MAIN_TASK_STACK_SIZE`
- Disable unused features in `menuconfig`

### Code:
- Remove unused libraries
- Use static buffers instead of dynamic allocation where possible
- Compress assets/data

## Debugging

### Serial Monitor:
```bash
python ../rg_tool.py --target YOUR_TARGET monitor
```

### Common Issues:

**App won't show in launcher:**
- Check extensions metadata is set correctly
- Verify partition has valid app header
- Check serial output for scan_dynamic_apps() messages

**Crashes on launch:**
- Increase stack size in sdkconfig
- Check for buffer overflows
- Verify rg_system_init() is called first

**Files not visible:**
- Extensions must match in server database
- Check applications.c for extension mapping
- Verify metadata file in /odroid/appstore/metadata/

## Updating Your App

1. Make changes to code
2. Rebuild: `python ../rg_tool.py --target YOUR_TARGET build my-custom-app`
3. Update server: `python ../appstore/server/manage.py update-apps --app "My Custom App"`
   - This auto-increments version
4. On device: App Store → Check for Updates → Install

## Advanced: Multi-System Apps

If your app handles multiple file types with different behaviors:

1. Use configNs to determine mode:
```c
void app_main(void) {
    rg_app_t *app = rg_system_init(0, NULL, NULL);
    
    if (strcmp(app->configNs, "txt") == 0) {
        // Text viewer mode
    } else if (strcmp(app->configNs, "dat") == 0) {
        // Data processor mode
    }
}
```

2. Add mapping in `launcher/main/applications.c`:
```c
// In application_start(), add your extensions:
} else if (strcmp(ext_lower, "txt") == 0) {
    strcpy(name, "txt");
} else if (strcmp(ext_lower, "dat") == 0) {
    strcpy(name, "dat");
}
```

## Next Steps

- Add custom graphics/icons
- Implement save/load state
- Add settings/configuration
- Network features (if enabled)
