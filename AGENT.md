# AGENT.md — คู่มือสำหรับ AI Agent ที่ทำงานในโปรเจกต์ ApexFace

> อ่านไฟล์นี้ให้ครบก่อนแก้ไขอะไรใน repo นี้

## 1) ApexFace คืออะไร

โปรแกรม C++20 (Windows x64) ที่รับ "โฟลเดอร์รูปภาพ" แล้วค้นหา**ใบหน้าที่คมชัดที่สุด**
ในทุกรูป ให้คะแนน 0–100 ต่อใบหน้า และสร้างรายงาน HTML พร้อมรูปที่วาดกรอบหน้า + คะแนน
ประกอบด้วย GUI (Dear ImGui + GLFW + OpenGL) และ CLI (`apexface-cli`)

## 2) ข้อเท็จจริงสำคัญของสภาพแวดล้อม (เครื่องพัฒนา)

| หัวข้อ | ค่า |
|---|---|
| OpenCV | 4.14.0 (build พร้อม CUDA 12.9) ติดตั้งที่ `C:\_OpenCV\install` — `find_package(OpenCV)` หาเจอผ่าน CMake user package registry |
| Compiler | VS 2022 Community (cl 19.44), สั่ง build ผ่าน `build.bat` (เรียก vcvars64 ให้เอง) |
| GUI stack | Dear ImGui (docking) + GLFW 3.4 + nfd — ทั้งหมด vendored ใน `third_party/` |
| โมเดล | `models/face_detection_yunet_2023mar.onnx` (Apache-2.0 จาก opencv_zoo) |
| ฟอนต์ | `fonts/Sarabun-*.ttf` (OFL) — จำเป็นสำหรับภาษาไทยใน GUI |
| รูปทดสอบส่วนตัว | `_photo/` (ภาพงานวิ่งจริง 9,416 รูป) — **ห้าม commit / ห้าม publish** อยู่ใน .gitignore แล้ว |
| รูปตัวอย่าง | `samples/` เป็นภาพ public domain จาก Wikimedia/NASA — commit ได้ |

## 3) คำสั่งที่ใช้บ่อย

```bat
build.bat                          :: configure + build Release (MSVC x64)
build.bat clean                    :: ลบ build/ แล้ว configure ใหม่
scripts\run_gui.bat                :: รัน GUI พร้อม PATH ของ OpenCV DLL
scripts\run_cli.bat <folder> ...   :: รัน CLI
```

- ถ้ารัน .exe ตรงๆ แล้วหา DLL ไม่เจอ ให้ copy `opencv_core4140.dll`, `opencv_imgproc4140.dll`,
  `opencv_imgcodecs4140.dll`, `opencv_objdetect4140.dll`, `opencv_dnn4140.dll` จาก
  `C:\_OpenCV\install\x64\vc17\bin\` ไปไว้ข้าง exe (เคยทำไว้แล้วใน `build/Release/`)
- ทดสอบเร็ว: `build\Release\apexface-cli.exe samples --out _tmp\test_report`
- Calibrate คะแนน: รัน CLI ด้วย `--dump-metrics file.csv` กับรูปจริง แล้วดู quantiles
  (วิธีอยู่ใน `docs/RESEARCH.md` §5)

## 4) โครงสร้างโค้ด

```
src/core/    Logger, Platform, ImageIO, Settings, Sharpness, FaceDetector, Analyzer, ReportGenerator
src/gui/     App.cpp (ทั้ง UI), Theme.cpp (สี/ฟอนต์)
src/app/     main_gui.cpp (WinMain), main_cli.cpp (CLI)
docs/        README/RESEARCH/CHANGELOG ทั้ง .md และ .html (แก้เป็นคู่เสมอ)
assets/img/  ภาพประกอบเอกสาร (ถ่ายจาก GUI/รายงานจริง)
samples/     ภาพ public domain สำหรับ demo
```

## 5) ข้อตกลง/กับดักที่เคยเจอ

- **MSVC ไม่รองรับ `std::atomic<std::string>`** — ใช้ `CurrentFileReporter` (mutex) ใน `Analyzer.h`
- **ImGui 1.92**: `PushFont(font, 0.0f)` เท่านั้น (เวอร์ชันพารามิเตอร์เดียวถูกลบ),
  `ImGui::Image` ใช้ `ImTextureRef`, uv0=(0,0) เมื่ออัปโหลด cv::Mat ตรงๆ
- **`GL/gl.h` ต้อง include หลัง `<windows.h>`** และ `GL_BGR`/`GL_CLAMP_TO_EDGE` ต้อง
  define เองถ้าไม่มี
- ทุกไฟล์ต้องคอมไพล์ด้วย `/utf-8` (มีข้อความไทยในซอร์ส)
- อ่าน/เขียนไฟล์รูปต้องผ่าน `imageio::loadBgr/saveJpeg` เท่านั้น (รองรับ path ไทย/ยูนิโคด)
  — ห้ามใช้ `cv::imread/imwrite` กับ path ตรงๆ
- ตรวจจับใบหน้าใช้ `cv::FaceDetectorYN` (objdetect) — อย่ากลับไปเรียก ONNX ผ่าน `cv::dnn` เอง
- สแกนโฟลเดอร์จะข้ามโฟลเดอร์ที่ขึ้นต้นด้วย `_apexface` เสมอ (กันสแกนรายงานเก่าซ้ำ)
- Log ทุกเหตุการณ์สำคัญผ่าน `AF_INFO/AF_WARN/...` พร้อม category เช่น `scan`, `detect`, `gui`
  และ event แบบมีโครงสร้างผ่าน `Logger::event()` (จะไปอยู่ใน `*.events.jsonl`)

## 6) เวลาแก้เอกสาร

- `README.md` (root) กับ `docs/README.md` ต้องเนื้อหาตรงกัน
- `docs/RESEARCH.md` + `docs/RESEARCH.html` และ `docs/CHANGELOG.md` + `docs/CHANGELOG.html`
  เป็นคู่ md/html — แก้แล้วต้อง sync ทั้งคู่
- ภาพประกอบใหม่ควรมาจากการรันจริง (CLI บน `samples/` แล้ว copy จาก report, หรือ
  screenshot จาก GUI) อย่าใช้ภาพจาก `_photo/`
