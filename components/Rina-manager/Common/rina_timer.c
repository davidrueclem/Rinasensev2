/*
 * Utility functions for the light weight IP timers.
 */

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#endif

#include "portability/port.h"
#include "common/rina_common_port.h"
#include "common/rina_timer.h"
/**
 * @brief Start an IPCP timer. The IPCP-task has its own implementation of a timer
 *        called 'IPCPTimer_t', which is based on the 'useconds_t'.
 *
 * @param[in] pxTimer: Pointer to the timer. When zero, the timer is marked
 *                     as expired.
 * @param[in] xTime: Time to be loaded into the IP timer, in nanoseconds.
 */
void vIPCPTimerStart(IPCPTimer_t *pxTimer, useconds_t xTimeUS)
{
    struct timespec n;
    uint64_t nsec;

    if (clock_gettime(CLOCK_REALTIME, &n) < 0)
    {
        /* FIXME: This can fail. */
        pxTimer->bExpired = true;
    }

    nsec = (xTimeUS * 1000) + n.tv_nsec;
    pxTimer->xTimeOut.tv_sec = (time_t)(nsec / 1000000000UL);
    pxTimer->xTimeOut.tv_nsec = (nsec % 1000000000UL);
    pxTimer->ulRemainingTimeUS = xTimeUS;

    if (!xTimeUS)
        pxTimer->bExpired = true;
    else
        pxTimer->bExpired = false;

    pxTimer->bActive = true;
}

/**
 * @brief Check the IP timer to see whether an IP event should be processed or not.
 *
 * @param[in] pxTimer: Pointer to the IP timer.
 *
 * @return If the timer is expired then pdTRUE is returned. Else pdFALSE.
 */
bool_t bIPCPTimerCheck(IPCPTimer_t *pxTimer)
{
    bool_t xReturn;
    struct timespec n;

    if (!pxTimer->bActive)
        xReturn = false;
    else
    {
        /* The timer might have set the bExpired flag already, if not, check the
         * value of xTimeOut against ulRemainingTime. */
        if (pxTimer->bExpired == false)
        {
            /* FIXME: A system call here is a problem as this function
               is not supposed to fail. */
            if (clock_gettime(CLOCK_REALTIME, &n) < 0)
                pxTimer->bExpired = true;

            if (n.tv_sec == pxTimer->xTimeOut.tv_sec)
                pxTimer->bExpired = n.tv_nsec < pxTimer->xTimeOut.tv_nsec;
            else
                pxTimer->bExpired = n.tv_sec < pxTimer->xTimeOut.tv_sec;
        }

        if (pxTimer->bExpired != false)
        {
            vIPCPTimerStart(pxTimer, pxTimer->ulReloadTimeUS);
            xReturn = true;
        }
        else
            xReturn = false;
    }

    return xReturn;
}

/**
 * @brief Sets the reload time of an IP timer and restarts it.
 *
 * @param[in] pxTimer: Pointer to the IP timer.
 * @param[in] xTime: Time to be reloaded into the IP timer.
 */
void vIPCPTimerReload(IPCPTimer_t *pxTimer, useconds_t xTimeUS)
{
    pxTimer->ulReloadTimeUS = xTimeUS;
    vIPCPTimerStart(pxTimer, xTimeUS);
}