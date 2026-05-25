"""
Claude.ai Usage Relay Server
-----------------------------
ESP32 calls  →  http://YOUR_PC_IP:8765/usage
This server  →  https://claude.ai/api/organizations/{org}/usage

Run:
    python poc_claudeai_relay.py

ESP32 will GET http://<pc-ip>:8765/usage and receive:
{
  "session_pct": 100.0,
  "session_resets_in": "3h 5m",
  "weekly_pct": 7.0,
  "weekly_resets_in": "1d 6h",
  "ok": true
}
"""

import os, json, time, subprocess
from pathlib import Path
from datetime import datetime, timezone
from http.server import HTTPServer, BaseHTTPRequestHandler

# ── load .env ─────────────────────────────────────────────
def load_env():
    p = Path(__file__).parent / ".env"
    if not p.exists(): return
    for line in p.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line: continue
        k, _, v = line.partition("=")
        k = k.strip(); v = v.strip().strip('"').strip("'")
        if k and k not in os.environ: os.environ[k] = v

load_env()

SESSION = os.environ.get("CLAUDEAI_SESSION", "")
ORG_ID  = os.environ.get("LASTACTIVE_ORG", "")
PORT    = int(os.environ.get("RELAY_PORT", "8765"))

if not SESSION or not ORG_ID:
    print("ERROR: CLAUDEAI_SESSION and LASTACTIVE_ORG must be set in .env")
    exit(1)

from curl_cffi import requests as creq

COOKIES = {
    "sessionKey":    SESSION,
    "lastActiveOrg": ORG_ID,
}

HEADERS = {
    "Accept":             "application/json",
    "Referer":            "https://claude.ai/",
    "Origin":             "https://claude.ai",
    "sec-fetch-site":     "same-origin",
    "sec-fetch-mode":     "cors",
    "sec-fetch-dest":     "empty",
}

USAGE_URL = f"https://claude.ai/api/organizations/{ORG_ID}/usage"

# ── cache (avoid hammering claude.ai) ────────────────────
_cache = {"data": None, "ts": 0}
CACHE_TTL = 60  # seconds

def fmt_resets(iso_str):
    if not iso_str: return "?"
    try:
        dt  = datetime.fromisoformat(iso_str.replace("Z", "+00:00"))
        sec = max(0, (dt - datetime.now(timezone.utc)).total_seconds())
        h   = int(sec // 3600)
        m   = int((sec % 3600) // 60)
        if h > 0: return f"{h}h {m}m"
        return f"{m}m"
    except Exception:
        return "?"

def fetch_usage():
    now = time.time()
    if _cache["data"] and now - _cache["ts"] < CACHE_TTL:
        return _cache["data"]

    try:
        r = creq.get(USAGE_URL, cookies=COOKIES, headers=HEADERS,
                     impersonate="chrome124", timeout=10)
        if r.status_code != 200:
            return {"ok": False, "error": f"HTTP {r.status_code}"}

        raw = r.json()
        fh  = raw.get("five_hour")  or {}
        sd  = raw.get("seven_day")  or {}

        data = {
            "ok":                  True,
            "session_pct":         fh.get("utilization", 0.0),
            "session_resets_in":   fmt_resets(fh.get("resets_at")),
            "weekly_pct":          sd.get("utilization", 0.0),
            "weekly_resets_in":    fmt_resets(sd.get("resets_at")),
            "raw":                 raw,
        }
        _cache["data"] = data
        _cache["ts"]   = now
        print(f"[fetch] session={data['session_pct']}%  weekly={data['weekly_pct']}%")
        return data
    except Exception as e:
        return {"ok": False, "error": str(e)}


def get_service_status():
    """Get systemd service status"""
    try:
        result = subprocess.run(
            ["systemctl", "is-active", "claude-relay.service"],
            capture_output=True, text=True, timeout=5
        )
        is_active = result.stdout.strip() == "active"
        
        # Get detailed status
        status_result = subprocess.run(
            ["systemctl", "status", "claude-relay.service", "--no-pager"],
            capture_output=True, text=True, timeout=5
        )
        status_text = status_result.stdout
        
        # Parse status info
        pid = memory = uptime = None
        for line in status_text.split('\n'):
            if 'Main PID:' in line:
                pid = line.split('Main PID:')[1].split()[0]
            elif 'Memory:' in line:
                memory = line.split('Memory:')[1].strip()
            elif 'Active:' in line and 'since' in line:
                uptime_part = line.split('since')[1].strip()
                uptime = uptime_part.split(';')[0] if ';' in uptime_part else uptime_part
        
        return {
            "active": is_active,
            "status": result.stdout.strip(),
            "pid": pid,
            "memory": memory,
            "uptime": uptime
        }
    except Exception as e:
        return {"active": False, "status": "unknown", "error": str(e)}


def control_service(action):
    """Control systemd service (start/stop/restart)"""
    try:
        result = subprocess.run(
            ["sudo", "systemctl", action, "claude-relay.service"],
            capture_output=True, text=True, timeout=10
        )
        
        if result.returncode == 0:
            return {
                "success": True,
                "message": f"Service {action} completed successfully",
                "output": result.stdout
            }
        else:
            return {
                "success": False,
                "error": result.stderr or f"Command failed with exit code {result.returncode}",
                "output": result.stdout
            }
    except Exception as e:
        return {"success": False, "error": str(e)}


def get_service_logs():
    """Get last 50 lines of service logs"""
    try:
        result = subprocess.run(
            ["journalctl", "-u", "claude-relay.service", "-n", "50", "--no-pager"],
            capture_output=True, text=True, timeout=5
        )
        return {"logs": result.stdout}
    except Exception as e:
        return {"logs": f"Error fetching logs: {str(e)}"}


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        print(f"[{self.client_address[0]}] {fmt % args}")

    def do_GET(self):
        if self.path == "/usage" or self.path == "/api/usage":
            data   = fetch_usage()
            body   = json.dumps(data, ensure_ascii=False).encode()
            status = 200 if data.get("ok") else 503
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(body)
        
        elif self.path == "/" or self.path == "/index.html":
            # Serve web interface
            try:
                # Try both web-interface.html and index.html
                web_file = Path(__file__).parent / "web-interface.html"
                if not web_file.exists():
                    web_file = Path(__file__).parent / "index.html"
                
                if web_file.exists():
                    body = web_file.read_bytes()
                else:
                    body = b"<h1>Claude Relay Server</h1><p>Web interface not found. Copy web-interface.html or index.html to this directory.</p>"
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
            except Exception as e:
                self.send_error(500, str(e))
        
        elif self.path == "/service/status":
            data = get_service_status()
            body = json.dumps(data, ensure_ascii=False).encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(body)
        
        elif self.path == "/service/logs":
            data = get_service_logs()
            body = json.dumps(data, ensure_ascii=False).encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(body)
        
        else:
            self.send_response(404)
            self.end_headers()
    
    def do_POST(self):
        if self.path in ["/service/start", "/service/stop", "/service/restart"]:
            action = self.path.split("/")[-1]
            data = control_service(action)
            body = json.dumps(data, ensure_ascii=False).encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(body)
        else:
            self.send_response(404)
            self.end_headers()


if __name__ == "__main__":
    import socket
    ip = socket.gethostbyname(socket.gethostname())
    print(f"\nRelay server starting on port {PORT}")
    print(f"ESP32 should call:  http://{ip}:{PORT}/usage")
    print(f"Org ID: {ORG_ID}")
    print(f"Cache TTL: {CACHE_TTL}s\n")

    srv = HTTPServer(("0.0.0.0", PORT), Handler)
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        print("\nStopped.")
