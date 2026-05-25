#!/bin/bash
# Quick script to copy files to Raspberry Pi and deploy

# Load configuration from .env file
if [ -f .env ]; then
    export $(cat .env | grep -v '^#' | xargs)
else
    echo "❌ Error: .env file not found"
    echo "Please copy .env.example to .env and configure your Raspberry Pi settings"
    exit 1
fi

# Use environment variables or defaults
RPI_HOST="${RPI_HOST:-pi@192.168.0.43}"
RPI_DIR="${RPI_DIR:-/home/pi/claude-relay}"

echo "📤 Copying files to Raspberry Pi..."

# Create directory on RPI
ssh $RPI_HOST "mkdir -p $RPI_DIR"

# Copy files
scp relay.py $RPI_HOST:$RPI_DIR/
scp claude-relay.service $RPI_HOST:$RPI_DIR/
scp requirements.txt $RPI_HOST:$RPI_DIR/
scp web-interface.html $RPI_HOST:$RPI_DIR/index.html
scp .env $RPI_HOST:$RPI_DIR/

# Run deployment script
echo "🚀 Running deployment on Raspberry Pi..."
ssh $RPI_HOST bash << 'EOF'
cd /home/pi/claude-relay

# Install dependencies with --break-system-packages for Raspberry Pi OS
echo "📦 Installing dependencies..."
pip3 install --break-system-packages curl_cffi flask python-dotenv 2>/dev/null || \
pip3 install --user curl_cffi flask python-dotenv

# Install systemd service
echo "⚙️  Installing systemd service..."
sudo cp claude-relay.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable claude-relay.service

# Restart service
echo "▶️  Restarting service..."
sudo systemctl restart claude-relay.service

# Show status
echo ""
echo "✅ Deployment complete!"
sudo systemctl status claude-relay.service --no-pager -l
EOF

echo ""
echo "✅ Done! Access web interface at: http://${RPI_HOST#*@}:8765"
