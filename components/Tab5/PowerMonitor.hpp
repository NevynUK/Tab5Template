/*-----------------------------------------------------------------------------
 * File        : PowerMonitor.hpp
 * Description : Singleton class (PowerMonitor) for the INA226 current and
 *               voltage monitor on the M5Stack Tab5.  Provides real-time
 *               measurements of bus voltage, shunt voltage, current and
 *               power over I2C.
 * Author      : Mark Stevens
 * Copyright   : Copyright (c) 2026 Mark Stevens
 * Licence     : MIT — see LICENSE in the repository root for full terms.
 * Target      : M5Stack Tab5 (ESP32-P4)
 *---------------------------------------------------------------------------*/

#pragma once

#include <cstdint>
#include <driver/gpio.h>
#include <driver/i2c_master.h>

/**
 * @brief Singleton for the INA226 power monitor on the M5Stack Tab5.
 *
 * Wraps the INA226 16-bit precision current/voltage/power monitor IC.
 * The device is accessed over the Tab5 internal I2C bus (I2C_NUM_1,
 * SDA = GPIO_NUM_31, SCL = GPIO_NUM_32).
 *
 * A 20 mΩ shunt resistor is assumed by default; pass a different value to
 * Initialise() if the board revision uses a different shunt.
 *
 * @note Initialise() must be called after display.init() because M5GFX
 *       creates the I2C bus handle that is reused here.
 */
class PowerMonitor
{
public:
    // =========================================================================
    // Static interface
    // =========================================================================

    static PowerMonitor *GetInstance();

    static PowerMonitor *Initialise(float shuntResistanceOhms = DEFAULT_SHUNT_RESISTANCE_OHMS);

    ~PowerMonitor();

    // =========================================================================
    // Measurements
    // =========================================================================

    float GetBusVoltageVolts() const;

    float GetShuntVoltageMillivolts() const;

    float GetCurrentMilliamps() const;

    float GetPowerMilliwatts() const;

    bool IsInitialised() const;

    // =========================================================================
    // Constants
    // =========================================================================

    /**
     * @brief Default shunt resistor value in ohms (20 mΩ).
     */
    static constexpr float DEFAULT_SHUNT_RESISTANCE_OHMS = 0.020f;

private:
    // =========================================================================
    // Hardware constants
    // =========================================================================

    /**
     * @brief INA226 7-bit I2C address on the Tab5 (A0=HIGH, A1=LOW → 0x41).
     */
    static constexpr uint8_t I2C_ADDRESS = 0x41;

    /**
     * @brief I2C clock frequency for INA226 communication.
     */
    static constexpr uint32_t I2C_FREQUENCY_HZ = 400000;

    /**
     * @brief INA226 expected manufacturer ID register value.
     */
    static constexpr uint16_t MANUFACTURER_ID = 0x5449;

    /**
     * @brief INA226 expected die ID register value.
     */
    static constexpr uint16_t DIE_ID = 0x2260;

    // =========================================================================
    // Register map
    // =========================================================================

    /**
     * @brief Configuration register.
     */
    static constexpr uint8_t REG_CONFIGURATION = 0x00;

    /**
     * @brief Shunt voltage register (2.5 μV per LSB).
     */
    static constexpr uint8_t REG_SHUNT_VOLTAGE = 0x01;

    /**
     * @brief Bus voltage register (1.25 mV per LSB).
     */
    static constexpr uint8_t REG_BUS_VOLTAGE = 0x02;

    /**
     * @brief Power register.
     */
    static constexpr uint8_t REG_POWER = 0x03;

    /**
     * @brief Current register (scaled by calibration).
     */
    static constexpr uint8_t REG_CURRENT = 0x04;

    /**
     * @brief Calibration register.
     */
    static constexpr uint8_t REG_CALIBRATION = 0x05;

    /**
     * @brief Manufacturer ID register (should read 0x5449).
     */
    static constexpr uint8_t REG_MANUFACTURER_ID = 0xFE;

    /**
     * @brief Die ID register (should read 0x2260).
     */
    static constexpr uint8_t REG_DIE_ID = 0xFF;

    // =========================================================================
    // Configuration bit fields
    // =========================================================================

    /**
     * @brief Configuration register value: 16 samples average, 1.1 ms
     *        conversion time for both bus and shunt, continuous mode.
     */
    static constexpr uint16_t CONFIG_VALUE = 0x4527;

    /**
     * @brief Shunt voltage LSB in microvolts.
     */
    static constexpr float SHUNT_VOLTAGE_LSB_UV = 2.5f;

    /**
     * @brief Bus voltage LSB in millivolts.
     */
    static constexpr float BUS_VOLTAGE_LSB_MV = 1.25f;

    // =========================================================================
    // Constructor and helpers
    // =========================================================================

    explicit PowerMonitor(float shuntResistanceOhms);

    bool WriteRegister(uint8_t registerAddress, uint16_t value) const;

    bool ReadRegister(uint8_t registerAddress, uint16_t &value) const;

    bool Configure(float shuntResistanceOhms);

    // =========================================================================
    // Members
    // =========================================================================

    /**
     * @brief The single instance of this class.
     */
    static PowerMonitor *_instance;

    /**
     * @brief I2C master bus handle (borrowed from M5GFX / display.init()).
     */
    i2c_master_bus_handle_t _busHandle;

    /**
     * @brief I2C device handle for the INA226.
     */
    i2c_master_dev_handle_t _deviceHandle;

    /**
     * @brief Current LSB in milliamps per bit, derived from calibration.
     */
    float _currentLsbMilliamps;

    /**
     * @brief Power LSB in milliwatts per bit (= 25 × current LSB).
     */
    float _powerLsbMilliwatts;

    /**
     * @brief True once the chip has been successfully initialised.
     */
    bool _initialised;
};
