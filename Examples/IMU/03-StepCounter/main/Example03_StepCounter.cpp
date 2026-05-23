/*-----------------------------------------------------------------------------
 * File        : Example03_StepCounter.cpp
 * Description : Demonstrates the BMI270 step counter and step activity
 *               features.  The step count and current activity (still,
 *               walking, running) are logged whenever a change is detected.
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
static constexpr const char *LOG_TAG = "Example03";

/**
 * @brief SDA pin for the Tab5 internal I2C bus (I2C_NUM_1).
 */
static constexpr gpio_num_t I2C_SDA_PIN = GPIO_NUM_31;

/**
 * @brief SCL pin for the Tab5 internal I2C bus (I2C_NUM_1).
 */
static constexpr gpio_num_t I2C_SCL_PIN = GPIO_NUM_32;

/**
 * @brief Polling interval in milliseconds.
 */
static constexpr uint32_t POLL_INTERVAL_MS = 500;

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
 * @brief Returns a human-readable string for a step activity value.
 *
 * @param activity  Activity value from GetStepActivity().
 * @return Pointer to a string literal describing the activity.
 */
static const char *ActivityName(uint8_t activity)
{
    switch (activity)
    {
        case Imu::STEP_ACTIVITY_STILL:   return ("still");
        case Imu::STEP_ACTIVITY_WALKING: return ("walking");
        case Imu::STEP_ACTIVITY_RUNNING: return ("running");
        default:                         return ("unknown");
    }
}

/**
 * @brief Main example task.
 *
 * Configures a step counter watermark of 1 (fires every 20 steps), then
 * polls the step count and activity every 500 ms, logging whenever either
 * value changes.
 *
 * The step counter, step detector, and step activity features are enabled
 * below before polling begins.  Their outputs are only meaningful after the
 * BMI270 feature engine has been enabled.
 *
 * @param pvParameters  Unused FreeRTOS task parameter.
 */
static void MainTask(void *pvParameters)
{
    ESP_LOGI(LOG_TAG, "BMI270 Example 3 — Step Counter");

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
     * Enable the step counter and step activity features in the BMI270
     * feature engine so their outputs are meaningful.
     */
    if (!Imu::EnableFeature(Imu::FEATURE_STEP_COUNTER))
    {
        ESP_LOGE(LOG_TAG, "Failed to enable step counter feature.");
    }

    if (!Imu::EnableFeature(Imu::FEATURE_STEP_ACTIVITY))
    {
        ESP_LOGE(LOG_TAG, "Failed to enable step activity feature.");
    }

    /*
     * Set the step counter watermark to 1 — an interrupt fires every
     * 20 steps (watermark × 20).
     */
    Imu::SetStepCountWatermark(1);

    /* Reset the step counter to start from zero. */
    Imu::ResetStepCount();

    ESP_LOGI(LOG_TAG, "Step counter reset.  Polling every %lu ms.", POLL_INTERVAL_MS);

    uint32_t previousStepCount = 0;
    uint8_t  previousActivity  = Imu::STEP_ACTIVITY_UNKNOWN;

    while (true)
    {
        uint32_t stepCount = 0;
        uint8_t  activity  = Imu::STEP_ACTIVITY_UNKNOWN;

        const bool countOk    = Imu::GetStepCount(stepCount);
        const bool activityOk = Imu::GetStepActivity(activity);

        if (!countOk || !activityOk)
        {
            ESP_LOGE(LOG_TAG, "Failed to read step data.");
            vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
            continue;
        }

        if (stepCount != previousStepCount || activity != previousActivity)
        {
            ESP_LOGI(LOG_TAG,
                     "Steps: %lu  |  Activity: %s",
                     stepCount, ActivityName(activity));

            previousStepCount = stepCount;
            previousActivity  = activity;
        }

        vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
    }
}

/**
 * @brief ESP-IDF application entry point.
 */
extern "C" void app_main(void)
{
    xTaskCreate(MainTask, "MainTask", 4096, nullptr, 5, nullptr);
}
