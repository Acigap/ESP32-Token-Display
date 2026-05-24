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
| `.github/workflows/build.yml` | push to `main`/`develop`, any PR | Compile `esp32dev`, upload artifact |
| `.github/workflows/release.yml` | push tag `v*.*.*` | Compile `esp32dev` + create GitHub Release with `.bin` |

Current CI compiles only `esp32dev` by default.
If new boards are introduced, workflows must be extended explicitly.

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

### New board added locally but CI still only validates `esp32dev`
Symptom: local S3/TTGO build works, but CI never catches regressions for those envs.

Fix option A (build all envs in one job):
```yaml
- name: Build all firmware envs
  run: |
    pio run -e esp32dev
    pio run -e esp32s3-touch-lcd-1_9
    pio run -e ttgo-t-display
```

Fix option B (matrix):
```yaml
strategy:
  matrix:
    env: [esp32dev, esp32s3-touch-lcd-1_9, ttgo-t-display]

steps:
  - name: Build ${{ matrix.env }}
    run: pio run -e ${{ matrix.env }}
```

### `esp32s3-touch-lcd-1_9` fails in CI, works locally
Check:
1. `platformio.ini` still contains the S3 env platform URL and board settings.
2. Cache was invalidated after changing platform URL or dependencies.
3. Build command targets the exact env name (`esp32s3-touch-lcd-1_9`).

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

If changing board-specific code, also run the affected env locally (for example `esp32s3-touch-lcd-1_9`).

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
