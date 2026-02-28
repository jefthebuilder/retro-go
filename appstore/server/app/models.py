"""Database models for the App Store"""
from flask_sqlalchemy import SQLAlchemy
from datetime import datetime
import uuid
import hashlib

db = SQLAlchemy()

class App(db.Model):
    """Model for applications in the store"""
    __tablename__ = 'apps'
    
    id = db.Column(db.String(36), primary_key=True, default=lambda: str(uuid.uuid4()))
    name = db.Column(db.String(128), nullable=False, unique=True, index=True)
    description = db.Column(db.Text, nullable=False)
    version = db.Column(db.String(32), nullable=False, default='1.0.0')
    author = db.Column(db.String(128), nullable=False)
    icon_url = db.Column(db.String(256))
    category = db.Column(db.String(64), nullable=False)  # 'game', 'utility', 'emulator', etc.
    extensions = db.Column(db.String(128), default='bin rom')  # Supported file extensions (space-separated)
    
    # File information
    file_path = db.Column(db.String(256), nullable=True)  # Optional for pre-installed apps
    file_size = db.Column(db.Integer, nullable=True)  # in bytes
    file_hash = db.Column(db.String(64), nullable=True)  # SHA256
    
    # Pricing
    price = db.Column(db.Float, default=0.0)  # 0.0 = free
    currency = db.Column(db.String(3), default='USD')
    
    # Metadata
    rating = db.Column(db.Float, default=0.0)  # 0-5 stars
    download_count = db.Column(db.Integer, default=0)
    release_date = db.Column(db.DateTime, default=datetime.utcnow)
    last_updated = db.Column(db.DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)
    
    # Status
    enabled = db.Column(db.Boolean, default=True, index=True)
    featured = db.Column(db.Boolean, default=False)
    
    # Requirements
    min_version = db.Column(db.String(32))  # Minimum retro-go version
    required_space = db.Column(db.Integer)  # Required storage in bytes
    
    # Relationships
    licenses = db.relationship('License', backref='app', lazy=True, cascade='all, delete-orphan')
    reviews = db.relationship('Review', backref='app', lazy=True, cascade='all, delete-orphan')
    installations = db.relationship('Installation', backref='app', lazy=True, cascade='all, delete-orphan')
    
    def to_dict(self, include_file=False):
        """Convert to dictionary representation"""
        data = {
            'id': self.id,
            'name': self.name,
            'description': self.description,
            'version': self.version,
            'author': self.author,
            'icon_url': self.icon_url,
            'category': self.category,
            'price': self.price,
            'currency': self.currency,
            'rating': self.rating,
            'download_count': self.download_count,
            'release_date': self.release_date.isoformat(),
            'featured': self.featured,
            'extensions': self.extensions or 'bin rom',
            'min_version': self.min_version,
            'required_space': self.required_space,
        }
        if include_file:
            data['file_size'] = self.file_size
            data['file_hash'] = self.file_hash
        return data

class License(db.Model):
    """Model for app licenses (purchase tracking)"""
    __tablename__ = 'licenses'
    
    id = db.Column(db.String(36), primary_key=True, default=lambda: str(uuid.uuid4()))
    app_id = db.Column(db.String(36), db.ForeignKey('apps.id'), nullable=False, index=True)
    device_id = db.Column(db.String(128), nullable=False, index=True)
    license_key = db.Column(db.String(256), nullable=False, unique=True, index=True)
    
    # License status
    purchased_date = db.Column(db.DateTime, default=datetime.utcnow)
    expires_date = db.Column(db.DateTime)  # None = lifetime license
    activated_date = db.Column(db.DateTime)
    is_active = db.Column(db.Boolean, default=True)
    
    # Trial
    is_trial = db.Column(db.Boolean, default=False)
    trial_expires = db.Column(db.DateTime)
    
    def is_valid(self):
        """Check if license is valid"""
        if not self.is_active:
            return False
        if self.expires_date and datetime.utcnow() > self.expires_date:
            return False
        if self.is_trial and self.trial_expires and datetime.utcnow() > self.trial_expires:
            return False
        return True
    
    def to_dict(self):
        """Convert to dictionary representation"""
        return {
            'license_key': self.license_key,
            'is_valid': self.is_valid(),
            'is_trial': self.is_trial,
            'purchased_date': self.purchased_date.isoformat(),
            'expires_date': self.expires_date.isoformat() if self.expires_date else None,
            'trial_expires': self.trial_expires.isoformat() if self.trial_expires else None,
        }

class Review(db.Model):
    """Model for app reviews and ratings"""
    __tablename__ = 'reviews'
    
    id = db.Column(db.String(36), primary_key=True, default=lambda: str(uuid.uuid4()))
    app_id = db.Column(db.String(36), db.ForeignKey('apps.id'), nullable=False, index=True)
    device_id = db.Column(db.String(128), nullable=False)
    
    rating = db.Column(db.Integer, nullable=False)  # 1-5 stars
    title = db.Column(db.String(256))
    comment = db.Column(db.Text)
    created_date = db.Column(db.DateTime, default=datetime.utcnow)
    
    def to_dict(self):
        """Convert to dictionary representation"""
        return {
            'id': self.id,
            'rating': self.rating,
            'title': self.title,
            'comment': self.comment,
            'created_date': self.created_date.isoformat(),
        }

class Installation(db.Model):
    """Track app installations on devices"""
    __tablename__ = 'installations'
    
    id = db.Column(db.String(36), primary_key=True, default=lambda: str(uuid.uuid4()))
    app_id = db.Column(db.String(36), db.ForeignKey('apps.id'), nullable=False, index=True)
    device_id = db.Column(db.String(128), nullable=False, index=True)
    
    installed_date = db.Column(db.DateTime, default=datetime.utcnow)
    last_launched = db.Column(db.DateTime)
    last_version = db.Column(db.String(32))
    status = db.Column(db.String(32), default='installed')  # 'installed', 'updating', 'error'
    
    __table_args__ = (
        db.UniqueConstraint('app_id', 'device_id', name='_app_device_uc'),
    )
    
    def to_dict(self):
        """Convert to dictionary representation"""
        return {
            'app_id': self.app_id,
            'device_id': self.device_id,
            'installed_date': self.installed_date.isoformat(),
            'last_launched': self.last_launched.isoformat() if self.last_launched else None,
            'last_version': self.last_version,
            'status': self.status,
        }

class Device(db.Model):
    """Track registered devices"""
    __tablename__ = 'devices'
    
    id = db.Column(db.String(128), primary_key=True)  # Device MAC or unique identifier
    name = db.Column(db.String(128))
    model = db.Column(db.String(64))  # e.g., 'ODROID-GO', 'MRGC-G32', etc.
    firmware_version = db.Column(db.String(32))
    
    first_seen = db.Column(db.DateTime, default=datetime.utcnow)
    last_seen = db.Column(db.DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)
    
    storage_total = db.Column(db.Integer)  # in bytes
    storage_used = db.Column(db.Integer)
    
    # Device settings
    auto_update = db.Column(db.Boolean, default=True)
    
    def to_dict(self):
        """Convert to dictionary representation"""
        return {
            'id': self.id,
            'name': self.name,
            'model': self.model,
            'firmware_version': self.firmware_version,
            'first_seen': self.first_seen.isoformat(),
            'last_seen': self.last_seen.isoformat(),
            'storage_total': self.storage_total,
            'storage_used': self.storage_used,
            'auto_update': self.auto_update,
        }
