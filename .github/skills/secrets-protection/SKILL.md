---
name: secrets-protection
description: >
  Guide for keeping API keys, WiFi passwords, and other credentials out of
  git history in the TokenDisplay project. Use this when asked to add a new
  API key, change credentials, audit the repo for leaked secrets, or set up
  a new developer's environment safely.
---

## Secret Inventory for This Project

| Secret | Lives in | Should be in git? |
|--------|----------|-------------------|
| `WIFI_SSID` | `include/config.h` | **NO** |
| `WIFI_PASSWORD` | `include/config.h` | **NO** |
| `API_KEYS[].key` (OpenRouter) | `include/config.h` | **NO** |
| `API_KEYS[].key` (Anthropic) | `include/config.h` | **NO** |
| `CLAUDEAI_SESSION` | `server/.env` | **NO** |
| `LASTACTIVE_ORG` | `server/.env` | **NO** |
| Template / placeholder values | `include/config.h.example` | **YES** |

`include/config.h` and `server/.env` are in `.gitignore` and must stay there.

---

## Setting Up a Fresh Clone

```bash
# 1. Copy the example
cp include/config.h.example include/config.h

# 2. Edit with real values
#    WIFI_SSID, WIFI_PASSWORD, API_KEYS
code include/config.h

# 3. Install the pre-commit hook so secrets can't slip through
bash scripts/install-hooks.sh
```

---

## Adding a New API Key

Edit **only** `include/config.h` (never `config.h.example`):

```cpp
const APIKeyConfig API_KEYS[] = {
  {"Personal", "sk-or-v1-REAL_KEY_HERE", false},
  {"Work",     "sk-ant-api03-REAL_KEY", true},
    // add more rows here
};
```

`NUM_API_KEYS` is derived via `sizeof`, so no counter to update.

---

## If a Secret Was Accidentally Committed

1. **Revoke/rotate immediately**:
  - OpenRouter key: rotate in OpenRouter dashboard.
  - Anthropic key: rotate in Anthropic console.
  - Claude session: log out/revoke active web sessions.
2. Remove from history:
   ```bash
   git filter-repo --path include/config.h --invert-paths
  git filter-repo --path server/.env --invert-paths
   git push --force-with-lease
   ```
   (Requires `git filter-repo` — install: `pip install git-filter-repo`)
3. Verify `.gitignore` still contains both `include/config.h` and `server/.env`.
4. Generate and configure a new key.

---

## Pre-commit Hook

`scripts/pre-commit` blocks commits that:
- Stage `include/config.h`
- Stage `server/.env`
- Contain `sk-or-v1-` (real OpenRouter key prefix)
- Contain `sk-ant-` patterns
- Contain `YOUR_WIFI_PASSWORD` replaced with an actual value pattern

Install once per clone: `bash scripts/install-hooks.sh`

---

## GitHub Actions — Secrets in CI

The build workflow uses `cp include/config.h.example include/config.h` in CI.
This compiles successfully because the example file contains only placeholder strings.

**Never** store real credentials in GitHub Actions secrets for this project —
the firmware is flashed locally; CI only validates that the code compiles.

---

## Quick Local Secret Audit

Before pushing, run a local scan in repo root:

```bash
git grep -nE "sk-or-v1-|sk-ant-|CLAUDEAI_SESSION|LASTACTIVE_ORG|WIFI_PASSWORD"
```

Expected hits should be templates/examples only (not real credential values).
