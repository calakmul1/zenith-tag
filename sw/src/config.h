/**
 * @file config.h
 * @brief Configuration constants for sensor measurement timing and flight control.
 *
 * Defines timing intervals and measurement counts used throughout the system.
 * Adjust these values to control sampling rates and measurement frequency
 * during different operational phases.
 */

#ifndef CONFIG_H_
#define CONFIG_H_

/**
 * @brief Software debounce interval for the button.
 */
#define DEBOUNCE_INTERVAL_MS (5)

/**
 * @brief LED blinking interval while in measurement mode.
 */
#define LED_TOGGLE_INTERVAL_MS (500)

/**
 * @brief Time interval between pressure measurements in milliseconds.
 */
#define MEASUREMENT_INTERVAL_MS (20)

/**
 * @brief Number of measurements to take before take-off for baseline calibration.
 */
#define PRE_TAKEOFF_MEASUREMENTS (10)

/**
 * @brief Base URL of the web-based GUI.
 *
 * Replace this value during the build with the production URL.
 */
#define GUI_BASE_URL "https://www.example.invalid"

#endif /* CONFIG_H_ */
