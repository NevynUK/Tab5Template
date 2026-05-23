/*-----------------------------------------------------------------------------
 * File        : Example01_BasicReadings.cpp
 * Description : Reads the 3-axis accelerometer and 3-axis gyroscope from
 *               the BMI270 IMU and logs the values at 50 Hz.
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
static constexpr const char *LOG_TAG = "Example01";

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
 * The Imu class retrieves this bus handle internally by port number, so the
 * bus must exist before calling Imu::Initialise().
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
 * Initialises the IMU and continuously reads the accelerometer and gyroscope
 * at 50 Hz (the default ODR configured by the Imu class).
 *
 * @param pvParameters  Unused FreeRTOS task parameter.
 */
static void MainTask(void *pvParameters)
{
    ESP_LOGI(LOG_TAG, "BMI270 Example 1 — Basic Readings");

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

        /* Print at 50 Hz. */
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
