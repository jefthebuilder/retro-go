"""Admin API endpoints for managing the app store"""
from flask import Blueprint, request, jsonify, current_app
from ..models import db, App, Device, Installation, License, Review
from datetime import datetime, timedelta
import os
import hashlib
import uuid
from werkzeug.utils import secure_filename

admin_bp = Blueprint('admin', __name__, url_prefix='/api/v1/admin')

# Dummy authentication - replace with proper auth in production
def require_api_key(f):
    """Decorator to require API key for admin endpoints"""
    from functools import wraps
    @wraps(f)
    def decorated_function(*args, **kwargs):
        api_key = request.headers.get('X-API-Key')
        if not api_key or api_key != os.environ.get('ADMIN_API_KEY', 'dev-api-key'):
            return jsonify({'error': 'Unauthorized'}), 401
        return f(*args, **kwargs)
    return decorated_function

def calculate_file_hash(filepath):
    """Calculate SHA256 hash of a file"""
    sha256_hash = hashlib.sha256()
    with open(filepath, "rb") as f:
        for byte_block in iter(lambda: f.read(4096), b""):
            sha256_hash.update(byte_block)
    return sha256_hash.hexdigest()

@admin_bp.route('/apps', methods=['POST'])
@require_api_key
def create_app():
    """Create a new app listing
    
    Expected JSON:
    {
        "name": "My App",
        "description": "App description",
        "version": "1.0.0",
        "author": "Author Name",
        "category": "game",
        "price": 0.99,
        "currency": "USD",
        "min_version": "1.0.0",
        "required_space": 10000000,
        "file_path": "/path/to/app.bin"
    }
    """
    data = request.get_json()
    
    required_fields = ['name', 'description', 'author', 'category', 'file_path']
    if not all(field in data for field in required_fields):
        return jsonify({'error': f'Missing required fields: {required_fields}'}), 400
    
    # Check if file exists
    if not os.path.exists(data['file_path']):
        return jsonify({'error': 'File not found'}), 400
    
    file_size = os.path.getsize(data['file_path'])
    if file_size > current_app.config['MAX_APP_SIZE']:
        return jsonify({'error': f'File size exceeds maximum of {current_app.config["MAX_APP_SIZE"]} bytes'}), 400
    
    # Calculate file hash
    file_hash = calculate_file_hash(data['file_path'])
    
    app = App(
        name=data['name'],
        description=data['description'],
        version=data.get('version', '1.0.0'),
        author=data['author'],
        icon_url=data.get('icon_url'),
        category=data['category'],
        file_path=data['file_path'],
        file_size=file_size,
        file_hash=file_hash,
        price=data.get('price', 0.0),
        currency=data.get('currency', 'USD'),
        min_version=data.get('min_version'),
        required_space=data.get('required_space'),
    )
    
    db.session.add(app)
    db.session.commit()
    
    return jsonify({
        'status': 'success',
        'app': app.to_dict(include_file=True)
    }), 201

@admin_bp.route('/apps/<app_id>', methods=['PUT'])
@require_api_key
def update_app(app_id):
    """Update app information"""
    app = App.query.get(app_id)
    
    if not app:
        return jsonify({'error': 'App not found'}), 404
    
    data = request.get_json()
    
    # Update fields
    if 'name' in data:
        app.name = data['name']
    if 'description' in data:
        app.description = data['description']
    if 'version' in data:
        app.version = data['version']
    if 'author' in data:
        app.author = data['author']
    if 'icon_url' in data:
        app.icon_url = data['icon_url']
    if 'category' in data:
        app.category = data['category']
    if 'price' in data:
        app.price = data['price']
    if 'currency' in data:
        app.currency = data['currency']
    if 'enabled' in data:
        app.enabled = data['enabled']
    if 'featured' in data:
        app.featured = data['featured']
    if 'min_version' in data:
        app.min_version = data['min_version']
    if 'required_space' in data:
        app.required_space = data['required_space']
    
    # Update file if provided
    if 'file_path' in data:
        if not os.path.exists(data['file_path']):
            return jsonify({'error': 'File not found'}), 400
        
        file_size = os.path.getsize(data['file_path'])
        if file_size > current_app.config['MAX_APP_SIZE']:
            return jsonify({'error': f'File size exceeds maximum'}), 400
        
        app.file_path = data['file_path']
        app.file_size = file_size
        app.file_hash = calculate_file_hash(data['file_path'])
    
    app.last_updated = datetime.utcnow()
    db.session.commit()
    
    return jsonify({
        'status': 'success',
        'app': app.to_dict(include_file=True)
    }), 200

@admin_bp.route('/apps/<app_id>', methods=['DELETE'])
@require_api_key
def delete_app(app_id):
    """Delete an app (soft delete - just disable it)"""
    app = App.query.get(app_id)
    
    if not app:
        return jsonify({'error': 'App not found'}), 404
    
    app.enabled = False
    db.session.commit()
    
    return jsonify({
        'status': 'success',
        'message': f'App {app_id} disabled'
    }), 200

@admin_bp.route('/apps', methods=['GET'])
@require_api_key
def list_all_apps():
    """List all apps (admin view)"""
    limit = int(request.args.get('limit', 50))
    offset = int(request.args.get('offset', 0))
    
    apps = App.query.offset(offset).limit(limit).all()
    total = App.query.count()
    
    return jsonify({
        'status': 'success',
        'total': total,
        'apps': [app.to_dict(include_file=True) for app in apps]
    }), 200

@admin_bp.route('/licenses', methods=['POST'])
@require_api_key
def create_license():
    """Create a license for an app
    
    Expected JSON:
    {
        "app_id": "app-uuid",
        "device_id": "aa:bb:cc:dd:ee:ff",
        "is_trial": false,
        "trial_days": 7,
        "expires_days": 365
    }
    """
    data = request.get_json()
    
    if not data or 'app_id' not in data or 'device_id' not in data:
        return jsonify({'error': 'Missing app_id or device_id'}), 400
    
    app = App.query.get(data['app_id'])
    if not app:
        return jsonify({'error': 'App not found'}), 404
    
    # Check if license already exists
    existing = License.query.filter_by(
        app_id=data['app_id'],
        device_id=data['device_id']
    ).first()
    
    if existing and existing.is_valid():
        return jsonify({'error': 'Device already has a valid license'}), 400
    
    license_key = str(uuid.uuid4())
    
    license_obj = License(
        app_id=data['app_id'],
        device_id=data['device_id'],
        license_key=license_key,
        is_trial=data.get('is_trial', False)
    )
    
    # Set trial expiration
    if license_obj.is_trial:
        trial_days = data.get('trial_days', 7)
        license_obj.trial_expires = datetime.utcnow() + timedelta(days=trial_days)
    
    # Set license expiration
    if 'expires_days' in data:
        license_obj.expires_date = datetime.utcnow() + timedelta(days=data['expires_days'])
    
    db.session.add(license_obj)
    db.session.commit()
    
    return jsonify({
        'status': 'success',
        'license': license_obj.to_dict()
    }), 201

@admin_bp.route('/licenses/<license_key>', methods=['PUT'])
@require_api_key
def update_license(license_key):
    """Update a license"""
    license_obj = License.query.filter_by(license_key=license_key).first()
    
    if not license_obj:
        return jsonify({'error': 'License not found'}), 404
    
    data = request.get_json()
    
    if 'is_active' in data:
        license_obj.is_active = data['is_active']
    if 'expires_days' in data:
        license_obj.expires_date = datetime.utcnow() + timedelta(days=data['expires_days'])
    
    db.session.commit()
    
    return jsonify({
        'status': 'success',
        'license': license_obj.to_dict()
    }), 200

@admin_bp.route('/devices', methods=['GET'])
@require_api_key
def list_devices():
    """List all registered devices"""
    limit = int(request.args.get('limit', 50))
    offset = int(request.args.get('offset', 0))
    
    devices = Device.query.offset(offset).limit(limit).all()
    total = Device.query.count()
    
    return jsonify({
        'status': 'success',
        'total': total,
        'devices': [dev.to_dict() for dev in devices]
    }), 200

@admin_bp.route('/devices/<device_id>', methods=['GET'])
@require_api_key
def get_device(device_id):
    """Get device information with stats"""
    device = Device.query.get(device_id)
    
    if not device:
        return jsonify({'error': 'Device not found'}), 404
    
    installations = Installation.query.filter_by(device_id=device_id).count()
    licenses = License.query.filter_by(device_id=device_id).count()
    reviews = Review.query.filter_by(device_id=device_id).count()
    
    data = device.to_dict()
    data['stats'] = {
        'installed_apps': installations,
        'licenses': licenses,
        'reviews': reviews
    }
    
    return jsonify({
        'status': 'success',
        'device': data
    }), 200

@admin_bp.route('/stats', methods=['GET'])
@require_api_key
def get_stats():
    """Get app store statistics"""
    return jsonify({
        'status': 'success',
        'stats': {
            'total_apps': App.query.count(),
            'enabled_apps': App.query.filter_by(enabled=True).count(),
            'total_devices': Device.query.count(),
            'total_installations': Installation.query.count(),
            'total_licenses': License.query.count(),
            'total_downloads': db.session.query(db.func.sum(App.download_count)).scalar() or 0,
            'total_reviews': Review.query.count(),
        }
    }), 200
@admin_bp.route('/upload', methods=['POST'])
@require_api_key
def upload_app():
    """Upload and register an app without restarting the server
    
    Accepts multipart form data:
    - file: The app binary (.bin file)
    - name: App display name
    - version: Version string (e.g., "1.0.0")
    - author: Author name
    - description: App description
    - category: Category (utility, game, emulator, tool)
    - extensions: Comma-separated file extensions (optional)
    - featured: Boolean, whether to feature the app (optional)
    """
    
    # Check for file
    if 'file' not in request.files:
        return jsonify({'error': 'No file provided'}), 400
    
    file = request.files['file']
    if file.filename == '':
        return jsonify({'error': 'No file selected'}), 400
    
    # Check required form fields
    required_fields = ['name', 'version', 'author', 'description', 'category']
    for field in required_fields:
        if field not in request.form:
            return jsonify({'error': f'Missing required field: {field}'}), 400
    
    # Save uploaded file
    apps_dir = os.path.join(os.path.dirname(__file__), '..', '..', 'apps')
    os.makedirs(apps_dir, exist_ok=True)
    
    # Use secure filename and preserve .bin extension
    filename = secure_filename(request.form['name'].lower().replace(' ', '_'))
    if not filename.endswith('.bin'):
        filename += '.bin'
    
    file_path = os.path.join(apps_dir, filename)
    file.save(file_path)
    
    # Verify file size
    file_size = os.path.getsize(file_path)
    if file_size > current_app.config.get('MAX_APP_SIZE', 1638400):  # 1.56MB default
        os.remove(file_path)
        return jsonify({'error': f'File size {file_size} exceeds maximum of {current_app.config.get("MAX_APP_SIZE", 1638400)} bytes'}), 400
    
    # Calculate file hash
    file_hash = calculate_file_hash(file_path)
    
    # Check if app with same name already exists
    existing_app = App.query.filter_by(name=request.form['name']).first()
    
    if existing_app:
        # Update existing app
        existing_app.version = request.form['version']
        existing_app.author = request.form['author']
        existing_app.description = request.form['description']
        existing_app.category = request.form['category']
        existing_app.extensions = request.form.get('extensions', '')
        existing_app.file_path = file_path
        existing_app.file_size = file_size
        existing_app.file_hash = file_hash
        existing_app.enabled = True
        existing_app.featured = request.form.get('featured', 'false').lower() == 'true'
        existing_app.last_updated = datetime.utcnow()
        db.session.commit()
        
        return jsonify({
            'status': 'success',
            'message': f'App "{request.form["name"]}" updated',
            'app': existing_app.to_dict(include_file=True)
        }), 200
    else:
        # Create new app
        new_app = App(
            name=request.form['name'],
            description=request.form['description'],
            version=request.form['version'],
            author=request.form['author'],
            category=request.form['category'],
            extensions=request.form.get('extensions', ''),
            file_path=file_path,
            file_size=file_size,
            file_hash=file_hash,
            enabled=True,
            featured=request.form.get('featured', 'false').lower() == 'true',
        )
        db.session.add(new_app)
        db.session.commit()
        
        return jsonify({
            'status': 'success',
            'message': f'App "{request.form["name"]}" uploaded successfully',
            'app': new_app.to_dict(include_file=True)
        }), 201