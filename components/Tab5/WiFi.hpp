/*-----------------------------------------------------------------------------
 * File        : WiFi.hpp
 * Description : Singleton class (WiFi) that provides WiFi actions via the
 *               ESP32-C6 coprocessor running ESP Hosted.
 * Author      : Mark Stevens
 * Copyright   : Copyright (c) 2026 Mark Stevens
 * Licence     : MIT — see LICENSE in the repository root for full terms.
 * Target      : M5Stack Tab5 (ESP32-P4)
 * Build system: ESP-IDF v5.5.3
 *---------------------------------------------------------------------------*/
#pragma once

#include <string>
#include <vector>

/**
 * @brief Information about a single WiFi access point discovered during a scan.
 */
struct AccessPointInfo
{
    /**
     * @brief The SSID (network name) of the access point. Empty string if hidden.
     */
    std::string ssid;

    /**
     * @brief Received signal strength in dBm.  Higher (less negative) is stronger.
     */
    int signalStrength;

    /**
     * @brief Primary WiFi channel (1–13 for 2.4 GHz, 36+ for 5 GHz).
     */
    int channel;

    /**
     * @brief True if the access point does not broadcast its SSID.
     */
    bool hidden;
};

/**
 * @brief Singleton class providing WiFi functionality via the ESP32-C6 coprocessor.
 *
 * Uses the standard ESP-IDF WiFi driver APIs which are transparently routed
 * through the ESP Hosted SDIO link to the ESP32-C6 coprocessor.
 *
 * @note Coprocessor::Initialise() must be called and succeed before
 *       WiFi::Initialise() is called.
 *
 * @note Only one instance may exist at a time.  Calling Initialise() when
 *       the singleton already exists returns nullptr.
 */
class WiFi
{
public:
    static WiFi *GetInstance();

    static WiFi *Initialise();

    ~WiFi();

    static std::vector<AccessPointInfo> ScanForAccessPoints();

private:
    WiFi();

    /**
     * @brief Singleton instance pointer.
     */
    static WiFi *_instance;

    /**
     * @brief True after the WiFi driver has been successfully started.
     */
    static bool _initialised;

    /**
     * @brief Maximum number of access point records to retrieve per scan.
     */
    static constexpr uint16_t MAX_SCAN_RESULTS = 20;
};
