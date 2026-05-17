---
name: github-actions-failure-debugging
description: >
  Guide for debugging failing GitHub Actions workflows in the TokenDisplay
  project. Use this when a build or release workflow fails and you need to
  find out why.
---

## Workflow Files

| File | Trigger | Purpose |
|------|---------|---------|
| `.github/workflows/build.yml` | push to `main`/`develop`, any PR | Compile firmware, upload artifact |
| `.github/workflows/release.yml` | push tag `v*.*.*` | Compile + create GitHub Release with `.bin` |

---

## Step 1 — Find the Failure

In the GitHub UI: **Actions tab → select the failed run → expand the failed step**.

Or with GitHub CLI:
```bash
# List recent runs
gh run list --repo OWNER/TokenDisplay --limit 10

# View a specific run's logs
gh run view RUN_ID --log-failed
```

---

## Step 2 — Common Failures and Fixes

### `config.h: No such file or directory`

The `cp include/config.h.example include/config.h` step failed or was skipped.

**Fix in `build.yml` / `release.yml`** — ensure this step appears before `pio run`:
```yaml
- name: Create config.h from example
  run: cp include/config.h.example include/config.h
```

### `pip install platformio` — network / timeout error

Re-run the workflow (transient). If it recurs:
```yaml
- name: Install PlatformIO Core
  run: pip install --upgrade platformio
  timeout-minutes: 5
```

### PlatformIO package download fails

The cache may be stale. Delete the cache key in **Actions → Caches** and re-run.

Alternatively, add a fallback:
```yaml
- name: Install PlatformIO Core
  run: pip install --upgrade platformio

- name: Build
  run: pio run -e esp32dev
  env:
    PLATFORMIO_SETTING_ENABLE_TELEMETRY: "false"
```

### Release workflow — `GITHUB_TOKEN` permission denied

Ensure `permissions: contents: write` is set at the job level in `release.yml`:
```yaml
permissions:
  contents: write
```

### Tag not matching `v*.*.*` pattern

The release workflow only triggers on tags like `v1.0.0`.  
Wrong: `git tag 1.0.0` → Right: `git tag v1.0.0`

---

## Step 3 — Validate Locally Before Pushing

```bash
# Simulate what CI does
cp include/config.h.example include/config.h
pio run -e esp32dev
```

If it compiles locally but not in CI, compare:
- `platformio.ini` `lib_deps` versions
- Python / pip version differences (CI uses `ubuntu-latest`)

---

## Step 4 — Artifact Inspection

After a successful build, download `firmware-SHA.bin` from the workflow artifacts.
Flash it manually to verify the build is correct before tagging a release:

```bash
esptool.py --port COM6 --baud 460800 write_flash 0x0 firmware.bin
```
