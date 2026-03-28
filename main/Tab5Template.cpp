/*-----------------------------------------------------------------------------
 * File        : Tab5Template.cpp
 * Description : Application entry point for the M5Stack Tab5 firmware.
 *               Initialises the display and the interrupt-driven touch input,
 *               then enters the main FreeRTOS loop.
 * Author      : Mark Stevens
 * Copyright   : Copyright (c) 2026 Mark Stevens
 * Licence     : MIT — see LICENSE in the repository root for full terms.
 * Target      : M5Stack Tab5 (ESP32-P4)
 * Build system: ESP-IDF v5.5.1
 *---------------------------------------------------------------------------*/

#include <M5GFX.h>
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
static void OnTouchEvent(const lgfx::touch_point_t* touchPoints, int pointCount)
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
                    display.drawLine(touchPoints[i].x - size, touchPoints[i].y - size,
                                     touchPoints[i].x + size, touchPoints[i].y + size);
                    display.drawLine(touchPoints[i].x - size, touchPoints[i].y + size,
                                     touchPoints[i].x + size, touchPoints[i].y - size);
                    break;
                default:
                    display.fillTriangle(touchPoints[i].x - size, touchPoints[i].y + size,
                                         touchPoints[i].x + size, touchPoints[i].y + size,
                                         touchPoints[i].x,        touchPoints[i].y - size);
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
 * Initialises the display, renders a splash screen, and creates the
 * TouchInput singleton with OnTouchEvent pre-registered as its callback.
 */
void Setup(void)
{
    display.init();
    display.setBrightness(128); // AXP2101 backlight — 0 by default, must be set explicitly
    display.setFont(&fonts::Font4);
    // Tab5 native panel is portrait (720×1280); rotation 3 = landscape rotated 180 degrees (1280×720)
    display.setRotation(3);

    // Confirm display pipeline with a visible splash
    display.fillScreen(TFT_WHITE);
    display.setTextColor(TFT_BLACK, TFT_WHITE);
    display.setTextDatum(textdatum_t::middle_center);
    display.drawString("Tab5 Ready", display.width() / 2, display.height() / 2 - 24);
    if (!display.touch())
    {
        display.drawString("Touch not found.", display.width() / 2, display.height() / 2 + 24);
    }
    display.display();
    display.setTextColor(TFT_WHITE, TFT_BLACK);

    // display.init() configured GPIO_NUM_23 as output-high to select the ST7123
    // I2C address.  Now that init is complete, TouchInput can safely reconfigure
    // it as a falling-edge interrupt input.
    TouchInput::Initialise(display, OnTouchEvent);
}

extern "C" void app_main(void)
{
    Setup();

    // All work is performed by the TouchInput FreeRTOS task.
    // Deleting this task frees its stack and TCB immediately rather than
    // keeping a do-nothing loop alive indefinitely.
    vTaskDelete(nullptr);
}
