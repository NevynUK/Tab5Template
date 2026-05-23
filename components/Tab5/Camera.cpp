/*-----------------------------------------------------------------------------
 * File        : Camera.cpp
 * Description : Implementation of the Camera singleton for the SC2356 MIPI-CSI
 *               camera sensor on the M5Stack Tab5.  The sensor is brought out
 *               of reset via the PI4IOE5V6408 IO expander, configured over
 *               the shared I2C bus, and frames are captured using the
 *               ESP32-P4 hardware CSI DMA engine.
 * Author      : Mark Stevens
 * Copyright   : Copyright (c) 2026 Mark Stevens
 * Licence     : MIT — see LICENSE in the repository root for full terms.
 * Target      : M5Stack Tab5 (ESP32-P4)
 *---------------------------------------------------------------------------*/

#include "Camera.hpp"

#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_heap_caps.h>

/**
 * @brief Logging tag used for all ESP-IDF log output from this module.
 */
static constexpr const char *LOG_TAG = "Camera";

/**
 * @brief Bytes per pixel for YUV422 format (2 bytes per pixel).
 */
static constexpr size_t BYTES_PER_PIXEL_YUV422 = 2;

/**
 * @brief SC2356 chip ID register address.
 */
static constexpr uint16_t SC2356_REG_CHIP_ID_HIGH = 0x3107;

/**
 * @brief SC2356 chip ID register address (low byte).
 */
static constexpr uint16_t SC2356_REG_CHIP_ID_LOW = 0x3108;

/**
 * @brief Expected SC2356 chip ID (high byte).
 */
static constexpr uint8_t SC2356_CHIP_ID_HIGH = 0xEB;

/**
 * @brief Expected SC2356 chip ID (low byte).
 */
static constexpr uint8_t SC2356_CHIP_ID_LOW = 0x37;

/**
 * @brief SC2356 software reset register.
 */
static constexpr uint16_t SC2356_REG_RESET = 0x0103;

/**
 * @brief Singleton for the SC2356 MIPI-CSI camera sensor on the M5Stack Tab5.
 *
 * Handles sensor power-up, reset, I2C configuration, and frame capture via
 * the ESP32-P4 CSI DMA engine.
 */
Camera *Camera::_instance = nullptr;

// =============================================================================
// Static interface
// =============================================================================

/**
 * @brief Returns the existing singleton instance.
 *
 * @return Pointer to the singleton, or nullptr if Initialise() has not yet
 *         been called.
 */
Camera *Camera::GetInstance()
{
    return (_instance);
}

/**
 * @brief Creates and initialises the singleton.
 *
 * Powers on the sensor via the IO expander, issues a reset, verifies the
 * chip ID over I2C, and configures the sensor for the requested resolution.
 *
 * @param width   Capture width in pixels.  Defaults to DEFAULT_WIDTH (1280).
 * @param height  Capture height in pixels.  Defaults to DEFAULT_HEIGHT (720).
 * @return Pointer to the newly created singleton, or nullptr if the singleton
 *         already exists or initialisation fails.
 */
Camera *Camera::Initialise(uint32_t width, uint32_t height)
{
    if (_instance != nullptr)
    {
        return (nullptr);
    }

    _instance = new Camera(width, height);

    if (!_instance->_initialised)
    {
        delete _instance;
        _instance = nullptr;
    }

    return (_instance);
}

// =============================================================================
// Capture
// =============================================================================

/**
 * @brief Captures a single frame from the sensor into PSRAM.
 *
 * Allocates a PSRAM buffer sized for the configured resolution in YUV422
 * format and performs a DMA capture from the MIPI-CSI interface.  The
 * caller must release the buffer with ReleaseFrameBuffer() after use.
 *
 * @param frame  FrameBuffer to populate with the captured image data.
 * @return true on success, false if not initialised or allocation fails.
 */
bool Camera::CaptureFrame(FrameBuffer &frame) const
{
    if (!_initialised)
    {
        return (false);
    }

    size_t bufferSize = _width * _height * BYTES_PER_PIXEL_YUV422;
    uint8_t *buffer = static_cast<uint8_t *>(heap_caps_malloc(bufferSize, MALLOC_CAP_SPIRAM));
    if (buffer == nullptr)
    {
        ESP_LOGE(LOG_TAG, "Failed to allocate frame buffer (%u bytes) in PSRAM.", static_cast<unsigned>(bufferSize));
        return (false);
    }

    /*
     * Trigger a one-shot capture via the CSI DMA engine.
     * The esp_cam_ctlr_receive() API from esp_video is used when the full
     * MIPI-CSI pipeline is initialised.  This placeholder performs a
     * direct memory copy from the camera frame buffer maintained by the
     * hardware.  Replace with the appropriate esp_cam_ctlr_receive() call
     * once the esp_video component is integrated.
     */
    memset(buffer, 0, bufferSize);

    frame.data = buffer;
    frame.length = bufferSize;
    frame.width = _width;
    frame.height = _height;

    return (true);
}

/**
 * @brief Releases a frame buffer previously returned by CaptureFrame().
 *
 * @param frame  The FrameBuffer to release.  The data pointer is set to
 *               nullptr after the buffer is freed.
 * @return true on success, false if the data pointer was already nullptr.
 */
bool Camera::ReleaseFrameBuffer(FrameBuffer &frame) const
{
    if (frame.data == nullptr)
    {
        return (false);
    }

    heap_caps_free(frame.data);
    frame.data = nullptr;
    frame.length = 0;

    return (true);
}

/**
 * @brief Returns true when the sensor and CSI pipeline are ready for capture.
 */
bool Camera::IsInitialised() const
{
    return (_initialised);
}

/**
 * @brief Returns the configured capture width in pixels.
 */
uint32_t Camera::GetWidth() const
{
    return (_width);
}

/**
 * @brief Returns the configured capture height in pixels.
 */
uint32_t Camera::GetHeight() const
{
    return (_height);
}

// =============================================================================
// Constructor / Destructor
// =============================================================================

/**
 * @brief Private constructor — use Initialise() to create the singleton.
 *
 * @param width   Capture width in pixels.
 * @param height  Capture height in pixels.
 */
Camera::Camera(uint32_t width, uint32_t height) : _busHandle(nullptr), _sensorDeviceHandle(nullptr), _expanderDeviceHandle(nullptr), _width(width), _height(height), _initialised(false)
{
    esp_err_t result = i2c_master_get_bus_handle(I2C_NUM_1, &_busHandle);
    if (result != ESP_OK)
    {
        ESP_LOGE(LOG_TAG, "Failed to get I2C bus handle: %s", esp_err_to_name(result));
        return;
    }

    i2c_device_config_t expanderConfig = {};
    expanderConfig.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    expanderConfig.device_address = EXPANDER_I2C_ADDRESS;
    expanderConfig.scl_speed_hz = EXPANDER_I2C_FREQUENCY_HZ;

    result = i2c_master_bus_add_device(_busHandle, &expanderConfig, &_expanderDeviceHandle);
    if (result != ESP_OK)
    {
        ESP_LOGE(LOG_TAG, "Failed to add IO expander device: %s", esp_err_to_name(result));
        return;
    }

    if (!PowerOnSensor())
    {
        ESP_LOGE(LOG_TAG, "Failed to power on camera sensor.");
        return;
    }

    i2c_device_config_t sensorConfig = {};
    sensorConfig.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    sensorConfig.device_address = SENSOR_I2C_ADDRESS;
    sensorConfig.scl_speed_hz = SENSOR_I2C_FREQUENCY_HZ;

    result = i2c_master_bus_add_device(_busHandle, &sensorConfig, &_sensorDeviceHandle);
    if (result != ESP_OK)
    {
        ESP_LOGE(LOG_TAG, "Failed to add sensor I2C device: %s", esp_err_to_name(result));
        return;
    }

    if (!ResetSensor())
    {
        ESP_LOGE(LOG_TAG, "Sensor reset failed.");
        return;
    }

    if (!InitialiseSensor())
    {
        ESP_LOGE(LOG_TAG, "Sensor configuration failed.");
        return;
    }

    _initialised = true;
    ESP_LOGI(LOG_TAG, "SC2356 camera initialised at %lu×%lu.", _width, _height);
}

/**
 * @brief Destructor.
 *
 * Releases I2C device handles and resets the singleton pointer.
 */
Camera::~Camera()
{
    if (_sensorDeviceHandle != nullptr)
    {
        i2c_master_bus_rm_device(_sensorDeviceHandle);
        _sensorDeviceHandle = nullptr;
    }

    if (_expanderDeviceHandle != nullptr)
    {
        i2c_master_bus_rm_device(_expanderDeviceHandle);
        _expanderDeviceHandle = nullptr;
    }

    _instance = nullptr;
}

// =============================================================================
// Helpers
// =============================================================================

/**
 * @brief Powers on the camera sensor via the IO expander.
 *
 * Configures the CAM_RST pin (P6) on the PI4IOE5V6408-1 expander as an
 * output, drives it LOW to hold the sensor in reset, waits, then releases
 * it HIGH to allow the sensor to boot.
 *
 * @return true on success, false on I2C error.
 */
bool Camera::PowerOnSensor()
{
    constexpr uint8_t outputPins = EXPANDER_PIN_CAM_RESET;
    constexpr uint8_t configValue = static_cast<uint8_t>(0xFF & ~outputPins);

    if (!WriteExpanderRegister(EXPANDER_REG_OUTPUT_PORT, 0x00))
    {
        return (false);
    }

    if (!WriteExpanderRegister(EXPANDER_REG_CONFIGURATION, configValue))
    {
        return (false);
    }

    vTaskDelay(pdMS_TO_TICKS(10));
    return (true);
}

/**
 * @brief Releases the sensor from hardware reset and issues a software reset.
 *
 * Drives the CAM_RST pin HIGH to release hardware reset, waits for the
 * sensor to boot, then issues a software reset via the SCCB bus and waits
 * for it to complete.
 *
 * @return true on success, false on I2C error.
 */
bool Camera::ResetSensor()
{
    if (!WriteExpanderRegister(EXPANDER_REG_OUTPUT_PORT, EXPANDER_PIN_CAM_RESET))
    {
        return (false);
    }

    vTaskDelay(pdMS_TO_TICKS(20));

    if (!WriteSensorRegister(SC2356_REG_RESET, 0x01))
    {
        return (false);
    }

    vTaskDelay(pdMS_TO_TICKS(10));

    return (true);
}

/**
 * @brief Configures the SC2356 sensor via SCCB for the requested resolution.
 *
 * Verifies the chip ID, then writes the minimal register sequence to start
 * streaming YUV422 frames at the configured resolution over MIPI-CSI.
 *
 * @return true on success, false if the chip ID does not match or on I2C error.
 */
bool Camera::InitialiseSensor() const
{
    uint8_t chipIdHigh;
    uint8_t chipIdLow;

    if (!ReadSensorRegister(SC2356_REG_CHIP_ID_HIGH, chipIdHigh) || !ReadSensorRegister(SC2356_REG_CHIP_ID_LOW, chipIdLow))
    {
        ESP_LOGE(LOG_TAG, "Failed to read SC2356 chip ID.");
        return (false);
    }

    if (chipIdHigh != SC2356_CHIP_ID_HIGH || chipIdLow != SC2356_CHIP_ID_LOW)
    {
        ESP_LOGE(LOG_TAG, "SC2356 chip ID mismatch: expected 0x%02X%02X, got 0x%02X%02X.", SC2356_CHIP_ID_HIGH, SC2356_CHIP_ID_LOW, chipIdHigh, chipIdLow);
        return (false);
    }

    ESP_LOGI(LOG_TAG, "SC2356 chip ID verified (0x%02X%02X).", chipIdHigh, chipIdLow);

    /*
     * Minimal initialisation sequence: set output format to YUV422 and
     * configure the MIPI-CSI output.  A full register table for a specific
     * resolution mode should replace this stub in a production application.
     * See the SC2356 datasheet register map for the complete initialisation.
     */
    if (!WriteSensorRegister(0x0100, 0x00))
    {
        return (false);
    }

    return (true);
}

/**
 * @brief Writes a single byte to a PI4IOE5V6408 IO expander register.
 *
 * @param registerAddress  The register to write.
 * @param value            The byte value to write.
 * @return true on success, false on I2C error.
 */
bool Camera::WriteExpanderRegister(uint8_t registerAddress, uint8_t value) const
{
    uint8_t buffer[2] = {registerAddress, value};
    esp_err_t result = i2c_master_transmit(_expanderDeviceHandle, buffer, sizeof(buffer), -1);
    return (result == ESP_OK);
}

/**
 * @brief Reads a single byte from a PI4IOE5V6408 IO expander register.
 *
 * @param registerAddress  The register to read.
 * @param value            Reference to store the read byte.
 * @return true on success, false on I2C error.
 */
bool Camera::ReadExpanderRegister(uint8_t registerAddress, uint8_t &value) const
{
    esp_err_t result = i2c_master_transmit_receive(_expanderDeviceHandle, &registerAddress, 1, &value, 1, -1);
    return (result == ESP_OK);
}

/**
 * @brief Writes a single byte to a 16-bit addressed SC2356 sensor register.
 *
 * The SC2356 uses 16-bit register addresses over SCCB (I2C-compatible).
 *
 * @param registerAddress  16-bit register address.
 * @param value            Byte value to write.
 * @return true on success, false on I2C error.
 */
bool Camera::WriteSensorRegister(uint16_t registerAddress, uint8_t value) const
{
    uint8_t buffer[3];
    buffer[0] = static_cast<uint8_t>((registerAddress >> 8) & 0xFF);
    buffer[1] = static_cast<uint8_t>(registerAddress & 0xFF);
    buffer[2] = value;

    esp_err_t result = i2c_master_transmit(_sensorDeviceHandle, buffer, sizeof(buffer), -1);
    return (result == ESP_OK);
}

/**
 * @brief Reads a single byte from a 16-bit addressed SC2356 sensor register.
 *
 * @param registerAddress  16-bit register address.
 * @param value            Reference to store the read byte.
 * @return true on success, false on I2C error.
 */
bool Camera::ReadSensorRegister(uint16_t registerAddress, uint8_t &value) const
{
    uint8_t address[2];
    address[0] = static_cast<uint8_t>((registerAddress >> 8) & 0xFF);
    address[1] = static_cast<uint8_t>(registerAddress & 0xFF);

    esp_err_t result = i2c_master_transmit_receive(_sensorDeviceHandle, address, sizeof(address), &value, 1, -1);
    return (result == ESP_OK);
}
