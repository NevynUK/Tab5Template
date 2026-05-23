/*-----------------------------------------------------------------------------
 * File        : Camera.hpp
 * Description : Singleton class (Camera) for the SC2356 MIPI-CSI camera
 *               sensor on the M5Stack Tab5.  Handles sensor power-up via
 *               the PI4IOE5V6408 IO expander, sensor configuration over
 *               I2C, MIPI-CSI pipeline initialisation, and frame capture.
 * Author      : Mark Stevens
 * Copyright   : Copyright (c) 2026 Mark Stevens
 * Licence     : MIT — see LICENSE in the repository root for full terms.
 * Target      : M5Stack Tab5 (ESP32-P4)
 *---------------------------------------------------------------------------*/

#pragma once

#include <cstdint>
#include <cstddef>
#include <driver/gpio.h>
#include <driver/i2c_master.h>

/**
 * @brief Singleton for the SC2356 MIPI-CSI camera sensor on the M5Stack Tab5.
 *
 * The SC2356 is a 2 MP (1600×1200) CMOS image sensor connected via a
 * 2-lane MIPI-CSI interface.  The sensor is configured over the shared
 * internal I2C bus (I2C_NUM_1, SDA = GPIO_NUM_31, SCL = GPIO_NUM_32).
 * MCLK is supplied on GPIO_NUM_36.  The sensor reset signal is driven
 * through the first PI4IOE5V6408 IO expander (address 0x43) on pin P6.
 *
 * Frame capture uses the ESP32-P4 hardware CSI DMA engine.  Captured
 * buffers are allocated from PSRAM and must be released with
 * ReleaseFrameBuffer() after processing.
 *
 * @note Initialise() must be called after display.init() because M5GFX
 *       creates the shared I2C bus handle that is reused here.
 */
class Camera
{
public:
    // =========================================================================
    // Types
    // =========================================================================

    /**
     * @brief Container for a single captured video frame.
     */
    struct FrameBuffer
    {
        /**
         * @brief Pointer to the raw pixel data (YUV422 or RGB565).
         */
        uint8_t *data;

        /**
         * @brief Length of the pixel data in bytes.
         */
        size_t length;

        /**
         * @brief Frame width in pixels.
         */
        uint32_t width;

        /**
         * @brief Frame height in pixels.
         */
        uint32_t height;
    };

    // =========================================================================
    // Static interface
    // =========================================================================

    static Camera *GetInstance();

    static Camera *Initialise(uint32_t width = DEFAULT_WIDTH, uint32_t height = DEFAULT_HEIGHT);

    ~Camera();

    // =========================================================================
    // Capture
    // =========================================================================

    bool CaptureFrame(FrameBuffer &frame) const;

    bool ReleaseFrameBuffer(FrameBuffer &frame) const;

    bool IsInitialised() const;

    uint32_t GetWidth() const;

    uint32_t GetHeight() const;

    // =========================================================================
    // Constants
    // =========================================================================

    /**
     * @brief Default capture width in pixels.
     */
    static constexpr uint32_t DEFAULT_WIDTH = 1280;

    /**
     * @brief Default capture height in pixels.
     */
    static constexpr uint32_t DEFAULT_HEIGHT = 720;

private:
    // =========================================================================
    // Hardware constants
    // =========================================================================

    /**
     * @brief SC2356 7-bit I2C address (default, SCCB address pin tied LOW).
     */
    static constexpr uint8_t SENSOR_I2C_ADDRESS = 0x30;

    /**
     * @brief I2C clock frequency for SCCB (sensor configuration) communication.
     */
    static constexpr uint32_t SENSOR_I2C_FREQUENCY_HZ = 100000;

    /**
     * @brief GPIO pin used to clock the sensor (MCLK output from ESP32-P4).
     */
    static constexpr gpio_num_t MCLK_PIN = GPIO_NUM_36;

    /**
     * @brief MCLK frequency supplied to the SC2356 sensor in Hz.
     */
    static constexpr uint32_t MCLK_FREQUENCY_HZ = 24000000;

    /**
     * @brief 7-bit I2C address of the first PI4IOE5V6408 IO expander.
     */
    static constexpr uint8_t EXPANDER_I2C_ADDRESS = 0x43;

    /**
     * @brief I2C clock frequency for IO expander communication.
     */
    static constexpr uint32_t EXPANDER_I2C_FREQUENCY_HZ = 400000;

    /**
     * @brief Bit mask for the camera reset pin on PI4IOE5V6408-1, pin P6.
     *
     * The reset is active-LOW; the pin is driven HIGH for normal operation.
     */
    static constexpr uint8_t EXPANDER_PIN_CAM_RESET = (1U << 6);

    // =========================================================================
    // IO Expander register map (PI4IOE5V6408)
    // =========================================================================

    /**
     * @brief IO expander output port register.
     */
    static constexpr uint8_t EXPANDER_REG_OUTPUT_PORT = 0x01;

    /**
     * @brief IO expander configuration register (0 = output, 1 = input).
     */
    static constexpr uint8_t EXPANDER_REG_CONFIGURATION = 0x03;

    // =========================================================================
    // Constructor and helpers
    // =========================================================================

    Camera(uint32_t width, uint32_t height);

    bool PowerOnSensor();

    bool ResetSensor();

    bool InitialiseSensor() const;

    bool WriteExpanderRegister(uint8_t registerAddress, uint8_t value) const;

    bool ReadExpanderRegister(uint8_t registerAddress, uint8_t &value) const;

    bool WriteSensorRegister(uint16_t registerAddress, uint8_t value) const;

    bool ReadSensorRegister(uint16_t registerAddress, uint8_t &value) const;

    // =========================================================================
    // Members
    // =========================================================================

    /**
     * @brief The single instance of this class.
     */
    static Camera *_instance;

    /**
     * @brief I2C master bus handle (borrowed from M5GFX / display.init()).
     */
    i2c_master_bus_handle_t _busHandle;

    /**
     * @brief I2C device handle for the SC2356 sensor (SCCB).
     */
    i2c_master_dev_handle_t _sensorDeviceHandle;

    /**
     * @brief I2C device handle for the PI4IOE5V6408-1 IO expander.
     */
    i2c_master_dev_handle_t _expanderDeviceHandle;

    /**
     * @brief Configured capture width in pixels.
     */
    uint32_t _width;

    /**
     * @brief Configured capture height in pixels.
     */
    uint32_t _height;

    /**
     * @brief True once the sensor and CSI pipeline are ready for capture.
     */
    bool _initialised;
};
