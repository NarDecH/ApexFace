# Changelog

All notable changes to ApexFace are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versioning: SemVer.

## [1.1.0] — 2026-10-27

### Added
- **Eye/mouth-weighted sharpness** — the whole-crop metrics are blended with eye-patch and
  mouth-patch metrics (25% weight, landmarks from YuNet): viewers judge portrait sharpness
  mostly by eyes and mouth, so a face with a soft face but crisp eyes now scores
  appropriately. Same calibrated 0–100 mapping.
- **Burst / near-duplicate grouping** — every image gets a 64-bit dHash; images within 10 bits
  of a group's anchor are grouped (greedy anchoring prevents runaway chains). The report shows
  ★ the best shot of each group, `data.csv`/`data.json` carry `group`, `group_best`,
  `group_size`. On the 400-photo race sample: 30 groups covering 352 images.
- **Per-run percentile** — every scored image reports "top X% of this run" (preview panel,
  report cards, hero card, CSV/JSON `percentile`).
- **Result cache** — unchanged files (same size + mtime + settings) are skipped on re-runs
  into the same output folder; results are restored from `<outDir>/analysis_cache.tsv`.
  Measured: 1.5 s → 0.65 s on a 5-photo folder, hours-scale savings on 9,000+ photo folders.
  GUI checkbox "Reuse cached results" / CLI `--no-cache`.
- **Drag & drop** — drop a folder onto the GUI window to select it as the source.
- **Recent folders** — the last five source folders are remembered (dropdown under the path).
- **Restore moved rejects** — every reject move is written to
  `<folder>/_apexface_rejects/_apexface_moved.tsv`; a GUI button (appears when a manifest
  exists) or `--restore-moved` moves everything back. Round-trip verified.
- **Unit tests + CI** — `tests/test_core.cpp` (rating boundaries, orientation, sharpness
  ordering, landmark completeness, EXIF) runs via ctest; GitHub Actions builds on
  windows-latest with OpenCV from vcpkg (manifest mode) and runs the tests on every push.

## [1.0.5] — 2026-10-27

### Added
- **Application icon & logo** — the magnifier-on-face logo (drawn with OpenCV,
  `tools/make_icon.cpp`) is embedded into `ApexFace.exe` (window, taskbar, Explorer) together
  with a VERSIONINFO resource, and used as the logo in the README. Regenerate with
  `scripts/make_icon.bat`.

## [1.0.4] — 2026-10-06

### Added
- **Facial-completeness check (grade D)** — the YuNet landmarks (eyes, nose, mouth corners)
  are now used: if the best face of an image is missing key components — an eye or the mouth
  occluded, cropped by the frame edge, or a strong profile that collapses the landmark
  geometry — the image is graded **D** (score 0), listed with an amber "incomplete" status in
  the GUI, counted in its own report stat, included in the D filter/distribution, and moved
  by move-rejects like other rejects. Incomplete faces are excluded from best-face selection
  and drawn with an amber "D" box in thumbnails/annotated copies.
  Verified: a synthetic half-cropped face is detected as `incomplete (missing an eye + mouth)`,
  while 400 clean photos produce zero false positives.

### Fixed
- Scanning a folder directly pointed at `_apexface_rejects` found 0 files (the own-output
  filter also matched the rejects folder itself). The filter now applies only to folders below
  the scanned root, so re-analyzing the rejects folder works.

## [1.0.3] — 2026-10-06

### Added
- **Report toggle** — "Generate HTML report + CSV/JSON" checkbox in the GUI and `--no-report`
  on the CLI turn a run into a pure scoring pass: no thumbnails, annotated copies, HTML, CSV
  or JSON are written (fast and disk-friendly for a first screening of huge folders).
  Move-rejects works independently of the toggle. When disabled the GUI shows
  "Analysis finished (report generation was disabled)" instead of the report buttons.

## [1.0.2] — 2026-10-06

### Fixed
- **Portrait photos displayed sideways.** The JPEG decoder in recent OpenCV versions applies
  the EXIF orientation automatically — and ApexFace then applied it a *second* time, rotating
  portrait photos back to landscape (which also made faces in them undetectable). Decoding now
  uses `IMREAD_IGNORE_ORIENTATION` and the orientation is applied exactly once, on every
  OpenCV version.
- `data.csv` rows for no-face/error images were missing columns; all rows now carry the full
  schema.

### Added
- **Move rejects** — grade-D photos *and* photos where no face was found can be moved
  automatically into a `_apexface_rejects` subfolder (keeping relative paths) of the source
  folder after the run finishes. Enable with the GUI checkbox ("Move grade-D / no-face files")
  or `--move-d` on the CLI. Originals are moved (never deleted); the report lists every moved
  file, `data.csv` gains a `moved_to` column, and every move is logged.
- **No-face photos are graded D** (score 0) instead of showing no rating — they now appear in
  the D filter, the score distribution, and the reject move.

### Changed
- Log panel refreshes its buffer only when new lines arrive (less GUI overhead during very
  large runs).

## [1.0.1] — 2026-10-06

Hotfix release; binary in the v1.0.0 asset was replaced (same tag, updated zip).

### Fixed
- **GUI crash dialog** — "16 visible items with conflicting ID!" appeared whenever the
  results table showed more than a couple of rows: the per-row rating button used its label
  ("A", "B", …) as the widget ID, which repeated across rows. Every row now opens a unique
  `PushID` scope.
- **Results table order** — rows appeared in worker completion order instead of file order;
  the table now always displays results sorted by file order.
- **Throughput on large folders** — JPEG encoding of annotated images was serialized behind
  a global mutex across all worker threads. Encoding is reentrant, so the mutex was removed;
  with "Save annotated images" enabled this roughly triples throughput (~6.7 → ~18 photos/s
  for 24 MP files on the dev machine).

## [1.0.0] — 2026-10-06

First public release.

### Added
- **Core engine** (`src/core/`): folder scanner (recursive, Unicode-safe, skips its own
  output), EXIF-orientation-aware image loading, YuNet face detection
  (`cv::FaceDetectorYN`) with automatic **CUDA → CPU** backend fallback and per-worker
  instances, multi-metric face sharpness scoring (Laplacian variance + Tenengrad + FFT
  high-frequency ratio + RMS contrast; 0–100 scale, A+/A/B/C/D ratings), thread pool with
  progress and cancel support.
- **GUI** (`ApexFace.exe`): Dear ImGui docking interface — native folder picker, source /
  detection / output options, live progress with ETA, results table with rating badges,
  preview panel with per-face metrics and annotated thumbnails, log panel with level
  filters, English/Thai language toggle, settings persisted to `config/settings.yml`.
- **CLI** (`apexface-cli.exe`): full engine access for batch use — `--out`, `--recursive`,
  `--min-face`, `--backend`, `--workers`, `--no-annotated`, `--no-csv`, `--no-json`,
  `--max-images`, `--dump-metrics`, `--quiet`.
- **HTML report**: summary page (stat cards, hero card for the folder's sharpest face, top
  gallery, score-distribution bars, error list), paginated card grid with per-rating
  filters, machine-readable `data.csv` + `data.json`, annotated full-resolution copies and
  480 px thumbnails with drawn face boxes and score labels.
- **Logging**: per-session `.log` file (TRACE–ERROR, millisecond timestamps, categories,
  rotation by session) plus structured `.events.jsonl` (per-image events with sizes,
  timings, face counts); session log copied into every report folder.
- **Docs**: README, RESEARCH (methodology, calibration data, benchmarks) and this
  changelog, each in Markdown and styled HTML, with screenshots and a pipeline diagram.
- **Vendored third-party**: Dear ImGui (docking), GLFW 3.4, nativefiledialog-extended;
  bundled YuNet ONNX model and Sarabun font (SIL OFL).
- Public-domain sample photos (incl. one synthetic blurred copy) for instant demos.
