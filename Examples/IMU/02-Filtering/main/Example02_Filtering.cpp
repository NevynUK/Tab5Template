/*-----------------------------------------------------------------------------
 * File        : Example02_Filtering.cpp
 * Description : Configures the BMI270 accelerometer and gyroscope with
 *               specific ODR, filter bandwidth, and performance mode settings,
 *               then reads sensor data at 50 Hz.
 * Author      : Mark Stevens
 * Copyright   : Copyright (c) 2026 Mark Stevens
 * Licence     : MIT — see LICENSE in the repository root for full terms.
 * Target      : M5Stack Tab5 (ESP32-P4)
 *---------------------------------------------------------------------------*/

#include "Imu.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_err.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/gpio.h>

/**
 * @brief Logging tag for this example.
 */
static constexpr const char *LOG_TAG = "Example02";

/**
 * @brief SDA pin for the Tab5 internal I2C bus (I2C_NUM_1).
 */
static constexpr gpio_num_t I2C_SDA_PIN = GPIO_NUM_31;

/**
 * @brief SCL pin for the Tab5 internal I2C bus (I2C_NUM_1).
 */
static constexpr gpio_num_t I2C_SCL_PIN = GPIO_NUM_32;

/**
 * @brief Initialises the I2C_NUM_1 master bus on SDA=GPIO31 / SCL=GPIO32.
 *
 * @return true on success, false on error.
 */
static bool InitialiseI2cBus()
{
    i2c_master_bus_config_t busConfig = {};
    busConfig.clk_source                   = I2C_CLK_SRC_DEFAULT;
    busConfig.i2c_port                     = I2C_NUM_1;
    busConfig.sda_io_num                   = I2C_SDA_PIN;
    busConfig.scl_io_num                   = I2C_SCL_PIN;
    busConfig.glitch_ignore_cnt            = 7;
    busConfig.flags.enable_internal_pullup = true;

    i2c_master_bus_handle_t busHandle;
    esp_err_t result = i2c_new_master_bus(&busConfig, &busHandle);
    if (result != ESP_OK)
    {
        ESP_LOGE(LOG_TAG, "Failed to create I2C bus: %s", esp_err_to_name(result));
        return (false);
    }

    return (true);
}

/**
 * @brief Main example task.
 *
 * Configures both sensors with 50 Hz ODR, OSR4 bandwidth (lowest noise
 * setting), and performance-optimised filter mode, then reads and logs
 * sensor data at 50 Hz.
 *
 * Filter settings reference:
 *
 * Accelerometer ODR / filter performance mode:
 *   PERF_OPT_MODE (performance) — valid ODR: 12.5 Hz to 1600 Hz.
 *   POWER_OPT_MODE (power)      — valid ODR: 0.78 Hz to 400 Hz.
 *
 * Accelerometer bandwidth parameter (ACCEL_BWP_*):
 *   In PERF_OPT_MODE: OSR4_AVG1, OSR2_AVG2, NORMAL_AVG4, CIC_AVG8.
 *   In POWER_OPT_MODE: AVG1 through AVG128.
 *
 * Gyroscope ODR / filter performance mode:
 *   PERF_OPT_MODE (performance) — valid ODR: 25 Hz to 3200 Hz.
 *   POWER_OPT_MODE (power)      — valid ODR: 25 Hz to 100 Hz.
 *
 * Gyroscope bandwidth parameter (GYRO_BWP_*): OSR4, OSR2, NORMAL, CIC.
 *
 * @param pvParameters  Unused FreeRTOS task parameter.
 */
static void MainTask(void *pvParameters)
{
    ESP_LOGI(LOG_TAG, "BMI270 Example 2 — Filtering");

    if (!InitialiseI2cBus())
    {
        vTaskDelete(nullptr);
        return;
    }

    if (Imu::Initialise() == nullptr)
    {
        ESP_LOGE(LOG_TAG, "IMU initialisation failed — check wiring and I2C address.");
        vTaskDelete(nullptr);
        return;
    }

    ESP_LOGI(LOG_TAG, "BMI270 connected.");

    /*
     * Configure accelerometer:
     *   ODR        = 50 Hz
     *   Bandwidth  = OSR4 (lowest noise, highest signal delay)
     *   Filter     = performance-optimised mode
     */
    Imu::SetAccelerometerOutputDataRate(Imu::ACCEL_ODR_50HZ);
    Imu::SetAccelerometerFilterBandwidth(Imu::ACCEL_BWP_OSR4_AVG1);
    Imu::SetAccelerometerPowerMode(Imu::PERF_OPT_MODE);

    /*
     * Configure gyroscope:
     *   ODR        = 50 Hz
     *   Bandwidth  = OSR4 (lowest noise, highest signal delay)
     *   Filter     = performance-optimised mode
     *   Noise      = performance-optimised mode
     */
    Imu::SetGyroscopeOutputDataRate(Imu::GYRO_ODR_50HZ);
    Imu::SetGyroscopeFilterBandwidth(Imu::GYRO_BWP_OSR4);
    Imu::SetGyroscopePowerMode(Imu::PERF_OPT_MODE, Imu::PERF_OPT_MODE);

    ESP_LOGI(LOG_TAG,
             "Config: accel 50 Hz / OSR4 / perf mode  |  gyro 50 Hz / OSR4 / perf mode");

    while (true)
    {
        Imu::Vector3 acceleration;
        Imu::Vector3 gyroscope;

        const bool accelOk = Imu::GetAcceleration(acceleration);
        const bool gyroOk  = Imu::GetGyroscope(gyroscope);

        if (accelOk && gyroOk)
        {
            ESP_LOGI(LOG_TAG,
                     "Accel (g)   X: %6.3f  Y: %6.3f  Z: %6.3f  |  "
                     "Gyro (°/s)  X: %7.3f  Y: %7.3f  Z: %7.3f",
                     acceleration.x, acceleration.y, acceleration.z,
                     gyroscope.x,   gyroscope.y,   gyroscope.z);
        }
        else
        {
            ESP_LOGE(LOG_TAG, "Failed to read sensor data.");
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/**
 * @brief ESP-IDF application entry point.
 */
extern "C" void app_main(void)
{
    xTaskCreate(MainTask, "MainTask", 4096, nullptr, 5, nullptr);
}
