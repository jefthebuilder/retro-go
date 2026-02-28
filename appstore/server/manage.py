#!/usr/bin/env python3
"""
Retro-Go App Store Management CLI

Usage:
    python manage.py init                    # Initialize database
    python manage.py populate-apps           # Add standard Retro-Go apps
    python manage.py add-app                 # Add a new app interactively
    python manage.py add-app-file FILE       # Add app from binary file
    python manage.py list-apps               # List all apps
    python manage.py list-devices            # List devices
    python manage.py create-license          # Create a license
    python manage.py show-stats              # Show statistics
    python manage.py export                  # Export database
    
Apps can be in two states:
    - Pre-installed (file_path=None): Listed in store but not downloadable
    - Downloadable (file_path=<path>): Can be downloaded from the device
"""

import os
import sys
import argparse
import json
from pathlib import Path
from datetime import datetime, timedelta

# Add server directory to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'server'))

from app import create_app
from app.models import db, App, Device, License, Installation, Review
import hashlib

def calculate_file_hash(filepath):
    """Calculate SHA256 hash of a file"""
    sha256_hash = hashlib.sha256()
    with open(filepath, "rb") as f:
        for byte_block in iter(lambda: f.read(4096), b""):
            sha256_hash.update(byte_block)
    return sha256_hash.hexdigest()

def cmd_init(args):
    """Initialize database"""
    app = create_app()
    with app.app_context():
        print("Creating database tables...")
        db.create_all()
        print("✓ Database initialized")

def cmd_populate_apps(args):
    """Populate database with standard Retro-Go apps"""
    app = create_app()
    
    # Path to retro-go root directory (relative to manage.py)
    retro_go_root = os.path.join(os.path.dirname(__file__), '..', '..')
    apps_download_dir = os.path.join(os.path.dirname(__file__), 'apps')
    
    # Create apps directory if it doesn't exist
    os.makedirs(apps_download_dir, exist_ok=True)
    
    # Standard Retro-Go apps with build paths
    standard_apps = [
        {
            "name": "Launcher",
            "description": "Retro-Go Launcher - Browse and launch games from your collection",
            "version": "1.46",
            "author": "Retro-Go Team",
            "extensions": "nes smc sfc fig md smd gen sms gg gb gbc a26 pce col",
            "category": "utility",
            "price": 0.0,
            "featured": True,
            "build_path": None,  # Pre-installed, not in downloadable form
        },
        {
            "name": "Retro Core",
            "description": "Multi-emulator core supporting NES, SNES, Genesis, GB, GBC, Atari 2600, and more.",
            "version": "1.46",
            "author": "Retro-Go Team",
            "extensions": "wad",
            "category": "emulator",
            "price": 0.0,
            "featured": True,
            "build_path": os.path.join(retro_go_root, 'retro-core', 'build'),
        },
        {
            "name": "PrBoom-Go",
            "description": "Doom and Doom II shooter for Retro-Go. Play the classic FPS on your retro device.",
            "version": "1.46",
            "author": "Retro-Go Team",
            "extensions": "gb gbc",
            "category": "game",
            "price": 0.0,
            "featured": True,
            "build_path": os.path.join(retro_go_root, 'prboom-go', 'build'),
        },
        {
            "name": "Game Boy Emulator",
            "description": "Play Game Boy and Game Boy Color games. Full support for GB/GBC cartridges.",
            "version": "1.46",
            "author": "Retro-Go Team",
            "extensions": "md smd gen bin",
            "category": "emulator",
            "price": 0.0,
            "featured": True,
            "build_path": os.path.join(retro_go_root, 'gbsp', 'build'),
        },
        {
            "name": "Genesis Emulator",
            "description": "Sega Genesis / Mega Drive emulator. Play all your favorite Genesis classics.",
            "version": "1.46",
            "author": "Retro-Go Team",
            "extensions": "nes fds unif",
            "category": "emulator",
            "price": 0.0,
            "featured": False,
            "build_path": os.path.join(retro_go_root, 'gwenesis', 'build'),
        },
        {
            "name": "NES Emulator",
            "description": "Classic Nintendo Entertainment System games. Full compatibility with NES cartridges.",
            "version": "1.46",
            "author": "Retro-Go Team",
            "category": "emulator",
            "price": 0.0,
            "featured": False,
            "build_path": None,  # No separate build
        },
        {
            "name": "SNES Emulator",
            "description": "Super Nintendo Entertainment System emulator. Play SNES classics on your device.",
            "version": "1.46",
            "author": "Retro-Go Team",
            "category": "emulator",
            "price": 0.0,
            "featured": False,
            "build_path": None,  # No separate build
        },
        {
            "name": "Atari 2600 Emulator",
            "description": "Emulator for Atari 2600 games. Experience the classics that started it all.",
            "version": "1.46",
            "author": "Retro-Go Team",
            "category": "emulator",
            "price": 0.0,
            "featured": False,
            "build_path": None,  # No separate build
        },
    ]
    
    def find_binary(build_path):
        """Find a binary file in the build directory"""
        if not build_path or not os.path.exists(build_path):
            return None
        
        # Get the project name from the parent directory
        project_dir = os.path.basename(os.path.dirname(build_path))
        
        # Try multiple naming patterns
        binary_names = [
            f'{project_dir}.bin',
            'app.bin',
            f'{project_dir}-go.bin',  # prboom-go pattern
        ]
        
        for bin_name in binary_names:
            project_bin = os.path.join(build_path, bin_name)
            if os.path.exists(project_bin) and os.path.getsize(project_bin) > 10000:  # At least 10KB
                return project_bin
        
        # Look for any .bin file in build directory
        binaries = list(Path(build_path).glob('*.bin'))
        # Filter out small files (bootloader, partition table, etc.)
        large_binaries = [b for b in binaries if b.stat().st_size > 100000]  # At least 100KB
        
        if large_binaries:
            # Return the largest binary (likely the app)
            largest = max(large_binaries, key=lambda f: f.stat().st_size)
            return str(largest)
        
        return None
    
    def copy_app_binary(build_path, app_name):
        """Copy app binary from build directory to downloadable location"""
        binary_file = find_binary(build_path)
        if not binary_file:
            print(f"  ⚠ No binary found in {build_path}")
            return None
        
        # Create downloadable copy
        app_binary_name = app_name.lower().replace(' ', '_') + '.bin'
        dest_path = os.path.join(apps_download_dir, app_binary_name)
        
        try:
            import shutil
            print(f"  → Copying {os.path.basename(binary_file)} to apps/")
            shutil.copy2(binary_file, dest_path)
            return dest_path
        except Exception as e:
            print(f"  ✗ Error copying binary: {e}")
            return None
    
    with app.app_context():
        # Check if apps already exist
        existing_count = App.query.count()
        if existing_count > 0:
            response = input(f"Database already has {existing_count} apps. Continue? (y/N): ")
            if response.lower() != 'y':
                print("Cancelled")
                return
        
        added_count = 0
        for app_data in standard_apps:
            # Check if app already exists
            existing = App.query.filter_by(name=app_data["name"]).first()
            if existing:
                print(f"⊘ App '{app_data['name']}' already exists, skipping")
                continue
            
            file_path = None
            file_size = None
            file_hash = None
            
            # Try to find and copy binary if build_path is specified
            if app_data.get("build_path"):
                print(f"Looking for binary for {app_data['name']}...")
                file_path = copy_app_binary(app_data["build_path"], app_data["name"])
                
                if file_path and os.path.exists(file_path):
                    # Calculate file size and hash
                    file_size = os.path.getsize(file_path)
                    file_hash = calculate_file_hash(file_path)
            
            app_obj = App(
                name=app_data["name"],
                description=app_data["description"],
                version=app_data["version"],
                author=app_data["author"],
                category=app_data["category"],
                extensions=app_data.get("extensions") or None,
                price=app_data["price"],
                currency="USD",
                featured=app_data["featured"],
                file_path=file_path,
                file_size=file_size,
                file_hash=file_hash,
            )
            
            db.session.add(app_obj)
            added_count += 1
            status = "downloadable" if file_path else "pre-installed"
            print(f"✓ Added: {app_data['name']} ({status})")
        
        if added_count > 0:
            db.session.commit()
            print(f"\n✓ Successfully added {added_count} apps to the store")
            print(f"  Apps directory: {apps_download_dir}")
        else:
            print("\nNo new apps were added")

def cmd_update_apps(args):
    """Update existing apps with new binaries"""
    app = create_app()
    
    # Map of app names to their build directories
    project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
    
    # Ensure apps directory exists
    apps_download_dir = os.path.join(os.path.dirname(__file__), 'apps')
    os.makedirs(apps_download_dir, exist_ok=True)
    
    app_build_map = {
        "Launcher": os.path.join(project_root, "launcher", "build"),
        "Retro Core": os.path.join(project_root, "retro-core", "build"),
        "PrBoom-Go": os.path.join(project_root, "prboom-go", "build"),
        "Game Boy Emulator": os.path.join(project_root, "gbsp", "build"),
        "Genesis Emulator": os.path.join(project_root, "gwenesis", "build"),
        "MSX Emulator": os.path.join(project_root, "fmsx", "build"),
    }

    extension_map = {
        "Retro Core": "nes gb gbc smc sfc fig md smd gen sms gg  a26 pce col",
        "PrBoom-Go": "wad",
        "Game Boy Emulator": "gb gbc",
        "Genesis Emulator": "md smd gen bin",
        "NES Emulator": "nes fds unif",
        "MSX Emulator": "rom mx1 mx2 col dsk cas",
    }

    def bump_version(version_str):
        """Increment the patch component of a semantic-ish version string."""
        parts = version_str.split(".") if version_str else []
        while len(parts) < 3:
            parts.append("0")
        try:
            major, minor, patch = [int(p) for p in parts[:3]]
            patch += 1
            return f"{major}.{minor}.{patch}"
        except ValueError:
            # Fallback if version is non-numeric
            return "1.0.0"
    
    def find_binary(build_path):
        """Find a binary file in the build directory"""
        if not build_path or not os.path.exists(build_path):
            return None
        
        project_dir = os.path.basename(os.path.dirname(build_path))
        
        # Try multiple naming patterns
        binary_names = [
            f'{project_dir}.bin',
            'app.bin',
            f'{project_dir}-go.bin',
        ]
        
        for bin_name in binary_names:
            project_bin = os.path.join(build_path, bin_name)
            if os.path.exists(project_bin) and os.path.getsize(project_bin) > 10000:
                return project_bin
        
        # Look for largest .bin file
        binaries = list(Path(build_path).glob('*.bin'))
        large_binaries = [b for b in binaries if b.stat().st_size > 100000]
        
        if large_binaries:
            largest = max(large_binaries, key=lambda f: f.stat().st_size)
            return str(largest)
        
        return None
    
    def copy_app_binary(build_path, app_name):
        """Copy app binary from build directory"""
        binary_file = find_binary(build_path)
        if not binary_file:
            return None
        
        app_binary_name = app_name.lower().replace(' ', '_') + '.bin'
        dest_path = os.path.join(apps_download_dir, app_binary_name)
        
        try:
            import shutil
            shutil.copy2(binary_file, dest_path)
            return dest_path
        except Exception as e:
            print(f"  ✗ Error copying binary: {e}")
            return None
    
    with app.app_context():
        if args.app:
            # Update specific app
            app_obj = App.query.filter_by(name=args.app).first()
            if not app_obj:
                print(f"✗ App '{args.app}' not found")
                return
            apps_to_update = [app_obj]
        elif args.all:
            # Update all apps
            apps_to_update = App.query.all()
        else:
            print("Please specify --all or --app <name>")
            return
        
        updated_count = 0
        for app_obj in apps_to_update:
            build_path = app_build_map.get(app_obj.name)
            
            if not build_path:
                print(f"⊘ {app_obj.name}: No build path configured")
                continue
            
            print(f"Updating {app_obj.name}...")
            file_path = copy_app_binary(build_path, app_obj.name)
            
            if file_path and os.path.exists(file_path):
                # Update app with new binary info
                app_obj.file_path = file_path
                app_obj.file_size = os.path.getsize(file_path)
                app_obj.file_hash = calculate_file_hash(file_path)
                app_obj.last_updated = datetime.utcnow()
                app_obj.version = bump_version(app_obj.version)

                if app_obj.name in extension_map:
                    app_obj.extensions = extension_map[app_obj.name]
                
                print(f"  ✓ Updated binary ({app_obj.file_size / 1024 / 1024:.2f} MB)")
                updated_count += 1
            else:
                print(f"  ✗ No binary found in {build_path}")
        
        if updated_count > 0:
            db.session.commit()
            print(f"\n✓ Successfully updated {updated_count} apps")
        else:
            print("\nNo apps were updated")

def cmd_add_app(args):

    """Add a new app to the store"""
    app = create_app()
    
    print("\n=== Add New App ===")
    
    # Get input
    name = input("App name: ")
    description = input("Description: ")
    version = input("Version (default: 1.0.0): ") or "1.0.0"
    author = input("Author: ")
    category = input("Category (game/utility/emulator/tool): ")
    
    price_str = input("Price in USD (0 for free): ")
    try:
        price = float(price_str) if price_str else 0.0
    except ValueError:
        price = 0.0
    
    file_path = input("Path to app binary file: ")
    
    if not os.path.exists(file_path):
        print(f"✗ File not found: {file_path}")
        return
    
    file_size = os.path.getsize(file_path)
    file_hash = calculate_file_hash(file_path)
    
    min_version = input("Minimum retro-go version (optional): ") or None
    required_space = input("Required storage in MB (optional): ")
    
    try:
        required_space = int(required_space) * 1024 * 1024 if required_space else None
    except ValueError:
        required_space = None
    
    # Add to database
    with app.app_context():
        app_obj = App(
            name=name,
            description=description,
            version=version,
            author=author,
            category=category,
            file_path=file_path,
            file_size=file_size,
            file_hash=file_hash,
            price=price,
            currency="USD",
            min_version=min_version,
            required_space=required_space,
        )
        
        db.session.add(app_obj)
        db.session.commit()
        
        print(f"\n✓ App added successfully!")
        print(f"  ID: {app_obj.id}")
        print(f"  Name: {app_obj.name}")
        print(f"  Size: {file_size / (1024*1024):.2f} MB")
        print(f"  Hash: {file_hash}")

def cmd_list_apps(args):
    """List all apps in the store"""
    app = create_app()
    
    with app.app_context():
        apps = App.query.all()
        
        if not apps:
            print("No apps found")
            return
        
        print(f"\n{'ID':<37} {'Name':<30} {'Version':<10} {'Price':<8} {'DL':<6}")
        print("-" * 95)
        
        for app_obj in apps:
            price_str = "FREE" if app_obj.price == 0 else f"${app_obj.price:.2f}"
            print(f"{app_obj.id:<37} {app_obj.name:<30} {app_obj.version:<10} {price_str:<8} {app_obj.download_count:<6}")
        
        print(f"\nTotal: {len(apps)} apps")
        total_size = sum(a.file_size for a in apps)
        print(f"Total size: {total_size / (1024*1024*1024):.2f} GB")

def cmd_list_devices(args):
    """List all registered devices"""
    app = create_app()
    
    with app.app_context():
        devices = Device.query.all()
        
        if not devices:
            print("No devices registered")
            return
        
        print(f"\n{'Device ID':<20} {'Name':<20} {'Model':<15} {'Last Seen':<20}")
        print("-" * 75)
        
        for device in devices:
            last_seen = device.last_seen.strftime("%Y-%m-%d %H:%M:%S") if device.last_seen else "Never"
            print(f"{device.id:<20} {device.name or '-':<20} {device.model or '-':<15} {last_seen:<20}")
        
        print(f"\nTotal: {len(devices)} devices")
        
        # Show device stats
        installations = Installation.query.count()
        licenses = License.query.count()
        
        print(f"\nStats:")
        print(f"  Total installations: {installations}")
        print(f"  Total licenses: {licenses}")

def cmd_create_license(args):
    """Create a license for an app"""
    app = create_app()
    
    print("\n=== Create License ===")
    
    app_id = input("App ID: ")
    device_id = input("Device ID: ")
    is_trial = input("Is trial? (y/n): ").lower() == 'y'
    trial_days = 7
    expires_days = 365
    
    if is_trial:
        trial_days = int(input("Trial days (default: 7): ") or "7")
    else:
        expires_days = int(input("Expires days (default: 365, 0 for lifetime): ") or "365")
    
    with app.app_context():
        # Check if app exists
        app_obj = App.query.get(app_id)
        if not app_obj:
            print(f"✗ App not found: {app_id}")
            return
        
        # Check if license already exists
        existing = License.query.filter_by(
            app_id=app_id,
            device_id=device_id
        ).first()
        
        if existing and existing.is_valid():
            print("✗ Valid license already exists for this app/device combination")
            return
        
        # Create license
        import uuid
        license_obj = License(
            app_id=app_id,
            device_id=device_id,
            license_key=str(uuid.uuid4()),
            is_trial=is_trial
        )
        
        if is_trial:
            license_obj.trial_expires = datetime.utcnow() + timedelta(days=trial_days)
        elif expires_days > 0:
            license_obj.expires_date = datetime.utcnow() + timedelta(days=expires_days)
        
        db.session.add(license_obj)
        db.session.commit()
        
        print(f"\n✓ License created successfully!")
        print(f"  Key: {license_obj.license_key}")
        print(f"  App: {app_obj.name}")
        print(f"  Device: {device_id}")
        print(f"  Trial: {is_trial}")
        if is_trial:
            print(f"  Expires: {license_obj.trial_expires.strftime('%Y-%m-%d')}")
        elif license_obj.expires_date:
            print(f"  Expires: {license_obj.expires_date.strftime('%Y-%m-%d')}")

def cmd_show_stats(args):
    """Show app store statistics"""
    app = create_app()
    
    with app.app_context():
        total_apps = App.query.count()
        enabled_apps = App.query.filter_by(enabled=True).count()
        total_devices = Device.query.count()
        total_installations = Installation.query.count()
        total_licenses = License.query.count()
        total_reviews = Review.query.count()
        
        total_downloads = db.session.query(
            db.func.sum(App.download_count)
        ).scalar() or 0
        
        total_size = db.session.query(
            db.func.sum(App.file_size)
        ).scalar() or 0
        
        print("\n=== App Store Statistics ===")
        print(f"\nApps:")
        print(f"  Total: {total_apps}")
        print(f"  Enabled: {enabled_apps}")
        print(f"  Total size: {total_size / (1024*1024*1024):.2f} GB")
        
        print(f"\nDevices:")
        print(f"  Registered: {total_devices}")
        
        print(f"\nInstallations:")
        print(f"  Total: {total_installations}")
        print(f"  Downloads: {total_downloads}")
        
        print(f"\nLicenses:")
        print(f"  Total: {total_licenses}")
        
        print(f"\nReviews:")
        print(f"  Total: {total_reviews}")
        
        # Top apps
        print(f"\nTop 5 Apps:")
        top_apps = App.query.order_by(App.download_count.desc()).limit(5).all()
        for i, app_obj in enumerate(top_apps, 1):
            print(f"  {i}. {app_obj.name} ({app_obj.download_count} downloads)")

def cmd_export(args):
    """Export database to JSON"""
    app = create_app()
    output_file = args.output or "appstore_export.json"
    
    with app.app_context():
        data = {
            "exported_at": datetime.utcnow().isoformat(),
            "apps": [app_obj.to_dict(include_file=True) for app_obj in App.query.all()],
            "devices": [dev.to_dict() for dev in Device.query.all()],
            "installations": [inst.to_dict() for inst in Installation.query.all()],
        }
        
        with open(output_file, 'w') as f:
            json.dump(data, f, indent=2)
        
        print(f"✓ Database exported to {output_file}")
        print(f"  Apps: {len(data['apps'])}")
        print(f"  Devices: {len(data['devices'])}")
        print(f"  Installations: {len(data['installations'])}")

def main():
    parser = argparse.ArgumentParser(
        description="Retro-Go App Store Management"
    )
    
    subparsers = parser.add_subparsers(dest='command', help='Commands')
    
    # Init command
    subparsers.add_parser('init', help='Initialize database')
    
    # Populate apps command
    subparsers.add_parser('populate-apps', help='Add standard Retro-Go apps to the store')
    
    # Update apps command
    update_parser = subparsers.add_parser('update-apps', help='Update existing apps with new binaries')
    update_parser.add_argument('--all', action='store_true', help='Update all apps')
    update_parser.add_argument('--app', help='Update specific app by name')
    
    # Add app command
    subparsers.add_parser('add-app', help='Add a new app to the store (interactive)')
    
    # List apps command
    subparsers.add_parser('list-apps', help='List all apps')
    
    # List devices command
    subparsers.add_parser('list-devices', help='List registered devices')
    
    # Create license command
    subparsers.add_parser('create-license', help='Create a license')
    
    # Stats command
    subparsers.add_parser('show-stats', help='Show statistics')
    
    # Export command
    export_parser = subparsers.add_parser('export', help='Export database to JSON')
    export_parser.add_argument('-o', '--output', help='Output file (default: appstore_export.json)')
    
    args = parser.parse_args()
    
    if not args.command:
        parser.print_help()
        sys.exit(1)
    
    # Route to command handler
    if args.command == 'init':
        cmd_init(args)
    elif args.command == 'populate-apps':
        cmd_populate_apps(args)
    elif args.command == 'update-apps':
        cmd_update_apps(args)
    elif args.command == 'add-app':
        cmd_add_app(args)
    elif args.command == 'list-apps':
        cmd_list_apps(args)
    elif args.command == 'list-devices':
        cmd_list_devices(args)
    elif args.command == 'create-license':
        cmd_create_license(args)
    elif args.command == 'show-stats':
        cmd_show_stats(args)
    elif args.command == 'export':
        cmd_export(args)
    else:
        parser.print_help()

if __name__ == '__main__':
    main()
