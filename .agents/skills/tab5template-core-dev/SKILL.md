---
name: tab5template-core-dev
description: Use for changes in Tab5Template firmware, component bring-up, splash behaviour, touch flow, WiFi scan logic, sdkconfig changes, and ESP-IDF build or debug tasks on M5Stack Tab5.
---

# Tab5Template Core Development

Repository-specific workflow for ESP32-P4 Tab5 firmware.

## When To Use This Skill

Use this skill when the request involves:
- Main app behaviour in main/Tab5Template.cpp.
- Hardware components in components/Tab5.
- Build and runtime issues tied to ESP-IDF or sdkconfig.
- Touch, SD, RTC, coprocessor, WiFi, IMU, power, charging, RS-485, or camera logic.
- Any request to add or modify board bring-up steps.

## Architecture Snapshot

- SoC: ESP32-P4, target is esp32p4.
- Main application entry: main/Tab5Template.cpp.
- Reusable board services: components/Tab5.
- Core component registration: components/Tab5/CMakeLists.txt.
- Top-level project CMake: CMakeLists.txt.

## Mandatory Bring-Up Ordering

Apply these ordering rules whenever modifying setup code:
1. Call display.init() before any feature that depends on internal Tab5 I2C or touch address selection.
2. Call TouchInput::Initialise(display, ...) only after display.init().
3. Call Rtc::Initialise() after display.init() because RTC reuses I2C_NUM_1 handle created during display init.
4. Call Coprocessor::Initialise() before WiFi::Initialise().
5. For SD card access, use SDCard::Initialise() so LDO channel 4 is acquired before mount.

## Display And Touch Rules

- MIPI-DSI updates require display.display() after drawing batches.
- Preferred pattern:
  - display.startWrite()
  - draw operations
  - display.endWrite()
  - display.display()
- Touch callbacks receive zero points when all fingers are released.

## WiFi Rules

- Scan API is WiFi::ScanForAccessPoints().
- Ensure WiFi singleton is initialised before scan.
- If hidden AP support is requested, preserve country/channel handling and BSSID-guided connect logic if already present.

## sdkconfig Safety Rules

When changing Kconfig-driven behaviour:
- Update both sdkconfig and sdkconfig.defaults.
- Preserve PSRAM requirements:
  - CONFIG_IDF_EXPERIMENTAL_FEATURES=y
  - CONFIG_SPIRAM_SPEED_200M=y
  - CONFIG_SPIRAM=y
  - CONFIG_SPIRAM_MODE_HEX=y

## Build And Validation

Preferred build commands:
- idf.py build
- idf.py flash monitor
- idf.py set-target esp32p4

Quality checks:
- ./run-checks.sh
- ./run-checks.sh --reformat

## Coding Conventions To Preserve

- C++20 and C11.
- Allman style with 4-space indentation.
- UK English spelling.
- return(...) style.
- nullptr over NULL.
- Full file header block with project copyright and target.
- Docstrings on namespaces, classes, functions, and variables as used in this repository.

## Productive Prompt Patterns

Use queries like:
- "Add a new Tab5 component for <feature> and wire it into components/Tab5/CMakeLists.txt and Setup()."
- "Refactor <component> to preserve singleton API and add robust error logging with ESP_LOGE."
- "Diagnose build/runtime failure by tracing bring-up order from display.init() through WiFi initialisation."
- "Patch sdkconfig and sdkconfig.defaults together for <option> and verify side effects on Tab5 display pipeline."
