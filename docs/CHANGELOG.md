# Changelog

All notable changes to ApexFace are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versioning: SemVer.

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
