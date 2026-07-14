#include "lib_chip_nss/inc/chip.h"
#include "button.h"
#include "config.h"

bool Button_Check_Pressed()
{
    bool button_pressed;
    button_pressed = !Chip_GPIO_GetPinState(NSS_GPIO, 0, IOCON_PIO0_1);

    if (button_pressed)
    {
        Chip_Clock_System_BusyWait_ms(DEBOUNCE_INTERVAL_MS);
        button_pressed = !Chip_GPIO_GetPinState(NSS_GPIO, 0, IOCON_PIO0_1);
        if (button_pressed)
        {
            return true;
        }
    }

    return false;
}
