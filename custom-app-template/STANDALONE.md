# Standalone Build Guide

This guide explains how to use this template **completely independently** from the retro-go workspace.

## Why Standalone?

- Develop your app in a separate repository
- Use your own version control
- Easier CI/CD integration
- Keep your project isolated

## Setup

### Step 1: Copy Template

Copy the entire template directory anywhere you want:

```bash
cp -r /path/to/retro-go/custom-app-template ~/my-projects/my-awesome-app
cd ~/my-projects/my-awesome-app
```

### Step 2: Configure Retro-Go Path

You have three options:

#### Option A: Modify CMakeLists.txt (Recommended)

Edit `CMakeLists.txt` and change the RETRO_GO_PATH:

```cmake
set(RETRO_GO_PATH "/absolute/path/to/retro-go" CACHE PATH "Path to retro-go root")
```

#### Option B: Environment Variable

```bash
export RETRO_GO_PATH=/path/to/retro-go
```

Add to your `~/.bashrc` or `~/.zshrc` to make permanent.

#### Option C: Pass to Build Command

```bash
make build RETRO_GO_PATH=/path/to/retro-go
# or
idf.py -DRETRO_GO_PATH=/path/to/retro-go build
```

### Step 3: Initialize Git (Optional)

```bash
git init
git add .
git commit -m "Initial commit from retro-go template"
```

### Step 4: Build

```bash
make build
```

## Project Structure (Standalone)

```
my-awesome-app/               # Your project root (anywhere on disk)
├── .git/                     # Your own git repo
├── CMakeLists.txt            # Points to retro-go via RETRO_GO_PATH
├── Makefile                  # Build commands
├── sdkconfig                 # Your ESP-IDF configuration
├── main/
│   ├── CMakeLists.txt
│   └── main.c                # Your application code
└── components/               # Your custom components (optional)
    └── my-lib/
        ├── CMakeLists.txt
        └── my-lib.c

# Retro-Go (elsewhere on disk)
/path/to/retro-go/
├── components/
│   └── retro-go/            # This is what we need
└── ... (rest of retro-go)
```

## Alternative: Git Submodule

If you want to bundle retro-go with your project:

```bash
# Initialize your repo
git init

# Add retro-go as submodule
git submodule add https://github.com/ducalex/retro-go.git external/retro-go

# Update CMakeLists.txt
set(RETRO_GO_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/retro-go" CACHE PATH "Path to retro-go root")

# Clone with submodules
git clone --recursive https://your-repo.git
```

## Alternative: Copy Components

For maximum isolation, copy just the retro-go component:

```bash
# Create local components directory
mkdir -p components

# Copy retro-go component
cp -r /path/to/retro-go/components/retro-go components/

# Update CMakeLists.txt
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/components")
# Remove or comment out RETRO_GO_PATH line
```

Now your project is completely self-contained with no external dependencies!

## CI/CD Integration

### GitHub Actions Example

```yaml
name: Build

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
        with:
          submodules: recursive  # If using git submodule
      
      - name: Setup ESP-IDF
        uses: espressif/esp-idf-ci-action@v1
        with:
          esp_idf_version: v5.0
      
      - name: Build
        run: |
          . $IDF_PATH/export.sh
          make build
      
      - name: Upload Binary
        uses: actions/upload-artifact@v3
        with:
          name: firmware
          path: build/*.bin
```

## Deployment

After building standalone, deploy to app store:

### Manual:
1. Copy `build/my-awesome-app.bin` to retro-go app store server
2. Run `python manage.py add-app` or `update-apps`

### Automated:
```bash
# Script to deploy to server
#!/bin/bash
BUILD_BIN="build/my-awesome-app.bin"
SERVER_PATH="/path/to/retro-go/appstore/server/apps/"

if [ -f "$BUILD_BIN" ]; then
    cp "$BUILD_BIN" "$SERVER_PATH"
    cd "$SERVER_PATH/.."
    python manage.py update-apps --app "My Awesome App"
    echo "Deployed successfully!"
else
    echo "Build not found. Run 'make build' first."
fi
```

## Troubleshooting

**CMake can't find retro-go:**
- Check RETRO_GO_PATH is correct (absolute path recommended)
- Ensure `/path/to/retro-go/components/retro-go` exists
- Try: `make build RETRO_GO_PATH=/absolute/path`

**Build fails with "component not found":**
- Verify ESP-IDF is properly installed: `idf.py --version`
- Ensure retro-go components are accessible
- Check EXTRA_COMPONENT_DIRS in CMakeLists.txt

**Binary too large for app slot:**
- See BUILDING.md "Size Optimization" section
- Use `-Os` optimization
- Remove unused features from sdkconfig

## Benefits of Standalone

✅ Independent version control  
✅ Custom CI/CD pipelines  
✅ Team collaboration easier  
✅ Deploy without full retro-go workspace  
✅ Cleaner project structure  
✅ Faster builds (only your code)  

## Sharing Your App

Once built standalone, share your project:

1. **GitHub/GitLab**: Push your standalone repo
2. **Binary**: Upload just the .bin file
3. **App Store**: Add to retro-go app store server

Users can install via app store without needing to build anything!
