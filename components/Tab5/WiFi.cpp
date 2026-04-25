/*-----------------------------------------------------------------------------
 * File        : WiFi.cpp
 * Description : Implementation of the WiFi singleton.  Provides WiFi station
 *               mode and access point scanning via the ESP32-C6 coprocessor
 *               using the standard ESP-IDF WiFi driver APIs.
 * Author      : Mark Stevens
 * Copyright   : Copyright (c) 2026 Mark Stevens
 * Licence     : MIT — see LICENSE in the repository root for full terms.
 * Target      : M5Stack Tab5 (ESP32-P4)
 * Build system: ESP-IDF v5.5.3
 *---------------------------------------------------------------------------*/
#include "WiFi.hpp"

#include <cstring>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>

/**
 * @brief Logging tag used for all ESP-IDF log output from this module.
 */
static constexpr const char *LOG_TAG = "WiFi";

/** 
 * @brief Initialise the static singleton pointer to null.
 */
WiFi *WiFi::_instance = nullptr;

/**
 * @brief Initialise the static initialisation flag to false.
 */
bool WiFi::_initialised = false;

// =============================================================================
// Public static interface
// =============================================================================

/**
 * @brief Returns the existing singleton instance.
 *
 * @return Pointer to the singleton, or nullptr if Initialise() has not
 *         yet been called.
 */
WiFi *WiFi::GetInstance()
{
    return (_instance);
}

/**
 * @brief Creates and initialises the singleton.
 *
 * Initialises the WiFi driver in station mode and starts the interface.
 * The WiFi driver communicates with the ESP32-C6 via the previously
 * established ESP Hosted SDIO transport.
 *
 * @return Pointer to the newly created singleton, or nullptr if the
 *         singleton already exists or initialisation fails.
 */
WiFi *WiFi::Initialise()
{
    if (_instance != nullptr)
    {
        return (nullptr);
    }

    WiFi *candidate = new WiFi();

    _instance = candidate;
    return (_instance);
}

// =============================================================================
// Constructor / Destructor
// =============================================================================

/**
 * @brief Private constructor — use Initialise() to create the singleton.
 *
 * Starts the WiFi driver in station mode.  Sets _initialised to true on
 * success.
 */
WiFi::WiFi()
{
    // Create the default WiFi station network interface.
    // This must be done before esp_wifi_init().
    esp_netif_create_default_wifi_sta();

    // Initialise the WiFi driver with the default configuration.
    // In ESP Hosted mode, this connects the driver to the ESP32-C6 via
    // the SDIO transport established by Coprocessor::Initialise().
    wifi_init_config_t initConfig = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t result = esp_wifi_init(&initConfig);
    if (result != ESP_OK)
    {
        ESP_LOGE(LOG_TAG, "WiFi driver initialisation failed: %s", esp_err_to_name(result));
        return;
    }

    // Set station mode — required before esp_wifi_start() and all scan operations.
    result = esp_wifi_set_mode(WIFI_MODE_STA);
    if (result != ESP_OK)
    {
        ESP_LOGE(LOG_TAG, "Failed to set WiFi station mode: %s", esp_err_to_name(result));
        esp_wifi_deinit();
        return;
    }

    // Start the WiFi interface.  The driver is now ready to scan and connect.
    result = esp_wifi_start();
    if (result != ESP_OK)
    {
        ESP_LOGE(LOG_TAG, "Failed to start WiFi driver: %s", esp_err_to_name(result));
        esp_wifi_deinit();
        return;
    }

    ESP_LOGI(LOG_TAG, "WiFi driver started in station mode");
    WiFi::_initialised = true;
}

/**
 * @brief Destructor.  Stops and de-initialises the WiFi driver.
 */
WiFi::~WiFi()
{
    esp_wifi_stop();
    esp_wifi_deinit();
    _instance = nullptr;
}

// =============================================================================
// Public static interface
// =============================================================================

/**
 * @brief Scans for nearby WiFi access points.
 *
 * Performs a blocking active scan across all channels.  Hidden networks
 * are included in the results with an empty SSID string.
 *
 * @return Vector of AccessPointInfo structs describing each access point
 *         found.  Returns an empty vector on failure.
 */
std::vector<AccessPointInfo> WiFi::ScanForAccessPoints()
{
    std::vector<AccessPointInfo> results;

    if (!_initialised)
    {
        ESP_LOGE(LOG_TAG, "ScanForAccessPoints called before WiFi was initialised");
        return (results);
    }

    // Configure the scan: include hidden networks, active scan, all channels.
    wifi_scan_config_t scanConfig = {};
    scanConfig.show_hidden = true;
    scanConfig.scan_type = WIFI_SCAN_TYPE_ACTIVE;

    // Start a blocking scan.  The call returns once all channels have been scanned.
    esp_err_t result = esp_wifi_scan_start(&scanConfig, true);
    if (result != ESP_OK)
    {
        ESP_LOGE(LOG_TAG, "WiFi scan failed to start: %s", esp_err_to_name(result));
        return (results);
    }

    // Retrieve the number of access points found, capped at MAX_SCAN_RESULTS.
    uint16_t apCount = MAX_SCAN_RESULTS;
    wifi_ap_record_t apRecords[MAX_SCAN_RESULTS];
    memset(apRecords, 0, sizeof(apRecords));

    result = esp_wifi_scan_get_ap_records(&apCount, apRecords);
    if (result != ESP_OK)
    {
        ESP_LOGE(LOG_TAG, "Failed to retrieve scan results: %s", esp_err_to_name(result));
        return (results);
    }

    ESP_LOGI(LOG_TAG, "WiFi scan complete: %u access point(s) found", apCount);

    results.reserve(apCount);
    for (uint16_t index = 0; index < apCount; ++index)
    {
        const wifi_ap_record_t &record = apRecords[index];

        AccessPointInfo info;
        info.ssid = std::string(reinterpret_cast<const char *>(record.ssid));
        info.signalStrength = record.rssi;
        info.channel = record.primary;
        info.hidden = (record.ssid[0] == '\0');

        results.push_back(info);
    }

    return (results);
}
