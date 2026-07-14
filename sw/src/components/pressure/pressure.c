#include <math.h>
#include <errno.h>
#include <float.h>

#include "lib_chip_nss/inc/chip.h"

#include "pressure.h"

#include "components/led/led.h"
#include "components/nfc/nfc.h"

/** @brief I2C address of the BMP388 pressure sensor. */
#define BMP388 0x77

/**
 * @brief Raw calibration coefficients as read directly from the BMP388 registers.
 *
 * This structure maps the unscaled integer coefficients stored in the BMP388
 * non-volatile memory (registers 0x31–0x45). These coefficients are device-specific
 * and must be converted into floating-point form before use in compensation
 * formulas.
 *
 * @see bmp388_calib_f32_t
 */
typedef struct
{
    uint16_t par_t1; /**< Temperature coefficient T1 (unsigned, LSB = 2⁻⁸). */
    uint16_t par_t2; /**< Temperature coefficient T2 (unsigned, LSB = 2⁻³⁰). */
    int8_t par_t3;   /**< Temperature coefficient T3 (signed, LSB = 2⁻⁴⁸). */
    int16_t par_p1;  /**< Pressure coefficient P1 (signed, LSB = 2⁻²⁰). */
    int16_t par_p2;  /**< Pressure coefficient P2 (signed, LSB = 2⁻²⁹). */
    int8_t par_p3;   /**< Pressure coefficient P3 (signed, LSB = 2⁻³²). */
    int8_t par_p4;   /**< Pressure coefficient P4 (signed, LSB = 2⁻³⁷). */
    uint16_t par_p5; /**< Pressure coefficient P5 (unsigned, LSB = 2⁻³). */
    uint16_t par_p6; /**< Pressure coefficient P6 (unsigned, LSB = 2⁻⁶). */
    int8_t par_p7;   /**< Pressure coefficient P7 (signed, LSB = 2⁻⁸). */
    int8_t par_p8;   /**< Pressure coefficient P8 (signed, LSB = 2⁻¹⁵). */
    int16_t par_p9;  /**< Pressure coefficient P9 (signed, LSB = 2⁻⁴⁸). */
    int8_t par_p10;  /**< Pressure coefficient P10 (signed, LSB = 2⁻⁴⁸). */
    int8_t par_p11;  /**< Pressure coefficient P11 (signed, LSB = 2⁻⁶⁵). */
} bmp388_calib_t;

/**
 * @brief Floating-point calibration coefficients for compensated measurements.
 *
 * This structure stores the scaled calibration coefficients converted from
 * @ref bmp388_calib_t using the formulas provided in the BMP388 datasheet.
 * These parameters are used in the temperature and pressure compensation functions.
 */
typedef struct
{
    float par_t1;  /**< Temperature coefficient T1 (scaled). */
    float par_t2;  /**< Temperature coefficient T2 (scaled). */
    float par_t3;  /**< Temperature coefficient T3 (scaled). */
    float par_p1;  /**< Pressure coefficient P1 (scaled). */
    float par_p2;  /**< Pressure coefficient P2 (scaled). */
    float par_p3;  /**< Pressure coefficient P3 (scaled). */
    float par_p4;  /**< Pressure coefficient P4 (scaled). */
    float par_p5;  /**< Pressure coefficient P5 (scaled). */
    float par_p6;  /**< Pressure coefficient P6 (scaled). */
    float par_p7;  /**< Pressure coefficient P7 (scaled). */
    float par_p8;  /**< Pressure coefficient P8 (scaled). */
    float par_p9;  /**< Pressure coefficient P9 (scaled). */
    float par_p10; /**< Pressure coefficient P10 (scaled). */
    float par_p11; /**< Pressure coefficient P11 (scaled). */
    float t_lin;   /**< Linearized temperature value (°C) used in pressure compensation. */
} bmp388_calib_f32_t;

/**
 * @brief Global instance of BMP388 floating-point calibration data.
 *
 * This structure holds the converted and scaled calibration coefficients
 * for the currently active BMP388 sensor. It is initialized during
 * @ref Pressure_Load_Calibration and subsequently used by
 * @ref Pressure_Compensate_Temperature and @ref Pressure_Compensate_Pressure
 * to compute compensated temperature and pressure readings.
 */
bmp388_calib_f32_t calib_float;

/**
 * @defgroup POW2_Constants Precomputed power-of-two constants
 * @brief Floating-point powers of two for fast scaling operations.
 * @{
 */
#define POW2_3 0x1p3f   /**< 2³  = 8.0f */
#define POW2_6 0x1p6f   /**< 2⁶  = 64.0f */
#define POW2_8 0x1p8f   /**< 2⁸  = 256.0f */
#define POW2_14 0x1p14f /**< 2¹⁴ = 16,384.0f */
#define POW2_15 0x1p15f /**< 2¹⁵ = 32,768.0f */
#define POW2_20 0x1p20f /**< 2²⁰ = 1,048,576.0f */
#define POW2_29 0x1p29f /**< 2²⁹ = 536,870,912.0f */
#define POW2_30 0x1p30f /**< 2³⁰ = 1,073,741,824.0f */
#define POW2_32 0x1p32f /**< 2³² = 4,294,967,296.0f */
#define POW2_37 0x1p37f /**< 2³⁷ ≈ 1.37e11f */
#define POW2_48 0x1p48f /**< 2⁴⁸ ≈ 2.81e14f */
#define POW2_65 0x1p65f /**< 2⁶⁵ ≈ 3.69e19f */
/** @} */

static bool Pressure_Detect_Sensor();
static bool Pressure_Load_Calibration();
static bool Pressure_Configure();
static float Pressure_Compensate_Temperature(uint32_t temperature_raw);
static float Pressure_Compensate_Pressure(uint32_t pressure_raw);

bool Pressure_Init()
{
    bool success = Pressure_Detect_Sensor();

    if (!success)
        return false;

    success = Pressure_Load_Calibration();

    if (!success)
        return false;

    success = Pressure_Configure();

    if (!success)
        return false;

    return true;
}

bool Pressure_Start_Read()
{
    uint8_t i2c_command[2];
    int i2c_result;

    uint8_t bmp_error_i2c_reg = 0x02;
    uint8_t bmp_error;

    i2c_command[0] = 0x1B;
    i2c_command[1] = 0x33; // Enable pressure and temperature sensors in normal mode, see
                           // chapter 4.3.16 of the BMP388 datasheet.

    i2c_result = Chip_I2C_MasterSend(I2C0, BMP388, i2c_command, 2);

    Chip_I2C_MasterCmdRead(I2C0, BMP388, &bmp_error_i2c_reg, &bmp_error, 1);

    if (i2c_result == 2 && bmp_error == 0x00)
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

bool Pressure_End_Read()
{
    uint8_t i2c_command[2];
    int i2c_result;

    uint8_t bmp_error_i2c_reg = 0x02;
    uint8_t bmp_error;

    i2c_command[0] = 0x1B;
    i2c_command[1] = 0x00; // Disable pressure and temperature sensors and go to sleep mode, see
                           // chapter 4.3.16 of the BMP388 datasheet.

    i2c_result = Chip_I2C_MasterSend(I2C0, BMP388, i2c_command, 2);

    Chip_I2C_MasterCmdRead(I2C0, BMP388, &bmp_error_i2c_reg, &bmp_error, 1);

    if (i2c_result == 2 && bmp_error == 0x00)
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

bool Pressure_Read(float *pressure, float *temperature)
{
    uint8_t i2c_reg = 0x04;
    uint8_t i2c_data[6];
    int i2c_result;

    uint8_t bmp_error_i2c_reg = 0x02;
    uint8_t bmp_error;

    i2c_result = Chip_I2C_MasterCmdRead(I2C0, BMP388, &i2c_reg, &i2c_data[0], 6);

    Chip_I2C_MasterCmdRead(I2C0, BMP388, &bmp_error_i2c_reg, &bmp_error, 1);

    if (i2c_result == 6 && bmp_error == 0x00)
    {
        // Success
    }
    else
    {
        LED_Error();
        return false;
    }

    uint32_t pressure_raw =
        ((uint32_t)i2c_data[2] << 16) | ((uint32_t)i2c_data[1] << 8) | i2c_data[0];
    uint32_t temperature_raw =
        ((uint32_t)i2c_data[5] << 16) | ((uint32_t)i2c_data[4] << 8) | i2c_data[3];

    *temperature = Pressure_Compensate_Temperature(temperature_raw);
    *pressure = Pressure_Compensate_Pressure(pressure_raw);

    return true;
}

float Pressure_Calculate_Height(float pressure_min, float pressure_max)
{
    if (pressure_min <= 0.0f || pressure_max <= 0.0f)
    {
        LED_Error();
    }

    errno = 0;
    float ratio = pressure_min / pressure_max;
    float exponent = 0.1903f;
    float pow_result = powf(ratio, exponent);

    if (errno == EDOM)
    {
        LED_Error();
    }
    if (errno == ERANGE)
    {
        LED_Error();
    }

    float delta_h = 44330.0f * (1.0f - pow_result);

    return delta_h;
}

/**
 * @brief Checks if the BMP388 pressure sensor is detected over the I2C bus.
 *
 * This function reads the chip ID register (0x00) of the BMP388 sensor to verify
 * communication. The expected chip ID value is 0x50, as specified in the BMP388 datasheet.
 * If the correct ID is read, the sensor is considered successfully detected.
 *
 * @retval true   BMP388 sensor detected and responded with the expected ID (0x50).
 * @retval false  I2C read failed or chip ID did not match 0x50.
 */
static bool Pressure_Detect_Sensor()
{
    uint8_t i2c_reg = 0x00;
    uint8_t i2c_data[2];

    int i2c_result = Chip_I2C_MasterCmdRead(I2C0, BMP388, &i2c_reg, &i2c_data[0], 1);
    if (i2c_result == 1 && i2c_data[0] == 0x50)
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
 * @brief Loads and converts calibration coefficients from the BMP388 pressure sensor.
 *
 * This function reads 21 bytes of factory calibration data from the BMP388’s calibration
 * registers (0x31–0x45) over I2C and converts the raw integer coefficients into floating-point
 * values according to the formulas provided in the BMP388 datasheet Appendix.
 *
 * The resulting floating-point coefficients are stored in the global
 * `calib_float` structure and used by the temperature and pressure
 * compensation routines.
 *
 * @note The global variable `calib_float.t_lin` is reset to 0.0f during initialization.
 *
 * @retval true   All calibration bytes were read and converted successfully.
 * @retval false  I2C read operation failed.
 */
static bool Pressure_Load_Calibration()
{
    uint8_t i2c_reg = 0x31;
    uint8_t i2c_data[21];
    bmp388_calib_t calib_raw;

    int i2c_result = Chip_I2C_MasterCmdRead(I2C0, BMP388, &i2c_reg, &i2c_data[0], 21);
    if (i2c_result == 21)
    {
        // Success
    }
    else
    {
        LED_Error();
        return false;
    }

    /* ---- Raw Calibration Data (refer to chapter 3.11.1 of the BMP388 data sheet) ---- */
    calib_raw.par_t1 =
        (uint16_t)((uint16_t)i2c_data[0] | ((uint16_t)i2c_data[1] << 8)); // 0x31..0x32
    calib_raw.par_t2 =
        (uint16_t)((uint16_t)i2c_data[2] | ((uint16_t)i2c_data[3] << 8)); // 0x33..0x34
    calib_raw.par_t3 = (int8_t)i2c_data[4];                               // 0x35
    calib_raw.par_p1 =
        (int16_t)((uint16_t)i2c_data[5] | ((uint16_t)i2c_data[6] << 8)); // 0x36..0x37
    calib_raw.par_p2 =
        (int16_t)((uint16_t)i2c_data[7] | ((uint16_t)i2c_data[8] << 8)); // 0x38..0x39
    calib_raw.par_p3 = (int8_t)i2c_data[9];                              // 0x3A
    calib_raw.par_p4 = (int8_t)i2c_data[10];                             // 0x3B
    calib_raw.par_p5 =
        (uint16_t)((uint16_t)i2c_data[11] | ((uint16_t)i2c_data[12] << 8)); // 0x3C..0x3D
    calib_raw.par_p6 =
        (uint16_t)((uint16_t)i2c_data[13] | ((uint16_t)i2c_data[14] << 8)); // 0x3E..0x3F
    calib_raw.par_p7 = (int8_t)i2c_data[15];                                // 0x40
    calib_raw.par_p8 = (int8_t)i2c_data[16];                                // 0x41
    calib_raw.par_p9 =
        (int16_t)((uint16_t)i2c_data[17] | ((uint16_t)i2c_data[18] << 8)); // 0x42..0x43
    calib_raw.par_p10 = (int8_t)i2c_data[19];                              // 0x44
    calib_raw.par_p11 = (int8_t)i2c_data[20];                              // 0x45

    /* ---- Convert to Floating Point Calibration Data  ----
     Formulas as per BMP388 datasheet Appendix:
     T1: par_t1_f = par_t1 / 2^-8  (== *256)
     T2: par_t2_f = par_t2 / 2^30
     T3: par_t3_f = par_t3 / 2^48
     P1: par_p1_f = (par_p1 - 2^14) / 2^20
     P2: par_p2_f = (par_p2 - 2^14) / 2^29
     P3: par_p3_f = par_p3 / 2^32
     P4: par_p4_f = par_p4 / 2^37
     P5: par_p5_f = par_p5 / 2^-3 (== *8)
     P6: par_p6_f = par_p6 / 2^6
     P7: par_p7_f = par_p7 / 2^8
     P8: par_p8_f = par_p8 / 2^15
     P9: par_p9_f = par_p9 / 2^48
     P10: par_p10_f = par_p10 / 2^48
     P11: par_p11_f = par_p11 / 2^65
     */

    calib_float.par_t1 = calib_raw.par_t1 * POW2_8;
    calib_float.par_t2 = calib_raw.par_t2 / POW2_30;
    calib_float.par_t3 = calib_raw.par_t3 / POW2_48;
    calib_float.par_p1 = (calib_raw.par_p1 - POW2_14) / POW2_20;
    calib_float.par_p2 = (calib_raw.par_p2 - POW2_14) / POW2_29;
    calib_float.par_p3 = calib_raw.par_p3 / POW2_32;
    calib_float.par_p4 = calib_raw.par_p4 / POW2_37;
    calib_float.par_p5 = calib_raw.par_p5 * POW2_3;
    calib_float.par_p6 = calib_raw.par_p6 / POW2_6;
    calib_float.par_p7 = calib_raw.par_p7 / POW2_8;
    calib_float.par_p8 = calib_raw.par_p8 / POW2_15;
    calib_float.par_p9 = calib_raw.par_p9 / POW2_48;
    calib_float.par_p10 = calib_raw.par_p10 / POW2_48;
    calib_float.par_p11 = calib_raw.par_p11 / POW2_65;
    calib_float.t_lin = 0.0f;

    return true;
}

/**
 * @brief Configures the BMP388 pressure sensor per Bosch guidance for drones (section 3.5 of the
 * datasheet).
 *
 * Sequence:
 *  1) Soft reset (0x7E ← 0xB6), short delay to let the device restart.
 *  2) Set oversampling  (0x1C): pressure x8, temperature x1 (section 4.3.17 of the datasheet).
 *  3) Set ODR           (0x1D): 50 Hz (section 4.3.19 of the datasheet).
 *  4) Set IIR filter    (0x1F): coefficient = 3 (section 4.3.20 of the datasheet).
 *
 * @note: Apply configuration while the device is in sleep/standby. Enter normal mode afterwards
 *       via Pressure_Start_Read() (0x1B). Keep a small delay after the soft reset.
 *
 * @retval true   All configuration steps succeeded.
 * @retval false  One or more I2C commands failed.
 */
static bool Pressure_Configure()
{
    uint8_t i2c_command[2];
    int i2c_result;

    uint8_t bmp_error_i2c_reg = 0x02;
    uint8_t bmp_error;

    /* ---- Soft Reset ---- */
    i2c_command[0] = 0x7E;
    i2c_command[1] = 0xB6;

    i2c_result = Chip_I2C_MasterSend(I2C0, BMP388, i2c_command, 2);

    Chip_I2C_MasterCmdRead(I2C0, BMP388, &bmp_error_i2c_reg, &bmp_error, 1);

    if (i2c_result == 2 && bmp_error == 0x00)
    {
        // Success
    }
    else
    {
        LED_Error();
        return false;
    }
    Chip_Clock_System_BusyWait_ms(5);

    /* ----  Over-sampling ---- */
    i2c_command[0] = 0x1C;
    i2c_command[1] =
        (uint8_t)((0u << 3) | 3u); // Set over-sampling of pressure to x8, and of temperature to x1,
                                   // see chapters 3.4.4 and 4.3.17 of the BMP388 datasheet.

    i2c_result = Chip_I2C_MasterSend(I2C0, BMP388, i2c_command, 2);

    Chip_I2C_MasterCmdRead(I2C0, BMP388, &bmp_error_i2c_reg, &bmp_error, 1);

    if (i2c_result == 2 && bmp_error == 0x00)
    {
        // Success
    }
    else
    {
        LED_Error();
        return false;
    }

    /* ---- Output Data Rate ---- */
    i2c_command[0] = 0x1D;
    i2c_command[1] =
        0x02; // Set output data rate to 50Hz, see chapter 4.3.19 of the BMP388 datasheet.

    i2c_result = Chip_I2C_MasterSend(I2C0, BMP388, i2c_command, 2);

    Chip_I2C_MasterCmdRead(I2C0, BMP388, &bmp_error_i2c_reg, &bmp_error, 1);

    if (i2c_result == 2 && bmp_error == 0x00)
    {
        // Success
    }
    else
    {
        LED_Error();
        return false;
    }

    /* ---- IIR Filter Coefficient ---- */
    i2c_command[0] = 0x1F;
    i2c_command[1] =
        0x02; // Set IIR filter coefficient to 3, see chapter 4.3.20 of the BMP388 datasheet.

    i2c_result = Chip_I2C_MasterSend(I2C0, BMP388, i2c_command, 2);

    Chip_I2C_MasterCmdRead(I2C0, BMP388, &bmp_error_i2c_reg, &bmp_error, 1);

    if (i2c_result == 2 && bmp_error == 0x00)
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
 * @brief Compensates the raw temperature reading from the BMP388 sensor.
 *
 * This function applies the temperature compensation formula as described in
 * section 9.2 of the BMP388 datasheet. The compensated temperature value is
 * stored as `t_lin` in the global calibration structure for later use in
 * pressure compensation.
 *
 * @param[in]  temperature_raw  Raw temperature value read from the BMP388.
 *
 * @return Compensated temperature in degrees Celsius.
 */
static float Pressure_Compensate_Temperature(uint32_t temperature_raw)
{
    float x;
    float y;
    float temperature_comp;

    x = (float)((float)temperature_raw - calib_float.par_t1);
    y = (float)(x * calib_float.par_t2);
    temperature_comp = y + (x * x) * calib_float.par_t3;

    calib_float.t_lin = temperature_comp;

    return temperature_comp;
}

/**
 * @brief Compensates the raw pressure reading from the BMP388 sensor.
 *
 * This function applies the multi-term polynomial compensation formula described in
 * section 9.3 of the BMP388 datasheet. The compensation uses both the raw pressure
 * measurement and the previously computed linearized temperature (`t_lin`) to produce
 * an accurate pressure value in Pascals (Pa).
 *
 * The calculation combines first-, second-, and third-order terms based on the
 * temperature-compensated coefficients read from the BMP388’s calibration registers.
 *
 * @param[in] pressure_raw  Raw pressure value read from the BMP388 (20-bit unsigned integer).
 *
 * @return Compensated pressure in Pascals (Pa).
 */
static float Pressure_Compensate_Pressure(uint32_t pressure_raw)
{
    float w;
    float x;
    float y;
    float z;

    float a;
    float b;
    float c;

    float pressure_comp;

    w = calib_float.par_p6 * calib_float.t_lin;
    x = calib_float.par_p7 * (calib_float.t_lin * calib_float.t_lin);
    y = calib_float.par_p8 * (calib_float.t_lin * calib_float.t_lin * calib_float.t_lin);
    a = calib_float.par_p5 + w + x + y;

    w = calib_float.par_p2 * calib_float.t_lin;
    x = calib_float.par_p3 * (calib_float.t_lin * calib_float.t_lin);
    y = calib_float.par_p4 * (calib_float.t_lin * calib_float.t_lin * calib_float.t_lin);
    b = (float)pressure_raw * (calib_float.par_p1 + w + x + y);

    w = (float)pressure_raw * (float)pressure_raw;
    x = calib_float.par_p9 + (calib_float.par_p10 * calib_float.t_lin);
    y = w * x;
    z = y + ((float)pressure_raw * (float)pressure_raw * (float)pressure_raw) * calib_float.par_p11;
    c = z;

    pressure_comp = a + b + c;

    return pressure_comp;
}
