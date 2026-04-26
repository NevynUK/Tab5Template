/*-----------------------------------------------------------------------------
 * File        : Rtc.hpp
 * Description : Singleton class (Rtc) for the Epson RX8130CE real-time clock
 *               on the M5Stack Tab5.  Provides time get/set, three alarm
 *               channels (minute, hour, day/week), and wakeup-timer interrupt
 *               support via the chip's /IRQ output and a configurable GPIO.
 * Author      : Mark Stevens
 * Copyright   : Copyright (c) 2026 Mark Stevens
 * Licence     : MIT — see LICENSE in the repository root for full terms.
 * Target      : M5Stack Tab5 (ESP32-P4)
 *---------------------------------------------------------------------------*/

#pragma once

#include <cstdint>
#include <ctime>
#include <driver/gpio.h>
#include <driver/i2c_master.h>

class Rtc
{
public:
    // =========================================================================
    // Types
    // =========================================================================

    /**
     * @brief Configuration for a single RTC alarm event.
     */
    struct AlarmConfig
    {
        uint8_t minute = 0;
        uint8_t hour = 0;
        uint8_t day = 0;

        bool matchMinute = false;
        bool matchHour = false;
        bool matchDay = false;
        bool matchWeekday = false;
    };

    /**
     * @brief Source clock frequencies for the wakeup timer.
     */
    enum class TimerClockSource : uint8_t {
        Hz4096 = 0x00,
        Hz64 = 0x01,
        Hz1 = 0x02,
        Min1 = 0x03
    };

    /**
     * @brief Bitmask type for RTC status and interrupt flags.
     */
    using StatusFlags = uint8_t;

    /**
     * @brief VBLF — VBAT low-voltage detection.
     */
    static constexpr StatusFlags FLAG_VBAT_LOW = 0x80;

    /**
     * @brief UF — time-update interrupt fired.
     */
    static constexpr StatusFlags FLAG_UPDATE = 0x20;

    /**
     * @brief TF — wakeup-timer interrupt fired.
     */
    static constexpr StatusFlags FLAG_TIMER = 0x10;

    /**
     * @brief AF — alarm interrupt fired.
     */
    static constexpr StatusFlags FLAG_ALARM = 0x08;

    /**
     * @brief VLF — oscillation-stop (voltage-low) flag.
     */
    static constexpr StatusFlags FLAG_VLF = 0x02;

    // =========================================================================
    // Static interface
    // =========================================================================

    static Rtc *GetInstance();

    static Rtc *Initialise();

    ~Rtc();

    // =========================================================================
    // Time
    // =========================================================================

    bool GetTime(struct tm &time) const;

    bool SetTime(const struct tm &time);

    // =========================================================================
    // Alarm
    // =========================================================================

    bool SetAlarm(const AlarmConfig &config);

    bool GetAlarm(AlarmConfig &config) const;

    bool EnableAlarmInterrupt(bool enable);

    // =========================================================================
    // Wakeup timer
    // =========================================================================

    bool StartWakeupTimer(uint16_t count, TimerClockSource clockSource, bool enableInterrupt = true);

    bool StopWakeupTimer();

    // =========================================================================
    // Status flags
    // =========================================================================

    StatusFlags GetFlags() const;

    bool ClearFlags(StatusFlags flags);

private:
    // =========================================================================
    // Hardware constants
    // =========================================================================

    /**
     * @brief RX8130CE 7-bit I2C address.
     */
    static constexpr uint8_t I2C_ADDRESS = 0x32;

    /**
     * @brief I2C clock frequency for RTC communication.
     */
    static constexpr uint32_t I2C_FREQUENCY_HZ = 400000;

    /**
     * @brief GPIO pin assigned to I2C SDA on the Tab5 RTC bus.
     */
    static constexpr gpio_num_t RTC_SDA_PIN = GPIO_NUM_44;

    /**
     * @brief GPIO pin assigned to I2C SCL on the Tab5 RTC bus.
     */
    static constexpr gpio_num_t RTC_SCL_PIN = GPIO_NUM_43;

    // =========================================================================
    // Register map
    // =========================================================================

    static constexpr uint8_t REG_SEC = 0x10;
    static constexpr uint8_t REG_MIN = 0x11;
    static constexpr uint8_t REG_HOUR = 0x12;
    static constexpr uint8_t REG_WEEK = 0x13;
    static constexpr uint8_t REG_DAY = 0x14;
    static constexpr uint8_t REG_MONTH = 0x15;
    static constexpr uint8_t REG_YEAR = 0x16;
    static constexpr uint8_t REG_ALARM_MIN = 0x17;
    static constexpr uint8_t REG_ALARM_HOUR = 0x18;
    static constexpr uint8_t REG_ALARM_WEEK_DAY = 0x19;
    static constexpr uint8_t REG_TIMER_LOW = 0x1A;
    static constexpr uint8_t REG_TIMER_HIGH = 0x1B;
    static constexpr uint8_t REG_EXTENSION = 0x1C;
    static constexpr uint8_t REG_FLAG = 0x1D;
    static constexpr uint8_t REG_CONTROL0 = 0x1E;
    static constexpr uint8_t REG_CONTROL1 = 0x1F;

    // =========================================================================
    // Constructor and helpers
    // =========================================================================

    Rtc();

    bool WriteRegister(uint8_t registerAddress, uint8_t value) const;

    bool ReadRegisters(uint8_t registerAddress, uint8_t *buffer, size_t length) const;

    static uint8_t BcdToDec(uint8_t bcd);

    static uint8_t DecToBcd(uint8_t decimal);

    bool SetStop(bool stop);

    bool PerformStartup();

    // =========================================================================
    // Members
    // =========================================================================

    /**
     * @brief The single instance of this class.
     */
    static Rtc *_instance;

    /**
     * @brief I2C master bus handle.
     */
    i2c_master_bus_handle_t _busHandle;

    /**
     * @brief I2C device handle for the RX8130CE.
     */
    i2c_master_dev_handle_t _deviceHandle;

    /**
     * @brief True once the chip has been successfully initialised.
     */
    bool _initialised;

    /**
     * @brief True if this instance created the I2C bus and must delete it on destruction.
     */
    bool _busOwned;
};
