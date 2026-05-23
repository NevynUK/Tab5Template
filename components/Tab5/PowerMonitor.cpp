/*-----------------------------------------------------------------------------
 * File        : PowerMonitor.cpp
 * Description : Implementation of the PowerMonitor singleton for the INA226
 *               current and voltage monitor on the M5Stack Tab5.  The device
 *               is accessed over the shared internal I2C bus (I2C_NUM_1) and
 *               calibrated for a 20 mΩ shunt resistor by default.
 * Author      : Mark Stevens
 * Copyright   : Copyright (c) 2026 Mark Stevens
 * Licence     : MIT — see LICENSE in the repository root for full terms.
 * Target      : M5Stack Tab5 (ESP32-P4)
 *---------------------------------------------------------------------------*/

#include "PowerMonitor.hpp"

#include <cmath>
#include <esp_err.h>
#include <esp_log.h>

/**
 * @brief Logging tag used for all ESP-IDF log output from this module.
 */
static constexpr const char *LOG_TAG = "PowerMonitor";

/**
 * @brief Constant used to compute the calibration register value.
 *
 * From the INA226 datasheet: CAL = 0.00512 / (CurrentLSB × RSHUNT).
 */
static constexpr float CALIBRATION_CONSTANT = 0.00512f;

/**
 * @brief Maximum expected current in amps used to derive the current LSB.
 *
 * The current LSB is calculated as MaxCurrent / 32768.  A 4 A maximum gives
 * approximately 122 μA per bit, which is suitable for battery monitoring on
 * the Tab5.
 */
static constexpr float MAX_EXPECTED_CURRENT_AMPS = 4.0f;

/**
 * @brief Singleton for the INA226 power monitor on the M5Stack Tab5.
 *
 * Provides real-time measurements of bus voltage, shunt voltage, current,
 * and power.  The chip is accessed over the shared internal I2C bus.
 */
PowerMonitor *PowerMonitor::_instance = nullptr;

// =============================================================================
// Static interface
// =============================================================================

/**
 * @brief Returns the existing singleton instance.
 *
 * @return Pointer to the singleton, or nullptr if Initialise() has not yet
 *         been called.
 */
PowerMonitor *PowerMonitor::GetInstance()
{
    return (_instance);
}

/**
 * @brief Creates and initialises the singleton.
 *
 * Borrows the existing I2C bus handle from M5GFX (I2C_NUM_1), adds the
 * INA226 device, verifies the manufacturer ID, and programs the calibration
 * register so that current and power readings are correctly scaled.
 *
 * @param shuntResistanceOhms  Value of the shunt resistor in ohms.  Defaults
 *                             to DEFAULT_SHUNT_RESISTANCE_OHMS (20 mΩ).
 * @return Pointer to the newly created singleton, or nullptr if the singleton
 *         already exists or initialisation fails.
 */
PowerMonitor *PowerMonitor::Initialise(float shuntResistanceOhms)
{
    if (_instance != nullptr)
    {
        return (nullptr);
    }

    _instance = new PowerMonitor(shuntResistanceOhms);

    if (!_instance->_initialised)
    {
        delete _instance;
        _instance = nullptr;
    }

    return (_instance);
}

// =============================================================================
// Measurements
// =============================================================================

/**
 * @brief Reads the bus voltage.
 *
 * @return Bus voltage in volts, or 0.0f if not initialised or on I2C error.
 */
float PowerMonitor::GetBusVoltageVolts() const
{
    if (!_initialised)
    {
        return (0.0f);
    }

    uint16_t raw;
    if (!ReadRegister(REG_BUS_VOLTAGE, raw))
    {
        return (0.0f);
    }

    return (static_cast<float>(raw) * BUS_VOLTAGE_LSB_MV / 1000.0f);
}

/**
 * @brief Reads the shunt voltage.
 *
 * @return Shunt voltage in millivolts, or 0.0f if not initialised or on
 *         I2C error.
 */
float PowerMonitor::GetShuntVoltageMillivolts() const
{
    if (!_initialised)
    {
        return (0.0f);
    }

    uint16_t raw;
    if (!ReadRegister(REG_SHUNT_VOLTAGE, raw))
    {
        return (0.0f);
    }

    return (static_cast<float>(static_cast<int16_t>(raw)) * SHUNT_VOLTAGE_LSB_UV / 1000.0f);
}

/**
 * @brief Reads the measured current through the shunt.
 *
 * @return Current in milliamps (positive = into the load), or 0.0f if not
 *         initialised or on I2C error.
 */
float PowerMonitor::GetCurrentMilliamps() const
{
    if (!_initialised)
    {
        return (0.0f);
    }

    uint16_t raw;
    if (!ReadRegister(REG_CURRENT, raw))
    {
        return (0.0f);
    }

    return (static_cast<float>(static_cast<int16_t>(raw)) * _currentLsbMilliamps);
}

/**
 * @brief Reads the calculated power.
 *
 * @return Power in milliwatts, or 0.0f if not initialised or on I2C error.
 */
float PowerMonitor::GetPowerMilliwatts() const
{
    if (!_initialised)
    {
        return (0.0f);
    }

    uint16_t raw;
    if (!ReadRegister(REG_POWER, raw))
    {
        return (0.0f);
    }

    return (static_cast<float>(raw) * _powerLsbMilliwatts);
}

/**
 * @brief Returns true when the device has been successfully initialised.
 */
bool PowerMonitor::IsInitialised() const
{
    return (_initialised);
}

// =============================================================================
// Constructor / Destructor
// =============================================================================

/**
 * @brief Private constructor — use Initialise() to create the singleton.
 *
 * @param shuntResistanceOhms  Value of the shunt resistor in ohms.
 */
PowerMonitor::PowerMonitor(float shuntResistanceOhms) : _busHandle(nullptr), _deviceHandle(nullptr), _currentLsbMilliamps(0.0f), _powerLsbMilliwatts(0.0f), _initialised(false)
{
    esp_err_t result = i2c_master_get_bus_handle(I2C_NUM_1, &_busHandle);
    if (result != ESP_OK)
    {
        ESP_LOGE(LOG_TAG, "Failed to get I2C bus handle: %s", esp_err_to_name(result));
        return;
    }

    i2c_device_config_t devConfig = {};
    devConfig.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    devConfig.device_address = I2C_ADDRESS;
    devConfig.scl_speed_hz = I2C_FREQUENCY_HZ;

    result = i2c_master_bus_add_device(_busHandle, &devConfig, &_deviceHandle);
    if (result != ESP_OK)
    {
        ESP_LOGE(LOG_TAG, "Failed to add I2C device: %s", esp_err_to_name(result));
        return;
    }

    uint16_t manufacturerId;
    if (!ReadRegister(REG_MANUFACTURER_ID, manufacturerId) || manufacturerId != MANUFACTURER_ID)
    {
        ESP_LOGE(LOG_TAG, "Manufacturer ID mismatch: expected 0x%04X, got 0x%04X", MANUFACTURER_ID, manufacturerId);
        return;
    }

    if (!Configure(shuntResistanceOhms))
    {
        ESP_LOGE(LOG_TAG, "Failed to configure INA226.");
        return;
    }

    _initialised = true;
    ESP_LOGI(LOG_TAG, "INA226 initialised. Current LSB = %.3f mA/bit, Shunt = %.1f mΩ", _currentLsbMilliamps, shuntResistanceOhms * 1000.0f);
}

/**
 * @brief Destructor.
 *
 * Releases the I2C device handle and resets the singleton pointer.
 */
PowerMonitor::~PowerMonitor()
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
 * @brief Writes a 16-bit value to an INA226 register.
 *
 * The INA226 expects the MSB first.
 *
 * @param registerAddress  The register to write.
 * @param value            The 16-bit value to write.
 * @return true on success, false on I2C error.
 */
bool PowerMonitor::WriteRegister(uint8_t registerAddress, uint16_t value) const
{
    uint8_t buffer[3];
    buffer[0] = registerAddress;
    buffer[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    buffer[2] = static_cast<uint8_t>(value & 0xFF);

    esp_err_t result = i2c_master_transmit(_deviceHandle, buffer, sizeof(buffer), -1);
    return (result == ESP_OK);
}

/**
 * @brief Reads a 16-bit value from an INA226 register.
 *
 * @param registerAddress  The register to read.
 * @param value            Reference to store the read value.
 * @return true on success, false on I2C error.
 */
bool PowerMonitor::ReadRegister(uint8_t registerAddress, uint16_t &value) const
{
    uint8_t buffer[2];
    esp_err_t result = i2c_master_transmit_receive(_deviceHandle, &registerAddress, 1, buffer, sizeof(buffer), -1);
    if (result != ESP_OK)
    {
        return (false);
    }

    value = (static_cast<uint16_t>(buffer[0]) << 8) | static_cast<uint16_t>(buffer[1]);
    return (true);
}

/**
 * @brief Configures the INA226 averaging, conversion time, and calibration.
 *
 * Programs the configuration register and the calibration register so that
 * current and power registers report correctly scaled values.
 *
 * @param shuntResistanceOhms  Shunt resistor value in ohms.
 * @return true on success, false on I2C error.
 */
bool PowerMonitor::Configure(float shuntResistanceOhms)
{
    if (!WriteRegister(REG_CONFIGURATION, CONFIG_VALUE))
    {
        return (false);
    }

    _currentLsbMilliamps = (MAX_EXPECTED_CURRENT_AMPS / 32768.0f) * 1000.0f;
    _powerLsbMilliwatts = 25.0f * _currentLsbMilliamps;

    float currentLsbAmps = _currentLsbMilliamps / 1000.0f;
    uint16_t calibrationValue = static_cast<uint16_t>(std::round(CALIBRATION_CONSTANT / (currentLsbAmps * shuntResistanceOhms)));

    return (WriteRegister(REG_CALIBRATION, calibrationValue));
}
