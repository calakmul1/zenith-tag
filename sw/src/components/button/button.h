/**
 * @file button.h
 * @brief Button input interface.
 *
 * This module provides a simple interface for reading the state of the
 * user button connected to the system. It includes basic software
 * debouncing to ensure reliable detection of button presses.
 *
 * The button is assumed to be connected to a GPIO pin configured as an
 * input and wired as **active-low** (pressed = logic 0).
 */

#ifndef COMPONENTS_BUTTON_BUTTON_H_
#define COMPONENTS_BUTTON_BUTTON_H_

/**
 * @brief Checks whether the user button has been pressed with basic debounce.
 *
 * This function reads the state of the button connected to `PIO0_1`. The input
 * is assumed to be **active-low**, so the GPIO value is inverted when read.
 *
 * If a press is detected, the function performs a short delay and
 * reads the button state again to filter out mechanical bounce. Only if the
 * button is still pressed after this delay will the press be considered valid.
 *
 * @note This function performs a blocking delay using
 *       `Chip_Clock_System_BusyWait_ms` for debouncing.
 *
 * @return
 * - `true`  if a valid button press is detected.
 * - `false` if the button is not pressed or the signal was caused by bounce.
 */
bool Button_Check_Pressed(void);

#endif /* COMPONENTS_BUTTON_BUTTON_H_ */
