# Claude Relay Server - Deploy to Raspberry Pi
# PowerShell deployment script for Windows

$RPI_HOST = "pi@192.168.0.43"
$RPI_DIR = "/home/pi/claude-relay"
$SERVER_DIR = "D:\git\ESP32-Token-Display\server"

Write-Host "🚀 Deploying Claude Relay Server to Raspberry Pi..." -ForegroundColor Cyan
Write-Host ""

# Check if .env exists
if (-not (Test-Path "$SERVER_DIR\.env")) {
    Write-Host "⚠️  Warning: .env file not found!" -ForegroundColor Yellow
    Write-Host "Creating .env from example..." -ForegroundColor Yellow
    Copy-Item "$SERVER_DIR\.env.example" "$SERVER_DIR\.env"
    Write-Host ""
    Write-Host "⚠️  Please edit server\.env with your Claude.ai credentials before continuing!" -ForegroundColor Red
    Write-Host "Press any key to continue or Ctrl+C to cancel..."
    $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
}

# Create directory on RPI
Write-Host "📁 Creating directory on RPI..." -ForegroundColor Yellow
ssh $RPI_HOST "mkdir -p $RPI_DIR"

# Copy files
Write-Host "📤 Copying files to RPI..." -ForegroundColor Yellow
scp "$SERVER_DIR\relay.py" "${RPI_HOST}:${RPI_DIR}/"
scp "$SERVER_DIR\claude-relay.service" "${RPI_HOST}:${RPI_DIR}/"
scp "$SERVER_DIR\requirements.txt" "${RPI_HOST}:${RPI_DIR}/"
scp "$SERVER_DIR\web-interface.html" "${RPI_HOST}:${RPI_DIR}/index.html"
scp "$SERVER_DIR\.env" "${RPI_HOST}:${RPI_DIR}/"

Write-Host ""
Write-Host "🔧 Running deployment on Raspberry Pi..." -ForegroundColor Yellow
Write-Host ""

# Run deployment commands on RPI
$deployScript = @"
cd $RPI_DIR

echo '📦 Installing Python dependencies...'
pip3 install --user curl_cffi flask python-dotenv

echo ''
echo '⚙️  Installing systemd service...'
sudo cp claude-relay.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable claude-relay.service

echo ''
echo '▶️  Restarting service...'
sudo systemctl restart claude-relay.service

echo ''
echo '✅ Deployment complete!'
echo ''
sudo systemctl status claude-relay.service --no-pager -l
"@

ssh $RPI_HOST $deployScript

Write-Host ""
Write-Host "=" -ForegroundColor Green -NoNewline
Write-Host "================================" -ForegroundColor Green
Write-Host "✅ Deployment completed successfully!" -ForegroundColor Green
Write-Host ""
Write-Host "🌐 Web Interface: " -NoNewline
Write-Host "http://192.168.0.43:8765" -ForegroundColor Cyan
Write-Host ""
Write-Host "📋 Useful commands:" -ForegroundColor Yellow
Write-Host "  View logs:    " -NoNewline -ForegroundColor Gray
Write-Host "ssh $RPI_HOST 'sudo journalctl -u claude-relay.service -f'" -ForegroundColor White
Write-Host "  Check status: " -NoNewline -ForegroundColor Gray
Write-Host "ssh $RPI_HOST 'sudo systemctl status claude-relay.service'" -ForegroundColor White
Write-Host "  Restart:      " -NoNewline -ForegroundColor Gray
Write-Host "ssh $RPI_HOST 'sudo systemctl restart claude-relay.service'" -ForegroundColor White
Write-Host ""
