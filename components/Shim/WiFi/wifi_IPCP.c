#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#endif

#include "common/list.h"
#include "common/rina_name.h"
#include "common/rina_ids.h"
#include "common/rina_common_port.h"
#include "common/rina_timer.h"
#include "portability/port.h"

#include "configRINA.h"
#include "configSensor.h"
#include "BufferManagement.h"
#include "IPCP_manager.h"

#include "Arp826.h"
#include "wifi_IPCP_ethernet.h"
#include "wifi_IPCP_events.h"
#include "wifi_IPCP.h"
#include "NetworkInterface.h"

/** @brief Set to pdTRUE when the IPCP task is ready to start processing packets. */
static bool_t xShimIpcpTaskInitialised = false;

static void prvProcessNetworkDownEvent(void);

/** @brief Stores the handle of the task that handles the stack.  The handle is used
 * (indirectly) by some utility function to determine if the utility function is
 * being called by a task (in which case it is ok to block) or by the IPCP task
 * itself (in which case it is not ok to block). */
static pthread_t xShimIpcpThread;

/** @brief The queue used to pass events into the IPCP-task for processing. */
RsQueue_t *xNetworkEventQueue = NULL;

/** @brief Simple set to pdTRUE or pdFALSE depending on whether the network is up or
 * down (connected, not connected) respectively. */
static bool_t xNetworkUp = false;

/** @brief ARP timer, to check its table entries. */
static IPCPTimer_t xARPTimer;

/** @brief Used to ensure network down events cannot be missed when they cannot be
 * posted to the network event queue because the network event queue is already
 * full. */
static volatile bool_t xNetworkDownEventPending = false;

void NetworkDown(void);

#ifdef ESP_PLATFORM
/* This saves the FreeRTOS current task handle. */
static TaskHandle_t xShimIpcpTaskHandle;
#endif

/*
 * Determine how long the IPCP task can sleep for, which depends on when the next
 * periodic or timeout processing must be performed.
 */
static long prvCalculateSleepTimeUS();

/*----------------------------------------------------------*/
/**
 * @brief Function to check whether the current context belongs to
 *        the IPCP-task.
 *
 * @return If the current context belongs to the IPCP-task, then pdTRUE is
 *         returned. Else pdFALSE is returned.
 *
 * @note Very important: the IPCP-task is not allowed to call its own API's,
 *        because it would easily get into a dead-lock.
 */
bool_t xIsCallingFromShimWiFiIpcpTask(void)
{
#ifdef ESP_PLATFORM
    /* On ESP32, we use the FreeRTOS API here because calling
     * pthread_self on a FreeRTOS task will crash the runtime with an
     * assertion failure. You can't call this method on a task that
     * has not been started with the pthread API. */
    return xTaskGetCurrentTaskHandle() == xShimIpcpTaskHandle;
#else
    return pthread_self() == xShimIpcpThread;
#endif
}

/**
 * @brief Check the network timers (ARP/DTP) and if they are
 *        expired, send an event to the IPCP-Task.
 */
static void prvCheckNetworkTimers(void)
{
    /* Is it time for ARP processing? */
    if (bIPCPTimerCheck(&xARPTimer) != false)
        (void)xSendEventToShimIPCPTask(eARPTimerEvent);
}

/**
 * @brief Calculate the maximum sleep time remaining. It will go through all
 *        timers to see which timer will expire first. That will be the amount
 *        of time to block in the next call to xQueueReceive().
 *
 * @return The maximum sleep time or ipconfigMAX_IP_TASK_SLEEP_TIME,
 *         whichever is smaller.
 */
static long prvCalculateSleepTimeUS()
{
    long xMaximumSleepTimeUS;

    /* Start with the maximum sleep time, then check this against the remaining
     * time in any other timers that are active. */
    xMaximumSleepTimeUS = MAX_IPCP_TASK_SLEEP_TIME_US;

    if (xARPTimer.bActive && xARPTimer.ulRemainingTimeUS < xMaximumSleepTimeUS)
        xMaximumSleepTimeUS = xARPTimer.ulRemainingTimeUS;

    return xMaximumSleepTimeUS;
}

/** @brief Check if task is ready */
bool_t xShimIpcpTaskReady(void)
{
    return xShimIpcpTaskInitialised;
}

/**
 * @brief Send an event (in form of struct) to the IP task to be processed.
 *
 * @param[in] pxEvent: The event to be sent.
 * @param[in] uxTimeout: Timeout for waiting in case the queue is full. 0 for non-blocking calls.
 *
 * @return pdPASS if the event was sent (or the desired effect was achieved). Else, pdFAIL.
 */
bool_t xSendEventStructToShimIPCPTask(const ShimWiFiTaskEvent_t *pxEvent, useconds_t xTimeOutUS)
{
    bool_t xReturn, xSendMessage;
    useconds_t xCalculatedTimeOutUS = xTimeOutUS;

    if (!xShimIpcpTaskReady() && (pxEvent->eEventType != eNetworkDownEvent))
    {
        LOGE(TAG_SHIM, "WiFi IPCP Task Not Ready, cannot send event");

        /* Only allow eNetworkDownEvent events if the IP task is not ready
         * yet.  Not going to attempt to send the message so the send failed. */
        xReturn = false;
    }
    else
    {
        xSendMessage = true;

        if (xSendMessage)
        {
            /* The IP task cannot block itself while waiting for
             * itself to respond. */
            if ((xIsCallingFromShimWiFiIpcpTask()) && (xCalculatedTimeOutUS > 0))
                xCalculatedTimeOutUS = 0;

            xReturn = xRsQueueSendToBack(xNetworkEventQueue, pxEvent, sizeof(ShimWiFiTaskEvent_t), xCalculatedTimeOutUS);

            if (!xReturn)
                LOGE(TAG_SHIM, "Failed to add message to IPCP queue");
        }
        else
        {
            /* It was not necessary to send the message to process the event so
             * even though the message was not sent the call was successful. */
            xReturn = true;
        }
    }

    return xReturn;
}

/**
 * @brief Send an event to the IPCP task. It calls 'xSendEventStructToShimIPCPTask' internally.
 *
 * @param[in] eEvent: The event to be sent.
 *
 * @return pdPASS if the event was sent (or the desired effect was achieved). Else, pdFAIL.
 */
bool_t xSendEventToShimIPCPTask(eShimWiFiEvent_t eEvent)
{
    ShimWiFiTaskEvent_t xEventMessage;

    xEventMessage.eEventType = eEvent;
    xEventMessage.xData.PV = (void *)NULL;

    return xSendEventStructToShimIPCPTask(&xEventMessage, 0);
}

/***
 *  Create the Shim IPCP Task
 * */
static void *prvShimIpcpTask(void *pvParameters)
{
    ShimWiFiTaskEvent_t xReceivedEvent;
    struct timespec xNextIPCPSleep;
    useconds_t xSleepTimeUS;

    /* Just to prevent compiler warnings about unused parameters. */
    (void)pvParameters;

#ifdef ESP_PLATFORM
    /* FIXME: Protect with mutex. */
    xShimIpcpTaskHandle = xTaskGetCurrentTaskHandle();
#endif

    /* Initialization is complete and events can now be processed. */
    xShimIpcpTaskInitialised = true;

    /* Generate a dummy message to say that the network connection has gone
     *  down.  This will cause this task to initialise the network interface.  After
     *  this it is the responsibility of the network interface hardware driver to
     *  send this message if a previously connected network is disconnected. */

    /* This is not necessary because the Manager task must request the enrollment
     * The enrollment is going to initialize the network interface and connect to the
     * Access Point.  */
    // NetworkDown();

    LOGI(TAG_SHIM, "ENTER: IPC Manager Thread");

    /* Loop, processing IP events. */
    for (;;)
    {
        // ipconfigWATCHDOG_TIMER();

        /* Check the ARP timers to see if there is any periodic
         * or timeout processing to perform. */
        // prvCheckNetworkTimers();
        LOGI(TAG_SHIM, "TASK WIFI");

        /* Calculate the acceptable maximum sleep time. */
        xSleepTimeUS = prvCalculateSleepTimeUS();

        /* Wait until there is something to do. If the following call exits
         * due to a time out rather than a message being received, set a
         * 'NoEvent' value. */
        if (xRsQueueReceive(xNetworkEventQueue, (void *)&xReceivedEvent, sizeof(ShimWiFiTaskEvent_t), xSleepTimeUS) == false)

            xReceivedEvent.eEventType = eNoNetworkEvent;

        switch (xReceivedEvent.eEventType)
        {
        case eNetworkDownEvent:
        {
            struct timespec ts = {INITIALISATION_RETRY_DELAY_SEC, 0};

            /* Attempt to establish a connection. */
            nanosleep(&ts, NULL);
            LOGI(TAG_SHIM, "eNetworkDownEvent");
            xNetworkUp = false;
            prvProcessNetworkDownEvent();
            break;
        }

        case eNetworkRxEvent:
            /* The network hardware driver has received a new packet.  A
             * pointer to the received buffer is located in the pvData member
             * of the received event structure. */
            prvHandleEthernetPacket((NetworkBufferDescriptor_t *)xReceivedEvent.xData.PV);
            break;

        case eNetworkTxEvent:
        {
            NetworkBufferDescriptor_t *pxDescriptor;

            pxDescriptor = (NetworkBufferDescriptor_t *)xReceivedEvent.xData.PV;

            /* Send a network packet. The ownership will  be transferred to
             * the driver, which will release it after delivery. */
            xNetworkInterfaceOutput(pxDescriptor, true);
        }
        break;

        case eNoNetworkEvent:
            /* xQueueReceive() returned because of a normal time-out. */
            break;

        default:
            /* Should not get here. */
            break;
        }

        if (xNetworkDownEventPending != false)
        {
            /* A network down event could not be posted to the network event
             * queue because the queue was full.
             * As this code runs in the IP-task, it can be done directly by
             * calling prvProcessNetworkDownEvent(). */
            // prvProcessNetworkDownEvent();
        }
    }

    LOGI(TAG_SHIM, "EXIT: IPC Manager Thread");
    return NULL;
}

/**
 * @brief Initialize the RINA network stack and initialize the IPCP-task.
 *
 *
 * @return pdPASS if the task was successfully created and added to a ready
 * list, otherwise an error code defined in the file projdefs.h
 */
bool_t xShimIpcpInit(void)
{
    bool_t xReturn = false;

    LOGI(TAG_SHIM, "************* INIT SHIM WIFI IPCP ***********");

    /* This function should only be called once. */
    RsAssert(xShimIpcpTaskReady() == false);
    RsAssert(xNetworkEventQueue == NULL);

#if 0

    /* ESP32 is 32-bits platform, so this is not executed*/
    if (sizeof(uintptr_t) == 8)
    {
        /* This is a 64-bit platform, make sure there is enough space in
         * pucEthernetBuffer to store a pointer. */

        RsAssert(BUFFER_PADDING >= 14);

        /* But it must have this strange alignment: */
        RsAssert((((BUFFER_PADDING) + 2) % 4) == 0);
    }

#endif

    /* Check if MTU is big enough. */

    /* Check structure packing is correct. */

    /* Attempt to create the queue used to communicate with the IPCP task. */
    xNetworkEventQueue = pxRsQueueCreate("ShimIPCPQueue",
                                         // EVENT_QUEUE_LENGTH,
                                         8,
                                         sizeof(ShimWiFiTaskEvent_t));
    RsAssert(xNetworkEventQueue != NULL);

    if (xNetworkEventQueue != NULL)
    {

        pthread_attr_t attr;

        if (pthread_attr_init(&attr) != 0)
            return false;

        pthread_attr_setstacksize(&attr, IPCP_TASK_STACK_SIZE);

        if (pthread_create(&xShimIpcpThread, &attr, prvShimIpcpTask, NULL) != 0)
        {
            LOGE(TAG_SHIM, "RINAInit: failed to start IPC process");
            xReturn = false;
        }
        else
            xReturn = true;

        pthread_attr_destroy(&attr);
    }
    else
        LOGE(TAG_SHIM, "RINAInit: Network event queue could not be created\n");

    return xReturn;
}

/**
 * @brief Process a 'Network down' event and complete required processing.
 */
static void prvProcessNetworkDownEvent(void)
{
    /* Stop the ARP timer while there is no network. */
    xARPTimer.bActive = false;

    /* Per the ARP Cache Validation section of https://tools.ietf.org/html/rfc1122,
     * treat network down as a "delivery problem" and flush the ARP cache for this
     * interface. */
    // RINA_vARPRemove(  );

    /* The network has been disconnected (or is being initialised for the first
     * time).  Perform whatever hardware processing is necessary to bring it up
     * again, or wait for it to be available again.  This is hardware dependent. */
#if 0
    if (xShimWiFiInit() != pdTRUE)
    {
        /* Ideally the network interface initialisation function will only
         * return when the network is available.  In case this is not the case,
         * wait a while before retrying the initialisation. */
        vTaskDelay(INITIALISATION_RETRY_DELAY);
        RINA_NetworkDown();
    }
    vTaskDelay(INITIALISATION_RETRY_DELAY);
#endif
}

bool_t prxHandleAllocateResponse(void)
{
    struct ipcpInstance_t *pxShimInstance;
    pxShimInstance = pvRsMemAlloc(sizeof(*pxShimInstance));

    pxShimInstance = pxIpcManagerActiveShimInstance();
    xShimFlowAllocateResponse(pxShimInstance->pxData, 1);
    return true;
}
