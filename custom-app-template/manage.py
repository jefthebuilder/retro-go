#!/usr/bin/env python3
"""
Custom App Manager for Retro-Go

This script helps build, package, and manage your custom application
for installation via the Retro-Go app store.

Usage:
    python manage.py build                      # Build the app binary
    python manage.py clean                      # Clean build artifacts
    python manage.py upload [--server URL]      # Upload to app store server
    python manage.py info                       # Show app information
    python manage.py help                       # Show this help message

Configuration:
    Set environment variables to customize:
    - RG_APPSTORE_URL: App store server URL (default: http://localhost:5000)
    - RG_APPSTORE_KEY: API key for app store (default: dev-api-key)
"""

import os
import sys
import argparse
import json
import shutil
import subprocess
from pathlib import Path
from datetime import datetime

# Get absolute paths
SCRIPT_DIR = Path(__file__).parent.absolute()
RETRO_GO_ROOT = SCRIPT_DIR.parent

# App metadata
APP_NAME = "my-custom-app"
APP_DISPLAY_NAME = "My Custom App"
APP_VERSION = "1.0.0"
APP_AUTHOR = "Your Name"
APP_DESCRIPTION = "A custom application built with the Retro-Go template"
APP_CATEGORY = "utility"  # utility, game, emulator, tool
APP_EXTENSIONS = ""  # Comma-separated file extensions this app handles (e.g., "txt log")
APP_ICON = None  # Path to icon.png (optional, 64x64)

# Server configuration
APPSTORE_URL = os.getenv("RG_APPSTORE_URL", "http://localhost:5000")
APPSTORE_API_KEY = os.getenv("RG_APPSTORE_KEY", "dev-api-key")


def run_command(cmd, cwd=None, check=True):
    """Run a shell command and return output"""
    print(f"$ {' '.join(cmd)}")
    try:
        result = subprocess.run(cmd, cwd=cwd, capture_output=False, check=check)
        return result.returncode == 0
    except Exception as e:
        print(f"✗ Error running command: {e}")
        return False


def cmd_build(args):
    """Build the app binary"""
    print(f"\n🔨 Building {APP_DISPLAY_NAME}...\n")
    
    if not os.path.exists(SCRIPT_DIR / "Makefile"):
        print("✗ Makefile not found in app directory")
        return False
    
    # Clean old build
    if (SCRIPT_DIR / "build").exists():
        print("  Cleaning old build...")
        shutil.rmtree(SCRIPT_DIR / "build")
    
    if (SCRIPT_DIR / "managed_components").exists():
        print("  Cleaning managed components...")
        shutil.rmtree(SCRIPT_DIR / "managed_components")
    
    # Run build
    success = run_command(["make", "build"], cwd=SCRIPT_DIR)
    
    if success:
        binary = SCRIPT_DIR / "build" / f"{APP_NAME}.bin"
        if binary.exists():
            size_kb = binary.stat().st_size / 1024
            print(f"\n✓ Build successful!")
            print(f"  Binary: {binary}")
            print(f"  Size: {size_kb:.1f} KB")
            return True
        else:
            print(f"\n✗ Build completed but binary not found at {binary}")
            return False
    else:
        print(f"\n✗ Build failed!")
        return False


def cmd_clean(args):
    """Clean build artifacts"""
    print(f"\n🧹 Cleaning {APP_DISPLAY_NAME}...\n")
    
    dirs_to_clean = [
        SCRIPT_DIR / "build",
        SCRIPT_DIR / "managed_components",
    ]
    
    for dir_path in dirs_to_clean:
        if dir_path.exists():
            print(f"  Removing {dir_path.relative_to(SCRIPT_DIR)}/")
            shutil.rmtree(dir_path)
    
    print("\n✓ Cleaned")
    return True


def cmd_upload(args):
    """Upload app to app store server"""
    print(f"\n📤 Uploading {APP_DISPLAY_NAME} to app store...\n")
    
    binary = SCRIPT_DIR / "build" / f"{APP_NAME}.bin"
    
    if not binary.exists():
        print(f"✗ Binary not found. Run 'python manage.py build' first")
        return False
    
    server_url = args.server or APPSTORE_URL
    api_key = APPSTORE_API_KEY
    
    # Prepare upload data
    upload_url = f"{server_url}/api/v1/admin/upload"
    
    files = {
        'file': open(binary, 'rb'),
    }
    
    data = {
        'name': APP_DISPLAY_NAME,
        'version': APP_VERSION,
        'author': APP_AUTHOR,
        'description': APP_DESCRIPTION,
        'category': APP_CATEGORY,
        'extensions': APP_EXTENSIONS,
        'featured': 'false',
    }
    
    headers = {
        'X-API-Key': api_key,
    }
    
    print(f"  Server: {server_url}")
    print(f"  Endpoint: {upload_url}")
    print(f"  File: {binary.name}")
    print(f"  Size: {binary.stat().st_size / 1024:.1f} KB")
    print()
    
    try:
        import requests
    except ImportError:
        print("✗ requests library not found. Install with: pip install requests")
        return False
    
    try:
        print(f"  Uploading...")
        response = requests.post(upload_url, files=files, data=data, headers=headers)
        
        if response.status_code in (200, 201):
            result = response.json()
            print(f"\n✓ Upload successful!")
            print(f"  Status: {result.get('message', 'App registered')}")
            
            if 'app' in result:
                app_info = result['app']
                print(f"  App ID: {app_info.get('id')}")
                print(f"  Version: {app_info.get('version')}")
            
            print(f"\n💡 The app is now available in the store!")
            print(f"   Restart the device or refresh the app store to see it.")
            return True
        else:
            error = response.json() if response.text else {}
            print(f"\n✗ Upload failed: {response.status_code}")
            print(f"  Error: {error.get('error', response.text)}")
            return False
            
    except requests.exceptions.ConnectionError:
        print(f"\n✗ Connection error: Could not reach {server_url}")
        print(f"  Make sure the app store server is running:")
        print(f"    cd {RETRO_GO_ROOT / 'appstore' / 'server'}")
        print(f"    python run.py")
        return False
    except Exception as e:
        print(f"\n✗ Upload error: {e}")
        return False
    finally:
        files['file'].close()


def cmd_install(args):
    """Install app on device"""
    print(f"\n📱 Installing {APP_DISPLAY_NAME} on device...\n")
    
    if not args.port:
        print("✗ Device port required. Use: python manage.py install --port /dev/ttyUSB0")
        return False
    
    binary = SCRIPT_DIR / "build" / f"{APP_NAME}.bin"
    
    if not binary.exists():
        print(f"✗ Binary not found. Run 'python manage.py build' first")
        return False
    
    # Try using rg_tool.py if available
    rg_tool = RETRO_GO_ROOT / "rg_tool.py"
    if rg_tool.exists():
        print(f"  Using rg_tool.py to flash...")
        # This would require modifications to rg_tool.py to support custom apps
        print(f"  (Feature not yet implemented - use app store to install instead)")
    else:
        print(f"  rg_tool.py not found")
    
    return False


def cmd_info(args):
    """Show app information"""
    binary = SCRIPT_DIR / "build" / f"{APP_NAME}.bin"
    
    print(f"\n📋 App Information\n")
    print(f"  Name:        {APP_DISPLAY_NAME}")
    print(f"  Version:     {APP_VERSION}")
    print(f"  Author:      {APP_AUTHOR}")
    print(f"  Category:    {APP_CATEGORY}")
    print(f"  Description: {APP_DESCRIPTION}")
    
    if APP_EXTENSIONS:
        print(f"  Extensions:  {APP_EXTENSIONS}")
    
    print(f"\n📂 Build Information\n")
    print(f"  Source:      {SCRIPT_DIR}")
    print(f"  Build dir:   {SCRIPT_DIR / 'build'}")
    
    if binary.exists():
        size_kb = binary.stat().st_size / 1024
        print(f"  Binary:      {binary.name}")
        print(f"  Size:        {size_kb:.1f} KB")
        print(f"  Max size:    1.56 MB (app slot limit)")
        print(f"  Status:      ✓ Ready to upload")
    else:
        print(f"  Binary:      Not built yet")
        print(f"  Status:      Run 'python manage.py build' first")
    
    print()
    return True


def cmd_help(args):
    """Show help message"""
    print(__doc__)
    return True


def main():
    parser = argparse.ArgumentParser(
        description="Custom App Manager for Retro-Go",
        add_help=False,
    )
    
    subparsers = parser.add_subparsers(dest="command", help="Command to run")
    
    # Build command
    build_parser = subparsers.add_parser("build", help="Build the app")
    
    # Clean command
    clean_parser = subparsers.add_parser("clean", help="Clean build artifacts")
    
    # Upload command
    upload_parser = subparsers.add_parser("upload", help="Upload to app store server")
    upload_parser.add_argument("--server", default=APPSTORE_URL, help="App store server URL (default: http://localhost:5000)")
    
    # Install command
    install_parser = subparsers.add_parser("install", help="Install on device")
    install_parser.add_argument("--port", help="Serial port (e.g., /dev/ttyUSB0)")
    
    # Info command
    info_parser = subparsers.add_parser("info", help="Show app information")
    
    # Help command
    help_parser = subparsers.add_parser("help", help="Show help message")
    
    args = parser.parse_args()
    
    if not args.command or args.command == "help":
        print(__doc__)
        return 0
    
    # Map commands to functions
    commands = {
        "build": cmd_build,
        "clean": cmd_clean,
        "upload": cmd_upload,
        "install": cmd_install,
        "info": cmd_info,
    }
    
    if args.command not in commands:
        print(f"✗ Unknown command: {args.command}")
        print(__doc__)
        return 1
    
    success = commands[args.command](args)
    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
