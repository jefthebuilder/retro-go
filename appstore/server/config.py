"""Configuration for the App Store Server"""
import os
from datetime import timedelta

class Config:
    """Base configuration"""
    # Flask
    SECRET_KEY = os.environ.get('SECRET_KEY') or 'dev-secret-key-change-in-production'
    
    # Database
    SQLALCHEMY_DATABASE_URI = os.environ.get('DATABASE_URL') or 'sqlite:///appstore.db'
    SQLALCHEMY_TRACK_MODIFICATIONS = False
    
    # App Store
    MAX_APP_SIZE = 50 * 1024 * 1024  # 50MB max app size
    UPLOAD_FOLDER = os.path.join(os.path.dirname(__file__), 'apps')
    ALLOWED_EXTENSIONS = {'zip', 'bin', 'img'}
    
    # Session
    PERMANENT_SESSION_LIFETIME = timedelta(days=30)
    SESSION_COOKIE_SECURE = False  # Set to True in production with HTTPS
    SESSION_COOKIE_HTTPONLY = True
    
    # API
    JSON_SORT_KEYS = False
    
    # Device configuration
    DEVICE_TIMEOUT = 30  # seconds
    
class DevelopmentConfig(Config):
    """Development configuration"""
    DEBUG = True
    TESTING = False

class ProductionConfig(Config):
    """Production configuration"""
    DEBUG = False
    TESTING = False
    SESSION_COOKIE_SECURE = True

class TestingConfig(Config):
    """Testing configuration"""
    TESTING = True
    SQLALCHEMY_DATABASE_URI = 'sqlite:///:memory:'
    WTF_CSRF_ENABLED = False

config = {
    'development': DevelopmentConfig,
    'production': ProductionConfig,
    'testing': TestingConfig,
    'default': DevelopmentConfig
}
