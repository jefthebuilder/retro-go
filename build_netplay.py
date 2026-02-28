#!/usr/bin/env python3
"""
Retro-Go Netplay Build Helper

This script simplifies building Retro-Go with netplay support enabled.
It handles configuration, building, and flashing with netplay features.
"""

import argparse
import os
import sys
import subprocess

NETPLAY_EMULATORS = [
    'retro-core',  # NES, GB, GBC, SMS, GG, PCE, SNES, etc.
    'gwenesis',    # Genesis/Mega Drive
    'fmsx',        # MSX
    'prboom-go',   # Doom
]

def run_command(cmd, cwd=None):
    """Run a shell command and return the result."""
    print(f"Running: {' '.join(cmd)}")
    result = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"Error: {result.stderr}")
        return False
    print(result.stdout)
    return True

def build_with_netplay(emulator, target, clean=False):
    """Build an emulator with netplay support."""
    print(f"\n{'='*60}")
    print(f"Building {emulator} with netplay support for {target}")
    print(f"{'='*60}\n")
    
    emulator_path = os.path.join(os.getcwd(), emulator)
    
    if not os.path.exists(emulator_path):
        print(f"Error: Emulator directory not found: {emulator_path}")
        return False
    
    # Clean if requested
    if clean:
        print("Cleaning previous build...")
        run_command(['idf.py', 'fullclean'], cwd=emulator_path)
    
    # Build with netplay enabled
    build_cmd = [
        'idf.py',
        '-D', f'RG_BUILD_TARGET={target}',
        '-D', 'RG_ENABLE_NETPLAY=1',
        '-D', 'RG_ENABLE_NETWORKING=1',
        'build'
    ]
    
    return run_command(build_cmd, cwd=emulator_path)

def flash_emulator(emulator, port=None):
    """Flash an emulator to the device."""
    print(f"\n{'='*60}")
    print(f"Flashing {emulator}")
    print(f"{'='*60}\n")
    
    emulator_path = os.path.join(os.getcwd(), emulator)
    
    flash_cmd = ['idf.py', 'flash']
    if port:
        flash_cmd.extend(['-p', port])
    
    return run_command(flash_cmd, cwd=emulator_path)

def build_all(target, clean=False):
    """Build all emulators with netplay support."""
    print(f"\nBuilding all emulators with netplay for {target}\n")
    
    failed = []
    for emulator in NETPLAY_EMULATORS:
        if not build_with_netplay(emulator, target, clean):
            failed.append(emulator)
    
    if failed:
        print(f"\n❌ Failed to build: {', '.join(failed)}")
        return False
    
    print(f"\n✅ All emulators built successfully!")
    return True

def create_firmware_image(target, output='retro-go-netplay.fw'):
    """Create a firmware image with netplay support."""
    print(f"\n{'='*60}")
    print(f"Creating firmware image: {output}")
    print(f"{'='*60}\n")
    
    cmd = [
        'python3', 'rg_tool.py', 'build-img',
        '--target', target,
        '--output', output,
        '--with-netplay'
    ]
    
    return run_command(cmd)

def main():
    parser = argparse.ArgumentParser(
        description='Build Retro-Go emulators with netplay support'
    )
    
    parser.add_argument(
        'action',
        choices=['build', 'build-all', 'flash', 'create-fw', 'info'],
        help='Action to perform'
    )
    
    parser.add_argument(
        '--emulator',
        choices=NETPLAY_EMULATORS + ['all'],
        default='retro-core',
        help='Emulator to build (default: retro-core)'
    )
    
    parser.add_argument(
        '--target',
        default='qtpy-s3',
        help='Build target (default: qtpy-s3)'
    )
    
    parser.add_argument(
        '--port',
        help='Serial port for flashing'
    )
    
    parser.add_argument(
        '--clean',
        action='store_true',
        help='Clean before building'
    )
    
    parser.add_argument(
        '--output',
        default='retro-go-netplay.fw',
        help='Output firmware filename'
    )
    
    args = parser.parse_args()
    
    if args.action == 'info':
        print("Retro-Go Netplay Build Information")
        print("=" * 60)
        print(f"\nSupported emulators:")
        for emu in NETPLAY_EMULATORS:
            print(f"  - {emu}")
        print(f"\nNetplay features:")
        print(f"  - Rollback netcode")
        print(f"  - Lockstep synchronization")
        print(f"  - Up to 4 players")
        print(f"  - Host/client architecture")
        print(f"  - Integrated UI")
        print(f"\nBuild with netplay:")
        print(f"  ./build_netplay.py build --emulator retro-core --target qtpy-s3")
        print(f"\nFlash to device:")
        print(f"  ./build_netplay.py flash --emulator retro-core --port /dev/ttyUSB0")
        print(f"\nBuild all emulators:")
        print(f"  ./build_netplay.py build-all --target qtpy-s3")
        return 0
    
    elif args.action == 'build':
        if args.emulator == 'all':
            success = build_all(args.target, args.clean)
        else:
            success = build_with_netplay(args.emulator, args.target, args.clean)
        return 0 if success else 1
    
    elif args.action == 'build-all':
        success = build_all(args.target, args.clean)
        return 0 if success else 1
    
    elif args.action == 'flash':
        if args.emulator == 'all':
            print("Error: Cannot flash all emulators at once")
            return 1
        success = flash_emulator(args.emulator, args.port)
        return 0 if success else 1
    
    elif args.action == 'create-fw':
        success = create_firmware_image(args.target, args.output)
        return 0 if success else 1
    
    return 0

if __name__ == '__main__':
    sys.exit(main())
