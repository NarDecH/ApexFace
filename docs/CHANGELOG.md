# Changelog

All notable changes to ApexFace are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versioning: SemVer.

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
