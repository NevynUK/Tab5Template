/*-----------------------------------------------------------------------------
 * File        : ChargeManager.cpp
 * Description : Implementation of the ChargeManager singleton for the IP2326
 *               charge management IC on the M5Stack Tab5.  Control signals
 *               are exercised through the PI4IOE5V6408 I2C GPIO expander
 *               at address 0x44 on the shared internal I2C bus (I2C_NUM_1).
 * Author      : Mark Stevens
 * Copyright   : Copyright (c) 2026 Mark Stevens
 * Licence     : MIT — see LICENSE in the repository root for full terms.
 * Target      : M5Stack Tab5 (ESP32-P4)
 *---------------------------------------------------------------------------*/

#include "ChargeManager.hpp"

#include <esp_err.h>
#include <esp_log.h>

/**
 * @brief Logging tag used for all ESP-IDF log output from this module.
 */
static constexpr const char *LOG_TAG = "ChargeManager";

/**
 * @brief Singleton for the IP2326 charge management IC on the M5Stack Tab5.
 *
 * Provides enable/disable control for charging and quick-charge mode, and
 * allows reading the current charging status, all via the PI4IOE5V6408 IO
 * expander connected to I2C_NUM_1.
 */
ChargeManager *ChargeManager::_instance = nullptr;

// =============================================================================
// Static interface
// =============================================================================

/**
 * @brief Returns the existing singleton instance.
 *
 * @return Pointer to the singleton, or nullptr if Initialise() has not yet
 *         been called.
 */
ChargeManager *ChargeManager::GetInstance()
{
    return (_instance);
}

/**
 * @brief Creates and initialises the singleton.
 *
 * Borrows the existing I2C bus handle from M5GFX (I2C_NUM_1), adds the
 * PI4IOE5V6408-2 device, configures P5 and P7 as outputs and P6 as an
 * input, then puts the charger into a safe initial state (charging enabled,
 * quick-charge disabled).
 *
 * @return Pointer to the newly created singleton, or nullptr if the singleton
 *         already exists or initialisation fails.
 */
ChargeManager *ChargeManager::Initialise()
{
    if (_instance != nullptr)
    {
        return (nullptr);
    }

    _instance = new ChargeManager();

    if (!_instance->_initialised)
    {
        delete _instance;
        _instance = nullptr;
    }

    return (_instance);
}

// =============================================================================
// Control
// =============================================================================

/**
 * @brief Enables or disables the battery charger.
 *
 * Sets the CHG_EN pin (P7) on the IO expander HIGH to enable charging, or
 * LOW to disable it.
 *
 * @param enable  True to enable charging, false to disable.
 * @return true on success, false on I2C error or if not initialised.
 */
bool ChargeManager::EnableCharging(bool enable)
{
    if (!_initialised)
    {
        return (false);
    }

    if (enable)
    {
        _outputState |= PIN_CHARGE_ENABLE;
    }
    else
    {
        _outputState &= static_cast<uint8_t>(~PIN_CHARGE_ENABLE);
    }

    return (WriteRegister(REG_OUTPUT_PORT, _outputState));
}

/**
 * @brief Enables or disables quick-charge (QC) mode.
 *
 * The nCHG_QC_EN pin (P5) is active-LOW: pulling it LOW enables QC mode.
 *
 * @param enable  True to enable QC, false to disable.
 * @return true on success, false on I2C error or if not initialised.
 */
bool ChargeManager::EnableQuickCharge(bool enable)
{
    if (!_initialised)
    {
        return (false);
    }

    if (enable)
    {
        _outputState &= static_cast<uint8_t>(~PIN_QC_ENABLE);
    }
    else
    {
        _outputState |= PIN_QC_ENABLE;
    }

    return (WriteRegister(REG_OUTPUT_PORT, _outputState));
}

/**
 * @brief Returns true when the battery is actively charging.
 *
 * Reads the CHG_STAT pin (P6).  The IP2326 drives this LOW while charging
 * is in progress.
 *
 * @return true if charging is active, false otherwise or on error.
 */
bool ChargeManager::IsCharging() const
{
    if (!_initialised)
    {
        return (false);
    }

    uint8_t inputState;
    if (!ReadRegister(REG_INPUT_PORT, inputState))
    {
        return (false);
    }

    return ((inputState & PIN_CHARGE_STATUS) == 0);
}

/**
 * @brief Returns true when the IO expander and control pins have been configured.
 */
bool ChargeManager::IsInitialised() const
{
    return (_initialised);
}

// =============================================================================
// Constructor / Destructor
// =============================================================================

/**
 * @brief Private constructor — use Initialise() to create the singleton.
 */
ChargeManager::ChargeManager() : _busHandle(nullptr), _deviceHandle(nullptr), _outputState(0), _initialised(false)
{
    esp_err_t result = i2c_master_get_bus_handle(I2C_NUM_1, &_busHandle);
    if (result != ESP_OK)
    {
        ESP_LOGE(LOG_TAG, "Failed to get I2C bus handle: %s", esp_err_to_name(result));
        return;
    }

    i2c_device_config_t devConfig = {};
    devConfig.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    devConfig.device_address = EXPANDER_I2C_ADDRESS;
    devConfig.scl_speed_hz = I2C_FREQUENCY_HZ;

    result = i2c_master_bus_add_device(_busHandle, &devConfig, &_deviceHandle);
    if (result != ESP_OK)
    {
        ESP_LOGE(LOG_TAG, "Failed to add IO expander device: %s", esp_err_to_name(result));
        return;
    }

    /*
     * Configure pin directions:
     *   P5 = output (nCHG_QC_EN)
     *   P6 = input  (CHG_STAT)
     *   P7 = output (CHG_EN)
     * All other pins are left at their power-on default (inputs, bit = 1).
     * Start with charging enabled (P7 HIGH) and QC disabled (P5 HIGH).
     */
    constexpr uint8_t outputPins = PIN_QC_ENABLE | PIN_CHARGE_ENABLE;
    constexpr uint8_t configValue = static_cast<uint8_t>(0xFF & ~outputPins);

    _outputState = PIN_CHARGE_ENABLE | PIN_QC_ENABLE;

    if (!WriteRegister(REG_OUTPUT_PORT, _outputState))
    {
        ESP_LOGE(LOG_TAG, "Failed to set initial output state.");
        return;
    }

    if (!WriteRegister(REG_CONFIGURATION, configValue))
    {
        ESP_LOGE(LOG_TAG, "Failed to configure IO expander pin directions.");
        return;
    }

    _initialised = true;
    ESP_LOGI(LOG_TAG, "ChargeManager initialised via IO expander 0x%02X.", EXPANDER_I2C_ADDRESS);
}

/**
 * @brief Destructor.
 *
 * Releases the I2C device handle and resets the singleton pointer.
 */
ChargeManager::~ChargeManager()
{
    if (_deviceHandle != nullptr)
    {
        i2c_master_bus_rm_device(_deviceHandle);
        _deviceHandle = nullptr;
    }

    _instance = nullptr;
}

// =============================================================================
// Helpers
// =============================================================================

/**
 * @brief Writes a single byte to a PI4IOE5V6408 register.
 *
 * @param registerAddress  The register to write.
 * @param value            The byte value to write.
 * @return true on success, false on I2C error.
 */
bool ChargeManager::WriteRegister(uint8_t registerAddress, uint8_t value) const
{
    uint8_t buffer[2] = {registerAddress, value};
    esp_err_t result = i2c_master_transmit(_deviceHandle, buffer, sizeof(buffer), -1);
    return (result == ESP_OK);
}

/**
 * @brief Reads a single byte from a PI4IOE5V6408 register.
 *
 * @param registerAddress  The register to read.
 * @param value            Reference to store the read byte.
 * @return true on success, false on I2C error.
 */
bool ChargeManager::ReadRegister(uint8_t registerAddress, uint8_t &value) const
{
    esp_err_t result = i2c_master_transmit_receive(_deviceHandle, &registerAddress, 1, &value, 1, -1);
    return (result == ESP_OK);
}
