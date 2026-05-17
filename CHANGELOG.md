# Changelog

All notable changes to **TokenDisplay** will be documented in this file.  
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).  
Versioning follows [Semantic Versioning](https://semver.org/).

---

## [Unreleased]

### Added
- Initial public release

---

## [v1.0.0] – 2026-05-17

### Added
- ST7789 1.9" TFT display driver (landscape 320 × 170)
- OpenRouter `/auth/key` API integration
- Multi-API-key support with BOOT button cycling
- Credit remaining + usage + limit progress bar UI
- WiFi connection screen with animated dots
- Auto-refresh every `UPDATE_INTERVAL` ms
- `config.h.example` — secrets never committed to git
- GitHub Actions CI build + Release workflow

---

<!-- Commit conventions used in this project

  feat:     new feature visible to the user
  fix:      bug fix
  ui:       display / visual change
  refactor: code structure, no behaviour change
  chore:    build, CI, tooling
  docs:     documentation only

  Examples
  --------
  git commit -m "feat: add brightness control via long-press"
  git commit -m "fix: prevent credit flicker on partial redraw"
  git commit -m "ui: increase credit font to size 5"
  git commit -m "chore: bump TFT_eSPI to 2.5.43"
-->
