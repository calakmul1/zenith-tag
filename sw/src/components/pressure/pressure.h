/**
 * @file pressure.c
 * @brief Driver for the Bosch BMP388 pressure and temperature sensor.
 *
 * This module provides functions for initializing, configuring, and reading
 * data from the Bosch BMP388 digital barometric pressure sensor over I²C.
 * It implements sensor detection, calibration data loading, measurement control,
 * and temperature/pressure compensation according to the BMP388 datasheet.
 *
 * ### Key Features
 * - I²C communication with the BMP388 (address 0x77)
 * - Automatic loading and scaling of factory calibration coefficients
 * - Compensated pressure and temperature readings
 * - Altitude calculation from pressure differences
 * - Configurable over-sampling, data rate, and IIR filtering
 *
 * ### Functions
 * - `Pressure_Init()` — Initialize and configure the BMP388
 * - `Pressure_Start_Read()` / `Pressure_End_Read()` — Control sensor power modes
 * - `Pressure_Read()` — Retrieve compensated pressure and temperature
 * - `Pressure_Calculate_Height()` — Compute altitude difference from two pressures
 *
 * @note This implementation is based on the Bosch BMP388 datasheet
 *       and follows recommended configuration for drone applications.
 */
#ifndef COMPONENTS_PRESSURE_PRESSURE_H_
#define COMPONENTS_PRESSURE_PRESSURE_H_

#include <stdbool.h>

/**
 * @brief Initializes the BMP388 pressure sensor at system startup.
 *
 * This function performs the full initialization sequence required for
 * the BMP388 sensor to operate correctly. It executes the following steps:
 *  1. Detects the sensor on the I2C bus and verifies its chip ID.
 *  2. Reads and converts the factory calibration coefficients.
 *  3. Configures oversampling, output data rate, and IIR filter parameters.
 *
 * Each step must succeed before proceeding to the next. If any step fails,
 * the function aborts initialization and returns `false`.
 *
 * @note After successful initialization, the sensor remains in sleep mode.
 *       To begin measurements, call `Pressure_Start_Read()`.
 *
 * @return
 * - `true`  if the sensor was detected, calibrated, and configured successfully.
 * - `false` if any stage of initialization failed.
 */
bool Pressure_Init();

/**
 * @brief Disables the BMP388 pressure and temperature sensor and puts the sensor into sleep mode.
 *
 * This function writes to the control register (0x1B) of the BMP388 to disable both
 * the pressure and temperature sensors and transitions the device into low-power sleep mode.
 * It then verifies success by reading the error register (0x02).
 *
 * @note The configuration value 0x00 disables both pressure and temperature measurements
 *       and sets the BMP388 to sleep mode, as specified in section 4.3.16 of the BMP388 datasheet.
 *
 * @retval true   Command was successfully sent and no error was reported.
 * @retval false  I2C write failed or the sensor reported an error.
 */
bool Pressure_End_Read();

/**
 * @brief Enables the BMP388 pressure and temperature sensors and sets the device to normal mode.
 *
 * This function writes to the control register (0x1B) of the BMP388 to enable both
 * the pressure and temperature sensors and to switch the sensor into normal operation mode.
 * It then verifies the command by checking the error register (0x02).
 *
 * @note The configuration value 0x33 enables both pressure and temperature measurements
 *       in normal mode, as specified in section 4.3.16 of the BMP388 datasheet.
 *
 * @retval true   Sensor was detected, calibrated, and configured successfully.
 * @retval false  One or more initialization steps failed.
 */
bool Pressure_Start_Read();

/**
 * @brief Reads the pressure and temperature from the BMP388 pressure sensor.
 *
 * This function reads raw pressure and temperature data from the BMP388 over I2C,
 * performs compensation using calibration data, and returns the compensated values
 * via the provided pointers.
 *
 * @param[out] pressure     Pointer to a float where the compensated pressure (in Pascals) will be
 * stored.
 * @param[out] temperature  Pointer to a float where the compensated temperature (in °C) will be
 * stored.
 *
 * @retval true   Read and compensation were successful.
 * @retval false  I2C communication failed or invalid data was received.
 */
bool Pressure_Read(float *pressure, float *temperature);

/**
 * @brief Calculate altitude difference (Δh) from two pressure readings.
 *
 * This function computes the height difference in meters between two pressure
 * measurements using the barometric formula:
 *
 *      Δh = 44330 × (1 - (p_min / p_max) ^ 0.1903)
 *
 * The pressures must be provided in Pascals (Pa). The function returns a positive
 * height difference when the second measurement corresponds to a higher pressure
 * (lower altitude) and the first measurement corresponds to a lower pressure
 * (higher altitude).
 *
 * @param pressure_min  The lower pressure value in Pascals (Pa), typically measured at higher
 * altitude.
 * @param pressure_max  The higher pressure value in Pascals (Pa), typically measured at lower
 * altitude.
 *
 * @retval Δh  Height difference in meters (float).
 * @retval 0.0f  Returned if inputs are invalid or a math error occurs.
 */
float Pressure_Calculate_Height(float pressure_min, float pressure_max);

#endif /* COMPONENTS_PRESSURE_PRESSURE_H_ */
