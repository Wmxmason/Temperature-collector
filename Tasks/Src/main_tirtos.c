#include <ti/drivers/Board.h>
#include <ti/sysbios/BIOS.h>

#include "collector_app.h"

int main(void)
{
    Board_init();
    collector_app_start();

    BIOS_start();
    return 0;
}
