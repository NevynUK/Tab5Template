/*-----------------------------------------------------------------------------
 * File        : Coprocessor.cpp
 * Description : Implementation of the Coprocessor singleton.  Initialises
 *               NVS flash, the TCP/IP network stack, the default event loop,
 *               and the ESP Hosted SDIO transport to the ESP32-C6 coprocessor.
 * Author      : Mark Stevens
 * Copyright   : Copyright (c) 2026 Mark Stevens
 * Licence     : MIT — see LICENSE in the repository root for full terms.
 * Target      : M5Stack Tab5 (ESP32-P4)
 *---------------------------------------------------------------------------*/
#include "Coprocessor.hpp"

#include <esp_err.h>
#include <esp_event.h>
#include <esp_hosted.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <nvs_flash.h>

/**
 * @brief Logging tag used for all ESP-IDF log output from this module.
 */
static constexpr const char *LOG_TAG = "Coprocessor";

/**
 * @brief Initialise the static singleton pointer to null.
 */
Coprocessor *Coprocessor::_instance = nullptr;

// =============================================================================
// Public static interface
// =============================================================================

/**
 * @brief Returns the existing singleton instance.
 *
 * @return Pointer to the singleton, or nullptr if Initialise() has not
 *         yet been called.
 */
Coprocessor *Coprocessor::GetInstance()
{
    return (_instance);
}

/**
 * @brief Creates and initialises the singleton.
 *
 * Performs the following sequence:
 *  1. Initialises NVS flash (erases and re-initialises on partition error).
 *  2. Initialises the TCP/IP network stack (esp_netif_init).
 *  3. Creates the default event loop (esp_event_loop_create_default).
 *  4. Starts the ESP Hosted SDIO transport via esp_hosted_init(), which
 *     asserts the hardware reset on GPIO 15, enumerates the ESP32-C6, and
 *     registers the virtual WiFi and Bluetooth network interfaces.
 *
 * @return Pointer to the newly created singleton, or nullptr if the
 *         singleton already exists or initialisation fails.
 */
Coprocessor *Coprocessor::Initialise()
{
    if (_instance != nullptr)
    {
        return (nullptr);
    }

    Coprocessor *candidate = new Coprocessor();
    if (!candidate->_initialised)
    {
        delete candidate;
        return (nullptr);
    }

    _instance = candidate;
    return (_instance);
}

// =============================================================================
// Constructor / Destructor
// =============================================================================

/**
 * @brief Private constructor — use Initialise() to create the singleton.
 *
 * Executes the full NVS, netif, event-loop, and ESP Hosted bring-up
 * sequence.  Sets _initialised to true on success.
 */
Coprocessor::Coprocessor() : _initialised(false)
{
    // -------------------------------------------------------------------------
    // Step 1: Initialise NVS flash.
    // NVS is required by the WiFi driver to store calibration data and
    // credentials.  Erase and re-initialise if the partition is corrupted.
    // -------------------------------------------------------------------------
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }

    if (result != ESP_OK)
    {
        ESP_LOGE(LOG_TAG, "NVS flash initialisation failed: %s", esp_err_to_name(result));
        return;
    }

    // -------------------------------------------------------------------------
    // Step 2: Initialise the TCP/IP network stack.
    // esp_netif_init() must be called once before any network interface is
    // created.  It is safe to call even if already initialised.
    // -------------------------------------------------------------------------
    result = esp_netif_init();
    if (result != ESP_OK)
    {
        ESP_LOGE(LOG_TAG, "TCP/IP network stack initialisation failed: %s", esp_err_to_name(result));
        return;
    }

    // -------------------------------------------------------------------------
    // Step 3: Create the default ESP-IDF event loop.
    // The WiFi driver and ESP Hosted both post events to this loop.
    // Guard against double-creation (ESP_ERR_INVALID_STATE).
    // -------------------------------------------------------------------------
    result = esp_event_loop_create_default();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(LOG_TAG, "Default event loop creation failed: %s", esp_err_to_name(result));
        return;
    }

    // -------------------------------------------------------------------------
    // Step 4: Start the ESP Hosted SDIO transport.
    // The SDIO GPIO assignments, clock speed, and the ESP32-C6 reset pin are
    // all configured in sdkconfig (see sdkconfig.defaults).  esp_hosted_init()
    // asserts the reset on GPIO 15, brings up the SDIO link between ESP32-P4
    // (host, slot 1) and ESP32-C6 (slave), and registers the virtual WiFi and
    // Bluetooth network interfaces.
    // -------------------------------------------------------------------------
    result = esp_hosted_init();
    if (result != ESP_OK)
    {
        ESP_LOGE(LOG_TAG, "ESP Hosted initialisation failed: %s", esp_err_to_name(result));
        return;
    }

    ESP_LOGI(LOG_TAG, "ESP32-C6 coprocessor ready (ESP Hosted over SDIO)");
    _initialised = true;
}

/**
 * @brief Destructor.  Resets the singleton pointer.
 */
Coprocessor::~Coprocessor()
{
    _instance = nullptr;
}

// =============================================================================
// Public instance interface
// =============================================================================

/**
 * @brief Returns whether the coprocessor was successfully initialised.
 *
 * @return true if ESP Hosted is running and the coprocessor is ready.
 */
bool Coprocessor::IsInitialised() const
{
    return (_initialised);
}
