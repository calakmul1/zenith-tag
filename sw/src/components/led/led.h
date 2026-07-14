#ifndef COMPONENTS_LED_LED_H_
#define COMPONENTS_LED_LED_H_

/**
 * @file led.h
 * @brief LED control interface.
 */

/**
 * @brief Toggle the state of the LED.
 *
 * Inverts the current output state of the LED GPIO pin. If the LED is
 * currently on, it will be turned off, and vice versa.
 */
void LED_Toggle(void);

/**
 * @brief Turn the LED on.
 *
 * Sets the LED GPIO pin to a high output state, enabling the LED.
 */
void LED_On(void);

/**
 * @brief Turn the LED off.
 *
 * Sets the LED GPIO pin to a low output state, ensuring the LED is disabled.
 */
void LED_Off(void);

/**
 * @brief Signal a fatal error using a blinking LED pattern.
 *
 * Repeatedly toggles the LED with a fixed delay and does not return.
 */
void LED_Error(void);

#endif /* COMPONENTS_LED_LED_H_ */
