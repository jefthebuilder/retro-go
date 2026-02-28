# Retro-Go App Store - Complete System Guide

## Overview

A production-ready app store system for Retro-Go devices, similar to Google Play Store or Apple App Store. The system allows users to discover, download, purchase, and manage applications on their retro gaming devices.

## System Components

### 1. Backend Server (Flask + SQLAlchemy)
**Location**: `/appstore/server/`

**Components**:
- `run.py` - Flask application entry point
- `config.py` - Configuration management (dev/prod)
- `app/__init__.py` - Flask app factory
- `app/models.py` - Database models (SQLAlchemy)
- `app/api/device_api.py` - Device/user-facing REST endpoints
- `app/api/admin_api.py` - Admin management endpoints
- `templates/index.html` - Web UI with JavaScript
- `manage.py` - CLI management tool

**Database Models**:
- **App** - Application listings with metadata, pricing, ratings
- **Device** - Registered devices with hardware info
- **Installation** - Tracks installed apps per device
- **License** - Purchase and trial tracking
- **Review** - User ratings and reviews

**Key Features**:
- RESTful API for app discovery and management
- OAuth/API key authentication for admin
- File integrity via SHA256 hashing
- Download statistics and analytics
- Trial and paid app support
- User reviews and ratings

### 2. Device Client (C/Embedded)
**Location**: `/appstore/client/`

**Files**:
- `rg_appstore.h` - Public API header
- `rg_appstore.c` - Implementation (uses rg_network, rg_storage)

**Key Functions**:
```c
// Discovery
rg_appstore_get_apps()
rg_appstore_search_apps()
rg_appstore_get_featured_apps()

// Management
rg_appstore_download_app()
rg_appstore_install_app()
rg_appstore_uninstall_app()

// Licensing
rg_appstore_check_license()
rg_appstore_get_trial()

// Community
rg_appstore_submit_review()
```

**Integration**:
- Uses existing `rg_network.h` HTTP client
- Uses `rg_storage.h` for file operations
- JSON parsing via cJSON library
- Device-specific ID (MAC address)

### 3. Web UI
**Location**: `/appstore/server/templates/index.html`

**Features**:
- Responsive design (works on desktop and mobile)
- Browse apps by category
- Full-text search
- View app details, reviews, ratings
- Download apps directly
- Works with JavaScript, no build tools needed
- Beautiful gradient UI with interactive cards

## Architecture Diagram

```
┌─────────────────────────────────────────────────┐
│          Retro-Go Devices (ESP32)               │
│  ┌─────────────────────────────────────────┐   │
│  │  rg_appstore.c (Device Client)          │   │
│  │  - Discover apps                        │   │
│  │  - Download & install                  │   │
│  │  - Submit reviews                       │   │
│  └─────────────────────────────────────────┘   │
└──────────────────────┬──────────────────────────┘
                       │ HTTP/JSON
                       ▼
┌─────────────────────────────────────────────────┐
│       App Store Server (Flask)                   │
│  ┌──────────────┐  ┌──────────────┐             │
│  │ Device API   │  │ Admin API     │             │
│  │ (Public)     │  │ (Authenticated)            │
│  └──────────────┘  └──────────────┘             │
│                                                  │
│  ┌──────────────────────────────────┐          │
│  │ SQLAlchemy Models                 │          │
│  │ - Apps, Devices, Licenses,        │          │
│  │   Installations, Reviews          │          │
│  └──────────────────────────────────┘          │
│                │                                │
│                ▼                                │
│  ┌──────────────────────────────────┐          │
│  │ SQLite Database                   │          │
│  │ (PostgreSQL for production)        │          │
│  └──────────────────────────────────┘          │
└─────────────────────────────────────────────────┘
         ▲                         ▲
         │                         │
         │ Web Browser             │ Devices
         │                         │
    ┌────┴────────────────────────┴──────┐
    │   Web UI (HTML/JS/CSS)              │
    │ - Browse & search apps              │
    │ - View reviews                      │
    │ - Download management               │
    └─────────────────────────────────────┘
```

## Quick Start

### 1. Install and Run Server

```bash
cd appstore/server

# Install dependencies
pip install -r requirements.txt

# Set environment
export FLASK_ENV=development
export ADMIN_API_KEY=your-secure-key

# Initialize database
python manage.py init

# Run server
python run.py
```

Server will be at: `http://localhost:5000`

### 2. Add Sample Apps

```bash
# Interactive app addition
python manage.py add-app

# Or via API
curl -X POST http://localhost:5000/api/v1/admin/apps \
  -H "X-API-Key: your-secure-key" \
  -H "Content-Type: application/json" \
  -d '{
    "name": "My Game",
    "description": "A retro game",
    "version": "1.0.0",
    "author": "Developer",
    "category": "game",
    "file_path": "/path/to/game.bin"
  }'
```

### 3. Test Web UI

Open browser: `http://localhost:5000`

### 4. Register Device and Download App

```bash
# Register device
curl -X POST http://localhost:5000/api/v1/device/register \
  -H "Content-Type: application/json" \
  -d '{
    "device_id": "aa:bb:cc:dd:ee:ff",
    "name": "My Device",
    "model": "ODROID-GO",
    "firmware_version": "1.0.0",
    "storage_total": 4000000000,
    "storage_used": 1000000000
  }'

# List apps
curl http://localhost:5000/api/v1/device/apps?limit=10

# Download app
curl -L http://localhost:5000/api/v1/device/apps/{app_id}/download?device_id=aa:bb:cc:dd:ee:ff \
  -o game.bin
```

## API Reference

### Device Endpoints (Public)

| Method | Endpoint | Purpose |
|--------|----------|---------|
| POST | `/api/v1/device/register` | Register device |
| GET | `/api/v1/device/apps` | List apps (with filters) |
| GET | `/api/v1/device/apps/{id}` | Get app details |
| GET | `/api/v1/device/apps/{id}/download` | Download app binary |
| POST | `/api/v1/device/apps/{id}/install` | Report installation |
| POST | `/api/v1/device/apps/{id}/uninstall` | Report uninstallation |
| POST | `/api/v1/device/apps/{id}/rate` | Submit review |
| GET | `/api/v1/device/licenses/{id}` | Check license status |
| GET | `/api/v1/device/categories` | Get app categories |
| GET | `/api/v1/device/devices/{id}/apps` | Get device's apps |
| POST | `/api/v1/device/stats` | Report device stats |

### Admin Endpoints (Authenticated)

| Method | Endpoint | Purpose |
|--------|----------|---------|
| POST | `/api/v1/admin/apps` | Create app listing |
| PUT | `/api/v1/admin/apps/{id}` | Update app |
| DELETE | `/api/v1/admin/apps/{id}` | Disable app |
| GET | `/api/v1/admin/apps` | List all apps |
| POST | `/api/v1/admin/licenses` | Create license |
| PUT | `/api/v1/admin/licenses/{key}` | Update license |
| GET | `/api/v1/admin/devices` | List devices |
| GET | `/api/v1/admin/devices/{id}` | Get device info |
| GET | `/api/v1/admin/stats` | Get statistics |

## Integration with Retro-Go Launcher

See [INTEGRATION.md](INTEGRATION.md) for detailed instructions on:
1. Adding app store client to launcher
2. Building app store UI screens
3. Handling downloads and installations
4. Network and WiFi configuration
5. Error handling and user feedback

## Management Tools

### CLI Management Tool

```bash
# Initialize database
python manage.py init

# Add app interactively
python manage.py add-app

# List all apps
python manage.py list-apps

# List registered devices
python manage.py list-devices

# Create license
python manage.py create-license

# Show statistics
python manage.py show-stats

# Export database to JSON
python manage.py export -o backup.json
```

## Database Schema

### Apps Table
```sql
CREATE TABLE apps (
    id VARCHAR(36) PRIMARY KEY,
    name VARCHAR(128) UNIQUE NOT NULL,
    description TEXT,
    version VARCHAR(32),
    author VARCHAR(128),
    category VARCHAR(64),
    file_path VARCHAR(256),
    file_size INTEGER,
    file_hash VARCHAR(64),
    price FLOAT DEFAULT 0.0,
    rating FLOAT DEFAULT 0.0,
    download_count INTEGER DEFAULT 0,
    featured BOOLEAN DEFAULT 0,
    enabled BOOLEAN DEFAULT 1,
    ...
);
```

### Devices Table
```sql
CREATE TABLE devices (
    id VARCHAR(128) PRIMARY KEY,
    name VARCHAR(128),
    model VARCHAR(64),
    firmware_version VARCHAR(32),
    storage_total INTEGER,
    storage_used INTEGER,
    first_seen DATETIME,
    last_seen DATETIME,
    auto_update BOOLEAN DEFAULT 1
);
```

### Installations Table
```sql
CREATE TABLE installations (
    id VARCHAR(36) PRIMARY KEY,
    app_id VARCHAR(36) FOREIGN KEY,
    device_id VARCHAR(128) FOREIGN KEY,
    installed_date DATETIME,
    last_launched DATETIME,
    last_version VARCHAR(32),
    status VARCHAR(32),
    UNIQUE(app_id, device_id)
);
```

### Licenses Table
```sql
CREATE TABLE licenses (
    id VARCHAR(36) PRIMARY KEY,
    app_id VARCHAR(36) FOREIGN KEY,
    device_id VARCHAR(128) FOREIGN KEY,
    license_key VARCHAR(256) UNIQUE,
    is_trial BOOLEAN DEFAULT 0,
    is_active BOOLEAN DEFAULT 1,
    purchased_date DATETIME,
    expires_date DATETIME,
    trial_expires DATETIME
);
```

## Configuration

### Server Configuration (config.py)

```python
class Config:
    SECRET_KEY = 'your-secret-key'
    SQLALCHEMY_DATABASE_URI = 'sqlite:///appstore.db'
    MAX_APP_SIZE = 50 * 1024 * 1024  # 50MB
    UPLOAD_FOLDER = 'apps/'
    DEVICE_TIMEOUT = 30  # seconds
```

### Environment Variables

```bash
FLASK_ENV=development|production
FLASK_HOST=0.0.0.0
FLASK_PORT=5000
ADMIN_API_KEY=your-secure-api-key
DATABASE_URL=sqlite:///appstore.db
```

## Security

### Production Checklist

- [ ] Change all default secrets
- [ ] Enable HTTPS/SSL
- [ ] Use strong API keys
- [ ] Set up rate limiting
- [ ] Enable CORS properly
- [ ] Use PostgreSQL instead of SQLite
- [ ] Set up database backups
- [ ] Monitor server logs
- [ ] Implement proper authentication/OAuth
- [ ] Validate all user inputs
- [ ] Implement abuse detection

### API Key Management

```python
# Generated key should be:
# - Long (32+ characters)
# - Random
# - Changed regularly
# - Never committed to version control

import secrets
api_key = secrets.token_urlsafe(32)
```

## Deployment

### Using Gunicorn (Production)

```bash
pip install gunicorn

gunicorn -w 4 -b 0.0.0.0:5000 \
  --access-logfile - \
  --error-logfile - \
  run:app
```

### Using Docker

```dockerfile
FROM python:3.9-slim

WORKDIR /app
COPY requirements.txt .
RUN pip install -r requirements.txt

COPY . .

CMD ["gunicorn", "-w 4", "-b 0.0.0.0:5000", "run:app"]
```

### Using Nginx Reverse Proxy

```nginx
server {
    listen 80;
    server_name appstore.example.com;
    
    location / {
        proxy_pass http://127.0.0.1:5000;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    }
    
    location ~* \.(js|css|png|jpg)$ {
        expires 30d;
        add_header Cache-Control "public, immutable";
    }
}
```

## Performance Optimization

### Caching
```python
from flask_caching import Cache

cache = Cache(app, config={'CACHE_TYPE': 'simple'})

@app.route('/api/v1/device/apps')
@cache.cached(timeout=300)
def list_apps():
    ...
```

### Database Indexing
```sql
CREATE INDEX idx_app_category ON apps(category);
CREATE INDEX idx_installation_device ON installations(device_id);
CREATE INDEX idx_license_device ON licenses(device_id);
```

### Connection Pooling
```python
from sqlalchemy.pool import QueuePool

app.config['SQLALCHEMY_ENGINE_OPTIONS'] = {
    'poolclass': QueuePool,
    'pool_size': 10,
    'pool_recycle': 3600,
}
```

## Monitoring and Analytics

### Key Metrics to Track
- Active devices
- Total downloads
- Most popular apps
- User ratings distribution
- License utilization
- Server response times
- API errors and failures

### Sample Monitoring Query
```bash
# Get statistics
curl http://server:5000/api/v1/admin/stats \
  -H "X-API-Key: your-api-key" | jq
```

## Troubleshooting

### Device can't connect
```bash
# Check server is running
curl http://localhost:5000

# Check firewall
sudo ufw allow 5000/tcp

# Check WiFi connection on device
# Verify server URL in code
```

### Apps don't download
```bash
# Check file exists
ls -lah appstore/apps/

# Check file permissions
chmod 644 appstore/apps/*

# Check disk space
df -h

# Check logs
tail -f /tmp/flask.log
```

### Database errors
```bash
# Backup database
cp instance/appstore.db instance/appstore.db.backup

# Reset database
rm instance/appstore.db
python manage.py init
```

## Future Enhancements

- [ ] Payment integration (Stripe, PayPal)
- [ ] User accounts and wish lists
- [ ] Auto-update system
- [ ] Social features (followers, recommendations)
- [ ] Admin dashboard UI
- [ ] Content moderation tools
- [ ] Developer store page
- [ ] Analytics dashboard
- [ ] A/B testing framework
- [ ] Multiplayer matchmaking

## File Structure

```
appstore/
├── README.md                 # Full documentation
├── QUICKSTART.md             # Quick start guide
├── INTEGRATION.md            # Launcher integration guide
├── example_manifest.json     # Sample app manifest
│
├── server/                   # Flask backend
│   ├── run.py               # Entry point
│   ├── config.py            # Configuration
│   ├── manage.py            # CLI tool
│   ├── requirements.txt      # Python dependencies
│   ├── templates/
│   │   └── index.html       # Web UI
│   ├── static/              # CSS, JS, images
│   └── app/
│       ├── __init__.py      # Flask factory
│       ├── models.py        # Database models
│       └── api/
│           ├── device_api.py    # Public endpoints
│           └── admin_api.py     # Admin endpoints
│
└── client/                   # C device client
    ├── rg_appstore.h        # Public API
    └── rg_appstore.c        # Implementation
```

## Support and Contributing

For issues, questions, or contributions:
1. Check existing documentation
2. Review GitHub issues
3. Submit pull requests with improvements
4. Share feedback on design

## License

Part of the Retro-Go project. See main LICENSE file.

---

**Last Updated**: January 2026  
**Version**: 1.0.0  
**Status**: Production Ready
