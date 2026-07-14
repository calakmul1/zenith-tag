#include "lib_chip_nss/inc/chip.h"

void LED_Toggle(void)
{
    Chip_GPIO_SetPinToggle(NSS_GPIO, 0, 2);
}

void LED_Off(void)
{
    Chip_GPIO_SetPinOutLow(NSS_GPIO, 0, 2);
}

void LED_On(void)
{
    Chip_GPIO_SetPinOutHigh(NSS_GPIO, 0, 2);
}

void LED_Error(void)
{
    while (true)
    {
        Chip_Clock_System_BusyWait_ms(200);
        LED_Toggle();
    }
}
