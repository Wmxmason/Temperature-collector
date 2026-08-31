#include <stdint.h>

#include <pthread.h>

#include <ti/drivers/Board.h>
#include <ti/sysbios/BIOS.h>

#include "collector_app.h"
#include "system_config.h"

int main(void)
{
    pthread_t thread;
    pthread_attr_t attributes;
    struct sched_param priority;
    int result;

    Board_init();

    pthread_attr_init(&attributes);
    priority.sched_priority = SYSTEM_MAIN_THREAD_PRIORITY;
    result = pthread_attr_setschedparam(&attributes, &priority);
    result |= pthread_attr_setdetachstate(&attributes,
                                          PTHREAD_CREATE_DETACHED);
    result |= pthread_attr_setstacksize(&attributes,
                                        SYSTEM_MAIN_THREAD_STACK_SIZE);
    if (result != 0)
    {
        while (1)
        {
        }
    }

    result = pthread_create(&thread,
                            &attributes,
                            collector_app_main,
                            NULL);
    if (result != 0)
    {
        while (1)
        {
        }
    }

    BIOS_start();
    return 0;
}
