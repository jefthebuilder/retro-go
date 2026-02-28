#!/usr/bin/env python3
"""
Retro-Go App Store Server
Run the Flask app store server
"""
import os
import sys
from app import create_app

if __name__ == '__main__':
    config_name = os.environ.get('FLASK_ENV', 'development')
    app = create_app(config_name)
    
    host = os.environ.get('FLASK_HOST', '0.0.0.0')
    port = int(os.environ.get('FLASK_PORT', 5000))
    debug = config_name == 'development'
    
    print(f"Starting Retro-Go App Store Server")
    print(f"Environment: {config_name}")
    print(f"Server: http://{host}:{port}")
    print(f"Debug: {debug}")
    
    app.run(host=host, port=port, debug=debug)
