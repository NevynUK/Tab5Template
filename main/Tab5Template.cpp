/*-----------------------------------------------------------------------------
 * File        : Tab5Template.cpp
 * Description : Application entry point for the M5Stack Tab5 firmware.
 *               Initialises the display and the interrupt-driven touch input,
 *               then enters the main FreeRTOS loop.
 * Author      : Mark Stevens
 * Copyright   : Copyright (c) 2026 Mark Stevens
 * Licence     : MIT — see LICENSE in the repository root for full terms.
 * Target      : M5Stack Tab5 (ESP32-P4)
 * Build system: ESP-IDF v5.5.3
 *---------------------------------------------------------------------------*/

#include <M5GFX.h>
#include <cstdio>
#include "Rtc.hpp"
#include "SDCard.hpp"
#include "Touch.hpp"

/** Global display instance. */
M5GFX display;

/**
 * @brief Touch event callback.
 *
 * Invoked by TouchInput from the touch processing task whenever the ST7123
 * reports a change.  Renders raw and converted co-ordinates as text and draws
 * a shape at each touch point.  Clears the screen when all fingers are lifted.
 *
 * @param touchPoints  Array of screen-space touch points.
 * @param pointCount   Number of valid entries in touchPoints.  Zero when all
 *                     fingers have been lifted.
 */
static void OnTouchEvent(const lgfx::touch_point_t *touchPoints, int pointCount)
{
    static bool drawn = false;

    if (pointCount > 0)
    {
        // Retrieve raw co-ordinates for display; getTouchRaw is called
        // internally by TouchInput before conversion, so we mirror them here
        // by showing the converted values in both rows for simplicity.
        display.startWrite();

        for (int i = 0; i < pointCount; ++i)
        {
            display.setCursor(16, 16 + i * 24);
            display.printf("Touch %d  X:%04d  Y:%04d    ", touchPoints[i].id, touchPoints[i].x, touchPoints[i].y);
        }
        display.display();

        display.setColor(display.isEPD() ? TFT_BLACK : TFT_WHITE);
        for (int i = 0; i < pointCount; ++i)
        {
            int size = touchPoints[i].size + 3;
            switch (touchPoints[i].id)
            {
                case 0:
                    display.fillCircle(touchPoints[i].x, touchPoints[i].y, size);
                    break;
                case 1:
                    display.drawLine(touchPoints[i].x - size, touchPoints[i].y - size, touchPoints[i].x + size, touchPoints[i].y + size);
                    display.drawLine(touchPoints[i].x - size, touchPoints[i].y + size, touchPoints[i].x + size, touchPoints[i].y - size);
                    break;
                default:
                    display.fillTriangle(touchPoints[i].x - size, touchPoints[i].y + size, touchPoints[i].x + size, touchPoints[i].y + size, touchPoints[i].x, touchPoints[i].y - size);
                    break;
            }
            display.display();
        }

        display.endWrite();
        drawn = true;
    }
    else if (drawn)
    {
        drawn = false;
        display.startWrite();
        display.waitDisplay();
        display.clear();
        display.display();
        display.endWrite();
    }
}

/**
 * @brief One-time application setup.
 *
 * Initialises the display, touch input, and microSD card, then renders a
 * unified status splash showing the result of each subsystem.  All hardware
 * is initialised before the display is updated so the splash is drawn in a
 * single pass.
 */
void Setup(void)
{
    display.init();
    display.setBrightness(128); // AXP2101 backlight — 0 by default, must be set explicitly
    display.setFont(&fonts::Font4);
    // Tab5 native panel is portrait (720×1280); rotation 3 = landscape rotated 180 degrees (1280×720)
    display.setRotation(3);

    // display.init() configured GPIO_NUM_23 as output-high to select the ST7123
    // I2C address.  Now that init is complete, TouchInput can safely reconfigure
    // it as a falling-edge interrupt input.
    TouchInput::Initialise(display, OnTouchEvent);

    // Initialise the microSD card and RTC before drawing the splash so their
    // statuses can be included in a single display update.
    SDCard *sdCard = SDCard::Initialise();
    Rtc *rtc = Rtc::Initialise();

    // -------------------------------------------------------------------------
    // Status splash — drawn once after all hardware is ready.
    // -------------------------------------------------------------------------
    display.startWrite();
    display.fillScreen(TFT_WHITE);
    display.setTextColor(TFT_BLACK, TFT_WHITE);
    display.setTextDatum(textdatum_t::middle_center);

    const int centreX = display.width() / 2;
    const int centreY = display.height() / 2;

    display.drawString("Tab5 Ready", centreX, centreY - 96);

    // Touch subsystem status
    if (!display.touch())
    {
        display.setTextColor(TFT_RED, TFT_WHITE);
        display.drawString("Touch: not found", centreX, centreY - 48);
        display.setTextColor(TFT_BLACK, TFT_WHITE);
    }
    else
    {
        display.drawString("Touch: OK", centreX, centreY - 48);
    }

    // SD card mount status
    if (sdCard != nullptr && sdCard->IsMounted())
    {
        const sdmmc_card_t *card = sdCard->GetCard();
        const uint64_t sizeBytes = static_cast<uint64_t>(card->csd.capacity) * static_cast<uint64_t>(card->csd.sector_size);
        const double sizeGb = static_cast<double>(sizeBytes) / (1024.0 * 1024.0 * 1024.0);

        char sdInfo[64];
        snprintf(sdInfo, sizeof(sdInfo), "SD: %.1f GB  (%s)", sizeGb, card->cid.name);
        display.drawString(sdInfo, centreX, centreY);
    }
    else
    {
        display.setTextColor(TFT_RED, TFT_WHITE);
        display.drawString("SD card: not found", centreX, centreY);
        display.setTextColor(TFT_BLACK, TFT_WHITE);
    }

    // RTC status and current time
    if (rtc != nullptr)
    {
        struct tm setTime = {};
        setTime.tm_year = 2026 - 1900;
        setTime.tm_mon = 3 - 1; // March (0-based)
        setTime.tm_mday = 21;
        setTime.tm_hour = 14;
        setTime.tm_min = 42;
        setTime.tm_sec = 0;
        setTime.tm_wday = 6; // Saturday
        setTime.tm_isdst = -1;
        rtc->SetTime(setTime);

        struct tm currentTime = {};
        if (rtc->GetTime(currentTime))
        {
            char timeInfo[64];
            snprintf(timeInfo, sizeof(timeInfo), "RTC: %04d-%02d-%02d %02d:%02d:%02d", currentTime.tm_year + 1900, currentTime.tm_mon + 1, currentTime.tm_mday, currentTime.tm_hour, currentTime.tm_min, currentTime.tm_sec);
            display.drawString(timeInfo, centreX, centreY + 48);
        }
        else
        {
            display.setTextColor(TFT_RED, TFT_WHITE);
            display.drawString("RTC: read failed", centreX, centreY + 48);
            display.setTextColor(TFT_BLACK, TFT_WHITE);
        }
    }
    else
    {
        display.setTextColor(TFT_RED, TFT_WHITE);
        display.drawString("RTC: not found", centreX, centreY + 48);
        display.setTextColor(TFT_BLACK, TFT_WHITE);
    }

    display.endWrite();
    display.display();
    display.setTextColor(TFT_WHITE, TFT_BLACK);
}

extern "C" void app_main(void)
{
    Setup();

    // All work is performed by the TouchInput FreeRTOS task.
    // Deleting this task frees its stack and TCB immediately rather than
    // keeping a do-nothing loop alive indefinitely.
    vTaskDelete(nullptr);
}
