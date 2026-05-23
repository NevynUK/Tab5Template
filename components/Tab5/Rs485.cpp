/*-----------------------------------------------------------------------------
 * File        : Rs485.cpp
 * Description : Implementation of the Rs485 singleton for the SIT3088 RS-485
 *               transceiver on the M5Stack Tab5.  Half-duplex operation is
 *               achieved by toggling GPIO_NUM_34 (DIR) around each transmit
 *               operation and draining the UART FIFO before reverting to
 *               receive mode.
 * Author      : Mark Stevens
 * Copyright   : Copyright (c) 2026 Mark Stevens
 * Licence     : MIT — see LICENSE in the repository root for full terms.
 * Target      : M5Stack Tab5 (ESP32-P4)
 *---------------------------------------------------------------------------*/

#include "Rs485.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_err.h>
#include <esp_log.h>

/**
 * @brief Logging tag used for all ESP-IDF log output from this module.
 */
static constexpr const char *LOG_TAG = "Rs485";

/**
 * @brief Singleton for the SIT3088 RS-485 transceiver on the M5Stack Tab5.
 *
 * Provides half-duplex RS-485 serial communication using UART_NUM_2 with
 * automatic direction-pin switching on GPIO_NUM_34.
 */
Rs485 *Rs485::_instance = nullptr;

// =============================================================================
// Static interface
// =============================================================================

/**
 * @brief Returns the existing singleton instance.
 *
 * @return Pointer to the singleton, or nullptr if Initialise() has not yet
 *         been called.
 */
Rs485 *Rs485::GetInstance()
{
    return (_instance);
}

/**
 * @brief Creates and initialises the singleton.
 *
 * Configures UART_NUM_2 with the specified baud rate and installs the UART
 * driver.  Also configures the direction GPIO as an output and sets it to
 * receive mode (LOW) after initialisation.
 *
 * @param baudRate  Initial baud rate.  Defaults to DEFAULT_BAUD_RATE.
 * @return Pointer to the newly created singleton, or nullptr if the singleton
 *         already exists or initialisation fails.
 */
Rs485 *Rs485::Initialise(uint32_t baudRate)
{
    if (_instance != nullptr)
    {
        return (nullptr);
    }

    _instance = new Rs485(baudRate);

    if (!_instance->_initialised)
    {
        delete _instance;
        _instance = nullptr;
    }

    return (_instance);
}

// =============================================================================
// Communication
// =============================================================================

/**
 * @brief Transmits data over the RS-485 bus.
 *
 * Switches the direction pin to transmit mode before writing, waits for the
 * UART FIFO to drain, then returns to receive mode.
 *
 * @param data    Pointer to the data buffer to transmit.
 * @param length  Number of bytes to transmit.
 * @return Number of bytes written, or -1 on error.
 */
int Rs485::Transmit(const uint8_t *data, size_t length) const
{
    if (!_initialised || data == nullptr || length == 0)
    {
        return (-1);
    }

    SetTransmitMode();

    int written = uart_write_bytes(UART_PORT, data, length);
    uart_wait_tx_done(UART_PORT, pdMS_TO_TICKS(1000));

    SetReceiveMode();

    return (written);
}

/**
 * @brief Receives data from the RS-485 bus.
 *
 * Reads up to maxLength bytes from the UART receive buffer, waiting up to
 * timeoutMilliseconds for data to arrive.
 *
 * @param buffer                 Destination buffer for received bytes.
 * @param maxLength              Maximum number of bytes to receive.
 * @param timeoutMilliseconds    How long to wait for data in milliseconds.
 * @return Number of bytes received, or -1 on error.
 */
int Rs485::Receive(uint8_t *buffer, size_t maxLength, uint32_t timeoutMilliseconds) const
{
    if (!_initialised || buffer == nullptr || maxLength == 0)
    {
        return (-1);
    }

    return (uart_read_bytes(UART_PORT, buffer, maxLength, pdMS_TO_TICKS(timeoutMilliseconds)));
}

/**
 * @brief Changes the baud rate at runtime.
 *
 * @param baudRate  The new baud rate to set.
 * @return true on success, false on error or if not initialised.
 */
bool Rs485::SetBaudRate(uint32_t baudRate) const
{
    if (!_initialised)
    {
        return (false);
    }

    return (uart_set_baudrate(UART_PORT, baudRate) == ESP_OK);
}

/**
 * @brief Returns true when the UART and direction pin have been configured.
 */
bool Rs485::IsInitialised() const
{
    return (_initialised);
}

// =============================================================================
// Constructor / Destructor
// =============================================================================

/**
 * @brief Private constructor — use Initialise() to create the singleton.
 *
 * @param baudRate  Baud rate for UART communication.
 */
Rs485::Rs485(uint32_t baudRate) : _initialised(false)
{
    uart_config_t uartConfig = {};
    uartConfig.baud_rate = static_cast<int>(baudRate);
    uartConfig.data_bits = UART_DATA_8_BITS;
    uartConfig.parity = UART_PARITY_DISABLE;
    uartConfig.stop_bits = UART_STOP_BITS_1;
    uartConfig.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uartConfig.rx_flow_ctrl_thresh = 122;
    uartConfig.source_clk = UART_SCLK_DEFAULT;

    esp_err_t result = uart_param_config(UART_PORT, &uartConfig);
    if (result != ESP_OK)
    {
        ESP_LOGE(LOG_TAG, "Failed to configure UART parameters: %s", esp_err_to_name(result));
        return;
    }

    result = uart_set_pin(UART_PORT, TX_PIN, RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (result != ESP_OK)
    {
        ESP_LOGE(LOG_TAG, "Failed to set UART pins: %s", esp_err_to_name(result));
        return;
    }

    result = uart_driver_install(UART_PORT, UART_BUFFER_SIZE, 0, 0, nullptr, 0);
    if (result != ESP_OK)
    {
        ESP_LOGE(LOG_TAG, "Failed to install UART driver: %s", esp_err_to_name(result));
        return;
    }

    gpio_config_t gpioConfig = {};
    gpioConfig.intr_type = GPIO_INTR_DISABLE;
    gpioConfig.mode = GPIO_MODE_OUTPUT;
    gpioConfig.pin_bit_mask = (1ULL << DIRECTION_PIN);
    gpioConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpioConfig.pull_up_en = GPIO_PULLUP_DISABLE;

    result = gpio_config(&gpioConfig);
    if (result != ESP_OK)
    {
        ESP_LOGE(LOG_TAG, "Failed to configure direction GPIO: %s", esp_err_to_name(result));
        uart_driver_delete(UART_PORT);
        return;
    }

    SetReceiveMode();

    _initialised = true;
    ESP_LOGI(LOG_TAG, "RS-485 initialised at %lu baud.", baudRate);
}

/**
 * @brief Destructor.
 *
 * Uninstalls the UART driver and resets the singleton pointer.
 */
Rs485::~Rs485()
{
    if (_initialised)
    {
        SetReceiveMode();
        uart_driver_delete(UART_PORT);
    }

    _instance = nullptr;
}

// =============================================================================
// Helpers
// =============================================================================

/**
 * @brief Switches the SIT3088 direction pin to transmit mode (HIGH).
 */
void Rs485::SetTransmitMode() const
{
    gpio_set_level(DIRECTION_PIN, 1);
}

/**
 * @brief Switches the SIT3088 direction pin to receive mode (LOW).
 */
void Rs485::SetReceiveMode() const
{
    gpio_set_level(DIRECTION_PIN, 0);
}
