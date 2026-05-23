/*-----------------------------------------------------------------------------
 * File        : Imu.cpp
 * Description : Implementation of the Imu singleton for the BMI270 6-axis
 *               inertial measurement unit on the M5Stack Tab5.  Initialisation
 *               uploads the 8 KiB BMI270 firmware configuration blob, then
 *               enables the accelerometer and gyroscope in normal mode.
 *               Readings are returned in physical units (g and degrees/s).
 * Author      : Mark Stevens
 * Copyright   : Copyright (c) 2026 Mark Stevens
 * Licence     : MIT — see LICENSE in the repository root for full terms.
 * Target      : M5Stack Tab5 (ESP32-P4)
 *---------------------------------------------------------------------------*/

#include "Imu.hpp"
#include "ImuConfigData.hpp"

#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_err.h>
#include <esp_log.h>

/**
 * @brief Logging tag used for all ESP-IDF log output from this module.
 */
static constexpr const char *LOG_TAG = "Imu";

/**
 * @brief Singleton for the BMI270 6-axis IMU on the M5Stack Tab5.
 *
 * Provides calibrated 3-axis accelerometer and 3-axis gyroscope readings
 * after completing the BMI270 firmware initialisation sequence.
 */
Imu *Imu::_instance = nullptr;
i2c_master_bus_handle_t Imu::_busHandle = nullptr;
i2c_master_dev_handle_t Imu::_deviceHandle = nullptr;
bool Imu::_initialised = false;

// =============================================================================
// Static interface
// =============================================================================

/**
 * @brief Returns the existing singleton instance.
 *
 * @return Pointer to the singleton, or nullptr if Initialise() has not yet
 *         been called.
 */
Imu *Imu::GetInstance()
{
    return (_instance);
}

/**
 * @brief Creates and initialises the singleton.
 *
 * Borrows the I2C bus handle from M5GFX (I2C_NUM_1), verifies the BMI270
 * chip ID, uploads the firmware configuration blob, and enables the
 * accelerometer and gyroscope.  Up to three attempts are made with a 500 ms
 * pause between them to handle transient I2C errors on first power-on.
 *
 * @return Pointer to the newly created singleton, or nullptr if the singleton
 *         already exists or all initialisation attempts fail.
 */
Imu *Imu::Initialise()
{
    if (_instance != nullptr)
    {
        return (nullptr);
    }

    constexpr int MAX_ATTEMPTS = 5;
    constexpr uint32_t RETRY_DELAY_MS = 500;

    for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt)
    {
        if (attempt > 0)
        {
            ESP_LOGW(LOG_TAG, "Retrying BMI270 initialisation (attempt %d of %d).",
                     attempt + 1, MAX_ATTEMPTS);
            vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
        }

        _instance = new Imu();

        if (_instance->_initialised)
        {
            return (_instance);
        }

        delete _instance;
        _instance = nullptr;
    }

    return (nullptr);
}

// =============================================================================
// Sensor readings
// =============================================================================

/**
 * @brief Reads the current 3-axis acceleration.
 *
 * @param acceleration  Structure to populate with X, Y, Z in units of g.
 * @return true on success, false on I2C error or if not initialised.
 */
bool Imu::GetAcceleration(Vector3 &acceleration)
{
    if (!_initialised)
    {
        return (false);
    }

    uint8_t buffer[AXIS_DATA_LENGTH];
    if (!ReadRegisters(REG_ACC_X_LSB, buffer, sizeof(buffer)))
    {
        return (false);
    }

    auto toSigned16 = [](uint8_t low, uint8_t high) -> int16_t
    {
        return (static_cast<int16_t>((static_cast<uint16_t>(high) << 8) | static_cast<uint16_t>(low)));
    };

    acceleration.x = static_cast<float>(toSigned16(buffer[0], buffer[1])) / ACC_SENSITIVITY_LSB_PER_G;
    acceleration.y = static_cast<float>(toSigned16(buffer[2], buffer[3])) / ACC_SENSITIVITY_LSB_PER_G;
    acceleration.z = static_cast<float>(toSigned16(buffer[4], buffer[5])) / ACC_SENSITIVITY_LSB_PER_G;

    return (true);
}

/**
 * @brief Reads the current 3-axis angular rate.
 *
 * @param gyroscope  Structure to populate with X, Y, Z in degrees per second.
 * @return true on success, false on I2C error or if not initialised.
 */
bool Imu::GetGyroscope(Vector3 &gyroscope)
{
    if (!_initialised)
    {
        return (false);
    }

    uint8_t buffer[AXIS_DATA_LENGTH];
    if (!ReadRegisters(REG_GYR_X_LSB, buffer, sizeof(buffer)))
    {
        return (false);
    }

    auto toSigned16 = [](uint8_t low, uint8_t high) -> int16_t
    {
        return (static_cast<int16_t>((static_cast<uint16_t>(high) << 8) | static_cast<uint16_t>(low)));
    };

    gyroscope.x = static_cast<float>(toSigned16(buffer[0], buffer[1])) / GYR_SENSITIVITY_LSB_PER_DPS;
    gyroscope.y = static_cast<float>(toSigned16(buffer[2], buffer[3])) / GYR_SENSITIVITY_LSB_PER_DPS;
    gyroscope.z = static_cast<float>(toSigned16(buffer[4], buffer[5])) / GYR_SENSITIVITY_LSB_PER_DPS;

    return (true);
}

/**
 * @brief Returns true when the firmware has been loaded and the axes enabled.
 */
bool Imu::IsInitialised()
{
    return (_initialised);
}

// =============================================================================
// Constructor / Destructor
// =============================================================================

/**
 * @brief Private constructor — use Initialise() to create the singleton.
 */
Imu::Imu()
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

    if (!PerformStartup())
    {
        ESP_LOGE(LOG_TAG, "BMI270 startup sequence failed.");
        return;
    }

    _initialised = true;
    ESP_LOGI(LOG_TAG, "BMI270 initialised.");
}

/**
 * @brief Destructor.
 *
 * Releases the I2C device handle and resets the singleton pointer.
 */
Imu::~Imu()
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
 * @brief Writes a single byte to a BMI270 register.
 *
 * @param registerAddress  The register to write.
 * @param value            The byte value to write.
 * @return true on success, false on I2C error.
 */
bool Imu::WriteRegister(uint8_t registerAddress, uint8_t value)
{
    uint8_t buffer[2] = {registerAddress, value};
    esp_err_t result = i2c_master_transmit(_deviceHandle, buffer, sizeof(buffer), -1);
    return (result == ESP_OK);
}

/**
 * @brief Reads one or more bytes from consecutive BMI270 registers.
 *
 * @param registerAddress  Starting register address.
 * @param buffer           Destination buffer.
 * @param length           Number of bytes to read.
 * @return true on success, false on I2C error.
 */
bool Imu::ReadRegisters(uint8_t registerAddress, uint8_t *buffer, size_t length)
{
    esp_err_t result = i2c_master_transmit_receive(_deviceHandle, &registerAddress, 1, buffer, length, -1);
    return (result == ESP_OK);
}

/**
 * @brief Performs a read-modify-write on a single BMI270 register.
 *
 * Reads the current register value, clears all bits indicated by @p mask,
 * ORs in @p value (which must already be shifted to its correct bit
 * position), and writes the result back.
 *
 * @param registerAddress  The register to modify.
 * @param mask             Bitmask of the field to update (1 = affected bit).
 * @param value            New field value, already positioned within the byte.
 * @return true on success, false on I2C error.
 */
bool Imu::ModifyRegister(uint8_t registerAddress, uint8_t mask, uint8_t value)
{
    uint8_t current;
    if (!ReadRegisters(registerAddress, &current, 1))
    {
        return (false);
    }

    current = static_cast<uint8_t>((current & ~mask) | (value & mask));
    return (WriteRegister(registerAddress, current));
}

/**
 * @brief Selects a BMI270 feature page and reads all 16 bytes of its window.
 *
 * Writes the page number to REG_FEAT_PAGE (0x2F), then reads 16 bytes
 * from REG_FEATURES (0x30).  @p buffer must be at least FEATURE_PAGE_SIZE
 * bytes long.
 *
 * @param page    Feature page number (0–7).
 * @param buffer  Destination for the 16-byte page window.
 * @return true on success, false on I2C error.
 */
bool Imu::ReadFeaturePage(uint8_t page, uint8_t *buffer)
{
    if (!WriteRegister(REG_FEAT_PAGE, page))
    {
        return (false);
    }

    return (ReadRegisters(REG_FEATURES, buffer, FEATURE_PAGE_SIZE));
}

/**
 * @brief Selects a BMI270 feature page and writes all 16 bytes of its window.
 *
 * Writes the page number to REG_FEAT_PAGE (0x2F), then writes 16 bytes to
 * REG_FEATURES (0x30).  @p buffer must contain exactly FEATURE_PAGE_SIZE
 * bytes.
 *
 * @param page    Feature page number (0–7).
 * @param buffer  Source of the 16-byte page data.
 * @return true on success, false on I2C error.
 */
bool Imu::WriteFeaturePage(uint8_t page, const uint8_t *buffer)
{
    if (!WriteRegister(REG_FEAT_PAGE, page))
    {
        return (false);
    }

    /*
     * Prepend the register address to the payload for a single I2C burst write.
     */
    uint8_t writeBuffer[FEATURE_PAGE_SIZE + 1];
    writeBuffer[0] = REG_FEATURES;
    memcpy(&writeBuffer[1], buffer, FEATURE_PAGE_SIZE);

    esp_err_t result = i2c_master_transmit(_deviceHandle, writeBuffer, sizeof(writeBuffer), -1);
    return (result == ESP_OK);
}

/**
 * @brief Uploads the BMI270 firmware configuration blob to the sensor.
 *
 * Writes CONFIG_CHUNK_SIZE bytes at a time to REG_INIT_DATA, updating the
 * address registers (REG_INIT_ADDR_0 and REG_INIT_ADDR_1) before each chunk.
 * After the upload, REG_INIT_CTRL is set to 1 to trigger the sensor to load
 * the configuration, and the code polls REG_INTERNAL_STATUS until bit 0 is
 * set (firmware initialisation complete) with a 2-second timeout.
 *
 * @return true if the firmware loaded successfully, false on error or timeout.
 */
bool Imu::UploadConfigFile()
{
    static constexpr size_t CHUNK_PAYLOAD = CONFIG_CHUNK_SIZE;
    static constexpr size_t WRITE_BUFFER_SIZE = CHUNK_PAYLOAD + 1;

    /*
     * Pre-build the data write buffer: first byte is the register address
     * (REG_INIT_DATA = 0x5E); the remaining bytes are filled per chunk.
     */
    uint8_t writeBuffer[WRITE_BUFFER_SIZE];
    writeBuffer[0] = REG_INIT_DATA;

    const size_t configSize = BMI270_CONFIG_SIZE;
    const size_t chunks = configSize / CHUNK_PAYLOAD;

    for (size_t index = 0; index < chunks; ++index)
    {
        uint16_t wordAddress = static_cast<uint16_t>((index * CHUNK_PAYLOAD) / 2);

        uint8_t addrLow  = static_cast<uint8_t>(wordAddress & 0x0F);
        uint8_t addrHigh = static_cast<uint8_t>((wordAddress >> 4) & 0xFF);

        /*
         * Write both address registers in a single I2C burst (Bosch reference
         * implementation pattern).  Splitting into two separate transactions
         * creates a window where the BMI270 sees a half-updated word address,
         * which can silently corrupt the config upload.
         */
        uint8_t addrBuffer[3] = { REG_INIT_ADDR_0, addrLow, addrHigh };
        esp_err_t result = i2c_master_transmit(_deviceHandle, addrBuffer, sizeof(addrBuffer), -1);
        if (result != ESP_OK)
        {
            return (false);
        }

        memcpy(&writeBuffer[1], &bmi270_config_file[index * CHUNK_PAYLOAD], CHUNK_PAYLOAD);

        result = i2c_master_transmit(_deviceHandle, writeBuffer, WRITE_BUFFER_SIZE, -1);
        if (result != ESP_OK)
        {
            return (false);
        }
    }

    if (!WriteRegister(REG_INIT_CTRL, 0x01))
    {
        return (false);
    }

    uint8_t status = 0;

    for (int attempt = 0; attempt < 100; ++attempt)
    {
        vTaskDelay(pdMS_TO_TICKS(20));

        if (!ReadRegisters(REG_INTERNAL_STATUS, &status, 1))
        {
            return (false);
        }

        if ((status & 0x01) != 0)
        {
            ESP_LOGI(LOG_TAG, "BMI270 firmware init complete after %d ms.", (attempt + 1) * 20);
            return (true);
        }
    }

    ESP_LOGE(LOG_TAG, "BMI270 firmware init timed out (INTERNAL_STATUS = 0x%02X).", status);
    return (false);
}

/**
 * @brief Performs the complete BMI270 startup sequence.
 *
 * Resets the sensor, verifies the chip ID, uploads the configuration file,
 * then configures and enables the accelerometer and gyroscope.
 *
 * @return true on success, false if any step fails.
 */
bool Imu::PerformStartup()
{
    if (!WriteRegister(REG_CMD, 0xB6))
    {
        return (false);
    }

    vTaskDelay(pdMS_TO_TICKS(50));

    uint8_t chipId;
    if (!ReadRegisters(REG_CHIP_ID, &chipId, 1) || chipId != CHIP_ID)
    {
        ESP_LOGE(LOG_TAG, "Unexpected chip ID: 0x%02X (expected 0x%02X).", chipId, CHIP_ID);
        return (false);
    }

    if (!WriteRegister(REG_PWR_CONF, 0x00))
    {
        return (false);
    }

    vTaskDelay(pdMS_TO_TICKS(1));

    if (!WriteRegister(REG_INIT_CTRL, 0x00))
    {
        return (false);
    }

    if (!UploadConfigFile())
    {
        return (false);
    }

    /*
     * ACC_CONF: ODR = 100 Hz (0x08), bandwidth = normal (0x02), filter = OSR4.
     * GYR_CONF: ODR = 200 Hz (0x09), bandwidth = normal (0x02), filter = OSR4.
     */
    if (!WriteRegister(REG_ACC_CONF, 0xA8))
    {
        return (false);
    }

    if (!WriteRegister(REG_GYR_CONF, 0xA9))
    {
        return (false);
    }

    /*
     * PWR_CTRL: bit2 = acc_en, bit3 = gyr_en.
     */
    if (!WriteRegister(REG_PWR_CTRL, 0x0E))
    {
        return (false);
    }

    vTaskDelay(pdMS_TO_TICKS(50));

    return (true);
}

// =============================================================================
// Sensor control
// =============================================================================

/**
 * @brief Performs a soft reset and reinitialises the BMI270.
 *
 * Issues a soft-reset command, waits for the sensor to restart, then
 * re-runs the full startup sequence (config upload and axis enable).
 *
 * @return true on success, false if reinitialisation fails.
 */
bool Imu::Reset()
{
    if (!_initialised)
    {
        return (false);
    }

    _initialised = false;

    if (!PerformStartup())
    {
        ESP_LOGE(LOG_TAG, "BMI270 reinitialisation after reset failed.");
        return (false);
    }

    _initialised = true;
    ESP_LOGI(LOG_TAG, "BMI270 reset and reinitialised.");
    return (true);
}

/**
 * @brief Reads the BMI270 status register.
 *
 * The status register contains data-ready flags for the accelerometer,
 * gyroscope, and auxiliary sensor, as well as the FOC-ready and NVM-ready
 * bits.
 *
 * @param status  Reference to receive the raw status byte.
 * @return true on success, false on I2C error or if not initialised.
 */
bool Imu::GetStatus(uint8_t &status)
{
    if (!_initialised)
    {
        return (false);
    }

    return (ReadRegisters(REG_STATUS, &status, 1));
}

// =============================================================================
// Sensor readings (additional)
// =============================================================================

/**
 * @brief Reads the die temperature from the BMI270.
 *
 * Raw 16-bit signed temperature data is read from registers 0x22–0x23 and
 * converted to degrees Celsius using the formula:
 *     T(°C) = raw / 512.0 + 23.0
 *
 * @param temperature  Reference to receive the temperature in degrees Celsius.
 * @return true on success, false on I2C error or if not initialised.
 */
bool Imu::GetTemperature(float &temperature)
{
    if (!_initialised)
    {
        return (false);
    }

    uint8_t buffer[2];
    if (!ReadRegisters(REG_TEMPERATURE_LSB, buffer, sizeof(buffer)))
    {
        return (false);
    }

    int16_t raw = static_cast<int16_t>(static_cast<uint16_t>(buffer[0]) | (static_cast<uint16_t>(buffer[1]) << 8));
    temperature = static_cast<float>(raw) / 512.0f + 23.0f;
    return (true);
}

// =============================================================================
// Interrupt status
// =============================================================================

/**
 * @brief Reads the combined interrupt status from the BMI270.
 *
 * Reads INT_STATUS_0 (0x1C) and INT_STATUS_1 (0x1D) and packs them into a
 * single 16-bit value: bits [7:0] = INT_STATUS_0, bits [15:8] = INT_STATUS_1.
 *
 * @param status  Reference to receive the combined interrupt status word.
 * @return true on success, false on I2C error or if not initialised.
 */
bool Imu::GetInterruptStatus(uint16_t &status)
{
    if (!_initialised)
    {
        return (false);
    }

    uint8_t buffer[2];
    if (!ReadRegisters(REG_INT_STATUS_0, buffer, sizeof(buffer)))
    {
        return (false);
    }

    status = static_cast<uint16_t>(buffer[0]) | (static_cast<uint16_t>(buffer[1]) << 8);
    return (true);
}

// =============================================================================
// Accelerometer configuration
// =============================================================================

/**
 * @brief Sets the accelerometer output data rate.
 *
 * Updates the ODR field (bits [3:0]) in ACC_CONF (0x40).
 * Use the ACCEL_ODR_* constants for the @p outputDataRate argument.
 *
 * @param outputDataRate  Desired ODR value (ACCEL_ODR_100HZ, etc.).
 * @return true on success, false on I2C error or if not initialised.
 */
bool Imu::SetAccelerometerOutputDataRate(uint8_t outputDataRate)
{
    if (!_initialised)
    {
        return (false);
    }

    return (ModifyRegister(REG_ACC_CONF, ACC_CONF_ODR_MASK, outputDataRate));
}

/**
 * @brief Sets the accelerometer filter performance mode.
 *
 * Updates bit [7] of ACC_CONF (0x40).  Use POWER_OPT_MODE for low-power
 * filtering or PERF_OPT_MODE for performance filtering.
 *
 * @param filterMode  POWER_OPT_MODE (0) or PERF_OPT_MODE (1).
 * @return true on success, false on I2C error or if not initialised.
 */
bool Imu::SetAccelerometerPowerMode(uint8_t filterMode)
{
    if (!_initialised)
    {
        return (false);
    }

    uint8_t value = static_cast<uint8_t>((filterMode & 0x01) != 0 ? ACC_CONF_FILTER_PERF_MASK : 0x00);
    return (ModifyRegister(REG_ACC_CONF, ACC_CONF_FILTER_PERF_MASK, value));
}

/**
 * @brief Sets the accelerometer filter bandwidth parameter.
 *
 * Updates bits [6:4] of ACC_CONF (0x40).
 * Use the ACCEL_BWP_* constants for the @p bandwidthParameter argument.
 *
 * @param bandwidthParameter  Desired bandwidth parameter (ACCEL_BWP_OSR4_AVG1, etc.).
 * @return true on success, false on I2C error or if not initialised.
 */
bool Imu::SetAccelerometerFilterBandwidth(uint8_t bandwidthParameter)
{
    if (!_initialised)
    {
        return (false);
    }

    uint8_t value = static_cast<uint8_t>((bandwidthParameter << ACC_CONF_BWP_POS) & ACC_CONF_BWP_MASK);
    return (ModifyRegister(REG_ACC_CONF, ACC_CONF_BWP_MASK, value));
}

// =============================================================================
// Gyroscope configuration
// =============================================================================

/**
 * @brief Sets the gyroscope output data rate.
 *
 * Updates the ODR field (bits [3:0]) in GYR_CONF (0x42).
 * Use the GYRO_ODR_* constants for the @p outputDataRate argument.
 *
 * @param outputDataRate  Desired ODR value (GYRO_ODR_200HZ, etc.).
 * @return true on success, false on I2C error or if not initialised.
 */
bool Imu::SetGyroscopeOutputDataRate(uint8_t outputDataRate)
{
    if (!_initialised)
    {
        return (false);
    }

    return (ModifyRegister(REG_GYR_CONF, GYR_CONF_ODR_MASK, outputDataRate));
}

/**
 * @brief Sets the gyroscope filter and noise performance modes.
 *
 * Updates bits [7] (filter) and [6] (noise) of GYR_CONF (0x42).
 * Use POWER_OPT_MODE or PERF_OPT_MODE for each parameter.
 *
 * @param filterMode  Filter performance mode — POWER_OPT_MODE or PERF_OPT_MODE.
 * @param noiseMode   Noise performance mode — POWER_OPT_MODE or PERF_OPT_MODE.
 * @return true on success, false on I2C error or if not initialised.
 */
bool Imu::SetGyroscopePowerMode(uint8_t filterMode, uint8_t noiseMode)
{
    if (!_initialised)
    {
        return (false);
    }

    uint8_t mask = GYR_CONF_FILTER_PERF_MASK | GYR_CONF_NOISE_PERF_MASK;
    uint8_t value = static_cast<uint8_t>(((filterMode & 0x01) != 0 ? GYR_CONF_FILTER_PERF_MASK : 0x00) |
                                          ((noiseMode & 0x01) != 0 ? GYR_CONF_NOISE_PERF_MASK : 0x00));
    return (ModifyRegister(REG_GYR_CONF, mask, value));
}

/**
 * @brief Sets the gyroscope filter bandwidth parameter.
 *
 * Updates bits [5:4] of GYR_CONF (0x42).
 * Use the GYRO_BWP_* constants for the @p bandwidthParameter argument.
 *
 * @param bandwidthParameter  Desired bandwidth parameter (GYRO_BWP_NORMAL, etc.).
 * @return true on success, false on I2C error or if not initialised.
 */
bool Imu::SetGyroscopeFilterBandwidth(uint8_t bandwidthParameter)
{
    if (!_initialised)
    {
        return (false);
    }

    uint8_t value = static_cast<uint8_t>((bandwidthParameter << GYR_CONF_BWP_POS) & GYR_CONF_BWP_MASK);
    return (ModifyRegister(REG_GYR_CONF, GYR_CONF_BWP_MASK, value));
}

// =============================================================================
// Power management
// =============================================================================

/**
 * @brief Enables or disables the BMI270 advanced power save mode.
 *
 * When enabled the sensor automatically enters a low-power state between
 * measurements.  Modifies bit [0] of PWR_CONF (0x7C).
 *
 * @param enable  true to enable advanced power save, false to disable.
 * @return true on success, false on I2C error or if not initialised.
 */
bool Imu::EnableAdvancedPowerSave(bool enable)
{
    if (!_initialised)
    {
        return (false);
    }

    uint8_t value = enable ? PWR_CONF_ADV_POWER_SAVE_MASK : 0x00;
    return (ModifyRegister(REG_PWR_CONF, PWR_CONF_ADV_POWER_SAVE_MASK, value));
}

// =============================================================================
// Step counter
// =============================================================================

/**
 * @brief Reads the current step count from the BMI270 feature output page.
 *
 * Selects feature page 0 and reads the 4-byte little-endian step counter
 * output located at byte offset STEP_COUNT_OUT_OFFSET.  The step counter
 * feature must be enabled before calling this function.
 *
 * @param count  Reference to receive the cumulative step count.
 * @return true on success, false on I2C error or if not initialised.
 */
bool Imu::GetStepCount(uint32_t &count)
{
    if (!_initialised)
    {
        return (false);
    }

    uint8_t page[FEATURE_PAGE_SIZE];
    if (!ReadFeaturePage(FEATURE_PAGE_OUTPUTS, page))
    {
        return (false);
    }

    const uint8_t index = STEP_COUNT_OUT_OFFSET;
    count = static_cast<uint32_t>(page[index]) |
            (static_cast<uint32_t>(page[index + 1]) << 8) |
            (static_cast<uint32_t>(page[index + 2]) << 16) |
            (static_cast<uint32_t>(page[index + 3]) << 24);
    return (true);
}

/**
 * @brief Resets the BMI270 step counter to zero.
 *
 * Reads the step counter configuration word from feature page 6, sets the
 * reset-counter bit (bit 10), and writes the word back.  The sensor clears
 * this bit automatically once the counter has been reset.
 *
 * @return true on success, false on I2C error or if not initialised.
 */
bool Imu::ResetStepCount()
{
    if (!_initialised)
    {
        return (false);
    }

    uint8_t page[FEATURE_PAGE_SIZE];
    if (!ReadFeaturePage(FEATURE_PAGE_STEP_COUNT_CONFIG, page))
    {
        return (false);
    }

    uint16_t word = static_cast<uint16_t>(page[STEP_COUNT_CONFIG_OFFSET]) |
                    (static_cast<uint16_t>(page[STEP_COUNT_CONFIG_OFFSET + 1]) << 8);

    word |= STEP_COUNT_RESET_MASK;

    page[STEP_COUNT_CONFIG_OFFSET] = static_cast<uint8_t>(word & 0xFF);
    page[STEP_COUNT_CONFIG_OFFSET + 1] = static_cast<uint8_t>((word >> 8) & 0xFF);

    return (WriteFeaturePage(FEATURE_PAGE_STEP_COUNT_CONFIG, page));
}

/**
 * @brief Sets the step counter watermark level.
 *
 * The watermark interrupt fires every (@p watermark × 20) steps.  Reads the
 * step counter configuration word from feature page 6, replaces the 10-bit
 * watermark field (bits [9:0]), and writes the word back.
 *
 * @param watermark  Number of 20-step increments between interrupts (1–1023).
 * @return true on success, false on I2C error or if not initialised.
 */
bool Imu::SetStepCountWatermark(uint16_t watermark)
{
    if (!_initialised)
    {
        return (false);
    }

    uint8_t page[FEATURE_PAGE_SIZE];
    if (!ReadFeaturePage(FEATURE_PAGE_STEP_COUNT_CONFIG, page))
    {
        return (false);
    }

    uint16_t word = static_cast<uint16_t>(page[STEP_COUNT_CONFIG_OFFSET]) |
                    (static_cast<uint16_t>(page[STEP_COUNT_CONFIG_OFFSET + 1]) << 8);

    word = static_cast<uint16_t>((word & ~STEP_COUNT_WATERMARK_MASK) | (watermark & STEP_COUNT_WATERMARK_MASK));

    page[STEP_COUNT_CONFIG_OFFSET] = static_cast<uint8_t>(word & 0xFF);
    page[STEP_COUNT_CONFIG_OFFSET + 1] = static_cast<uint8_t>((word >> 8) & 0xFF);

    return (WriteFeaturePage(FEATURE_PAGE_STEP_COUNT_CONFIG, page));
}

/**
 * @brief Reads the current step activity from the BMI270 feature output page.
 *
 * Selects feature page 0 and reads the step activity byte at offset
 * STEP_ACTIVITY_OUT_OFFSET.  Compare the result against the STEP_ACTIVITY_*
 * constants.  The step activity feature must be enabled before calling this
 * function.
 *
 * @param activity  Reference to receive the activity value.
 * @return true on success, false on I2C error or if not initialised.
 */
bool Imu::GetStepActivity(uint8_t &activity)
{
    if (!_initialised)
    {
        return (false);
    }

    uint8_t page[FEATURE_PAGE_SIZE];
    if (!ReadFeaturePage(FEATURE_PAGE_OUTPUTS, page))
    {
        return (false);
    }

    activity = page[STEP_ACTIVITY_OUT_OFFSET];
    return (true);
}

// =============================================================================
// Wrist gesture
// =============================================================================

/**
 * @brief Reads the latest wrist gesture output from the BMI270 feature page.
 *
 * Selects feature page 0 and reads the wrist gesture byte at offset
 * WRIST_GESTURE_OUT_OFFSET.  Compare the result against the
 * WRIST_GESTURE_* constants.  The wrist gesture feature must be enabled
 * before calling this function.
 *
 * @param gesture  Reference to receive the gesture value.
 * @return true on success, false on I2C error or if not initialised.
 */
bool Imu::GetWristGesture(uint8_t &gesture)
{
    if (!_initialised)
    {
        return (false);
    }

    uint8_t page[FEATURE_PAGE_SIZE];
    if (!ReadFeaturePage(FEATURE_PAGE_OUTPUTS, page))
    {
        return (false);
    }

    gesture = page[WRIST_GESTURE_OUT_OFFSET];
    return (true);
}

// =============================================================================
// Calibration and NVM
// =============================================================================

/**
 * @brief Performs fast offset calibration (FOC) for the accelerometer.
 *
 * The BMI270 measures the static offset error while the sensor is held
 * motionless with one axis aligned with gravity.  The @p gravityDirection
 * parameter encodes both the axis and the sign using the GRAVITY_* constants
 * (e.g. GRAVITY_POSITIVE_Z means gravity is acting on the +Z axis).
 *
 * The function enables the accelerometer self-test feature register, triggers
 * the FOC sequence by writing the axis direction, and polls the internal
 * status until the calibration completes (up to 2 seconds).
 *
 * @param gravityDirection  Combined axis and sign byte (use the GRAVITY_* constants).
 * @return true on success, false on I2C error, timeout, or if not initialised.
 */
bool Imu::PerformAccelerometerOffsetCalibration(uint8_t gravityDirection)
{
    if (!_initialised)
    {
        return (false);
    }

    uint8_t axisDirection = 0x00;
    if ((gravityDirection & GRAVITY_AXIS_X) != 0)
    {
        axisDirection = (gravityDirection & GRAVITY_DIRECTION_POSITIVE) != 0 ? 0x01 : 0x02;
    }
    else if ((gravityDirection & GRAVITY_AXIS_Y) != 0)
    {
        axisDirection = (gravityDirection & GRAVITY_DIRECTION_POSITIVE) != 0 ? 0x03 : 0x04;
    }
    else if ((gravityDirection & GRAVITY_AXIS_Z) != 0)
    {
        axisDirection = (gravityDirection & GRAVITY_DIRECTION_POSITIVE) != 0 ? 0x05 : 0x06;
    }
    else
    {
        ESP_LOGE(LOG_TAG, "PerformAccelerometerOffsetCalibration: invalid gravity direction 0x%02X.", gravityDirection);
        return (false);
    }

    if (!WriteRegister(REG_ACC_SELF_TEST, axisDirection))
    {
        return (false);
    }

    for (int attempt = 0; attempt < 100; ++attempt)
    {
        vTaskDelay(pdMS_TO_TICKS(20));

        uint8_t status;
        if (!ReadRegisters(REG_STATUS, &status, 1))
        {
            return (false);
        }

        if ((status & 0x08) != 0)
        {
            ESP_LOGI(LOG_TAG, "Accelerometer FOC complete after %d ms.", (attempt + 1) * 20);
            WriteRegister(REG_ACC_SELF_TEST, 0x00);
            return (true);
        }
    }

    ESP_LOGE(LOG_TAG, "Accelerometer FOC timed out.");
    WriteRegister(REG_ACC_SELF_TEST, 0x00);
    return (false);
}

/**
 * @brief Performs fast offset calibration (FOC) for the gyroscope.
 *
 * The sensor must be held completely motionless during this procedure.
 * The gyroscope FOC sequence is triggered via the NVM configuration register
 * and completes in approximately 200 ms.  The function polls the FOC-ready
 * flag in the status register with a 2-second timeout.
 *
 * @return true on success, false on I2C error, timeout, or if not initialised.
 */
bool Imu::PerformGyroscopeOffsetCalibration()
{
    if (!_initialised)
    {
        return (false);
    }

    if (!WriteRegister(REG_NVM_CONF, 0x02))
    {
        return (false);
    }

    for (int attempt = 0; attempt < 100; ++attempt)
    {
        vTaskDelay(pdMS_TO_TICKS(20));

        uint8_t status;
        if (!ReadRegisters(REG_STATUS, &status, 1))
        {
            return (false);
        }

        if ((status & 0x08) != 0)
        {
            ESP_LOGI(LOG_TAG, "Gyroscope FOC complete after %d ms.", (attempt + 1) * 20);
            WriteRegister(REG_NVM_CONF, 0x00);
            return (true);
        }
    }

    ESP_LOGE(LOG_TAG, "Gyroscope FOC timed out.");
    WriteRegister(REG_NVM_CONF, 0x00);
    return (false);
}

/**
 * @brief Performs the built-in self-test for the accelerometer and gyroscope.
 *
 * Runs the accelerometer self-test by writing 0x01 to REG_ACC_SELF_TEST and
 * polling for completion, then performs a soft reset and full reinitialisation
 * to restore normal operation.
 *
 * @warning  The NVM supports only 14 write cycles.  Avoid calling SaveNvm()
 *           inside calibration sequences that may run frequently.
 *
 * @return true on success, false on I2C error, test failure, or if not
 *         initialised.
 */
bool Imu::PerformSelfTest()
{
    if (!_initialised)
    {
        return (false);
    }

    if (!WriteRegister(REG_ACC_SELF_TEST, 0x01))
    {
        return (false);
    }

    bool passed = false;
    for (int attempt = 0; attempt < 100; ++attempt)
    {
        vTaskDelay(pdMS_TO_TICKS(200));

        uint8_t status;
        if (!ReadRegisters(REG_STATUS, &status, 1))
        {
            break;
        }

        if ((status & 0x01) != 0)
        {
            passed = true;
            break;
        }
    }

    WriteRegister(REG_ACC_SELF_TEST, 0x00);

    if (!passed)
    {
        ESP_LOGE(LOG_TAG, "IMU self-test failed or timed out.");
        return (false);
    }

    if (!Reset())
    {
        return (false);
    }

    ESP_LOGI(LOG_TAG, "IMU self-test passed.");
    return (true);
}

/**
 * @brief Programs all NVM-backed registers to the BMI270's non-volatile memory.
 *
 * Writes the NVM_PROG command (0xA0) to the command register.  The NVM
 * supports a maximum of 14 write cycles; use this function sparingly.
 *
 * @return true on success, false on I2C error or if not initialised.
 */
bool Imu::SaveNvm()
{
    if (!_initialised)
    {
        return (false);
    }

    return (WriteRegister(REG_CMD, 0xA0));
}

// =============================================================================
// Feature enable/disable
// =============================================================================

/**
 * @brief Sets or clears the feature-enable bit for the given feature.
 *
 * Looks up the feature's page, byte offset, and bitmask, then performs a
 * read-modify-write on the BMI270 feature page RAM.
 *
 * @param feature  One of the FEATURE_* constants.
 * @param enable   true to enable, false to disable.
 * @return true on success, false on I2C error or unknown feature ID.
 */
bool Imu::SetFeatureEnabled(uint8_t feature, bool enable)
{
    FeatureEnableInfo info;

    switch (feature)
    {
        case FEATURE_SIG_MOTION:
            /*
             * Page 2, start_addr=0x04, en_offset=0x0A → byte 0x0E, mask 0x01.
             */
            info = {2, 0x0E, 0x01};
            break;
        case FEATURE_ANY_MOTION:
            /*
             * Page 1, start_addr=0x0C, en_offset=0x03 → byte 0x0F, mask 0x80.
             */
            info = {1, 0x0F, 0x80};
            break;
        case FEATURE_NO_MOTION:
            /*
             * Page 1, start_addr=0x00, en_offset=0x03 → byte 0x03, mask 0x80.
             */
            info = {1, 0x03, 0x80};
            break;
        case FEATURE_STEP_DETECTOR:
            /*
             * Page 6, start_addr=0x02, en_offset=0x01 → byte 0x03, mask 0x08.
             */
            info = {6, 0x03, 0x08};
            break;
        case FEATURE_STEP_COUNTER:
            /*
             * Page 6, start_addr=0x02, en_offset=0x01 → byte 0x03, mask 0x10.
             */
            info = {6, 0x03, 0x10};
            break;
        case FEATURE_STEP_ACTIVITY:
            /*
             * Page 6, start_addr=0x02, en_offset=0x01 → byte 0x03, mask 0x20.
             */
            info = {6, 0x03, 0x20};
            break;
        case FEATURE_WRIST_GESTURE:
            /*
             * Page 6, start_addr=0x06, en_offset=0x00 → byte 0x06, mask 0x20.
             */
            info = {6, 0x06, 0x20};
            break;
        case FEATURE_WRIST_WEAR_WAKE_UP:
            /*
             * Page 7, start_addr=0x00, en_offset=0x00 → byte 0x00, mask 0x10.
             */
            info = {7, 0x00, 0x10};
            break;
        default:
            ESP_LOGE(LOG_TAG, "SetFeatureEnabled: unknown feature ID %u.", feature);
            return (false);
    }

    uint8_t page[FEATURE_PAGE_SIZE];

    if (!ReadFeaturePage(info.page, page))
    {
        return (false);
    }

    if (enable)
    {
        page[info.byteIndex] |= info.mask;
    }
    else
    {
        page[info.byteIndex] &= static_cast<uint8_t>(~info.mask);
    }

    return (WriteFeaturePage(info.page, page));
}

/**
 * @brief Enables a named BMI270 feature in the on-chip feature engine.
 *
 * @param feature  One of the FEATURE_* constants.
 * @return true on success, false on I2C error, unknown feature ID, or if
 *         not initialised.
 */
bool Imu::EnableFeature(uint8_t feature)
{
    if (!_initialised)
    {
        return (false);
    }

    return (SetFeatureEnabled(feature, true));
}

/**
 * @brief Disables a named BMI270 feature in the on-chip feature engine.
 *
 * @param feature  One of the FEATURE_* constants.
 * @return true on success, false on I2C error, unknown feature ID, or if
 *         not initialised.
 */
bool Imu::DisableFeature(uint8_t feature)
{
    if (!_initialised)
    {
        return (false);
    }

    return (SetFeatureEnabled(feature, false));
}

// =============================================================================
// Axis remapping
// =============================================================================

/**
 * @brief Remaps the output axes of the BMI270 accelerometer and gyroscope.
 *
 * Converts the user-facing AXIS_POS_* / AXIS_NEG_* source values into the
 * two-byte register format used by the BMI270 feature page axis-map entry
 * (page 1, byte offset 0x04–0x05), then writes the updated page.
 *
 * Byte 0 (index 0x04): x_axis[1:0] | x_sign[2] | y_axis[4:3] | y_sign[5] | z_axis[7:6]
 * Byte 1 (index 0x05): z_sign[0] (other bits preserved — shared with the
 *                       gyroscope self-offset correction feature enable bit).
 *
 * @param xSource  AXIS_POS_* or AXIS_NEG_* value for the logical X output.
 * @param ySource  AXIS_POS_* or AXIS_NEG_* value for the logical Y output.
 * @param zSource  AXIS_POS_* or AXIS_NEG_* value for the logical Z output.
 * @return true on success, false on I2C error or if not initialised.
 */
bool Imu::RemapAxes(uint8_t xSource, uint8_t ySource, uint8_t zSource)
{
    if (!_initialised)
    {
        return (false);
    }

    /*
     * Convert AXIS_POS_X/NEG_X etc. → axis field (0=X, 1=Y, 2=Z) + sign bit.
     * The user constants follow the BMI2 convention: 0x01=X, 0x02=Y, 0x04=Z,
     * with sign bit 0x08.  Axis field = trailing-zero index of bits[2:0].
     */
    auto toAxisNum = [](uint8_t source) -> uint8_t
    {
        uint8_t bits = static_cast<uint8_t>(source & AXIS_SOURCE_MASK);
        if ((bits & 0x01) != 0)
        {
            return (0);
        }

        if ((bits & 0x02) != 0)
        {
            return (1);
        }

        return (2);
    };

    auto toSign = [](uint8_t source) -> uint8_t
    {
        return ((source & AXIS_SIGN_BIT) != 0) ? 1 : 0;
    };

    uint8_t xAxis = toAxisNum(xSource);
    uint8_t xSign = toSign(xSource);
    uint8_t yAxis = toAxisNum(ySource);
    uint8_t ySign = toSign(ySource);
    uint8_t zAxis = toAxisNum(zSource);
    uint8_t zSign = toSign(zSource);

    uint8_t page[FEATURE_PAGE_SIZE];

    if (!ReadFeaturePage(AXIS_MAP_PAGE, page))
    {
        return (false);
    }

    /*
     * Pack the axis fields into byte 0x04 (fully replaced — all six fields
     * are encoded here).
     */
    page[AXIS_MAP_START_ADDR] = static_cast<uint8_t>(
        (xAxis & AXIS_X_MASK) |
        ((xSign << AXIS_X_SIGN_POS) & AXIS_X_SIGN_MASK) |
        ((yAxis << AXIS_Y_POS) & AXIS_Y_MASK) |
        ((ySign << AXIS_Y_SIGN_POS) & AXIS_Y_SIGN_MASK) |
        ((zAxis << AXIS_Z_POS) & AXIS_Z_MASK));

    /*
     * Byte 0x05 also holds the gyroscope self-offset correction enable bit
     * (bit 1), so only bit 0 (z_axis_sign) is updated here.
     */
    page[AXIS_MAP_START_ADDR + 1] = static_cast<uint8_t>(
        (page[AXIS_MAP_START_ADDR + 1] & static_cast<uint8_t>(~AXIS_Z_SIGN_MASK)) |
        (zSign & AXIS_Z_SIGN_MASK));

    return (WriteFeaturePage(AXIS_MAP_PAGE, page));
}

// =============================================================================
// FIFO
// =============================================================================

/**
 * @brief Configures the BMI270 FIFO.
 *
 * Programs FIFO_CONFIG_0 (0x48), FIFO_CONFIG_1 (0x49), FIFO_DOWNS (0x45),
 * and the watermark registers FIFO_WTM_0/1 (0x46–0x47).
 *
 * @param config  Desired FIFO configuration.
 * @return true on success, false on I2C error or if not initialised.
 */
bool Imu::ConfigureFifo(const FifoConfig &config)
{
    if (!_initialised)
    {
        return (false);
    }

    uint8_t cfg0 = 0;

    if (config.stopOnFull)
    {
        cfg0 |= FIFO_STOP_ON_FULL_MASK;
    }

    if (config.timeEnabled)
    {
        cfg0 |= FIFO_TIME_EN_MASK;
    }

    if (!WriteRegister(REG_FIFO_CONFIG_0, cfg0))
    {
        return (false);
    }

    uint8_t cfg1 = 0;

    if (config.headerEnabled)
    {
        cfg1 |= FIFO_HEADER_EN_MASK;
    }

    if (config.accelEnabled)
    {
        cfg1 |= FIFO_ACC_EN_MASK;
    }

    if (config.gyroEnabled)
    {
        cfg1 |= FIFO_GYR_EN_MASK;
    }

    if (!WriteRegister(REG_FIFO_CONFIG_1, cfg1))
    {
        return (false);
    }

    uint8_t downs = 0;
    downs |= static_cast<uint8_t>(config.gyroDownSample & 0x07);

    if (config.gyroFilteredData)
    {
        downs |= FIFO_GYR_FILT_MASK;
    }

    downs |= static_cast<uint8_t>((config.accelDownSample << FIFO_ACC_DOWNS_POS) & FIFO_ACC_DOWNS_MASK);

    if (config.accelFilteredData)
    {
        downs |= FIFO_ACC_FILT_MASK;
    }

    if (!WriteRegister(REG_FIFO_DOWNS, downs))
    {
        return (false);
    }

    if (!WriteRegister(REG_FIFO_WTM_0, static_cast<uint8_t>(config.watermark & 0xFF)))
    {
        return (false);
    }

    if (!WriteRegister(REG_FIFO_WTM_1, static_cast<uint8_t>((config.watermark >> 8) & 0x07)))
    {
        return (false);
    }

    return (true);
}

/**
 * @brief Reads the current FIFO fill level in bytes.
 *
 * Reads the 14-bit value from FIFO_LENGTH_0 (0x24) and FIFO_LENGTH_1 (0x25).
 *
 * @param length  Reference to receive the FIFO fill level in bytes.
 * @return true on success, false on I2C error or if not initialised.
 */
bool Imu::GetFifoLength(uint16_t &length)
{
    if (!_initialised)
    {
        return (false);
    }

    uint8_t buffer[2];

    if (!ReadRegisters(REG_FIFO_LENGTH_0, buffer, sizeof(buffer)))
    {
        return (false);
    }

    length = static_cast<uint16_t>(
        static_cast<uint16_t>(buffer[0]) |
        (static_cast<uint16_t>(buffer[1] & 0x3F) << 8));

    return (true);
}

/**
 * @brief Reads bytes from the FIFO into the supplied buffer.
 *
 * On entry @p length must contain the capacity of @p buffer.  On return it
 * is updated to the number of bytes actually transferred.  If the FIFO
 * contains fewer bytes than the buffer can hold, only the available bytes
 * are read.
 *
 * @param buffer  Destination buffer.
 * @param length  In: buffer capacity.  Out: bytes read.
 * @return true on success, false on I2C error or if not initialised.
 */
bool Imu::ReadFifo(uint8_t *buffer, uint16_t &length)
{
    if (!_initialised || buffer == nullptr)
    {
        return (false);
    }

    uint16_t available;

    if (!GetFifoLength(available))
    {
        return (false);
    }

    if (available == 0)
    {
        length = 0;
        return (true);
    }

    uint16_t readLength = (available < length) ? available : length;
    length = readLength;

    return (ReadRegisters(REG_FIFO_DATA, buffer, readLength));
}

/**
 * @brief Flushes all data currently stored in the FIFO.
 *
 * Sends the FIFO_FLUSH command (0xB0) to the command register.
 *
 * @return true on success, false on I2C error or if not initialised.
 */
bool Imu::FlushFifo()
{
    if (!_initialised)
    {
        return (false);
    }

    return (WriteRegister(REG_CMD, FIFO_FLUSH_CMD));
}

// =============================================================================
// Component retrim (CRT)
// =============================================================================

/**
 * @brief Performs the BMI270 gyroscope component retrim (CRT) procedure.
 *
 * Prepares the sensor for CRT by disabling the gyroscope and disabling the
 * FIFO, then triggers the G_TRIGGER command and polls the GYR_CRT_RUNNING bit
 * in GYR_CRT_CONF (0x69) until the procedure completes or the 2-second
 * timeout expires.  The gyroscope is re-enabled after a successful retrim.
 *
 * @return true on success, false on timeout, I2C error, or if not initialised.
 */
bool Imu::PerformComponentRetrim()
{
    if (!_initialised)
    {
        return (false);
    }

    /*
     * Disable advanced power save before CRT.
     */
    if (!WriteRegister(REG_PWR_CONF, 0x00))
    {
        return (false);
    }

    vTaskDelay(pdMS_TO_TICKS(1));

    /*
     * Disable the gyroscope.  PWR_CTRL = 0x06 keeps accel + temp enabled
     * while clearing the gyroscope enable bit (bit 3).
     */
    if (!WriteRegister(REG_PWR_CTRL, 0x06))
    {
        return (false);
    }

    vTaskDelay(pdMS_TO_TICKS(5));

    /*
     * Disable FIFO for all sensors by clearing FIFO_CONFIG_1.
     */
    if (!WriteRegister(REG_FIFO_CONFIG_1, 0x00))
    {
        return (false);
    }

    /*
     * Set the GYR_CRT_RUNNING bit in GYR_CRT_CONF (0x69) before triggering.
     */
    if (!ModifyRegister(REG_GYR_CRT_CONF, GYR_CRT_RUNNING_MASK, GYR_CRT_RUNNING_MASK))
    {
        return (false);
    }

    /*
     * Trigger the CRT by writing G_TRIGGER_CMD to the command register.
     */
    if (!WriteRegister(REG_CMD, G_TRIGGER_CMD))
    {
        return (false);
    }

    /*
     * Poll GYR_CRT_RUNNING until cleared by the sensor (up to 2 seconds,
     * checking every 10 ms).
     */
    bool completed = false;

    for (int attempt = 0; attempt < 200; ++attempt)
    {
        vTaskDelay(pdMS_TO_TICKS(10));

        uint8_t crtConf;

        if (!ReadRegisters(REG_GYR_CRT_CONF, &crtConf, 1))
        {
            return (false);
        }

        if ((crtConf & GYR_CRT_RUNNING_MASK) == 0)
        {
            completed = true;
            break;
        }
    }

    if (!completed)
    {
        ESP_LOGE(LOG_TAG, "CRT timed out — GYR_CRT_RUNNING never cleared.");
    }

    /*
     * Re-enable the gyroscope regardless of the CRT outcome.
     */
    if (!WriteRegister(REG_PWR_CTRL, 0x0E))
    {
        return (false);
    }

    if (completed)
    {
        ESP_LOGI(LOG_TAG, "CRT complete.");
    }

    return (completed);
}

// =============================================================================
// Auxiliary I2C
// =============================================================================

/**
 * @brief Configures the BMI270 auxiliary I2C interface.
 *
 * Enables the auxiliary interface via IF_CONF (0x6B), sets the I2C device
 * address and manual/auto mode in the AUX_DEV_ID (0x4B) / AUX_IF_CONF (0x4C)
 * register pair, then writes the ODR to AUX_CONF (0x44).
 *
 * @param config  Desired auxiliary interface configuration.
 * @return true on success, false on I2C error or if not initialised.
 */
bool Imu::ConfigureAux(const AuxConfig &config)
{
    if (!_initialised)
    {
        return (false);
    }

    /*
     * Enable the auxiliary interface via IF_CONF (0x6B) bit 5.
     */
    if (!ModifyRegister(REG_IF_CONF, AUX_IF_EN_MASK, AUX_IF_EN_MASK))
    {
        return (false);
    }

    /*
     * Read the two-byte AUX_DEV_ID / AUX_IF_CONF pair and update fields.
     */
    uint8_t devIfRegs[2];

    if (!ReadRegisters(REG_AUX_DEV_ID, devIfRegs, sizeof(devIfRegs)))
    {
        return (false);
    }

    devIfRegs[0] = static_cast<uint8_t>(
        (devIfRegs[0] & static_cast<uint8_t>(~AUX_SET_I2C_ADDR_MASK)) |
        ((config.i2cAddress << AUX_SET_I2C_ADDR_POS) & AUX_SET_I2C_ADDR_MASK));

    devIfRegs[1] = static_cast<uint8_t>(
        (devIfRegs[1] & static_cast<uint8_t>(~(AUX_MAN_MODE_EN_MASK | AUX_MAN_READ_BURST_MASK | AUX_READ_BURST_MASK))) |
        (config.manualMode ? AUX_MAN_MODE_EN_MASK : 0x00) |
        static_cast<uint8_t>((config.burstReadLength << AUX_MAN_READ_BURST_POS) & AUX_MAN_READ_BURST_MASK) |
        static_cast<uint8_t>(config.burstReadLength & AUX_READ_BURST_MASK));

    uint8_t writeBuffer[3];
    writeBuffer[0] = REG_AUX_DEV_ID;
    writeBuffer[1] = devIfRegs[0];
    writeBuffer[2] = devIfRegs[1];

    esp_err_t result = i2c_master_transmit(_deviceHandle, writeBuffer, sizeof(writeBuffer), -1);

    if (result != ESP_OK)
    {
        return (false);
    }

    vTaskDelay(pdMS_TO_TICKS(1));

    /*
     * Set the AUX output data rate in AUX_CONF (0x44).
     */
    return (ModifyRegister(REG_AUX_CONF, AUX_ODR_MASK, config.odr & AUX_ODR_MASK));
}

/**
 * @brief Reads bytes from an auxiliary sensor register in manual mode.
 *
 * Waits until the AUX interface is not busy, writes the target register
 * address to REG_AUX_RD_ADDR, then reads the result from the AUX data
 * output registers starting at REG_AUX_X_LSB (0x04).
 *
 * @param regAddr  Register address on the auxiliary sensor.
 * @param buffer   Destination buffer (must be at least @p length bytes).
 * @param length   Number of bytes to read (clamped to AUX_NUM_BYTES = 8).
 * @return true on success, false on I2C error, timeout, or if not
 *         initialised.
 */
bool Imu::ReadAux(uint8_t regAddr, uint8_t *buffer, uint8_t length)
{
    if (!_initialised || buffer == nullptr)
    {
        return (false);
    }

    if (length > AUX_NUM_BYTES)
    {
        length = AUX_NUM_BYTES;
    }

    /*
     * Wait for the AUX interface to become not busy (STATUS bit 2 = 0).
     */
    for (int attempt = 0; attempt < 20; ++attempt)
    {
        uint8_t status;

        if (!ReadRegisters(REG_STATUS, &status, 1))
        {
            return (false);
        }

        if ((status & AUX_BUSY_MASK) == 0)
        {
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(10));

        if (attempt == 19)
        {
            ESP_LOGE(LOG_TAG, "ReadAux: AUX interface busy timeout.");
            return (false);
        }
    }

    if (!WriteRegister(REG_AUX_RD_ADDR, regAddr))
    {
        return (false);
    }

    vTaskDelay(pdMS_TO_TICKS(1));

    return (ReadRegisters(REG_AUX_X_LSB, buffer, length));
}

/**
 * @brief Writes a single byte to an auxiliary sensor register in manual mode.
 *
 * Waits until not busy, writes the data byte to REG_AUX_WR_DATA, waits
 * again, then writes the register address to REG_AUX_WR_ADDR which triggers
 * the write on the auxiliary bus.
 *
 * @param regAddr  Register address on the auxiliary sensor.
 * @param data     Byte value to write.
 * @return true on success, false on I2C error, timeout, or if not
 *         initialised.
 */
bool Imu::WriteAux(uint8_t regAddr, uint8_t data)
{
    if (!_initialised)
    {
        return (false);
    }

    /*
     * Helper lambda: wait for AUX not busy, then write a register.
     */
    auto waitAndWrite = [](uint8_t reg, uint8_t value) -> bool
    {
        for (int attempt = 0; attempt < 20; ++attempt)
        {
            uint8_t status;

            if (!ReadRegisters(REG_STATUS, &status, 1))
            {
                return (false);
            }

            if ((status & AUX_BUSY_MASK) == 0)
            {
                if (!WriteRegister(reg, value))
                {
                    return (false);
                }

                vTaskDelay(pdMS_TO_TICKS(1));
                return (true);
            }

            vTaskDelay(pdMS_TO_TICKS(10));
        }

        ESP_LOGE(LOG_TAG, "WriteAux: AUX interface busy timeout.");
        return (false);
    };

    /*
     * Write data first, then address (address write triggers the AUX write).
     */
    if (!waitAndWrite(REG_AUX_WR_DATA, data))
    {
        return (false);
    }

    return (waitAndWrite(REG_AUX_WR_ADDR, regAddr));
}
