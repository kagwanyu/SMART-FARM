#!/bin/bash
set -e

# === Smart Farm API – Server Deployment Script ===
# Run this as root on the server.
# Usage: ssh root@64.227.174.0 'bash -s' < deploy.sh
# Or: copy this to the server and run: bash deploy.sh

echo "========================================"
echo "  Smart Farm API – Server Setup"
echo "========================================"

# ── Config ──────────────────────────────────────
APP_USER="naila"
APP_DIR="/home/$APP_USER/smart-farm-api"
DB_NAME="smartfarm"
DB_USER="smartfarm"
DB_PASS="$(openssl rand -base64 16)"
API_SECRET="$(openssl rand -base64 32)"
REPO_URL="https://github.com/Lynser/kynext_farm_attiny.git"
# ────────────────────────────────────────────────

# ── 1. System packages ─────────────────────────
echo "[1/8] Installing system packages..."
apt-get update -qq
apt-get install -y -qq python3 python3-pip python3-venv mariadb-server mariadb-client git curl
python3 -m pip install --upgrade pip

# ── 2. MariaDB setup ────────────────────────────
echo "[2/8] Configuring MariaDB..."
systemctl enable mariadb
systemctl start mariadb

mysql -u root <<SQL
CREATE DATABASE IF NOT EXISTS $DB_NAME CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER IF NOT EXISTS '$DB_USER'@'localhost' IDENTIFIED BY '$DB_PASS';
GRANT ALL PRIVILEGES ON $DB_NAME.* TO '$DB_USER'@'localhost';
FLUSH PRIVILEGES;
SQL

# ── 3. App user ─────────────────────────────────
echo "[3/8] Ensuring app user exists..."
id -u $APP_USER &>/dev/null || useradd -m -s /bin/bash $APP_USER
usermod -aG $APP_USER $APP_USER

# ── 4. Project files ────────────────────────────
echo "[4/8] Cloning project from GitHub..."
if [ -d "$APP_DIR/.git" ]; then
    echo "Repo already exists, pulling latest..."
    cd $APP_DIR && git pull
else
    rm -rf $APP_DIR
    git clone $REPO_URL $APP_DIR
fi

# ── 5. Python virtual env ───────────────────────
echo "[5/8] Creating virtual environment..."
python3 -m venv $APP_DIR/venv
source $APP_DIR/venv/bin/activate
pip install -q -r $APP_DIR/requirements.txt

# ── 6. Environment file ─────────────────────────
echo "[6/8] Writing .env..."
cat > $APP_DIR/.env <<EOF
API_NAME=Smart Farm API
API_VERSION=1.0.0
DEBUG=False
DB_HOST=localhost
DB_PORT=3306
DB_USER=$DB_USER
DB_PASSWORD=$DB_PASS
DB_NAME=$DB_NAME
SECRET_KEY=$API_SECRET
EOF

chown -R $APP_USER:$APP_USER $APP_DIR
chmod 600 $APP_DIR/.env

# ── 7. Systemd service ─────────────────────────
echo "[7/8] Creating systemd service..."
cat > /etc/systemd/system/smartfarm-api.service <<EOF
[Unit]
Description=Smart Farm API
After=network.target mariadb.service

[Service]
Type=simple
User=$APP_USER
Group=$APP_USER
WorkingDirectory=$APP_DIR
EnvironmentFile=$APP_DIR/.env
ExecStart=$APP_DIR/venv/bin/uvicorn app.main:app --host 0.0.0.0 --port 8000
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
systemctl enable smartfarm-api
systemctl start smartfarm-api

# ── 8. Verify ──────────────────────────────────
echo "[8/8] Verifying deployment..."
sleep 3
if systemctl is-active --quiet smartfarm-api; then
    echo ""
    echo "========================================"
    echo "  SUCCESS! API is running."
    echo "========================================"
    echo "  URL:      http://64.227.174.0:8000"
    echo "  Docs:     http://64.227.174.0:8000/docs"
    echo "  Health:   http://64.227.174.0:8000/health"
    echo ""
    echo "  DB name:  $DB_NAME"
    echo "  DB user:  $DB_USER"
    echo "  DB pass:  $DB_PASS (save this!)"
    echo "========================================"
else
    echo "ERROR: Service failed to start."
    systemctl status smartfarm-api --no-pager
    exit 1
fi
