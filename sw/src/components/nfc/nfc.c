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
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib_chip_nss/inc/chip.h"

#include "components/led/led.h"
#include "config.h"

/** @brief Maximum size of an NFC message in bytes. */
#define NFC_MAX_MESSAGE_SIZE 128

static bool NFC_Store_String(const char *body) __attribute__((unused));
static bool NFC_Store_URI(const char *base_uri, const char *param, float value);
static char *NFC_Convert_Float_String(float value, char *buffer, uint8_t precision);

bool NFC_Store_Flight_Height(float height)
{
    memset((void *)NFC_SHARED_MEM_START, 0x00, NFC_SHARED_MEM_BYTE_SIZE);

    bool success = NFC_Store_URI(GUI_BASE_URL, "height", height);

    if (success)
    {
        // Success
    }
    else
    {
        LED_Error();
        return false;
    }
    return true;
}

/**
 * @brief Writes a text payload to the NFC shared memory as an NDEF message.
 *
 * This function constructs a valid NFC NDEF text record from the provided string
 * and stores it in the NFC shared memory region. The record is encoded according
 * to the NFC Forum Type 2 Tag specification, using a well-known type (`'T'` for Text).
 *
 * The resulting memory layout:
 * ```
 * +------+-------------------+
 * | 0x03 | NDEF TLV Tag      |
 * | len  | Payload length    |
 * | 0xD1 | Record header     |
 * | 0x01 | Type length (T)   |
 * | ...  | Payload bytes     |
 * | 0xFE | Terminator TLV    |
 * +------+-------------------+
 * ```
 *
 * @param[in] body  Null-terminated text string to be written as the NFC payload.
 *
 * @retval true   The NDEF message was successfully constructed and written.
 * @retval false  The input was invalid, exceeded memory limits, or the NFC write failed.
 *
 * @note
 * - The maximum allowed payload length is limited by `NFC_MAX_MESSAGE_SIZE` minus 7 bytes
 *   of NDEF and TLV overhead.
 * - The total message length is aligned to 4 bytes to match the NFC shared memory word alignment.
 * - Uses the `Chip_NFC_WordWrite()` API to perform the actual write.
 */
static bool NFC_Store_String(const char *body)
{
    if (body == NULL)
    {
        LED_Error();
        return false;
    }

    size_t payload_len = strlen(body);

    // Minimum overhead: TLV (2) + header (3) + type (1) + terminator (1) = 7 bytes
    if (payload_len > (NFC_MAX_MESSAGE_SIZE - 7))
    {
        LED_Error();
        return false;
    }

    __attribute__((aligned(4))) uint8_t message[NFC_MAX_MESSAGE_SIZE];

    size_t idx = 0;
    message[idx++] = 0x03;                           // NDEF TLV
    message[idx++] = (uint8_t)(3 + 1 + payload_len); // record header (3) + type (1) + payload
    message[idx++] = 0xD1;                 // Record header: MB=1, ME=1, TNF=1 (Well-known)
    message[idx++] = 0x01;                 // Type length
    message[idx++] = (uint8_t)payload_len; // Payload length
    message[idx++] = 'T';                  // Type: 'T' (Text)
    memcpy(&message[idx], body, payload_len);
    idx += payload_len;
    message[idx++] = 0xFE; // Terminator TLV

    // Align total length to 4 bytes
    size_t len = (idx + 3U) & ~3U;

    if (len > NFC_SHARED_MEM_BYTE_SIZE)
    {
        LED_Error();
        return false;
    }

    bool success = Chip_NFC_WordWrite(
        NSS_NFC, (uint32_t *)NFC_SHARED_MEM_START, (uint32_t *)message, (int)len / 4);

    if (success)
    {
        // Success
    }
    else
    {
        LED_Error();
        return false;
    }

    return true;
}

/**
 * @brief Encodes and stores a parameterized URI as an NDEF record in NFC shared memory.
 *
 * This function constructs a complete URI by appending a query parameter with a
 * numeric value to the provided base URI, then encodes it as an NDEF URI record
 * (well-known type 'U') and writes it to NFC shared memory.
 *
 * The resulting URI has the form:
 * ```
 * <base_uri>?<param>=<value>
 * ```
 * where the value is converted to a string with 2 decimal places.
 *
 * The NDEF encoding uses URI compression: the prefix "https://www." is replaced
 * with the single byte 0x02 to reduce message size.
 *
 * The resulting memory layout:
 * ```
 * +------+-------------------+
 * | 0x03 | NDEF TLV Tag      |
 * | len  | Payload length    |
 * | 0xD1 | Record header     |
 * | 0x01 | Type length (U)   |
 * | ...  | Payload length    |
 * | 'U'  | Type: 'U' (URI)   |
 * | 0x02 | URI prefix        |
 * | ...  | URI body bytes    |
 * | 0xFE | Terminator TLV    |
 * +------+-------------------+
 * ```
 *
 * @param[in] base_uri  The base URI, which must start with "https://www.".
 * @param[in] param     The parameter name (e.g., "height", "temperature").
 *                      Must be a valid URI query parameter name.
 * @param[in] value     The floating-point value to encode (converted to 2 decimal places).
 *
 * @retval true   The URI record was successfully encoded and written to NFC shared memory.
 * @retval false  The operation failed due to:
 *                - NULL input pointers
 *                - base_uri not starting with "https://www."
 *                - Resulting URI exceeding `NFC_MAX_MESSAGE_SIZE` limits
 *                - Hardware write failure
 *
 * @note
 * - The function does NOT clear NFC shared memory; the caller must do this beforehand.
 * - On failure, @ref LED_Error() is triggered.
 * - The total NDEF message is aligned to 4-byte boundaries for NFC shared memory word alignment.
 */
static bool NFC_Store_URI(const char *base_uri, const char *param, float value)
{
    if ((base_uri == NULL) || (param == NULL))
    {
        LED_Error();
        return false;
    }

    // Assumes URI starts with "https://www."
    static const char prefix[] = "https://";
    if (strncmp(base_uri, prefix, sizeof(prefix) - 1) != 0)
    {
        LED_Error();
        return false;
    }

    const char *uri_body = base_uri + (sizeof(prefix) - 1);

    char value_str[32];
    NFC_Convert_Float_String(value, value_str, 2);

    char body[NFC_MAX_MESSAGE_SIZE];
    body[0] = '\0';

    size_t uri_body_len = strlen(uri_body);
    size_t param_len = strlen(param);
    size_t value_len = strlen(value_str);

    // Build URI body: "<uri_body>?<param>=<value>"
    if ((uri_body_len + 1U + param_len + 1U + value_len + 1U) > sizeof(body))
    {
        LED_Error();
        return false;
    }

    strcat(body, uri_body);
    strcat(body, "?");
    strcat(body, param);
    strcat(body, "=");
    strcat(body, value_str);

    size_t body_len = strlen(body);
    size_t payload_len = 1U + body_len; // URI prefix byte + URI body

    // Minimum overhead: TLV (2) + header (3) + type (1) + payload prefix (1) + terminator (1) = 8
    // bytes
    if (payload_len > (NFC_MAX_MESSAGE_SIZE - 8))
    {
        LED_Error();
        return false;
    }

    __attribute__((aligned(4))) uint8_t message[NFC_MAX_MESSAGE_SIZE];

    size_t idx = 0;
    message[idx++] = 0x03;                           // NDEF TLV
    message[idx++] = (uint8_t)(3 + 1 + payload_len); // record header (3) + type (1) + payload
    message[idx++] = 0xD1;                 // Record header: MB=1, ME=1, TNF=1 (Well-known)
    message[idx++] = 0x01;                 // Type length
    message[idx++] = (uint8_t)payload_len; // Payload length
    message[idx++] = 'U';                  // Type: 'U' (URI)
    message[idx++] = 0x04;                 // URI prefix: "https://"
    memcpy(&message[idx], body, body_len);
    idx += body_len;
    message[idx++] = 0xFE; // Terminator TLV

    // Align total length to 4 bytes
    size_t len = (idx + 3U) & ~3U;

    if (len > NFC_SHARED_MEM_BYTE_SIZE)
    {
        LED_Error();
        return false;
    }

    bool success = Chip_NFC_WordWrite(
        NSS_NFC, (uint32_t *)NFC_SHARED_MEM_START, (uint32_t *)message, (int)len / 4);

    if (success)
    {
        // Success
    }
    else
    {
        LED_Error();
        return false;
    }

    return true;
}

static char *NFC_Convert_Float_String(float value, char *buffer, uint8_t precision)
{
    char *start = buffer;
    // Handle negative numbers
    if (value < 0.0)
    {
        *buffer++ = '-';
        value = -value;
    }

    // Split integer and fractional parts
    uint64_t int_part = (uint64_t)value;
    float frac_part = value - (float)int_part;

    // Convert integer part to string (stored reversed first)
    char tmp[32];
    int i = 0;
    do
    {
        tmp[i++] = (char)('0') + (char)(int_part % 10);
        int_part /= 10;
    } while (int_part && i < (int)sizeof(tmp));

    // Copy integer part in correct order
    while (i--)
    {
        *buffer++ = tmp[i];
    }

    // Decimal point
    *buffer++ = '.';

    // Convert fractional part
    for (int j = 0; j < precision; j++)
    {
        frac_part *= 10.0f;
        int digit = (int)frac_part;
        *buffer++ = (char)('0' + digit);
        frac_part -= (float)digit;
    }

    // Null terminator
    *buffer = '\0';
    return start;
}
