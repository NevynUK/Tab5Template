/*-----------------------------------------------------------------------------
 * File        : Rtc.cpp
 * Description : Implementation of the Rtc singleton for the Epson RX8130CE
 *               real-time clock on the M5Stack Tab5.  The chip is accessed
 *               over I2C using the ESP-IDF v5 new i2c_master driver.  On
 *               first power-up the startup sequence detects an oscillation
 *               stop (VLF flag) and issues a software reset as required by
 *               the datasheet.
 * Author      : Mark Stevens
 * Copyright   : Copyright (c) 2026 Mark Stevens
 * Licence     : MIT — see LICENSE in the repository root for full terms.
 * Target      : M5Stack Tab5 (ESP32-P4)
 *---------------------------------------------------------------------------*/

#include "Rtc.hpp"

#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_check.h>
#include <esp_err.h>
#include <esp_log.h>

/**
 * @brief Logging tag used for all ESP-IDF log output from this module.
 */
static constexpr const char *LOG_TAG = "Rtc";

/**
 * @brief Singleton for the Epson RX8130CE real-time clock on the M5Stack Tab5.
 *
 * Provides a high-level interface for reading and writing time and date,
 * managing alarms, and using the wakeup timer.  The chip is accessed over
 * the Tab5's internal RTC I2C bus.
 *
 * @note Only one instance may exist at a time.  Calling Initialise() when
 *       the singleton already exists returns nullptr.
 */
Rtc *Rtc::_instance = nullptr;

// =============================================================================
// Static interface
// =============================================================================

/**
 * @brief Returns the existing singleton instance.
 *
 * @return Pointer to the singleton, or nullptr if Initialise() has not
 *         yet been called.
 */
Rtc *Rtc::GetInstance()
{
    return (_instance);
}

/**
 * @brief Creates and initialises the singleton.
 *
 * Configures the I2C bus and device handle, performs a software reset
 * if an oscillation stop is detected, and initialises internal state.
 *
 * @return Pointer to the newly created singleton, or nullptr if the
 *         singleton already exists.
 */
Rtc *Rtc::Initialise()
{
    if (_instance != nullptr)
    {
        return (nullptr);
    }

    _instance = new Rtc();
    return (_instance);
}

// =============================================================================
// Time
// =============================================================================

/**
 * @brief Reads the current time and date from the RTC.
 *
 * @param time  Structure to populate with the current time.
 * @return true if the time was read successfully, false on I2C error.
 */
bool Rtc::GetTime(struct tm &time) const
{
    if (!_initialised)
    {
        return (false);
    }

    uint8_t buffer[7];
    if (!ReadRegisters(REG_SEC, buffer, sizeof(buffer)))
    {
        return (false);
    }

    time.tm_sec = BcdToDec(buffer[0] & 0x7F);
    time.tm_min = BcdToDec(buffer[1] & 0x7F);
    time.tm_hour = BcdToDec(buffer[2] & 0x3F);
    time.tm_wday = BcdToDec(buffer[3] & 0x7F);
    time.tm_mday = BcdToDec(buffer[4] & 0x3F);
    time.tm_mon = BcdToDec(buffer[5] & 0x1F) - 1;
    time.tm_year = BcdToDec(buffer[6]) + 100;

    return (true);
}

/**
 * @brief Sets the RTC time and date.
 *
 * @param time  Structure containing the new time to set.
 * @return true if the time was set successfully, false on I2C error.
 */
bool Rtc::SetTime(const struct tm &time)
{
    if (!_initialised)
    {
        return (false);
    }

    uint8_t buffer[7];
    buffer[0] = DecToBcd(time.tm_sec);
    buffer[1] = DecToBcd(time.tm_min);
    buffer[2] = DecToBcd(time.tm_hour);
    buffer[3] = DecToBcd(time.tm_wday);
    buffer[4] = DecToBcd(time.tm_mday);
    buffer[5] = DecToBcd(time.tm_mon + 1);
    buffer[6] = DecToBcd(time.tm_year % 100);

    return (WriteRegister(REG_SEC, buffer[0]) && WriteRegister(REG_MIN, buffer[1]) && WriteRegister(REG_HOUR, buffer[2]) && WriteRegister(REG_WEEK, buffer[3]) && WriteRegister(REG_DAY, buffer[4]) && WriteRegister(REG_MONTH, buffer[5]) && WriteRegister(REG_YEAR, buffer[6]));
}

// =============================================================================
// Alarm
// =============================================================================

/**
 * @brief Sets the RTC alarm time and matching conditions.
 *
 * @param config  Alarm configuration structure.
 * @return true if set successfully, false on I2C error.
 */
bool Rtc::SetAlarm(const AlarmConfig &config)
{
    if (!_initialised)
    {
        return (false);
    }

    uint8_t buffer[3];
    buffer[0] = DecToBcd(config.minute) | (config.matchMinute ? 0x00 : 0x80);
    buffer[1] = DecToBcd(config.hour) | (config.matchHour ? 0x00 : 0x80);
    buffer[2] = DecToBcd(config.day) | (config.matchDay ? 0x00 : 0x80);

    if (config.matchWeekday)
    {
        buffer[2] |= 0x40;
    }

    return (WriteRegister(REG_ALARM_MIN, buffer[0]) && WriteRegister(REG_ALARM_HOUR, buffer[1]) && WriteRegister(REG_ALARM_WEEK_DAY, buffer[2]));
}

/**
 * @brief Reads the current RTC alarm configuration.
 *
 * @param config  Structure to populate with the alarm settings.
 * @return true if read successfully, false on I2C error.
 */
bool Rtc::GetAlarm(AlarmConfig &config) const
{
    if (!_initialised)
    {
        return (false);
    }

    uint8_t buffer[3];
    if (!ReadRegisters(REG_ALARM_MIN, buffer, sizeof(buffer)))
    {
        return (false);
    }

    config.minute = BcdToDec(buffer[0] & 0x7F);
    config.hour = BcdToDec(buffer[1] & 0x3F);
    config.day = BcdToDec(buffer[2] & 0x3F);

    config.matchMinute = !(buffer[0] & 0x80);
    config.matchHour = !(buffer[1] & 0x80);
    config.matchDay = !(buffer[2] & 0x80);
    config.matchWeekday = (buffer[2] & 0x40);

    return (true);
}

/**
 * @brief Enables or disables the alarm interrupt output.
 *
 * @param enable  True to enable the interrupt, false to disable.
 * @return true if updated successfully, false on I2C error.
 */
bool Rtc::EnableAlarmInterrupt(bool enable)
{
    if (!_initialised)
    {
        return (false);
    }

    uint8_t control0;
    if (!ReadRegisters(REG_CONTROL0, &control0, 1))
    {
        return (false);
    }

    if (enable)
    {
        control0 |= 0x08;
    }
    else
    {
        control0 &= ~0x08;
    }

    return (WriteRegister(REG_CONTROL0, control0));
}

// =============================================================================
// Wakeup timer
// =============================================================================

/**
 * @brief Starts the wakeup timer with the specified duration and clock.
 *
 * @param count            Number of source clock ticks before interrupt.
 * @param clockSource      The source clock frequency to use.
 * @param enableInterrupt  True to enable the interrupt output.
 * @return true if started successfully, false on I2C error.
 */
bool Rtc::StartWakeupTimer(uint16_t count, TimerClockSource clockSource, bool enableInterrupt)
{
    if (!_initialised)
    {
        return (false);
    }

    uint8_t extension;
    if (!ReadRegisters(REG_EXTENSION, &extension, 1))
    {
        return (false);
    }

    extension &= ~0x13;
    extension |= static_cast<uint8_t>(clockSource);
    if (enableInterrupt)
    {
        extension |= 0x10;
    }

    uint8_t buffer[2];
    buffer[0] = count & 0xFF;
    buffer[1] = (count >> 8) & 0xFF;

    return (WriteRegister(REG_TIMER_LOW, buffer[0]) && WriteRegister(REG_TIMER_HIGH, buffer[1]) && WriteRegister(REG_EXTENSION, extension));
}

/**
 * @brief Stops the wakeup timer and disables its interrupt output.
 *
 * @return true if stopped successfully, false on I2C error.
 */
bool Rtc::StopWakeupTimer()
{
    if (!_initialised)
    {
        return (false);
    }

    uint8_t extension;
    if (!ReadRegisters(REG_EXTENSION, &extension, 1))
    {
        return (false);
    }

    extension &= ~0x10;

    return (WriteRegister(REG_EXTENSION, extension));
}

// =============================================================================
// Status flags
// =============================================================================

/**
 * @brief Reads the RTC status and interrupt flags.
 *
 * @return A bitmask of StatusFlags.
 */
Rtc::StatusFlags Rtc::GetFlags() const
{
    if (!_initialised)
    {
        return (0);
    }

    uint8_t flags;
    if (!ReadRegisters(REG_FLAG, &flags, 1))
    {
        return (0);
    }

    return (flags);
}

/**
 * @brief Clears the specified status and interrupt flags.
 *
 * @param flags  Bitmask of flags to clear.
 * @return true if cleared successfully, false on I2C error.
 */
bool Rtc::ClearFlags(StatusFlags flags)
{
    if (!_initialised)
    {
        return (false);
    }

    uint8_t currentFlags;
    if (!ReadRegisters(REG_FLAG, &currentFlags, 1))
    {
        return (false);
    }

    currentFlags &= ~flags;

    return (WriteRegister(REG_FLAG, currentFlags));
}

// =============================================================================
// Constructor / Destructor
// =============================================================================

/**
 * @brief Private constructor — use Initialise() to create the singleton.
 */
Rtc::Rtc() : _busHandle(nullptr), _deviceHandle(nullptr), _initialised(false), _busOwned(false)
{
    i2c_master_bus_config_t busConfig = {};
    busConfig.i2c_port = I2C_NUM_0;
    busConfig.sda_io_num = RTC_SDA_PIN;
    busConfig.scl_io_num = RTC_SCL_PIN;
    busConfig.clk_source = I2C_CLK_SRC_DEFAULT;
    busConfig.glitch_ignore_cnt = 7;
    busConfig.flags.enable_internal_pullup = true;

    esp_err_t result = i2c_new_master_bus(&busConfig, &_busHandle);
    if (result == ESP_OK)
    {
        _busOwned = true;
    }
    else if (result == ESP_ERR_INVALID_STATE)
    {
        _busHandle = i2c_master_bus_handle_t(I2C_NUM_0);
        _busOwned = false;
    }
    else
    {
        ESP_LOGE(LOG_TAG, "Failed to create I2C bus: %s", esp_err_to_name(result));
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

    if (!PerformStartup())
    {
        ESP_LOGE(LOG_TAG, "RTC startup sequence failed.");
        return;
    }

    _initialised = true;
}

/**
 * @brief Destructor.
 *
 * Releases the I2C device handle and (if owned) the bus handle.  Resets
 * the singleton pointer so that Initialise() may be called again.
 */
Rtc::~Rtc()
{
    if (_deviceHandle != nullptr)
    {
        i2c_master_bus_rm_device(_deviceHandle);
        _deviceHandle = nullptr;
    }

    if (_busOwned && _busHandle != nullptr)
    {
        i2c_del_master_bus(_busHandle);
        _busHandle = nullptr;
    }

    _instance = nullptr;
}

// =============================================================================
// Helpers
// =============================================================================

/**
 * @brief Writes a single byte to an RTC register.
 *
 * @param registerAddress  The address of the register to write.
 * @param value            The byte value to write.
 * @return true if written successfully, false on I2C error.
 */
bool Rtc::WriteRegister(uint8_t registerAddress, uint8_t value) const
{
    uint8_t buffer[2] = {registerAddress, value};
    esp_err_t result = i2c_master_transmit(_deviceHandle, buffer, sizeof(buffer), -1);
    return (result == ESP_OK);
}

/**
 * @brief Reads one or more bytes from RTC registers.
 *
 * @param registerAddress  The starting register address.
 * @param buffer           Buffer to store the read bytes.
 * @param length           The number of bytes to read.
 * @return true if read successfully, false on I2C error.
 */
bool Rtc::ReadRegisters(uint8_t registerAddress, uint8_t *buffer, size_t length) const
{
    esp_err_t result = i2c_master_transmit_receive(_deviceHandle, &registerAddress, 1, buffer, length, -1);
    return (result == ESP_OK);
}

/**
 * @brief Converts a Binary Coded Decimal (BCD) byte to decimal.
 */
uint8_t Rtc::BcdToDec(uint8_t bcd)
{
    return (((bcd >> 4) * 10) + (bcd & 0x0F));
}

/**
 * @brief Converts a decimal byte to Binary Coded Decimal (BCD).
 */
uint8_t Rtc::DecToBcd(uint8_t decimal)
{
    return (((decimal / 10) << 4) | (decimal % 10));
}

/**
 * @brief Sets or clears the STOP bit in the Control1 register.
 *
 * The STOP bit must be cleared for the clock to begin oscillating.
 */
bool Rtc::SetStop(bool stop)
{
    uint8_t control1;
    if (!ReadRegisters(REG_CONTROL1, &control1, 1))
    {
        return (false);
    }

    if (stop)
    {
        control1 |= 0x01;
    }
    else
    {
        control1 &= ~0x01;
    }

    return (WriteRegister(REG_CONTROL1, control1));
}

/**
 * @brief Performs the initialisation sequence required by the datasheet.
 */
bool Rtc::PerformStartup()
{
    uint8_t flags = GetFlags();

    if (flags & FLAG_VLF)
    {
        ESP_LOGW(LOG_TAG, "Oscillation stop detected (VLF set). Performing software reset.");

        if (!WriteRegister(REG_CONTROL1, 0x01))
        {
            return (false);
        }

        vTaskDelay(pdMS_TO_TICKS(10));

        if (!ClearFlags(FLAG_VLF))
        {
            return (false);
        }
    }

    return (SetStop(false));
}
