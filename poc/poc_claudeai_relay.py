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

import os, json, time
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

SESSION   = os.environ.get("CLAUDEAI_SESSION", "")
CF_CLR    = os.environ.get("CF_CLEARANCE", "")
CF_BM     = os.environ.get("CF_BM", "")
SSID      = os.environ.get("SSID", "")
DEV_ID    = os.environ.get("ANTHROPIC_DEVICE_ID", "")
ORG_ID    = os.environ.get("LASTACTIVE_ORG", "")
PORT      = int(os.environ.get("RELAY_PORT", "8765"))

if not SESSION or not ORG_ID:
    print("ERROR: CLAUDEAI_SESSION and LASTACTIVE_ORG must be set in .env")
    exit(1)

from curl_cffi import requests as creq

COOKIES = {k: v for k, v in {
    "sessionKey":          SESSION,
    "cf_clearance":        CF_CLR,
    "__cf_bm":             CF_BM,
    "__ssid":              SSID,
    "lastActiveOrg":       ORG_ID,
    "anthropic-device-id": DEV_ID,
}.items() if v}

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


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        print(f"[{self.client_address[0]}] {fmt % args}")

    def do_GET(self):
        if self.path == "/usage" or self.path == "/":
            data   = fetch_usage()
            body   = json.dumps(data, ensure_ascii=False).encode()
            status = 200 if data.get("ok") else 503
            self.send_response(status)
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
