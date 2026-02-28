"""Device API endpoints for retro-go devices"""
from flask import Blueprint, request, jsonify, send_file, current_app
from ..models import db, App, Device, Installation, License
from datetime import datetime, timedelta
import os
import hashlib
import uuid

device_bp = Blueprint('device', __name__, url_prefix='/api/v1/device')

def calculate_file_hash(filepath):
    """Calculate SHA256 hash of a file"""
    sha256_hash = hashlib.sha256()
    with open(filepath, "rb") as f:
        for byte_block in iter(lambda: f.read(4096), b""):
            sha256_hash.update(byte_block)
    return sha256_hash.hexdigest()

def get_or_create_device(device_id):
    """Get or create a device record"""
    device = Device.query.get(device_id)
    if not device:
        device = Device(id=device_id)
        db.session.add(device)
        db.session.commit()
    return device

@device_bp.route('/register', methods=['POST'])
def register_device():
    """Register a device with the app store
    
    Expected JSON:
    {
        "device_id": "aa:bb:cc:dd:ee:ff",
        "name": "My Retro-Go",
        "model": "ODROID-GO",
        "firmware_version": "1.0.0",
        "storage_total": 4000000000,
        "storage_used": 1000000000
    }
    """
    data = request.get_json()
    
    if not data or 'device_id' not in data:
        return jsonify({'error': 'Missing device_id'}), 400
    
    device = Device.query.get(data['device_id'])
    if not device:
        device = Device(id=data['device_id'])
    
    device.name = data.get('name', device.name)
    device.model = data.get('model', device.model)
    device.firmware_version = data.get('firmware_version', device.firmware_version)
    device.storage_total = data.get('storage_total', device.storage_total)
    device.storage_used = data.get('storage_used', device.storage_used)
    device.last_seen = datetime.utcnow()
    
    db.session.add(device)
    db.session.commit()
    
    return jsonify({
        'status': 'success',
        'device': device.to_dict()
    }), 200

@device_bp.route('/apps', methods=['GET'])
def list_apps():
    """List available apps
    
    Query parameters:
    - category: Filter by category
    - featured: Show only featured apps (true/false)
    - limit: Max results (default 50)
    - offset: Pagination offset (default 0)
    - search: Search by name or description
    """
    category = request.args.get('category')
    featured = request.args.get('featured', 'false').lower() == 'true'
    limit = min(int(request.args.get('limit', 50)), 100)
    offset = int(request.args.get('offset', 0))
    search = request.args.get('search', '')
    
    query = App.query.filter_by(enabled=True)
    
    if featured:
        query = query.filter_by(featured=True)
    
    if category:
        query = query.filter_by(category=category)
    
    if search:
        query = query.filter(
            (App.name.ilike(f'%{search}%')) |
            (App.description.ilike(f'%{search}%'))
        )
    
    total = query.count()
    apps = query.order_by(App.download_count.desc()).offset(offset).limit(limit).all()
    
    return jsonify({
        'status': 'success',
        'total': total,
        'limit': limit,
        'offset': offset,
        'apps': [app.to_dict(include_file=True) for app in apps]
    }), 200

@device_bp.route('/apps/<app_id>', methods=['GET'])
def get_app(app_id):
    """Get detailed app information"""
    app = App.query.filter_by(id=app_id, enabled=True).first()
    
    if not app:
        return jsonify({'error': 'App not found'}), 404
    
    data = app.to_dict(include_file=True)
    
    # Include reviews
    data['reviews'] = [review.to_dict() for review in app.reviews[:5]]
    
    return jsonify({
        'status': 'success',
        'app': data
    }), 200

@device_bp.route('/apps/<app_id>/download', methods=['GET'])
def download_app(app_id):
    """Download an app file
    
    Query parameters:
    - device_id: Device requesting download
    """
    device_id = request.args.get('device_id')
    
    if not device_id:
        return jsonify({'error': 'Missing device_id'}), 400
    
    app = App.query.filter_by(id=app_id, enabled=True).first()
    
    if not app:
        return jsonify({'error': 'App not found'}), 404
    
    # Check if file path is set
    if not app.file_path:
        return jsonify({'error': 'App is not downloadable (pre-installed only)'}), 404
    
    # Check if file exists
    if not os.path.exists(app.file_path):
        return jsonify({'error': f'App file not found on server: {app.file_path}'}), 500
    
    # Update device last seen
    device = get_or_create_device(device_id)
    device.last_seen = datetime.utcnow()
    db.session.commit()
    
    # Update download count
    app.download_count += 1
    db.session.commit()
    
    return send_file(
        app.file_path,
        as_attachment=True,
        download_name=f"{app.name}-{app.version}.bin"
    ), 200

@device_bp.route('/apps/<app_id>/install', methods=['POST'])
def install_app(app_id):
    """Report app installation
    
    Expected JSON:
    {
        "device_id": "aa:bb:cc:dd:ee:ff",
        "version": "1.0.0",
        "storage_used": 1000000000
    }
    """
    device_id = request.args.get('device_id')
    data = request.get_json() or {}
    
    if not device_id:
        return jsonify({'error': 'Missing device_id'}), 400
    
    app = App.query.filter_by(id=app_id, enabled=True).first()
    
    if not app:
        return jsonify({'error': 'App not found'}), 404
    
    device = get_or_create_device(device_id)
    
    # Check or create installation record
    installation = Installation.query.filter_by(app_id=app_id, device_id=device_id).first()
    if not installation:
        installation = Installation(app_id=app_id, device_id=device_id)
    
    installation.installed_date = datetime.utcnow()
    installation.last_version = data.get('version', app.version)
    installation.status = 'installed'
    
    # Update device storage
    device.storage_used = data.get('storage_used', device.storage_used)
    device.last_seen = datetime.utcnow()
    
    db.session.add(installation)
    db.session.commit()
    
    return jsonify({
        'status': 'success',
        'message': 'Installation recorded'
    }), 200

@device_bp.route('/apps/<app_id>/uninstall', methods=['POST'])
def uninstall_app(app_id):
    """Report app uninstallation
    
    Expected JSON:
    {
        "device_id": "aa:bb:cc:dd:ee:ff",
        "storage_used": 1000000000
    }
    """
    device_id = request.args.get('device_id')
    data = request.get_json() or {}
    
    if not device_id:
        return jsonify({'error': 'Missing device_id'}), 400
    
    installation = Installation.query.filter_by(app_id=app_id, device_id=device_id).first()
    
    if installation:
        db.session.delete(installation)
    
    device = get_or_create_device(device_id)
    device.storage_used = data.get('storage_used', device.storage_used)
    device.last_seen = datetime.utcnow()
    db.session.commit()
    
    return jsonify({
        'status': 'success',
        'message': 'Uninstallation recorded'
    }), 200

@device_bp.route('/apps/<app_id>/rate', methods=['POST'])
def rate_app(app_id):
    """Submit a review/rating
    
    Expected JSON:
    {
        "device_id": "aa:bb:cc:dd:ee:ff",
        "rating": 5,
        "title": "Great app!",
        "comment": "Works perfectly!"
    }
    """
    device_id = request.args.get('device_id')
    data = request.get_json()
    
    if not device_id or not data or 'rating' not in data:
        return jsonify({'error': 'Missing device_id or rating'}), 400
    
    if not 1 <= data['rating'] <= 5:
        return jsonify({'error': 'Rating must be between 1 and 5'}), 400
    
    app = App.query.filter_by(id=app_id, enabled=True).first()
    
    if not app:
        return jsonify({'error': 'App not found'}), 404
    
    from ..models import Review
    review = Review(
        app_id=app_id,
        device_id=device_id,
        rating=data['rating'],
        title=data.get('title'),
        comment=data.get('comment')
    )
    
    db.session.add(review)
    
    # Update app rating
    reviews = Review.query.filter_by(app_id=app_id).all()
    if reviews:
        avg_rating = sum(r.rating for r in reviews) / len(reviews)
        app.rating = avg_rating
    
    db.session.commit()
    
    return jsonify({
        'status': 'success',
        'message': 'Review submitted'
    }), 201

@device_bp.route('/licenses/<app_id>', methods=['GET'])
def check_license(app_id):
    """Check if app is licensed on device
    
    Query parameters:
    - device_id: Device to check
    - license_key: License key (optional)
    """
    device_id = request.args.get('device_id')
    license_key = request.args.get('license_key')
    
    if not device_id:
        return jsonify({'error': 'Missing device_id'}), 400
    
    app = App.query.filter_by(id=app_id, enabled=True).first()
    
    if not app:
        return jsonify({'error': 'App not found'}), 404
    
    # Check if app is free
    if app.price == 0:
        return jsonify({
            'status': 'success',
            'licensed': True,
            'is_free': True
        }), 200
    
    # Check license
    if license_key:
        license_obj = License.query.filter_by(
            license_key=license_key,
            device_id=device_id
        ).first()
    else:
        license_obj = License.query.filter_by(
            app_id=app_id,
            device_id=device_id
        ).first()
    
    if not license_obj:
        # Check for trial
        return jsonify({
            'status': 'success',
            'licensed': False,
            'trial_available': True
        }), 200
    
    is_valid = license_obj.is_valid()
    
    return jsonify({
        'status': 'success',
        'licensed': is_valid,
        'license': license_obj.to_dict() if is_valid else None
    }), 200

@device_bp.route('/devices/<device_id>/apps', methods=['GET'])
def get_device_apps(device_id):
    """Get list of installed apps on a device"""
    installations = Installation.query.filter_by(device_id=device_id).all()
    
    apps = []
    for inst in installations:
        app_data = inst.app.to_dict()
        app_data['installation'] = inst.to_dict()
        apps.append(app_data)
    
    return jsonify({
        'status': 'success',
        'device_id': device_id,
        'installed_apps': apps,
        'count': len(apps)
    }), 200

@device_bp.route('/categories', methods=['GET'])
def list_categories():
    """Get list of available app categories"""
    categories = db.session.query(App.category).distinct().filter(
        App.enabled == True
    ).all()
    
    return jsonify({
        'status': 'success',
        'categories': [cat[0] for cat in categories]
    }), 200

@device_bp.route('/stats', methods=['POST'])
def report_stats():
    """Report device statistics
    
    Expected JSON:
    {
        "device_id": "aa:bb:cc:dd:ee:ff",
        "uptime": 3600,
        "memory_free": 500000,
        "storage_used": 1000000000
    }
    """
    data = request.get_json()
    
    if not data or 'device_id' not in data:
        return jsonify({'error': 'Missing device_id'}), 400
    
    device = get_or_create_device(data['device_id'])
    device.storage_used = data.get('storage_used', device.storage_used)
    device.last_seen = datetime.utcnow()
    
    db.session.commit()
    
    return jsonify({
        'status': 'success',
        'message': 'Stats recorded'
    }), 200

@device_bp.route('/partitions/<device_id>', methods=['GET'])
def get_partition_info(device_id):
    """Get partition space info for a device
    
    Query parameters:
    - app_id: Optional app to check space for
    
    ESP32 configuration:
    - launcher (ota_0): 1.1 MB
    - app_1 to app_5: 1 MB each (5 total)
    - Total app space: 6 MB
    """
    app_id = request.args.get('app_id')
    
    device = get_or_create_device(device_id)
    
    # Get installed apps and their sizes
    installations = Installation.query.filter_by(device_id=device_id).all()
    
    installed_apps = []
    total_app_space = 0
    
    for inst in installations:
        app_data = {
            'id': inst.app_id,
            'name': inst.app.name,
            'version': inst.last_version,
            'size': inst.app.file_size or 0,
            'installed_date': inst.installed_date.isoformat() if inst.installed_date else None
        }
        installed_apps.append(app_data)
        total_app_space += inst.app.file_size or 0
    
    # ESP32 partition layout
    APP_PARTITION_SIZE = 1048576  # 1 MB per app partition (ota_1 to ota_5)
    MAX_APPS = 5
    PARTITION_OVERHEAD = 4096  # ~4KB per partition header
    USABLE_SIZE_PER_APP = APP_PARTITION_SIZE - PARTITION_OVERHEAD
    
    # Count how many slots are used
    used_slots = len(installed_apps)
    available_slots = MAX_APPS - used_slots
    
    # Check if we can fit a new app
    can_fit = None
    if app_id:
        app = App.query.get(app_id)
        if app:
            # Check if app fits in remaining slots
            if available_slots > 0:
                can_fit = app.file_size <= USABLE_SIZE_PER_APP if app.file_size else True
            else:
                can_fit = False
    
    return jsonify({
        'status': 'success',
        'device_id': device_id,
        'device_type': 'ESP32',
        'partition_layout': {
            'launcher': {
                'type': 'ota_0',
                'size': 1114112,  # 1.1 MB
                'offset': '0x010000'
            },
            'apps': {
                'type': 'ota_1 to ota_5',
                'size_each': APP_PARTITION_SIZE,
                'total_slots': MAX_APPS,
                'used_slots': used_slots,
                'available_slots': available_slots
            }
        },
        'total_app_partition_space': APP_PARTITION_SIZE * MAX_APPS,
        'used_app_space': total_app_space,
        'available_app_space': (available_slots * USABLE_SIZE_PER_APP),
        'partition_overhead_per_app': PARTITION_OVERHEAD,
        'usable_per_app': USABLE_SIZE_PER_APP,
        'installed_apps': installed_apps,
        'max_apps': MAX_APPS,
        'can_fit_new_app': can_fit,
        'message': f'{available_slots} app slots available' if available_slots > 0 else 'All app slots full'
    }), 200
