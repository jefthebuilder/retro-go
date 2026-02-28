# Retro-Go App Store

A complete app store system for Retro-Go firmware, allowing users to discover, download, purchase, and manage applications on their retro gaming devices.

## Architecture Overview

The App Store consists of three main components:

1. **Flask Backend Server** - REST API for managing apps and devices
2. **Web UI** - Browser-based app store interface 
3. **Device Client** - C code running on retro-go devices to interact with the store

## Features

### For Users
- Browse available apps by category
- Search for specific apps
- View app details, ratings, and reviews
- Download and install apps directly to device
- Manage installed apps
- Submit reviews and ratings
- Track disk space and app inventory
- Free and paid app support

### For Developers/Admins
- Upload and manage app listings
- Track download statistics
- View device inventory and analytics
- Manage licenses and trial versions
- Monitor user reviews and ratings

## Installation

### Backend Server

1. Install Python 3.8+ and dependencies:
```bash
cd appstore/server
pip install -r requirements.txt
```

2. Configure environment variables:
```bash
export FLASK_ENV=development
export FLASK_HOST=0.0.0.0
export FLASK_PORT=5000
export ADMIN_API_KEY=your-secure-api-key
```

3. Run the server:
```bash
python run.py
```

The server will be available at `http://localhost:5000`

### Database

The app store uses SQLite by default. The database is automatically created on first run:

```bash
# Access database directly if needed
sqlite3 instance/appstore.db
```

For production, configure PostgreSQL:
```bash
export DATABASE_URL=postgresql://user:password@localhost/appstore
```

## API Documentation

### Device Endpoints (User-Facing)

#### Register Device
```
POST /api/v1/device/register
Content-Type: application/json

{
    "device_id": "aa:bb:cc:dd:ee:ff",
    "name": "My Retro-Go",
    "model": "ODROID-GO",
    "firmware_version": "1.0.0",
    "storage_total": 4000000000,
    "storage_used": 1000000000
}
```

#### List Apps
```
GET /api/v1/device/apps?category=game&limit=50&offset=0&search=query

Response:
{
    "status": "success",
    "total": 150,
    "limit": 50,
    "offset": 0,
    "apps": [
        {
            "id": "uuid",
            "name": "Game Name",
            "description": "Description",
            "version": "1.0.0",
            "author": "Author",
            "category": "game",
            "price": 0.99,
            "rating": 4.5,
            "download_count": 1234,
            "featured": true
        }
    ]
}
```

#### Get App Details
```
GET /api/v1/device/apps/{app_id}
```

#### Download App
```
GET /api/v1/device/apps/{app_id}/download?device_id=aa:bb:cc:dd:ee:ff

Downloads the binary file for the app
```

#### Install App
```
POST /api/v1/device/apps/{app_id}/install?device_id=aa:bb:cc:dd:ee:ff
Content-Type: application/json

{
    "version": "1.0.0",
    "storage_used": 2000000000
}
```

#### Uninstall App
```
POST /api/v1/device/apps/{app_id}/uninstall?device_id=aa:bb:cc:dd:ee:ff
Content-Type: application/json

{
    "storage_used": 1500000000
}
```

#### Rate App
```
POST /api/v1/device/apps/{app_id}/rate?device_id=aa:bb:cc:dd:ee:ff
Content-Type: application/json

{
    "rating": 5,
    "title": "Great app!",
    "comment": "Works perfectly!"
}
```

#### Check License
```
GET /api/v1/device/licenses/{app_id}?device_id=aa:bb:cc:dd:ee:ff&license_key=optional

Response:
{
    "status": "success",
    "licensed": true,
    "is_free": true,
    "license": {...}
}
```

#### List Categories
```
GET /api/v1/device/categories

Response:
{
    "status": "success",
    "categories": ["game", "utility", "emulator", "tool"]
}
```

#### Get Device Apps
```
GET /api/v1/device/devices/{device_id}/apps

Returns list of apps installed on the device
```

#### Report Stats
```
POST /api/v1/device/stats
Content-Type: application/json

{
    "device_id": "aa:bb:cc:dd:ee:ff",
    "uptime": 3600,
    "memory_free": 500000,
    "storage_used": 2000000000
}
```

### Admin Endpoints

All admin endpoints require `X-API-Key` header.

#### Create App
```
POST /api/v1/admin/apps
X-API-Key: your-api-key
Content-Type: application/json

{
    "name": "App Name",
    "description": "Description",
    "version": "1.0.0",
    "author": "Author",
    "category": "game",
    "price": 0.99,
    "currency": "USD",
    "file_path": "/path/to/app.bin",
    "min_version": "1.0.0",
    "required_space": 10000000
}
```

#### Update App
```
PUT /api/v1/admin/apps/{app_id}
X-API-Key: your-api-key
Content-Type: application/json
```

#### Delete App
```
DELETE /api/v1/admin/apps/{app_id}
X-API-Key: your-api-key
```

#### Create License
```
POST /api/v1/admin/licenses
X-API-Key: your-api-key
Content-Type: application/json

{
    "app_id": "uuid",
    "device_id": "aa:bb:cc:dd:ee:ff",
    "is_trial": false,
    "trial_days": 7,
    "expires_days": 365
}
```

#### Get Statistics
```
GET /api/v1/admin/stats
X-API-Key: your-api-key

Response:
{
    "status": "success",
    "stats": {
        "total_apps": 150,
        "enabled_apps": 140,
        "total_devices": 5000,
        "total_installations": 15000,
        "total_licenses": 2000,
        "total_downloads": 25000,
        "total_reviews": 1200
    }
}
```

## Device Client Integration

### Adding to Launcher/Apps

To integrate the app store client into a retro-go application:

1. Copy the client files to your component:
```bash
cp appstore/client/rg_appstore.h components/retro-go/
cp appstore/client/rg_appstore.c components/retro-go/
```

2. Include in your application:
```c
#include "rg_appstore.h"

// Initialize
rg_appstore_init("http://appstore.local:5000");

// Register device
rg_appstore_device_t device = {
    .name = "My Device",
    .model = "ODROID-GO",
    .firmware_version = "1.0.0",
    .storage_total = 4000000000,
    .storage_used = 1000000000
};
rg_appstore_register_device(&device);

// List apps
int count = 0;
rg_appstore_app_t *apps = NULL;
rg_appstore_get_apps("game", &count, &apps);

// Download app
rg_appstore_download_app(app_id, "/sd/apps/myapp.bin", progress_callback);

// Report installation
rg_appstore_install_app(app_id, "1.0.0");
```

## Web UI Usage

1. Navigate to `http://server:5000` in your browser
2. Browse apps by category or use search
3. Click on an app to see details, reviews, and ratings
4. Click "Download" to download the app file
5. Transfer the downloaded file to your device via USB

## Database Schema

### Apps Table
- id (UUID, primary key)
- name (unique)
- description
- version
- author
- category
- file_path
- file_size
- file_hash (SHA256)
- price
- rating
- download_count
- featured
- enabled

### Devices Table
- id (MAC address, primary key)
- name
- model
- firmware_version
- storage_total
- storage_used
- auto_update

### Installations Table
- id (UUID)
- app_id (foreign key)
- device_id (foreign key)
- installed_date
- last_launched
- last_version
- status

### Licenses Table
- id (UUID)
- app_id (foreign key)
- device_id (foreign key)
- license_key (unique)
- purchased_date
- expires_date
- is_active

### Reviews Table
- id (UUID)
- app_id (foreign key)
- device_id
- rating (1-5)
- title
- comment
- created_date

## Configuration

### Server Configuration (config.py)

```python
# Maximum app file size
MAX_APP_SIZE = 50 * 1024 * 1024  # 50MB

# Upload folder
UPLOAD_FOLDER = 'apps/'

# Session timeout
PERMANENT_SESSION_LIFETIME = timedelta(days=30)

# Device timeout
DEVICE_TIMEOUT = 30  # seconds
```

## Security Considerations

1. **API Keys**: Use strong, randomly generated API keys for admin endpoints
2. **HTTPS**: Enable in production (`SESSION_COOKIE_SECURE = True`)
3. **Authentication**: Current implementation uses API key. Consider OAuth2/JWT for production
4. **File Validation**: All uploaded files are validated for size and hash
5. **Device Verification**: Device IDs should be verified against actual device MACs

## Troubleshooting

### Device cannot connect to server
- Check network connectivity
- Verify server URL in device configuration
- Check firewall rules
- Ensure CORS is properly configured

### Apps not downloading
- Verify file path exists on server
- Check file permissions
- Ensure sufficient disk space
- Check HTTP status codes in logs

### Database errors
- Ensure database directory is writable
- Check PostgreSQL connection if using production DB
- Verify schema is created: `sqlite3 appstore.db ".schema"`

## Future Enhancements

- [ ] Payment integration (Stripe, PayPal)
- [ ] User accounts and wish lists
- [ ] App auto-update system
- [ ] Social features (sharing, recommendations)
- [ ] Admin dashboard UI
- [ ] App store moderation system
- [ ] DRM and license enforcement
- [ ] Game achievements/leaderboards
- [ ] Community ratings and reviews
- [ ] WebSocket real-time updates

## License

This app store system is part of the Retro-Go project. See the main LICENSE file for details.

## Contributing

To contribute:

1. Fork the repository
2. Create a feature branch
3. Submit a pull request with details

## Support

For issues and feature requests, please use the GitHub issue tracker.
