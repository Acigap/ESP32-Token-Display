"""
POC: Claude.ai Web Usage (Session / Weekly limits)
Uses curl_cffi to impersonate Chrome TLS fingerprint and bypass Cloudflare.

Install: python -m pip install curl_cffi
Run:     python poc_claudeai_usage.py
"""

import os, json
from pathlib import Path

# ── load .env ─────────────────────────────────────────────
def load_env():
    p = Path(__file__).parent / ".env"
    if not p.exists():
        return
    for line in p.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        k, _, v = line.partition("=")
        k = k.strip(); v = v.strip().strip('"').strip("'")
        if k and k not in os.environ:
            os.environ[k] = v

load_env()

# ── get creds from .env ────────────────────────────────────
SESSION        = os.environ.get("CLAUDEAI_SESSION", "")
CF_CLEARANCE   = os.environ.get("CF_CLEARANCE", "")
CF_BM          = os.environ.get("CF_BM", "")
SSID           = os.environ.get("SSID", "")
DEVICE_ID      = os.environ.get("ANTHROPIC_DEVICE_ID", "")
ORG_ID         = os.environ.get("LASTACTIVE_ORG", "")

if not SESSION:
    SESSION = input("Session key (sk-ant-sid02-...): ").strip()

# ── build cookie header ───────────────────────────────────
cookies = {
    "sessionKey":          SESSION,
    "cf_clearance":        CF_CLEARANCE,
    "__cf_bm":             CF_BM,
    "__ssid":              SSID,
    "lastActiveOrg":       ORG_ID,
    "anthropic-device-id": DEVICE_ID,
}
cookies = {k: v for k, v in cookies.items() if v}  # drop empty

headers = {
    "Accept":          "application/json",
    "Referer":         "https://claude.ai/",
    "Origin":          "https://claude.ai",
    "sec-fetch-site":  "same-origin",
    "sec-fetch-mode":  "cors",
    "sec-fetch-dest":  "empty",
    "sec-ch-ua":       '"Chromium";v="124", "Google Chrome";v="124"',
    "sec-ch-ua-mobile":"?0",
    "sec-ch-ua-platform": '"Windows"',
}

BASE = "https://claude.ai"

# ── impersonate Chrome with curl_cffi ─────────────────────
try:
    from curl_cffi import requests as creq
except ImportError:
    print("ERROR: pip install curl_cffi")
    raise

def get(path):
    url = f"{BASE}{path}"
    try:
        r = creq.get(url, cookies=cookies, headers=headers,
                     impersonate="chrome124", timeout=15)
        return r.status_code, r.text
    except Exception as e:
        return 0, str(e)

def print_result(label, code, text):
    color = "\033[32m" if code == 200 else "\033[31m"
    reset = "\033[0m"
    print(f"\n{color}[{code}] {label}{reset}")
    if code == 200 and text:
        try:
            obj = json.loads(text)
            print(json.dumps(obj, indent=2, ensure_ascii=False)[:3000])
        except Exception:
            print(text[:500])
    elif text and code != 200:
        print(text[:200])

# ══════════════════════════════════════════════════════════
print(f"\nOrg ID : {ORG_ID}")
print(f"Cookie count : {len(cookies)}")
print("=" * 60)

# ── endpoints to try ──────────────────────────────────────
endpoints = [
    "/api/auth/session",
    "/api/bootstrap",
    "/api/organizations",
]

if ORG_ID:
    endpoints += [
        f"/api/organizations/{ORG_ID}",
        f"/api/organizations/{ORG_ID}/usage",
        f"/api/organizations/{ORG_ID}/limits",
        f"/api/organizations/{ORG_ID}/entitlements",
        f"/api/organizations/{ORG_ID}/subscription",
        f"/api/organizations/{ORG_ID}/members/me",
    ]

endpoints += [
    "/api/usage_limits",
    "/api/entitlements",
]

for ep in endpoints:
    code, text = get(ep)
    print_result(ep, code, text)

print("\n" + "=" * 60)
print("Done. Status 200 = usable data for ESP32 relay.")
