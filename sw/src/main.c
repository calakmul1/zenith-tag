#include <stdint.h>

#include "config.h"
#include "board/inc/board.h"
#include "components/button/button.h"
#include "components/led/led.h"
#include "components/pressure/pressure.h"
#include "components/nfc/nfc.h"

int main(void)
{
    Board_Init();

    // This wait call is intended to avoid bricking the IC. Don't remove.
    Chip_Clock_System_BusyWait_ms(1000);

    bool success;
    success = Peripherals_Init();

    if (!success)
    {
        LED_Error();
        return -1;
    }

    Chip_Clock_System_BusyWait_ms(1000);

    LED_On();

    while (true)
    {
        if (Button_Check_Pressed() == true)
        {
            Pressure_Start_Read();

            float pressure_current = 0.0f;
            float temperature_current = 0.0f;
            float pressure_sum = 0.0f;

            for (int i = 0; i < PRE_TAKEOFF_MEASUREMENTS; i++)
            {
                Chip_Clock_System_BusyWait_ms(MEASUREMENT_INTERVAL_MS);
                Pressure_Read(&pressure_current, &temperature_current);
                pressure_sum += pressure_current;
            }

            float pressure_baseline = pressure_sum / (float)PRE_TAKEOFF_MEASUREMENTS;
            float pressure_max = pressure_baseline;
            float pressure_min = pressure_baseline;

            uint32_t led_timer = 0U;

            while (true)
            {
                Chip_Clock_System_BusyWait_ms(MEASUREMENT_INTERVAL_MS);

                Pressure_Read(&pressure_current, &temperature_current);

                if (pressure_current < pressure_min)
                {
                    pressure_min = pressure_current;
                }

                if (pressure_current > pressure_max)
                {
                    pressure_max = pressure_current;
                }

                led_timer += MEASUREMENT_INTERVAL_MS;

                if (led_timer >= LED_TOGGLE_INTERVAL_MS)
                {
                    LED_Toggle();
                    led_timer = 0;
                }

                if (Button_Check_Pressed() == true)
                {
                    break;
                }
            }

            float h_up = Pressure_Calculate_Height(pressure_min, pressure_baseline);
            float h_down = Pressure_Calculate_Height(pressure_baseline, pressure_max);

            float delta_h;

            if (h_up > h_down)
            {
                delta_h = h_up;
            }
            else
            {
                delta_h = -h_down;
            }

            delta_h -= 0.50f;

            NFC_Store_Flight_Height(delta_h);

            Pressure_End_Read();

            LED_On();
            Chip_Clock_System_BusyWait_ms(200);
        }
    }

    return 0;
}
