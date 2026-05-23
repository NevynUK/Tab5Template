/*-----------------------------------------------------------------------------
 * File        : Tab5Template.cpp
 * Description : Application entry point for the M5Stack Tab5 firmware.
 *               Initialises all on-board hardware (display, touch, SD card,
 *               RTC, IMU, power monitor, charge manager, RS-485 and camera),
 *               renders a status splash, then enters the main FreeRTOS loop.
 * Author      : Mark Stevens
 * Copyright   : Copyright (c) 2026 Mark Stevens
 * Licence     : MIT — see LICENSE in the repository root for full terms.
 * Target      : M5Stack Tab5 (ESP32-P4)
 *---------------------------------------------------------------------------*/

#include <M5GFX.h>
#include <cstdio>
#include "Camera.hpp"
#include "ChargeManager.hpp"
#include "Coprocessor.hpp"
#include "Imu.hpp"
#include "PowerMonitor.hpp"
#include "Rs485.hpp"
#include "Rtc.hpp"
#include "SDCard.hpp"
#include "Touch.hpp"
#include "WiFi.hpp"
#include "Console.hpp"

/** Global display instance. */
M5GFX display;

const char *LOG_TAG = "Tab5Template";

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
 * Initialises the display, touch input, microSD card, RTC, IMU, power
 * monitor, charge manager, RS-485 transceiver and camera, then renders a
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

    // Initialise all on-board hardware before drawing the splash so every
    // subsystem status can be included in a single display update.
    SDCard *sdCard = SDCard::Initialise();
    Rtc *rtc = Rtc::Initialise();
    Imu::Initialise();
    PowerMonitor *powerMonitor = PowerMonitor::Initialise();
    ChargeManager *chargeManager = ChargeManager::Initialise();
    Rs485 *rs485 = Rs485::Initialise();
    Camera *camera = Camera::Initialise();

    // -------------------------------------------------------------------------
    // Status splash — drawn once after all hardware is ready.
    // -------------------------------------------------------------------------
    display.startWrite();
    display.fillScreen(TFT_WHITE);
    display.setTextColor(TFT_BLACK, TFT_WHITE);
    display.setTextDatum(textdatum_t::middle_center);

    const int centreX = display.width() / 2;
    const int lineHeight = 36;
    int lineY = 40;

    display.drawString("Tab5 Ready", centreX, lineY);
    lineY += lineHeight;

    // Touch subsystem status
    if (!display.touch())
    {
        display.setTextColor(TFT_RED, TFT_WHITE);
        display.drawString("Touch: not found", centreX, lineY);
        display.setTextColor(TFT_BLACK, TFT_WHITE);
    }
    else
    {
        display.drawString("Touch: OK", centreX, lineY);
    }

    lineY += lineHeight;

    // SD card mount status
    if (sdCard != nullptr && sdCard->IsMounted())
    {
        const sdmmc_card_t *card = sdCard->GetCard();
        const uint64_t sizeBytes = static_cast<uint64_t>(card->csd.capacity) * static_cast<uint64_t>(card->csd.sector_size);
        const double sizeGb = static_cast<double>(sizeBytes) / (1024.0 * 1024.0 * 1024.0);

        char sdInfo[64];
        snprintf(sdInfo, sizeof(sdInfo), "SD: %.1f GB  (%s)", sizeGb, card->cid.name);
        display.drawString(sdInfo, centreX, lineY);
    }
    else
    {
        display.setTextColor(TFT_RED, TFT_WHITE);
        display.drawString("SD: not found", centreX, lineY);
        display.setTextColor(TFT_BLACK, TFT_WHITE);
    }

    lineY += lineHeight;

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
            display.drawString(timeInfo, centreX, lineY);
        }
        else
        {
            display.setTextColor(TFT_RED, TFT_WHITE);
            display.drawString("RTC: read failed", centreX, lineY);
            display.setTextColor(TFT_BLACK, TFT_WHITE);
        }
    }
    else
    {
        display.setTextColor(TFT_RED, TFT_WHITE);
        display.drawString("RTC: not found", centreX, lineY);
        display.setTextColor(TFT_BLACK, TFT_WHITE);
    }

    lineY += lineHeight;

    // IMU status
    if (Imu::IsInitialised())
    {
        Imu::Vector3 acceleration;
        if (Imu::GetAcceleration(acceleration))
        {
            char imuInfo[64];
            snprintf(imuInfo, sizeof(imuInfo), "IMU: %.2fg  %.2fg  %.2fg", acceleration.x, acceleration.y, acceleration.z);
            display.drawString(imuInfo, centreX, lineY);
        }
        else
        {
            display.setTextColor(TFT_RED, TFT_WHITE);
            display.drawString("IMU: read failed", centreX, lineY);
            display.setTextColor(TFT_BLACK, TFT_WHITE);
        }
    }
    else
    {
        display.setTextColor(TFT_RED, TFT_WHITE);
        display.drawString("IMU: not found", centreX, lineY);
        display.setTextColor(TFT_BLACK, TFT_WHITE);
    }

    lineY += lineHeight;

    // Power monitor status
    if (powerMonitor != nullptr)
    {
        char powerInfo[64];
        snprintf(powerInfo, sizeof(powerInfo), "Power: %.2f V  %.0f mA", powerMonitor->GetBusVoltageVolts(), powerMonitor->GetCurrentMilliamps());
        display.drawString(powerInfo, centreX, lineY);
    }
    else
    {
        display.setTextColor(TFT_RED, TFT_WHITE);
        display.drawString("Power monitor: not found", centreX, lineY);
        display.setTextColor(TFT_BLACK, TFT_WHITE);
    }

    lineY += lineHeight;

    // Charge manager status
    if (chargeManager != nullptr)
    {
        const char *chargingStatus = chargeManager->IsCharging() ? "charging" : "not charging";
        char chargeInfo[48];
        snprintf(chargeInfo, sizeof(chargeInfo), "Charger: %s", chargingStatus);
        display.drawString(chargeInfo, centreX, lineY);
    }
    else
    {
        display.setTextColor(TFT_RED, TFT_WHITE);
        display.drawString("Charger: not found", centreX, lineY);
        display.setTextColor(TFT_BLACK, TFT_WHITE);
    }

    lineY += lineHeight;

    // RS-485 status
    if (rs485 != nullptr)
    {
        display.drawString("RS-485: OK", centreX, lineY);
    }
    else
    {
        display.setTextColor(TFT_RED, TFT_WHITE);
        display.drawString("RS-485: not found", centreX, lineY);
        display.setTextColor(TFT_BLACK, TFT_WHITE);
    }

    lineY += lineHeight;

    // Camera status
    if (camera != nullptr)
    {
        char cameraInfo[48];
        snprintf(cameraInfo, sizeof(cameraInfo), "Camera: %lu×%lu", camera->GetWidth(), camera->GetHeight());
        display.drawString(cameraInfo, centreX, lineY);
    }
    else
    {
        display.setTextColor(TFT_RED, TFT_WHITE);
        display.drawString("Camera: not found", centreX, lineY);
        display.setTextColor(TFT_BLACK, TFT_WHITE);
    }

    display.endWrite();
    display.display();
    display.setTextColor(TFT_WHITE, TFT_BLACK);

    Coprocessor::Initialise();
    WiFi::Initialise();
}

extern "C" void app_main(void)
{
    Setup();

    uint32_t x = 10;
    uint32_t y = 10;
    uint32_t w = display.width() - 20;
    uint32_t h = display.height() - 20;
    Console console(display, x, y, w, h);
    console.Printf("Boot complete, free heap: %lu", esp_get_free_heap_size());
    console.Println("Scanning WiFi...");
    while (true)
    {
        console.Printf("");
        console.Printf("********** Scan started **********");
        std::vector<AccessPointInfo> accessPoints = WiFi::ScanForAccessPoints();
        for (auto &ap: accessPoints)
        {
            console.Printf("Found WiFi network: SSID='%s', Signal=%d dBm, Channel=%d, Hidden=%s", ap.ssid.c_str(), ap.signalStrength, ap.channel, ap.hidden ? "yes" : "no");
            ESP_LOGI(LOG_TAG, "Found WiFi network: SSID='%s', Signal=%d dBm, Channel=%d, Hidden=%s", ap.ssid.c_str(), ap.signalStrength, ap.channel, ap.hidden ? "yes" : "no");
        }
        console.Printf("Scan complete, %d network(s) found.", accessPoints.size());
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    // All work is performed by the TouchInput FreeRTOS task.
    // Deleting this task frees its stack and TCB immediately rather than
    // keeping a do-nothing loop alive indefinitely.
    vTaskDelete(nullptr);
}
