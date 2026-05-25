#!/bin/bash
# Deploy script for Claude Relay on Raspberry Pi
# Run from Windows: ssh YOUR_RPI_USER@YOUR_RPI_IP < deploy.sh

set -e

echo "🚀 Deploying Claude Relay Server..."

# Create directory
mkdir -p /home/pi/claude-relay
cd /home/pi/claude-relay

# Check if .env exists
if [ ! -f .env ]; then
    echo "⚠️  .env file not found! Creating from example..."
    cat > .env << 'EOF'
CLAUDEAI_SESSION=your_session_key_here
LASTACTIVE_ORG=your_organization_id_here
RELAY_PORT=8765
EOF
    echo "⚠️  Please edit /home/pi/claude-relay/.env with your credentials!"
fi

# Install Python dependencies
echo "📦 Installing Python dependencies..."
pip3 install --user curl_cffi flask python-dotenv

# Install systemd service
echo "⚙️  Installing systemd service..."
sudo cp claude-relay.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable claude-relay.service

# Start service
echo "▶️  Starting service..."
sudo systemctl restart claude-relay.service

# Show status
echo ""
echo "✅ Deployment complete!"
echo ""
echo "Service status:"
sudo systemctl status claude-relay.service --no-pager -l
echo ""
echo "📊 Web interface: http://YOUR_RPI_IP:8765"
echo "📝 View logs: sudo journalctl -u claude-relay.service -f"
echo "🔧 Edit config: nano /home/pi/claude-relay/.env"
