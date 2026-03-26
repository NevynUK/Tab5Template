# Copilot Instructions — M5Test (M5Stack Tab5 / ESP32-P4)

## Project Overview

**M5Test** is ESP-IDF firmware for the **M5Stack Tab5** development tablet.

| Item | Value |
|---|---|
| SoC | ESP32-P4 (dual-core Xtensa LX9, 400 MHz) |
| PSRAM | 32 MB HEX PSRAM @ 200 MHz |
| Display | 1280×800 MIPI-DSI (native portrait: 720×1280) |
| Co-processor | ESP32-C6 (WiFi 6 / BLE 5) |
| Flash | 16 MB |
| PMU | AXP2101 |
| Language | C++23 |
| Build system | ESP-IDF v5.5.1 (CMake) |
| Libraries (Managed Components) | M5GFX, M5Unified |

Entry point: `main/M5Test.cpp` — `app_main()` calls `setup()` then loops `loop()`.

---

## File Structure

```
Tab5Template/
├── .github/copilot-instructions.md   # This file
├── main/
│   ├── Tab5Template.cpp              # Application entry point
│   ├── idf_component.yml             # IDF component manager manifest
│   └── CMakeLists.txt
├── CMakeLists.txt                    # Top-level; sets C++23, adds component dirs
├── partitions.csv                    # Custom 16MB partition table (4MB app + SPIFFS)
├── sdkconfig                         # Live build config — do not edit manually
└── sdkconfig.defaults                # Canonical intended settings
```

---

## Build & Flash

```bash
# One-time environment setup
. $IDF_PATH/export.sh

# Build
idf.py build

# Flash + monitor
idf.py flash monitor

# Set target (also regenerates sdkconfig from sdkconfig.defaults)
idf.py set-target esp32p4
```

---

## Critical sdkconfig Rules

### ⚠️ PSRAM 200 MHz — the most important setting in this project

The Tab5's MIPI-DSI display pipeline streams the framebuffer directly from PSRAM via DMA.
**200 MHz is mandatory.** M5GFX validates this at boot and logs a fatal error if PSRAM speed
is ≤ 80 MHz.

```
CONFIG_IDF_EXPERIMENTAL_FEATURES=y   # REQUIRED gate — without this, SPIRAM_SPEED_200M
                                     # is invisible to Kconfig and silently ignored
CONFIG_SPIRAM_SPEED_200M=y           # Tab5 MIPI-DSI requires exactly 200 MHz
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_HEX=y             # ESP32-P4 uses HEX PSRAM, not OCT
```

All four are set in `sdkconfig.defaults`. To verify the live build value:
```bash
grep CONFIG_SPIRAM_SPEED sdkconfig
# Must show: CONFIG_SPIRAM_SPEED=200
```

### sdkconfig vs sdkconfig.defaults

`sdkconfig.defaults` is **only read when sdkconfig is first generated**. Once `sdkconfig`
exists, the build uses it directly — `sdkconfig.defaults` is ignored.

**Rule:** When changing a config value mid-project, patch **both** `sdkconfig` and
`sdkconfig.defaults` and commit both together.

To force regeneration from `sdkconfig.defaults`:
```bash
idf.py set-target esp32p4   # deletes and recreates sdkconfig
```

### Partition table

Custom `partitions.csv` is required — the ESP-IDF default single-app layout is only 1 MB,
which is too small for M5Unified + M5GFX with MIPI-DSI drivers.

| Partition | Size |
|---|---|
| factory app | 4 MB |
| SPIFFS storage | ~12 MB |

---

## Display API (M5GFX / LovyanGFX)

```cpp
#include <M5GFX.h>
M5GFX display;   // global instance
```

### Initialisation (in setup())

```cpp
display.init();                  // init panel + auto-enables backlight at brightness 127
display.setBrightness(128);      // AXP2101 backlight; set explicitly for Tab5
display.setRotation(3);          // landscape (rotated 180°) for Tab5 cable orientation
                                 // use setRotation(1) for standard landscape
display.fillScreen(TFT_BLACK);
display.display();               // flush framebuffer to MIPI-DSI panel — required
```

### Drawing

```cpp
display.startWrite();            // begin batched SPI/DSI transaction
display.fillScreen(color);
display.fillRect(x, y, w, h, color);
display.fillCircle(x, y, r);
display.drawString("text", x, y);
display.printf("fmt %d", val);
display.endWrite();
display.display();               // flush — always call after drawing in MIPI-DSI mode
```

### Touch

```cpp
lgfx::touch_point_t tp[5];
int count = display.getTouchRaw(tp, 5);   // raw panel coords
display.convertRawXY(tp, count);          // convert to screen coords after rotation
```

### Display dimensions (after setRotation)

- `display.width()` — 1280 (landscape)
- `display.height()` — 720 (landscape)
- Native panel is portrait (720×1280); rotation adjusts these values.

---

## Programming Conventions

The project uses the following language versions:

- C++20
- C11

### Formatting

- Use Allman style
- Formatting rules are in the `.clang-format` file
- All spelling will be UK English.
- ise is preferred over ize
- All Namespaces, Classes, Functions and variables should have a docstring.

### Naming

- All constants will be upper case use snake style names
- Namespaces, Classes and Function names will be in PascalCase
- Class and Namespace private variables will start with an underscore and be in camelCase
- Parameters to functions will be in camelCase
- File names will be in PascalCase with lower case extensions
- Use whole words instead of abbreviations

### Indentation

- Use 4 spaces for indentation

### C/C++ Specific

- Use `nullptr` instead of `NULL`
- Return statements use `return()` style
- Use `const` where appropriate
- All constants will be upper case with underscores
- Class and namespace private variables will start with an underscore and be in camelCase
- Parameters to functions will be in camelCase
- File names will be in PascalCase with lower case extensions
- Use whole words
- Use spaces around operators
- Use spaces after commas
- Use spaces after casts