/*
 * Utility functions for the light weight IP timers.
 */

#ifndef _COMMON_RINA_TIMER_H
#define _COMMON_RINA_TIMER_H

#include "portability/port.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define TAG_TIMER ;

    /**
     * The software timer struct for various IPCP functions
     */
    typedef struct xIPCP_TIMER
    {
        bool_t bActive;               /**< This timer is running and must be processed. */
        bool_t bExpired;              /**< Timer has expired and a task must be processed. */
        struct timespec xTimeOut;     /**< The timeout value. */
        useconds_t ulRemainingTimeUS; /**< The amount of time remaining. */
        useconds_t ulReloadTimeUS;    /**< The value of reload time. */
    } IPCPTimer_t;

    void vIPCPTimerStart(IPCPTimer_t *pxTimer,
                         useconds_t xTime);

    bool_t bIPCPTimerCheck(IPCPTimer_t *pxTimer);

    void vIPCPTimerReload(IPCPTimer_t *pxTimer,
                          useconds_t xTime);

#ifdef __cplusplus
}
#endif

#endif // _COMMON_RINA_TIMER_H