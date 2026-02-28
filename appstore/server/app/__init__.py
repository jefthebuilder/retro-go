"""Flask application factory"""
from flask import Flask, render_template
from config import config
from app.models import db
from app.api.device_api import device_bp
from app.api.admin_api import admin_bp
import os

def create_app(config_name=None):
    """Create and configure the Flask application"""
    
    if config_name is None:
        config_name = os.environ.get('FLASK_ENV', 'development')
    
    # Setup Flask with template and static folders
    app_dir = os.path.dirname(os.path.abspath(__file__))
    server_dir = os.path.dirname(app_dir)
    
    app = Flask(
        __name__,
        template_folder=os.path.join(server_dir, 'templates'),
        static_folder=os.path.join(server_dir, 'static')
    )
    
    # Load configuration
    app.config.from_object(config[config_name])
    
    # Ensure directories exist
    os.makedirs(app.config['UPLOAD_FOLDER'], exist_ok=True)
    
    # Initialize database
    db.init_app(app)
    
    # Register blueprints
    app.register_blueprint(device_bp)
    app.register_blueprint(admin_bp)
    
    # Create tables
    with app.app_context():
        db.create_all()
    
    # Add routes
    @app.route('/')
    def index():
        """Web UI for app store"""
        return render_template('index.html')
    
    @app.route('/api')
    def api_docs():
        """API documentation"""
        return {
            'name': 'Retro-Go App Store API',
            'version': '1.0.0',
            'description': 'RESTful API for managing and distributing apps for Retro-Go devices',
            'endpoints': {
                'device': {
                    'register': 'POST /api/v1/device/register',
                    'list_apps': 'GET /api/v1/device/apps',
                    'get_app': 'GET /api/v1/device/apps/<app_id>',
                    'download_app': 'GET /api/v1/device/apps/<app_id>/download?device_id=<id>',
                    'install_app': 'POST /api/v1/device/apps/<app_id>/install?device_id=<id>',
                    'uninstall_app': 'POST /api/v1/device/apps/<app_id>/uninstall?device_id=<id>',
                    'rate_app': 'POST /api/v1/device/apps/<app_id>/rate?device_id=<id>',
                    'check_license': 'GET /api/v1/device/licenses/<app_id>?device_id=<id>',
                    'list_categories': 'GET /api/v1/device/categories',
                    'get_device_apps': 'GET /api/v1/device/devices/<device_id>/apps',
                    'report_stats': 'POST /api/v1/device/stats',
                },
                'admin': {
                    'create_app': 'POST /api/v1/admin/apps',
                    'update_app': 'PUT /api/v1/admin/apps/<app_id>',
                    'delete_app': 'DELETE /api/v1/admin/apps/<app_id>',
                    'list_apps': 'GET /api/v1/admin/apps',
                    'create_license': 'POST /api/v1/admin/licenses',
                    'update_license': 'PUT /api/v1/admin/licenses/<license_key>',
                    'list_devices': 'GET /api/v1/admin/devices',
                    'get_device': 'GET /api/v1/admin/devices/<device_id>',
                    'get_stats': 'GET /api/v1/admin/stats',
                }
            }
        }, 200
    
    @app.errorhandler(404)
    def not_found(error):
        return {'error': 'Not found'}, 404
    
    @app.errorhandler(500)
    def server_error(error):
        return {'error': 'Internal server error'}, 500
    
    return app

if __name__ == '__main__':
    app = create_app()
    app.run(host='0.0.0.0', port=5000, debug=True)
