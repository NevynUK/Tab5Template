/*-----------------------------------------------------------------------------
 * File        : Rs485.hpp
 * Description : Singleton class (Rs485) for the SIT3088 RS-485 transceiver
 *               on the M5Stack Tab5.  Provides half-duplex serial
 *               communication with direction-pin control over UART.
 * Author      : Mark Stevens
 * Copyright   : Copyright (c) 2026 Mark Stevens
 * Licence     : MIT — see LICENSE in the repository root for full terms.
 * Target      : M5Stack Tab5 (ESP32-P4)
 *---------------------------------------------------------------------------*/

#pragma once

#include <cstddef>
#include <cstdint>
#include <driver/gpio.h>
#include <driver/uart.h>

/**
 * @brief Singleton for the SIT3088 RS-485 transceiver on the M5Stack Tab5.
 *
 * Uses UART_NUM_2 with TX on GPIO_NUM_20, RX on GPIO_NUM_21, and a direction
 * pin on GPIO_NUM_34.  Setting the direction pin HIGH enables the driver
 * (transmit mode); LOW enables the receiver (receive mode).
 *
 * All transmit operations automatically toggle the direction pin around the
 * data bytes and wait for the UART FIFO to drain before releasing the bus.
 */
class Rs485
{
public:
    // =========================================================================
    // Static interface
    // =========================================================================

    static Rs485 *GetInstance();

    static Rs485 *Initialise(uint32_t baudRate = DEFAULT_BAUD_RATE);

    ~Rs485();

    // =========================================================================
    // Communication
    // =========================================================================

    int Transmit(const uint8_t *data, size_t length) const;

    int Receive(uint8_t *buffer, size_t maxLength, uint32_t timeoutMilliseconds = DEFAULT_RECEIVE_TIMEOUT_MS) const;

    bool SetBaudRate(uint32_t baudRate) const;

    bool IsInitialised() const;

    // =========================================================================
    // Constants
    // =========================================================================

    /**
     * @brief Default baud rate for RS-485 communication.
     */
    static constexpr uint32_t DEFAULT_BAUD_RATE = 115200;

    /**
     * @brief Default receive timeout in milliseconds.
     */
    static constexpr uint32_t DEFAULT_RECEIVE_TIMEOUT_MS = 100;

private:
    // =========================================================================
    // Hardware constants
    // =========================================================================

    /**
     * @brief UART port number used for RS-485 communication.
     */
    static constexpr uart_port_t UART_PORT = UART_NUM_2;

    /**
     * @brief GPIO pin for UART transmit data.
     */
    static constexpr gpio_num_t TX_PIN = GPIO_NUM_20;

    /**
     * @brief GPIO pin for UART receive data.
     */
    static constexpr gpio_num_t RX_PIN = GPIO_NUM_21;

    /**
     * @brief GPIO pin used to control the SIT3088 driver-enable / receiver-enable.
     *
     * HIGH = driver enabled (transmit), LOW = receiver enabled (receive).
     */
    static constexpr gpio_num_t DIRECTION_PIN = GPIO_NUM_34;

    /**
     * @brief UART receive ring-buffer size in bytes.
     */
    static constexpr int UART_BUFFER_SIZE = 1024;

    // =========================================================================
    // Constructor and helpers
    // =========================================================================

    explicit Rs485(uint32_t baudRate);

    void SetTransmitMode() const;

    void SetReceiveMode() const;

    // =========================================================================
    // Members
    // =========================================================================

    /**
     * @brief The single instance of this class.
     */
    static Rs485 *_instance;

    /**
     * @brief True once the UART and direction pin have been configured.
     */
    bool _initialised;
};
