# Copilot Instructions — Tab5Template (M5Stack Tab5 / ESP32-P4)

## Project Overview

**Tab5Template** is ESP-IDF firmware for the **M5Stack Tab5** development tablet.

| Item | Value |
|---|---|
| SoC | ESP32-P4 (dual-core Xtensa LX9, 400 MHz) |
| PSRAM | 32 MB HEX PSRAM @ 200 MHz |
| Display | 1280×800 MIPI-DSI (native portrait: 720×1280) |
| Touch controller | ST7123 (I2C address 0x14, interrupt on GPIO 23) |
| Co-processor | ESP32-C6 (WiFi 6 / BLE 5) |
| Flash | 16 MB |
| PMU | AXP2101 |
| Language | C++20 |
| Build system | ESP-IDF v5.5.1 (CMake) |
| Libraries (Managed Components) | M5GFX, M5Unified |

Entry point: `main/Tab5Template.cpp` — `app_main()` calls `Setup()` then deletes
itself via `vTaskDelete(nullptr)`.  All ongoing work runs in FreeRTOS tasks owned
by the component classes (e.g. `TouchInput`).

---

## File Structure

```
Tab5Template/
├── .github/
│   └── copilot-instructions.md       # This file
├── main/
│   ├── Tab5Template.cpp              # Application entry point
│   ├── idf_component.yml             # IDF component manager manifest
│   └── CMakeLists.txt                # Registers Tab5Template.cpp; requires Tab5, m5unified, m5gfx
├── components/
│   └── Tab5/
│       ├── CMakeLists.txt            # Registers Tab5 component; requires m5gfx, driver, freertos, sdmmc, fatfs
│       ├── Touch.hpp                 # TouchInput singleton — interrupt-driven touch input
│       ├── Touch.cpp
│       ├── SDCard.hpp                # SDCard singleton — SDMMC 4-bit microSD mount
│       └── SDCard.cpp
├── CMakeLists.txt                    # Top-level; sets C++20, adds components/Tab5
├── partitions.csv                    # Custom 16 MB partition table (4 MB app + SPIFFS)
├── sdkconfig                         # Live build config — do not edit manually
└── sdkconfig.defaults                # Canonical intended settings
```

---

## Architecture

### Component layout

Application-level code lives in `main/`.  Reusable hardware abstractions live
as individual ESP-IDF components under `components/`.  The top-level
`CMakeLists.txt` registers additional component directories:

```cmake
list(APPEND EXTRA_COMPONENT_DIRS components/Tab5)
```

New hardware drivers or services should be added as separate components under
`components/` following the same pattern, then listed in `EXTRA_COMPONENT_DIRS`
and added to the consumer's `REQUIRES`.

### TouchInput (`components/Tab5/`)

`TouchInput` is a singleton class that owns all touch input handling:

- Configures GPIO 23 as a falling-edge interrupt input (after `display.init()`
  has finished using it as an address-select output).
- Spawns a FreeRTOS task (`TouchTask`) that blocks on a binary semaphore given
  by the ISR.
- On wakeup, calls `getTouchRaw()` and `convertRawXY()`, then dispatches
  screen-space `lgfx::touch_point_t` data to all registered callbacks.
- Supports multiple callbacks added/removed at runtime; thread-safe via a mutex.
- Destructor removes the ISR, deletes the task and both semaphores, resets the
  singleton pointer.

**Key constraint:** `TouchInput::Initialise()` must be called _after_
`display.init()` because M5GFX drives GPIO 23 high during initialisation to
select the ST7123 I2C address (HIGH = 0x14).

```cpp
// Typical usage
TouchInput::Initialise(display, OnTouchEvent);          // with initial callback
TouchInput::Initialise(display);                        // no initial callback
TouchInput::GetInstance()->AddCallback(myCallback);
TouchInput::GetInstance()->RemoveCallback(myCallback);
```

Callback signature:

```cpp
void MyCallback(const lgfx::touch_point_t* touchPoints, int pointCount);
// pointCount == 0 means all fingers lifted
```

### SDCard (`components/Tab5/`)

`SDCard` is a singleton class that mounts the microSD card slot via SDMMC 4-bit mode:

- Acquires **on-chip LDO channel 4** to power the SD card slot (mandatory on Tab5).
- Configures the SDMMC host (slot 0, 40 MHz) with the Tab5 GPIO pin assignments.
- Mounts a FAT filesystem at `/sdcard` using `esp_vfs_fat_sdmmc_mount()`.
- After a successful mount, standard POSIX file I/O works under `/sdcard/`.
- Destructor unmounts the filesystem and releases the LDO.

**Pin assignments** (from M5Stack Tab5 schematics):

| Signal | GPIO |
|--------|------|
| CLK    | 43   |
| CMD    | 44   |
| D0     | 39   |
| D1     | 40   |
| D2     | 41   |
| D3     | 42   |

```cpp
// Typical usage
SDCard *sdCard = SDCard::Initialise();
if (sdCard != nullptr && sdCard->IsMounted())
{
    const sdmmc_card_t *card = sdCard->GetCard();  // card->cid.name, card->csd.*

    FILE *file = fopen("/sdcard/data.txt", "w");
    fprintf(file, "Hello, Tab5!\n");
    fclose(file);
}
```

**Key constraint:** LDO channel 4 *must* be acquired before attempting to access
the card.  Do not bypass `SDCard::Initialise()` and call `esp_vfs_fat_sdmmc_mount()`
directly without first initialising the LDO — the card will not respond.

### app_main pattern

`app_main` calls `Setup()` then immediately calls `vTaskDelete(nullptr)` to free
its stack.  No polling loop is needed; all work is driven by FreeRTOS tasks owned
by the component layer.

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
M5GFX display;   // global instance in main/Tab5Template.cpp
```

### Initialisation (in Setup())

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
display.startWrite();            // begin batched DSI transaction
display.fillScreen(color);
display.fillRect(x, y, w, h, color);
display.fillCircle(x, y, r);
display.drawString("text", x, y);
display.printf("fmt %d", val);
display.endWrite();
display.display();               // flush — always call after drawing in MIPI-DSI mode
```

### Touch

Touch is handled exclusively through `TouchInput`.  Do not call `getTouchRaw()`
or `convertRawXY()` directly from application code — register a callback instead.

### Display dimensions (after setRotation)

- `display.width()` — 1280 (landscape)
- `display.height()` — 720 (landscape)
- Native panel is portrait (720×1280); rotation adjusts these values.

---

## File Headers

Every `.cpp` and `.hpp` file must begin with a block comment containing:

```cpp
/*-----------------------------------------------------------------------------
 * File        : FileName.cpp
 * Description : One or two sentences describing the file's purpose.
 * Author      : Mark Stevens
 * Copyright   : Copyright (c) 2026 Mark Stevens
 * Licence     : MIT — see LICENSE in the repository root for full terms.
 * Target      : M5Stack Tab5 (ESP32-P4)
 * Build system: ESP-IDF v5.5.1
 *---------------------------------------------------------------------------*/
```

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
- Prefer `char *variable` over `char* variable` for pointer declarations
- Prefer `char &variable` over `char& variable` for reference declarations