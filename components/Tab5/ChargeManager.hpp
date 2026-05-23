/*-----------------------------------------------------------------------------
 * File        : ChargeManager.hpp
 * Description : Singleton class (ChargeManager) for the IP2326 charge
 *               management IC on the M5Stack Tab5.  Charging enable and
 *               quick-charge selection are controlled via the PI4IOE5V6408
 *               I2C GPIO expander; charging status is read back from the
 *               same expander.
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
 * @brief Singleton for the IP2326 charge management IC on the M5Stack Tab5.
 *
 * The IP2326 control signals are routed through the second PI4IOE5V6408
 * I2C GPIO expander (address 0x44, I2C_NUM_1):
 *
 *   - P5 (output) → nCHG_QC_EN : LOW enables quick-charge (QC) mode.
 *   - P6 (input)  → CHG_STAT   : LOW indicates active charging.
 *   - P7 (output) → CHG_EN     : HIGH enables the charger.
 *
 * @note Initialise() must be called after display.init() because M5GFX
 *       creates the shared I2C bus handle that is reused here.
 */
class ChargeManager
{
public:
    // =========================================================================
    // Static interface
    // =========================================================================

    static ChargeManager *GetInstance();

    static ChargeManager *Initialise();

    ~ChargeManager();

    // =========================================================================
    // Control
    // =========================================================================

    bool EnableCharging(bool enable);

    bool EnableQuickCharge(bool enable);

    bool IsCharging() const;

    bool IsInitialised() const;

private:
    // =========================================================================
    // Hardware constants — IO expander (PI4IOE5V6408-2)
    // =========================================================================

    /**
     * @brief 7-bit I2C address of the second PI4IOE5V6408 IO expander.
     */
    static constexpr uint8_t EXPANDER_I2C_ADDRESS = 0x44;

    /**
     * @brief I2C clock frequency for IO expander communication.
     */
    static constexpr uint32_t I2C_FREQUENCY_HZ = 400000;

    // =========================================================================
    // PI4IOE5V6408 register map
    // =========================================================================

    /**
     * @brief Input port register — read current logic levels on all pins.
     */
    static constexpr uint8_t REG_INPUT_PORT = 0x00;

    /**
     * @brief Output port register — write desired output levels.
     */
    static constexpr uint8_t REG_OUTPUT_PORT = 0x01;

    /**
     * @brief Configuration register — 0 = output, 1 = input.
     */
    static constexpr uint8_t REG_CONFIGURATION = 0x03;

    // =========================================================================
    // IP2326 control pin bit masks (within the expander output/config byte)
    // =========================================================================

    /**
     * @brief Bit mask for nCHG_QC_EN on expander P5 (active-LOW QC enable).
     */
    static constexpr uint8_t PIN_QC_ENABLE = (1U << 5);

    /**
     * @brief Bit mask for CHG_STAT on expander P6 (active-LOW charge status).
     */
    static constexpr uint8_t PIN_CHARGE_STATUS = (1U << 6);

    /**
     * @brief Bit mask for CHG_EN on expander P7 (active-HIGH charge enable).
     */
    static constexpr uint8_t PIN_CHARGE_ENABLE = (1U << 7);

    // =========================================================================
    // Constructor and helpers
    // =========================================================================

    ChargeManager();

    bool WriteRegister(uint8_t registerAddress, uint8_t value) const;

    bool ReadRegister(uint8_t registerAddress, uint8_t &value) const;

    // =========================================================================
    // Members
    // =========================================================================

    /**
     * @brief The single instance of this class.
     */
    static ChargeManager *_instance;

    /**
     * @brief I2C master bus handle (borrowed from M5GFX / display.init()).
     */
    i2c_master_bus_handle_t _busHandle;

    /**
     * @brief I2C device handle for the PI4IOE5V6408 expander.
     */
    i2c_master_dev_handle_t _deviceHandle;

    /**
     * @brief Shadow of the output port register, kept in sync with every write.
     */
    uint8_t _outputState;

    /**
     * @brief True once the IO expander and control pins have been configured.
     */
    bool _initialised;
};
