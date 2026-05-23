/*-----------------------------------------------------------------------------
 * File        : Imu.hpp
 * Description : Singleton class (Imu) for the BMI270 6-axis inertial
 *               measurement unit on the M5Stack Tab5.  Provides calibrated
 *               accelerometer and gyroscope readings, sensor configuration,
 *               step counting, wrist gesture recognition, interrupt status,
 *               and calibration over I2C, after uploading the required
 *               firmware configuration blob.
 * Author      : Mark Stevens
 * Copyright   : Copyright (c) 2026 Mark Stevens
 * Licence     : MIT — see LICENSE in the repository root for full terms.
 * Target      : M5Stack Tab5 (ESP32-P4)
 *---------------------------------------------------------------------------*/

#pragma once

#include <cstdint>
#include <driver/gpio.h>
#include <driver/i2c_master.h>

/**
 * @brief Singleton for the BMI270 6-axis IMU on the M5Stack Tab5.
 *
 * The BMI270 is accessed over the shared internal I2C bus (I2C_NUM_1,
 * SDA = GPIO_NUM_31, SCL = GPIO_NUM_32) at address 0x68 (SDO tied LOW).
 *
 * Initialisation writes the 8 KiB configuration blob required by the BMI270
 * firmware before enabling the accelerometer and gyroscope.  Once running,
 * the class provides single-shot 3-axis reads in physical units, output data
 * rate control, power mode selection, filter bandwidth control, advanced power
 * save mode, interrupt status, step counting, wrist gesture recognition,
 * accelerometer and gyroscope fast offset calibration, NVM programming, and
 * self-test.
 *
 * @note Initialise() must be called after display.init() because M5GFX
 *       creates the shared I2C bus handle that is reused here.
 *
 * @note Tab5 power-on note: if the device was powered down abruptly (without
 *       a proper shutdown) the IMU may fail to initialise.  Wait at least
 *       5 seconds after reconnecting power before calling Initialise().
 */
class Imu
{
public:
    // =========================================================================
    // Types
    // =========================================================================

    /**
     * @brief 3-axis floating-point vector.
     */
    struct Vector3
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    // =========================================================================
    // Accelerometer output data rate values for SetAccelerometerOutputDataRate().
    // =========================================================================

    /**
     * @brief Accelerometer ODR 0.78 Hz.
     */
    static constexpr uint8_t ACCEL_ODR_0_78HZ = 0x01;

    /**
     * @brief Accelerometer ODR 1.56 Hz.
     */
    static constexpr uint8_t ACCEL_ODR_1_56HZ = 0x02;

    /**
     * @brief Accelerometer ODR 3.12 Hz.
     */
    static constexpr uint8_t ACCEL_ODR_3_12HZ = 0x03;

    /**
     * @brief Accelerometer ODR 6.25 Hz.
     */
    static constexpr uint8_t ACCEL_ODR_6_25HZ = 0x04;

    /**
     * @brief Accelerometer ODR 12.5 Hz.
     */
    static constexpr uint8_t ACCEL_ODR_12_5HZ = 0x05;

    /**
     * @brief Accelerometer ODR 25 Hz.
     */
    static constexpr uint8_t ACCEL_ODR_25HZ = 0x06;

    /**
     * @brief Accelerometer ODR 50 Hz.
     */
    static constexpr uint8_t ACCEL_ODR_50HZ = 0x07;

    /**
     * @brief Accelerometer ODR 100 Hz (default).
     */
    static constexpr uint8_t ACCEL_ODR_100HZ = 0x08;

    /**
     * @brief Accelerometer ODR 200 Hz.
     */
    static constexpr uint8_t ACCEL_ODR_200HZ = 0x09;

    /**
     * @brief Accelerometer ODR 400 Hz.
     */
    static constexpr uint8_t ACCEL_ODR_400HZ = 0x0A;

    /**
     * @brief Accelerometer ODR 800 Hz.
     */
    static constexpr uint8_t ACCEL_ODR_800HZ = 0x0B;

    /**
     * @brief Accelerometer ODR 1600 Hz.
     */
    static constexpr uint8_t ACCEL_ODR_1600HZ = 0x0C;

    // =========================================================================
    // Gyroscope output data rate values for SetGyroscopeOutputDataRate().
    // =========================================================================

    /**
     * @brief Gyroscope ODR 25 Hz.
     */
    static constexpr uint8_t GYRO_ODR_25HZ = 0x06;

    /**
     * @brief Gyroscope ODR 50 Hz.
     */
    static constexpr uint8_t GYRO_ODR_50HZ = 0x07;

    /**
     * @brief Gyroscope ODR 100 Hz.
     */
    static constexpr uint8_t GYRO_ODR_100HZ = 0x08;

    /**
     * @brief Gyroscope ODR 200 Hz (default).
     */
    static constexpr uint8_t GYRO_ODR_200HZ = 0x09;

    /**
     * @brief Gyroscope ODR 400 Hz.
     */
    static constexpr uint8_t GYRO_ODR_400HZ = 0x0A;

    /**
     * @brief Gyroscope ODR 800 Hz.
     */
    static constexpr uint8_t GYRO_ODR_800HZ = 0x0B;

    /**
     * @brief Gyroscope ODR 1600 Hz.
     */
    static constexpr uint8_t GYRO_ODR_1600HZ = 0x0C;

    /**
     * @brief Gyroscope ODR 3200 Hz.
     */
    static constexpr uint8_t GYRO_ODR_3200HZ = 0x0D;

    // =========================================================================
    // Power / filter mode constants for SetAccelerometerPowerMode(),
    // SetGyroscopePowerMode(), and bandwidth setters.
    // =========================================================================

    /**
     * @brief Power-optimised filter mode (lower noise, higher power use).
     */
    static constexpr uint8_t POWER_OPT_MODE = 0x00;

    /**
     * @brief Performance-optimised filter mode (default).
     */
    static constexpr uint8_t PERF_OPT_MODE = 0x01;

    // =========================================================================
    // Accelerometer filter bandwidth values for
    // SetAccelerometerFilterBandwidth().
    // =========================================================================

    /**
     * @brief Accelerometer bandwidth: OSR4 / average-1.
     */
    static constexpr uint8_t ACCEL_BWP_OSR4_AVG1 = 0x00;

    /**
     * @brief Accelerometer bandwidth: OSR2 / average-2.
     */
    static constexpr uint8_t ACCEL_BWP_OSR2_AVG2 = 0x01;

    /**
     * @brief Accelerometer bandwidth: normal / average-4 (default).
     */
    static constexpr uint8_t ACCEL_BWP_NORMAL_AVG4 = 0x02;

    /**
     * @brief Accelerometer bandwidth: CIC / average-8.
     */
    static constexpr uint8_t ACCEL_BWP_CIC_AVG8 = 0x03;

    /**
     * @brief Accelerometer bandwidth: reserved / average-16.
     */
    static constexpr uint8_t ACCEL_BWP_AVG16 = 0x04;

    /**
     * @brief Accelerometer bandwidth: reserved / average-32.
     */
    static constexpr uint8_t ACCEL_BWP_AVG32 = 0x05;

    /**
     * @brief Accelerometer bandwidth: reserved / average-64.
     */
    static constexpr uint8_t ACCEL_BWP_AVG64 = 0x06;

    /**
     * @brief Accelerometer bandwidth: reserved / average-128.
     */
    static constexpr uint8_t ACCEL_BWP_AVG128 = 0x07;

    // =========================================================================
    // Gyroscope filter bandwidth values for SetGyroscopeFilterBandwidth().
    // =========================================================================

    /**
     * @brief Gyroscope bandwidth: OSR4 mode.
     */
    static constexpr uint8_t GYRO_BWP_OSR4 = 0x00;

    /**
     * @brief Gyroscope bandwidth: OSR2 mode.
     */
    static constexpr uint8_t GYRO_BWP_OSR2 = 0x01;

    /**
     * @brief Gyroscope bandwidth: normal mode (default).
     */
    static constexpr uint8_t GYRO_BWP_NORMAL = 0x02;

    /**
     * @brief Gyroscope bandwidth: CIC mode.
     */
    static constexpr uint8_t GYRO_BWP_CIC = 0x03;

    // =========================================================================
    // Gravity direction constants for PerformAccelerometerOffsetCalibration().
    // =========================================================================

    /**
     * @brief Gravity direction axis bit — X axis.
     */
    static constexpr uint8_t GRAVITY_AXIS_X = 0x01;

    /**
     * @brief Gravity direction axis bit — Y axis.
     */
    static constexpr uint8_t GRAVITY_AXIS_Y = 0x02;

    /**
     * @brief Gravity direction axis bit — Z axis.
     */
    static constexpr uint8_t GRAVITY_AXIS_Z = 0x04;

    /**
     * @brief Gravity direction sign bit — positive direction.
     */
    static constexpr uint8_t GRAVITY_DIRECTION_POSITIVE = 0x08;

    /**
     * @brief Gravity pointing in +X direction.
     */
    static constexpr uint8_t GRAVITY_POSITIVE_X = GRAVITY_AXIS_X | GRAVITY_DIRECTION_POSITIVE;

    /**
     * @brief Gravity pointing in +Y direction.
     */
    static constexpr uint8_t GRAVITY_POSITIVE_Y = GRAVITY_AXIS_Y | GRAVITY_DIRECTION_POSITIVE;

    /**
     * @brief Gravity pointing in +Z direction.
     */
    static constexpr uint8_t GRAVITY_POSITIVE_Z = GRAVITY_AXIS_Z | GRAVITY_DIRECTION_POSITIVE;

    /**
     * @brief Gravity pointing in −X direction.
     */
    static constexpr uint8_t GRAVITY_NEGATIVE_X = GRAVITY_AXIS_X;

    /**
     * @brief Gravity pointing in −Y direction.
     */
    static constexpr uint8_t GRAVITY_NEGATIVE_Y = GRAVITY_AXIS_Y;

    /**
     * @brief Gravity pointing in −Z direction.
     */
    static constexpr uint8_t GRAVITY_NEGATIVE_Z = GRAVITY_AXIS_Z;

    // =========================================================================
    // Step activity output constants (returned by GetStepActivity()).
    // =========================================================================

    /**
     * @brief Step activity: device is stationary.
     */
    static constexpr uint8_t STEP_ACTIVITY_STILL = 0x00;

    /**
     * @brief Step activity: device owner is walking.
     */
    static constexpr uint8_t STEP_ACTIVITY_WALKING = 0x01;

    /**
     * @brief Step activity: device owner is running.
     */
    static constexpr uint8_t STEP_ACTIVITY_RUNNING = 0x02;

    /**
     * @brief Step activity: unknown activity.
     */
    static constexpr uint8_t STEP_ACTIVITY_UNKNOWN = 0x03;

    // =========================================================================
    // Wrist gesture output constants (returned by GetWristGesture()).
    // =========================================================================

    /**
     * @brief Wrist gesture: unrecognised or no gesture.
     */
    static constexpr uint8_t WRIST_GESTURE_UNKNOWN = 0x00;

    /**
     * @brief Wrist gesture: arm moved downward (screen away from face).
     */
    static constexpr uint8_t WRIST_GESTURE_ARM_DOWN = 0x01;

    /**
     * @brief Wrist gesture: arm moved upward (screen toward face).
     */
    static constexpr uint8_t WRIST_GESTURE_ARM_UP = 0x02;

    /**
     * @brief Wrist gesture: shake / jiggle motion detected.
     */
    static constexpr uint8_t WRIST_GESTURE_SHAKE_JIGGLE = 0x03;

    /**
     * @brief Wrist gesture: flick-in motion detected.
     */
    static constexpr uint8_t WRIST_GESTURE_FLICK_IN = 0x04;

    /**
     * @brief Wrist gesture: flick-out motion detected.
     */
    static constexpr uint8_t WRIST_GESTURE_FLICK_OUT = 0x05;

    // =========================================================================
    // Static interface
    // =========================================================================

    static Imu *GetInstance();

    static Imu *Initialise();

    ~Imu();

    // =========================================================================
    // Sensor control
    // =========================================================================

    static bool Reset();

    static bool GetStatus(uint8_t &status);

    static bool IsInitialised();

    // =========================================================================
    // Sensor readings
    // =========================================================================

    static bool GetAcceleration(Vector3 &acceleration);

    static bool GetGyroscope(Vector3 &gyroscope);

    static bool GetTemperature(float &temperature);

    // =========================================================================
    // Interrupt status
    // =========================================================================

    static bool GetInterruptStatus(uint16_t &status);

    // =========================================================================
    // Accelerometer configuration
    // =========================================================================

    static bool SetAccelerometerOutputDataRate(uint8_t outputDataRate);

    static bool SetAccelerometerPowerMode(uint8_t filterMode);

    static bool SetAccelerometerFilterBandwidth(uint8_t bandwidthParameter);

    // =========================================================================
    // Gyroscope configuration
    // =========================================================================

    static bool SetGyroscopeOutputDataRate(uint8_t outputDataRate);

    static bool SetGyroscopePowerMode(uint8_t filterMode, uint8_t noiseMode);

    static bool SetGyroscopeFilterBandwidth(uint8_t bandwidthParameter);

    // =========================================================================
    // Power management
    // =========================================================================

    static bool EnableAdvancedPowerSave(bool enable);

    // =========================================================================
    // Step counter
    // =========================================================================

    static bool GetStepCount(uint32_t &count);

    static bool ResetStepCount();

    static bool SetStepCountWatermark(uint16_t watermark);

    static bool GetStepActivity(uint8_t &activity);

    // =========================================================================
    // Wrist gesture
    // =========================================================================

    static bool GetWristGesture(uint8_t &gesture);

    // =========================================================================
    // Calibration and NVM
    // =========================================================================

    static bool PerformAccelerometerOffsetCalibration(uint8_t gravityDirection);

    static bool PerformGyroscopeOffsetCalibration();

    static bool PerformSelfTest();

    static bool SaveNvm();

    // =========================================================================
    // Feature enable/disable
    // =========================================================================

    /**
     * @brief Feature identifier for significant-motion detection.
     */
    static constexpr uint8_t FEATURE_SIG_MOTION = 3;

    /**
     * @brief Feature identifier for any-motion detection.
     */
    static constexpr uint8_t FEATURE_ANY_MOTION = 4;

    /**
     * @brief Feature identifier for no-motion detection.
     */
    static constexpr uint8_t FEATURE_NO_MOTION = 5;

    /**
     * @brief Feature identifier for the step detector.
     */
    static constexpr uint8_t FEATURE_STEP_DETECTOR = 6;

    /**
     * @brief Feature identifier for the step counter.
     */
    static constexpr uint8_t FEATURE_STEP_COUNTER = 7;

    /**
     * @brief Feature identifier for step activity classification.
     */
    static constexpr uint8_t FEATURE_STEP_ACTIVITY = 8;

    /**
     * @brief Feature identifier for wrist gesture recognition.
     */
    static constexpr uint8_t FEATURE_WRIST_GESTURE = 19;

    /**
     * @brief Feature identifier for wrist wear wake-up.
     */
    static constexpr uint8_t FEATURE_WRIST_WEAR_WAKE_UP = 20;

    /**
     * @brief Enables a named BMI270 feature in the on-chip feature engine.
     *
     * Reads the appropriate feature page, sets the enable bit for the
     * requested feature, and writes the page back.
     *
     * @param feature  One of the FEATURE_* constants.
     * @return true on success, false on I2C error, unknown feature ID, or
     *         if not initialised.
     */
    static bool EnableFeature(uint8_t feature);

    /**
     * @brief Disables a named BMI270 feature in the on-chip feature engine.
     *
     * Reads the appropriate feature page, clears the enable bit for the
     * requested feature, and writes the page back.
     *
     * @param feature  One of the FEATURE_* constants.
     * @return true on success, false on I2C error, unknown feature ID, or
     *         if not initialised.
     */
    static bool DisableFeature(uint8_t feature);

    // =========================================================================
    // Axis remapping
    // =========================================================================

    /**
     * @brief Axis remap source: positive X axis.
     */
    static constexpr uint8_t AXIS_POS_X = 0x01;

    /**
     * @brief Axis remap source: negative X axis.
     */
    static constexpr uint8_t AXIS_NEG_X = 0x09;

    /**
     * @brief Axis remap source: positive Y axis.
     */
    static constexpr uint8_t AXIS_POS_Y = 0x02;

    /**
     * @brief Axis remap source: negative Y axis.
     */
    static constexpr uint8_t AXIS_NEG_Y = 0x0A;

    /**
     * @brief Axis remap source: positive Z axis.
     */
    static constexpr uint8_t AXIS_POS_Z = 0x04;

    /**
     * @brief Axis remap source: negative Z axis.
     */
    static constexpr uint8_t AXIS_NEG_Z = 0x0C;

    /**
     * @brief Remaps the output axes of the BMI270 accelerometer and
     *        gyroscope.
     *
     * Writes two bytes into feature page 1 at the axis-map location.  Each
     * source argument specifies which physical sensor axis is routed to that
     * logical axis, and whether it should be negated.  Use the AXIS_POS_*
     * and AXIS_NEG_* constants.
     *
     * Example — swap X and Y, leave Z positive:
     * @code
     *   Imu::RemapAxes(Imu::AXIS_POS_Y, Imu::AXIS_POS_X, Imu::AXIS_POS_Z);
     * @endcode
     *
     * @param xSource  Physical source axis for the logical X output.
     * @param ySource  Physical source axis for the logical Y output.
     * @param zSource  Physical source axis for the logical Z output.
     * @return true on success, false on I2C error or if not initialised.
     */
    static bool RemapAxes(uint8_t xSource, uint8_t ySource, uint8_t zSource);

    // =========================================================================
    // FIFO
    // =========================================================================

    /**
     * @brief FIFO configuration parameters for ConfigureFifo().
     */
    struct FifoConfig
    {
        /**
         * @brief Stop filling the FIFO once it is full (true) or overwrite
         *        oldest frames (false).
         */
        bool stopOnFull = false;

        /**
         * @brief Append a sensor-time frame to the FIFO after each data
         *        frame.
         */
        bool timeEnabled = false;

        /**
         * @brief Prefix each FIFO frame with a 1-byte header identifying the
         *        frame type.
         */
        bool headerEnabled = true;

        /**
         * @brief Store accelerometer data in the FIFO.
         */
        bool accelEnabled = false;

        /**
         * @brief Store gyroscope data in the FIFO.
         */
        bool gyroEnabled = false;

        /**
         * @brief FIFO watermark in bytes.  The watermark interrupt fires when
         *        the FIFO fill level reaches this value (0 = disabled).
         */
        uint16_t watermark = 0;

        /**
         * @brief Accelerometer down-sampling factor 0–7 where 0 means no
         *        down-sampling.
         */
        uint8_t accelDownSample = 0;

        /**
         * @brief Gyroscope down-sampling factor 0–7 where 0 means no
         *        down-sampling.
         */
        uint8_t gyroDownSample = 0;

        /**
         * @brief Use filtered accelerometer data in the FIFO (true) or
         *        pre-filtered / unfiltered data (false).
         */
        bool accelFilteredData = true;

        /**
         * @brief Use filtered gyroscope data in the FIFO (true) or
         *        pre-filtered / unfiltered data (false).
         */
        bool gyroFilteredData = true;
    };

    /**
     * @brief Configures the BMI270 FIFO according to the supplied parameters.
     *
     * Writes FIFO_CONFIG_0 (0x48), FIFO_CONFIG_1 (0x49), FIFO_DOWNS (0x45),
     * and the two watermark registers (0x46–0x47).
     *
     * @param config  Desired FIFO configuration.
     * @return true on success, false on I2C error or if not initialised.
     */
    static bool ConfigureFifo(const FifoConfig &config);

    /**
     * @brief Reads the current number of bytes waiting in the FIFO.
     *
     * @param length  Reference to receive the FIFO fill level in bytes.
     * @return true on success, false on I2C error or if not initialised.
     */
    static bool GetFifoLength(uint16_t &length);

    /**
     * @brief Reads up to @p length bytes from the FIFO into @p buffer.
     *
     * On entry @p length must hold the capacity of @p buffer.  On return it
     * is updated to the number of bytes actually read (the lesser of the
     * buffer capacity and the current FIFO fill level).
     *
     * @param buffer  Destination buffer for raw FIFO data.
     * @param length  In: buffer capacity in bytes.  Out: bytes read.
     * @return true on success, false on I2C error or if not initialised.
     */
    static bool ReadFifo(uint8_t *buffer, uint16_t &length);

    /**
     * @brief Discards all data currently in the FIFO by sending the FIFO
     *        flush command.
     *
     * @return true on success, false on I2C error or if not initialised.
     */
    static bool FlushFifo();

    // =========================================================================
    // Component retrim (CRT)
    // =========================================================================

    /**
     * @brief Performs the BMI270 gyroscope component retrim (CRT) procedure.
     *
     * Disables the gyroscope, triggers the G_TRIGGER_CMD command, and polls
     * the GYR_CRT_RUNNING bit until the procedure completes (up to 2 seconds).
     * The gyroscope is re-enabled after a successful retrim.
     *
     * CRT corrects manufacturing sensitivity variations and should be run
     * once at system start-up in a stable, vibration-free environment.
     *
     * @return true on success, false on timeout, I2C error, or if not
     *         initialised.
     */
    static bool PerformComponentRetrim();

    // =========================================================================
    // Auxiliary I2C
    // =========================================================================

    /**
     * @brief Auxiliary I2C interface configuration for ConfigureAux().
     */
    struct AuxConfig
    {
        /**
         * @brief 7-bit I2C address of the auxiliary sensor device.
         */
        uint8_t i2cAddress = 0;

        /**
         * @brief Output data rate for automatic reads.  Use a BMI2_AUX_ODR_*
         *        value; 0x08 gives 100 Hz.
         */
        uint8_t odr = 0x08;

        /**
         * @brief Enable manual mode for register-level read/write access.
         *        When false the sensor reads the auxiliary registers
         *        automatically at the configured ODR.
         */
        bool manualMode = false;

        /**
         * @brief Burst read length encoding for manual mode:
         *        0 = 1 byte, 1 = 2 bytes, 2 = 6 bytes, 3 = 8 bytes.
         */
        uint8_t burstReadLength = 0;
    };

    /**
     * @brief Configures the BMI270 auxiliary I2C interface.
     *
     * Enables the auxiliary interface, sets the I2C device address, selects
     * manual or automatic mode, sets the burst read length, and configures
     * the output data rate.
     *
     * @param config  Desired auxiliary interface configuration.
     * @return true on success, false on I2C error or if not initialised.
     */
    static bool ConfigureAux(const AuxConfig &config);

    /**
     * @brief Reads bytes from an auxiliary sensor register in manual mode.
     *
     * Writes the register address to the AUX_RD_ADDR register and reads the
     * result from the AUX data registers (0x04 onwards).  The auxiliary
     * interface must have been configured in manual mode via ConfigureAux().
     *
     * @param regAddr  Register address on the auxiliary sensor.
     * @param buffer   Destination buffer (must be at least @p length bytes).
     * @param length   Number of bytes to read (1–8).
     * @return true on success, false on I2C error or if not initialised.
     */
    static bool ReadAux(uint8_t regAddr, uint8_t *buffer, uint8_t length);

    /**
     * @brief Writes a single byte to an auxiliary sensor register in manual
     *        mode.
     *
     * Writes the data byte to AUX_WR_DATA and the register address to
     * AUX_WR_ADDR; the BMI270 forwards the write to the auxiliary sensor.
     *
     * @param regAddr  Register address on the auxiliary sensor.
     * @param data     Byte value to write.
     * @return true on success, false on I2C error or if not initialised.
     */
    static bool WriteAux(uint8_t regAddr, uint8_t data);

private:
    // =========================================================================
    // Hardware constants
    // =========================================================================

    /**
     * @brief BMI270 7-bit I2C address (SDO = LOW → 0x68).
     */
    static constexpr uint8_t I2C_ADDRESS = 0x68;

    /**
     * @brief I2C clock frequency for BMI270 communication.
     */
    static constexpr uint32_t I2C_FREQUENCY_HZ = 400000;

    /**
     * @brief Expected chip ID read from register 0x00.
     */
    static constexpr uint8_t CHIP_ID = 0x24;

    // =========================================================================
    // Register map
    // =========================================================================

    /**
     * @brief Chip ID register — should read CHIP_ID (0x24).
     */
    static constexpr uint8_t REG_CHIP_ID = 0x00;

    /**
     * @brief Sensor status register — data-ready flags for accel, gyro, aux.
     */
    static constexpr uint8_t REG_STATUS = 0x03;

    /**
     * @brief First byte of accelerometer data (X-axis LSB).
     */
    static constexpr uint8_t REG_ACC_X_LSB = 0x0C;

    /**
     * @brief First byte of gyroscope data (X-axis LSB).
     */
    static constexpr uint8_t REG_GYR_X_LSB = 0x12;

    /**
     * @brief Interrupt status register 0 — feature interrupt flags.
     */
    static constexpr uint8_t REG_INT_STATUS_0 = 0x1C;

    /**
     * @brief Interrupt status register 1 — data-ready / FIFO interrupt flags.
     */
    static constexpr uint8_t REG_INT_STATUS_1 = 0x1D;

    /**
     * @brief Internal status register — bit 0 set when firmware init complete.
     */
    static constexpr uint8_t REG_INTERNAL_STATUS = 0x21;

    /**
     * @brief Temperature data LSB register.
     */
    static constexpr uint8_t REG_TEMPERATURE_LSB = 0x22;

    /**
     * @brief Feature page selector register.
     */
    static constexpr uint8_t REG_FEAT_PAGE = 0x2F;

    /**
     * @brief Feature register base address — 16-byte window onto the selected
     *        feature page.
     */
    static constexpr uint8_t REG_FEATURES = 0x30;

    /**
     * @brief Accelerometer configuration register (ODR, bandwidth, filter).
     */
    static constexpr uint8_t REG_ACC_CONF = 0x40;

    /**
     * @brief Accelerometer range register.
     */
    static constexpr uint8_t REG_ACC_RANGE = 0x41;

    /**
     * @brief Gyroscope configuration register (ODR, bandwidth, filter, noise).
     */
    static constexpr uint8_t REG_GYR_CONF = 0x42;

    /**
     * @brief Gyroscope range register.
     */
    static constexpr uint8_t REG_GYR_RANGE = 0x43;

    /**
     * @brief Initialisation control register.
     */
    static constexpr uint8_t REG_INIT_CTRL = 0x59;

    /**
     * @brief Low byte of the config-file burst-write address pointer.
     */
    static constexpr uint8_t REG_INIT_ADDR_0 = 0x5B;

    /**
     * @brief High byte of the config-file burst-write address pointer.
     */
    static constexpr uint8_t REG_INIT_ADDR_1 = 0x5C;

    /**
     * @brief Data register used to stream the configuration blob.
     */
    static constexpr uint8_t REG_INIT_DATA = 0x5E;

    /**
     * @brief NVM configuration register — NVM_PROG_EN bit controls programming.
     */
    static constexpr uint8_t REG_NVM_CONF = 0x6A;

    /**
     * @brief Accelerometer self-test register.
     */
    static constexpr uint8_t REG_ACC_SELF_TEST = 0x6D;

    /**
     * @brief Power configuration register.
     */
    static constexpr uint8_t REG_PWR_CONF = 0x7C;

    /**
     * @brief Power control register — enables accelerometer and gyroscope.
     */
    static constexpr uint8_t REG_PWR_CTRL = 0x7D;

    /**
     * @brief Command register — write 0xB6 for a soft reset, 0xA0 to program NVM.
     */
    static constexpr uint8_t REG_CMD = 0x7E;

    // =========================================================================
    // Register bit-field masks and positions — ACC_CONF (0x40)
    // =========================================================================

    /**
     * @brief ACC_CONF bits [3:0] — output data rate.
     */
    static constexpr uint8_t ACC_CONF_ODR_MASK = 0x0F;

    /**
     * @brief ACC_CONF bits [6:4] — filter bandwidth parameter.
     */
    static constexpr uint8_t ACC_CONF_BWP_MASK = 0x70;

    /**
     * @brief Bit position of the bandwidth field within ACC_CONF.
     */
    static constexpr uint8_t ACC_CONF_BWP_POS = 4;

    /**
     * @brief ACC_CONF bit [7] — filter performance mode (0 = power, 1 = perf).
     */
    static constexpr uint8_t ACC_CONF_FILTER_PERF_MASK = 0x80;

    // =========================================================================
    // Register bit-field masks and positions — GYR_CONF (0x42)
    // =========================================================================

    /**
     * @brief GYR_CONF bits [3:0] — output data rate.
     */
    static constexpr uint8_t GYR_CONF_ODR_MASK = 0x0F;

    /**
     * @brief GYR_CONF bits [5:4] — filter bandwidth parameter.
     */
    static constexpr uint8_t GYR_CONF_BWP_MASK = 0x30;

    /**
     * @brief Bit position of the bandwidth field within GYR_CONF.
     */
    static constexpr uint8_t GYR_CONF_BWP_POS = 4;

    /**
     * @brief GYR_CONF bit [6] — noise performance mode (0 = power, 1 = perf).
     */
    static constexpr uint8_t GYR_CONF_NOISE_PERF_MASK = 0x40;

    /**
     * @brief GYR_CONF bit [7] — filter performance mode (0 = power, 1 = perf).
     */
    static constexpr uint8_t GYR_CONF_FILTER_PERF_MASK = 0x80;

    // =========================================================================
    // Register bit-field masks — PWR_CONF (0x7C)
    // =========================================================================

    /**
     * @brief PWR_CONF bit [0] — advanced power save enable.
     */
    static constexpr uint8_t PWR_CONF_ADV_POWER_SAVE_MASK = 0x01;

    // =========================================================================
    // Sensor scaling
    // =========================================================================

    /**
     * @brief Accelerometer sensitivity in LSB per g for ±8 g range.
     */
    static constexpr float ACC_SENSITIVITY_LSB_PER_G = 4096.0f;

    /**
     * @brief Gyroscope sensitivity in LSB per degree/s for ±2000 dps range.
     */
    static constexpr float GYR_SENSITIVITY_LSB_PER_DPS = 16.4f;

    /**
     * @brief Number of bytes per axis in the raw sensor data burst.
     */
    static constexpr uint8_t BYTES_PER_AXIS = 2;

    /**
     * @brief Total bytes in one 3-axis raw sensor read (X, Y, Z).
     */
    static constexpr uint8_t AXIS_DATA_LENGTH = BYTES_PER_AXIS * 3;

    /**
     * @brief Chunk size for configuration blob uploads.
     *
     * Must not exceed the ESP32 I2C hardware FIFO depth (32 bytes).  Larger
     * values cause the driver to refill the FIFO mid-transaction; any latency
     * in that refill corrupts the config stream and the BMI270 reports
     * INTERNAL_STATUS = 0x02 (INIT_ERR).  The SparkFun BMI270 Arduino driver
     * independently chose 32 for the same reason.
     */
    static constexpr size_t CONFIG_CHUNK_SIZE = 32;

    // =========================================================================
    // Feature page constants
    // =========================================================================

    /**
     * @brief Number of bytes in one feature page window.
     */
    static constexpr uint8_t FEATURE_PAGE_SIZE = 16;

    /**
     * @brief Page number that holds the sensor output feature data.
     */
    static constexpr uint8_t FEATURE_PAGE_OUTPUTS = 0;

    /**
     * @brief Page number that holds the step counter input configuration.
     */
    static constexpr uint8_t FEATURE_PAGE_STEP_COUNT_CONFIG = 6;

    /**
     * @brief Byte offset within the output page for the step counter value
     *        (4-byte little-endian uint32_t).
     */
    static constexpr uint8_t STEP_COUNT_OUT_OFFSET = 0x00;

    /**
     * @brief Byte offset within the output page for the step activity byte.
     */
    static constexpr uint8_t STEP_ACTIVITY_OUT_OFFSET = 0x04;

    /**
     * @brief Byte offset within the output page for the wrist gesture byte.
     */
    static constexpr uint8_t WRIST_GESTURE_OUT_OFFSET = 0x06;

    /**
     * @brief Byte offset within the step-count config page for the watermark
     *        and reset-counter word.
     */
    static constexpr uint8_t STEP_COUNT_CONFIG_OFFSET = 0x02;

    /**
     * @brief Mask for the 10-bit watermark field in the step-count config word.
     */
    static constexpr uint16_t STEP_COUNT_WATERMARK_MASK = 0x03FF;

    /**
     * @brief Bit mask for the reset-counter bit in the step-count config word.
     */
    static constexpr uint16_t STEP_COUNT_RESET_MASK = 0x0400;

    // =========================================================================
    // Feature enable support
    // =========================================================================

    /**
     * @brief Lookup entry describing where a feature's enable bit lives in
     *        the BMI270 feature page RAM.
     */
    struct FeatureEnableInfo
    {
        uint8_t page;      /**< @brief Feature page number (0–7). */
        uint8_t byteIndex; /**< @brief Byte offset within the 16-byte page window. */
        uint8_t mask;      /**< @brief Bitmask of the enable bit within that byte. */
    };

    /**
     * @brief Sets or clears the feature-enable bit for the specified feature.
     *
     * @param feature  One of the FEATURE_* constants.
     * @param enable   true to enable, false to disable.
     * @return true on success, false on I2C error or unknown feature ID.
     */
    static bool SetFeatureEnabled(uint8_t feature, bool enable);

    // =========================================================================
    // FIFO register constants
    // =========================================================================

    /**
     * @brief FIFO byte-count low register.
     */
    static constexpr uint8_t REG_FIFO_LENGTH_0 = 0x24;

    /**
     * @brief FIFO byte-count high register (14-bit count: bits [5:0] valid).
     */
    static constexpr uint8_t REG_FIFO_LENGTH_1 = 0x25;

    /**
     * @brief FIFO data read register.
     */
    static constexpr uint8_t REG_FIFO_DATA = 0x26;

    /**
     * @brief FIFO down-sampling register for accelerometer and gyroscope.
     */
    static constexpr uint8_t REG_FIFO_DOWNS = 0x45;

    /**
     * @brief FIFO watermark threshold low byte.
     */
    static constexpr uint8_t REG_FIFO_WTM_0 = 0x46;

    /**
     * @brief FIFO watermark threshold high byte.
     */
    static constexpr uint8_t REG_FIFO_WTM_1 = 0x47;

    /**
     * @brief FIFO configuration register 0 (stop-on-full, time-enable).
     */
    static constexpr uint8_t REG_FIFO_CONFIG_0 = 0x48;

    /**
     * @brief FIFO configuration register 1 (header, accel, gyro enable).
     */
    static constexpr uint8_t REG_FIFO_CONFIG_1 = 0x49;

    static constexpr uint8_t  FIFO_STOP_ON_FULL_MASK = 0x01;
    static constexpr uint8_t  FIFO_TIME_EN_MASK      = 0x02;
    static constexpr uint8_t  FIFO_HEADER_EN_MASK    = 0x10;
    static constexpr uint8_t  FIFO_ACC_EN_MASK       = 0x40;
    static constexpr uint8_t  FIFO_GYR_EN_MASK       = 0x80;
    static constexpr uint8_t  FIFO_GYR_DOWNS_MASK    = 0x07;
    static constexpr uint8_t  FIFO_GYR_FILT_MASK     = 0x08;
    static constexpr uint8_t  FIFO_ACC_DOWNS_MASK    = 0x70;
    static constexpr uint8_t  FIFO_ACC_DOWNS_POS     = 4;
    static constexpr uint8_t  FIFO_ACC_FILT_MASK     = 0x80;
    static constexpr uint8_t  FIFO_FLUSH_CMD         = 0xB0;

    // =========================================================================
    // Axis remap constants
    // =========================================================================

    static constexpr uint8_t AXIS_MAP_PAGE       = 1;
    static constexpr uint8_t AXIS_MAP_START_ADDR = 0x04;
    static constexpr uint8_t AXIS_X_MASK         = 0x03;
    static constexpr uint8_t AXIS_X_SIGN_MASK    = 0x04;
    static constexpr uint8_t AXIS_X_SIGN_POS     = 2;
    static constexpr uint8_t AXIS_Y_MASK         = 0x18;
    static constexpr uint8_t AXIS_Y_POS          = 3;
    static constexpr uint8_t AXIS_Y_SIGN_MASK    = 0x20;
    static constexpr uint8_t AXIS_Y_SIGN_POS     = 5;
    static constexpr uint8_t AXIS_Z_MASK         = 0xC0;
    static constexpr uint8_t AXIS_Z_POS          = 6;
    static constexpr uint8_t AXIS_Z_SIGN_MASK    = 0x01;
    static constexpr uint8_t AXIS_SOURCE_MASK    = 0x07;
    static constexpr uint8_t AXIS_SIGN_BIT       = 0x08;

    // =========================================================================
    // Component retrim register constants
    // =========================================================================

    /**
     * @brief Gyroscope CRT configuration register — bit 2 is GYR_CRT_RUNNING.
     */
    static constexpr uint8_t REG_GYR_CRT_CONF     = 0x69;

    /**
     * @brief Mask for the GYR_CRT_RUNNING bit in REG_GYR_CRT_CONF.
     */
    static constexpr uint8_t GYR_CRT_RUNNING_MASK = 0x04;

    /**
     * @brief Command value for triggering the G_TRIGGER (CRT) operation.
     */
    static constexpr uint8_t G_TRIGGER_CMD        = 0x02;

    // =========================================================================
    // Auxiliary I2C register constants
    // =========================================================================

    /**
     * @brief First AUX data output register (8 bytes, 0x04–0x0B).
     */
    static constexpr uint8_t REG_AUX_X_LSB          = 0x04;

    /**
     * @brief AUX interface ODR and read-offset configuration register.
     */
    static constexpr uint8_t REG_AUX_CONF           = 0x44;

    /**
     * @brief AUX sensor I2C device address register (7-bit addr in bits[7:1]).
     */
    static constexpr uint8_t REG_AUX_DEV_ID         = 0x4B;

    /**
     * @brief AUX interface configuration register (manual mode, burst length).
     */
    static constexpr uint8_t REG_AUX_IF_CONF        = 0x4C;

    /**
     * @brief AUX read address register — write the target sensor register here
     *        to trigger a manual read.
     */
    static constexpr uint8_t REG_AUX_RD_ADDR        = 0x4D;

    /**
     * @brief AUX write address register.
     */
    static constexpr uint8_t REG_AUX_WR_ADDR        = 0x4E;

    /**
     * @brief AUX write data register.
     */
    static constexpr uint8_t REG_AUX_WR_DATA        = 0x4F;

    /**
     * @brief Interface configuration register (AUX enable, OIS, SPI3 mode).
     */
    static constexpr uint8_t REG_IF_CONF            = 0x6B;

    static constexpr uint8_t AUX_IF_EN_MASK         = 0x20;
    static constexpr uint8_t AUX_SET_I2C_ADDR_MASK  = 0xFE;
    static constexpr uint8_t AUX_SET_I2C_ADDR_POS   = 1;
    static constexpr uint8_t AUX_MAN_MODE_EN_MASK   = 0x80;
    static constexpr uint8_t AUX_MAN_READ_BURST_MASK = 0x0C;
    static constexpr uint8_t AUX_MAN_READ_BURST_POS = 2;
    static constexpr uint8_t AUX_READ_BURST_MASK    = 0x03;
    static constexpr uint8_t AUX_ODR_MASK           = 0x0F;
    static constexpr uint8_t AUX_BUSY_MASK          = 0x04;
    static constexpr uint8_t AUX_NUM_BYTES          = 8;

    // =========================================================================
    // Constructor and helpers
    // =========================================================================

    Imu();

    static bool WriteRegister(uint8_t registerAddress, uint8_t value);

    static bool ReadRegisters(uint8_t registerAddress, uint8_t *buffer, size_t length);

    static bool ModifyRegister(uint8_t registerAddress, uint8_t mask, uint8_t value);

    static bool ReadFeaturePage(uint8_t page, uint8_t *buffer);

    static bool WriteFeaturePage(uint8_t page, const uint8_t *buffer);

    static bool UploadConfigFile();

    static bool PerformStartup();

    // =========================================================================
    // Members
    // =========================================================================

    /**
     * @brief The single instance of this class.
     */
    static Imu *_instance;

    /**
     * @brief I2C master bus handle (borrowed from M5GFX / display.init()).
     */
    static i2c_master_bus_handle_t _busHandle;

    /**
     * @brief I2C device handle for the BMI270.
     */
    static i2c_master_dev_handle_t _deviceHandle;

    /**
     * @brief True once the sensor firmware has been loaded and the axes enabled.
     */
    static bool _initialised;
};
