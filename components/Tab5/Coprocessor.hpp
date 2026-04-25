/*-----------------------------------------------------------------------------
 * File        : Coprocessor.hpp
 * Description : Singleton class (Coprocessor) that initialises the ESP32-C6
 *               as a WiFi/BT coprocessor via ESP Hosted over SDIO.
 * Author      : Mark Stevens
 * Copyright   : Copyright (c) 2026 Mark Stevens
 * Licence     : MIT — see LICENSE in the repository root for full terms.
 * Target      : M5Stack Tab5 (ESP32-P4)
 * Build system: ESP-IDF v5.5.3
 *---------------------------------------------------------------------------*/
#pragma once

/**
 * @brief Singleton driver for the ESP32-C6 WiFi/BT coprocessor on the M5Stack Tab5.
 *
 * Initialises the NVS flash storage, TCP/IP network stack, default event loop,
 * and the ESP Hosted transport layer over SDIO.  After a successful
 * Initialise() call the ESP32-P4 communicates with the ESP32-C6 transparently
 * via the standard ESP-IDF WiFi and Bluetooth APIs.
 *
 * The ESP32-C6 must be pre-flashed with the ESP Hosted slave firmware before
 * this class is used.
 *
 * SDIO GPIO assignments for ESP32-P4 → ESP32-C6 connection.
 * These are configured in sdkconfig.defaults and used by the ESP Hosted
 * library internally.  Documented here for reference.
 *   SDIO CLK   : GPIO 12
 *   SDIO CMD   : GPIO 13
 *   SDIO D0    : GPIO 11
 *   SDIO D1    : GPIO 10
 *   SDIO D2    : GPIO  9
 *   SDIO D3    : GPIO  8
 *   C6 RESET   : GPIO 15 (CONFIG_ESP_HOSTED_SDIO_GPIO_RESET_SLAVE)
 *
 * @note Only one instance may exist at a time.  Calling Initialise() when
 *       the singleton already exists returns nullptr.
 *
 * @note Initialise() must be called before WiFi::Initialise() because the
 *       ESP Hosted transport must be active before the WiFi driver starts.
 */
class Coprocessor
{
public:
    static Coprocessor *GetInstance();

    static Coprocessor *Initialise();

    ~Coprocessor();

    bool IsInitialised() const;

private:
    Coprocessor();

    /**
     * @brief Singleton instance pointer.
     */
    static Coprocessor *_instance;

    /**
     * @brief True after the coprocessor has been successfully initialised.
     */
    bool _initialised;
};
