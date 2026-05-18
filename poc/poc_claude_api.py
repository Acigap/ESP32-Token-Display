"""
POC: Claude (Anthropic) API - Rate Limit & Time-Based Data
"""

import os
import sys
import json
import requests
from datetime import datetime, timezone
from pathlib import Path

# ── load .env ─────────────────────────────────────────────
def _load_dotenv():
    env_path = Path(__file__).parent / ".env"
    if not env_path.exists():
        return
    for line in env_path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        k, _, v = line.partition("=")
        k = k.strip()
        v = v.strip().strip('"').strip("'")
        if k and k not in os.environ:
            os.environ[k] = v
_load_dotenv()

ANTHROPIC_API_KEY = os.environ.get("ANTHROPIC_API_KEY", "")
BASE_URL = "https://api.anthropic.com"
API_VERSION = "2023-06-01"

def print_section(title):
    print(f"\n{'='*60}\n  {title}\n{'='*60}", flush=True)

def parse_reset_time(iso_str):
    if not iso_str:
        return "N/A"
    try:
        reset_dt = datetime.fromisoformat(iso_str.replace("Z", "+00:00"))
        diff = (reset_dt - datetime.now(timezone.utc)).total_seconds()
        if diff <= 0:
            return "already reset"
        m, s = divmod(int(diff), 60)
        return f"{m}m {s}s" if m > 0 else f"{s}s"
    except Exception as e:
        return f"parse error: {e}"

def test_minimal_message(api_key):
    print_section("TEST 1: Minimal /v1/messages call")
    url = f"{BASE_URL}/v1/messages"
    headers = {
        "x-api-key": api_key,
        "anthropic-version": API_VERSION,
        "content-type": "application/json",
    }
    payload = {
        "model": "claude-haiku-4-5",
        "max_tokens": 1,
        "messages": [{"role": "user", "content": "Hi"}],
    }
    try:
        resp = requests.post(url, headers=headers, json=payload, timeout=15)
    except Exception as e:
        print(f"Request error: {e}", flush=True)
        return {"error": str(e)}

    print(f"Status: {resp.status_code}", flush=True)
    print("\n[Rate Limit Headers]", flush=True)
    rl_headers = {k: v for k, v in resp.headers.items() if "ratelimit" in k.lower() or k.lower() == "retry-after"}
    if rl_headers:
        for k, v in rl_headers.items():
            print(f"  {k}: {v}", flush=True)
    else:
        print("  (no rate limit headers)", flush=True)

    result = {}
    h = resp.headers
    for prefix in ["requests", "tokens", "input-tokens", "output-tokens"]:
        result[f"{prefix}_limit"]     = h.get(f"anthropic-ratelimit-{prefix}-limit", "N/A")
        result[f"{prefix}_remaining"] = h.get(f"anthropic-ratelimit-{prefix}-remaining", "N/A")
        reset_iso = h.get(f"anthropic-ratelimit-{prefix}-reset", "")
        result[f"{prefix}_reset_in"]  = parse_reset_time(reset_iso)
    result["retry_after"] = h.get("retry-after", "N/A")

    if resp.status_code == 200:
        body = resp.json()
        usage = body.get("usage", {})
        print(f"\n[Response Body]", flush=True)
        print(f"  Model: {body.get('model')}", flush=True)
        print(f"  Input tokens:  {usage.get('input_tokens', 0)}", flush=True)
        print(f"  Output tokens: {usage.get('output_tokens', 0)}", flush=True)
    else:
        print(f"\n[Error Response]\n{resp.text[:500]}", flush=True)
        result["error"] = resp.text[:500]
    return result

def print_summary(data):
    print_section("SUMMARY")
    fields = [
        ("Requests Limit",         "requests_limit"),
        ("Requests Remaining",     "requests_remaining"),
        ("Requests Reset in",      "requests_reset_in"),
        ("Tokens Limit",           "tokens_limit"),
        ("Tokens Remaining",       "tokens_remaining"),
        ("Tokens Reset in",        "tokens_reset_in"),
        ("Input Tokens Limit",     "input-tokens_limit"),
        ("Input Tokens Remaining", "input-tokens_remaining"),
        ("Input Tokens Reset in",  "input-tokens_reset_in"),
        ("Output Tokens Limit",    "output-tokens_limit"),
        ("Output Tokens Remaining","output-tokens_remaining"),
        ("Output Tokens Reset in", "output-tokens_reset_in"),
        ("Retry-After",            "retry_after"),
    ]
    for label, key in fields:
        val = data.get(key, "N/A")
        mark = "OK " if val not in ("N/A", "", None) else "-- "
        print(f"  {mark}{label:30s}: {val}", flush=True)

if __name__ == "__main__":
    api_key = ANTHROPIC_API_KEY
    if not api_key:
        print("No API key found in environment or .env", flush=True)
        sys.exit(1)
    print(f"Key prefix: {api_key[:12]}... (length={len(api_key)})", flush=True)
    data = test_minimal_message(api_key)
    print_summary(data)
    print("\nDone.", flush=True)
