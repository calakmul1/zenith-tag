/**
 * @file nfc.c
 * @brief NFC message handling for flight data logging.
 *
 * Provides functions to store flight height or error messages as
 * NFC NDEF text records in shared memory. Implements a lightweight
 * text encoder compatible with NFC Forum Type 2 Tag format.
 *
 * @note Requires the NFC peripheral and shared memory to be initialized.
 * @see NFC_Store_Flight_Height()
 * @see NFC_Store_Error()
 */
#ifndef COMPONENTS_NFC_NFC_H_
#define COMPONENTS_NFC_NFC_H_

/**
 * @brief Stores the measured flight height as a URI-encoded NFC record.
 *
 * This function clears the NFC shared memory and writes a URI record pointing to
 * the Zenith tracking service with the flight height as a query parameter.
 *
 * The resulting NFC message encodes the URI:
 * ```
 * https://www.<uri>?height=<value>
 * ```
 *
 * The URI is stored as an NDEF URI record (well-known type 'U') with the
 * "https://www." prefix compressed into a single byte (0x02) for efficiency.
 *
 * @param[in] height  The measured flight height in meters (floating-point value).
 *
 * @retval true   The URI record was successfully written to NFC shared memory.
 * @retval false  The write failed, the URI exceeded size limits, or the height value
 *                could not be converted to a string.
 *
 * @note
 * - The NFC shared memory is cleared before writing the new record.
 * - The height value is converted to a string with two decimal places.
 * - The total message is aligned to 4-byte boundaries for NFC shared memory word alignment.
 * - On failure, the LED error indicator is triggered via @ref LED_Error().
 */
bool NFC_Store_Flight_Height(float height);

#endif /* COMPONENTS_NFC_NFC_H_ */
