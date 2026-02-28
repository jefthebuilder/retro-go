# App Store Setup Guide

Quick start guide for setting up and running the Retro-Go App Store.

## Quick Start

### 1. Install Dependencies

```bash
cd appstore/server
pip install -r requirements.txt
```

### 2. Configure Environment

Create `.env` file:
```bash
FLASK_ENV=development
FLASK_HOST=0.0.0.0
FLASK_PORT=5000
ADMIN_API_KEY=dev-key-change-this
DATABASE_URL=sqlite:///appstore.db
```

Or set environment variables:
```bash
export FLASK_ENV=development
export ADMIN_API_KEY=your-secure-key
```

### 3. Run Server

```bash
python run.py
```

Server will start at `http://0.0.0.0:5000`

### 4. Access Web UI

Open browser to `http://localhost:5000`

## Adding Apps

### Via API

```bash
curl -X POST http://localhost:5000/api/v1/admin/apps \
  -H "X-API-Key: dev-key-change-this" \
  -H "Content-Type: application/json" \
  -d '{
    "name": "My Game",
    "description": "A fun retro game",
    "version": "1.0.0",
    "author": "Game Developer",
    "category": "game",
    "price": 0.99,
    "currency": "USD",
    "file_path": "/path/to/game.bin",
    "min_version": "1.0.0",
    "required_space": 5242880
  }'
```

### Response Example

```json
{
  "status": "success",
  "app": {
    "id": "550e8400-e29b-41d4-a716-446655440000",
    "name": "My Game",
    "version": "1.0.0",
    "author": "Game Developer",
    "category": "game",
    "price": 0.99,
    "rating": 0.0,
    "download_count": 0
  }
}
```

## Testing Device Endpoints

### Register Device

```bash
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
```

### List Available Apps

```bash
curl http://localhost:5000/api/v1/device/apps?limit=10
```

### Get App Details

```bash
curl http://localhost:5000/api/v1/device/apps/{app_id}
```

### Rate an App

```bash
curl -X POST http://localhost:5000/api/v1/device/apps/{app_id}/rate?device_id=aa:bb:cc:dd:ee:ff \
  -H "Content-Type: application/json" \
  -d '{
    "rating": 5,
    "title": "Great app!",
    "comment": "Works perfectly!"
  }'
```

## Database Management

### View Database

```bash
sqlite3 instance/appstore.db
sqlite> SELECT * FROM apps;
sqlite> SELECT * FROM devices;
sqlite> SELECT * FROM installations;
```

### Reset Database

```bash
rm instance/appstore.db
python -c "from app import create_app; app = create_app(); app.app_context().push()"
```

### Backup Database

```bash
cp instance/appstore.db instance/appstore.db.backup.$(date +%Y%m%d)
```

## Running in Production

### 1. Use Production Config

```bash
export FLASK_ENV=production
```

### 2. Enable HTTPS

Configure with SSL certificates:
```bash
# Generate self-signed cert
openssl req -x509 -newkey rsa:4096 -nodes -out cert.pem -keyout key.pem -days 365
```

### 3. Use Gunicorn

```bash
pip install gunicorn
gunicorn -w 4 -b 0.0.0.0:5000 run:app
```

### 4. Setup Nginx Reverse Proxy

```nginx
server {
    listen 80;
    server_name appstore.example.com;
    
    location / {
        proxy_pass http://127.0.0.1:5000;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }
}
```

## Device Configuration

### Configure Device to Use Store

Edit your device's network settings to point to the app store server:

```c
// In your application initialization
rg_appstore_init("http://appstore.local:5000");
```

### Register Device

The device will auto-register on first connection:

```c
rg_appstore_device_t device = {
    .name = "My Retro-Go",
    .model = "ODROID-GO",
    .firmware_version = "1.0.0",
    .storage_total = 4000000000,
    .storage_used = 1000000000
};
rg_appstore_register_device(&device);
```

## Monitoring

### Check Server Logs

```bash
# Flask logs (if running in development)
tail -f /tmp/flask.log

# System logs (if using systemd)
journalctl -u appstore -f
```

### Monitor Stats

```bash
curl http://localhost:5000/api/v1/admin/stats \
  -H "X-API-Key: your-api-key"
```

## Troubleshooting

### Port Already in Use

```bash
# Find process using port 5000
lsof -i :5000

# Kill the process
kill -9 <PID>
```

### Database Locked

SQLite database is locked - this happens with concurrent writes:
1. Ensure only one Flask instance is running
2. Use PostgreSQL for production

### Import Errors

```bash
# Reinstall packages
pip install --force-reinstall -r requirements.txt
```

### CORS Issues

Add CORS headers to Flask app:
```python
from flask_cors import CORS
CORS(app)
```

## Next Steps

1. [Read Full Documentation](README.md)
2. [Review API Endpoints](README.md#api-documentation)
3. [Integrate Device Client](README.md#device-client-integration)
4. [Set Up Database Backups](README.md#database-management)
