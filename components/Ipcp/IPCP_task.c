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

#include "IPCP_events.h"
#include "IPCP_instance.h"
#include "IPCP_normal_defs.h"
#include "IPCP_normal_api.h"

#include "efcp.h"
#include "enrollment_api.h"
#include "du.h"
#include "flowAllocator_api.h"

#include "rina_api.h"

bool_t xSendEventToIPCPTask(eRINAEvent_t eEvent);

/** @brief Shim Instance that will be the N-1 DIF. The IPCP must query the IPCP manager instance table
 * to get the shim instance activated.
 */
struct ipcpInstance_t *pxShimInstance;

struct du_t *pxMessagePDU;

/** @brief Normal IPCP Data that stores the IPCP information. */
struct ipcpInstanceData_t *pxIpcpData;

/** N-1 PortId that the IPCP is going to be connected. */
static portId_t xN1PortId;

/** @brief Set to pdTRUE when the IPCP task is ready to start processing events. */
static bool_t xIpcpTaskInitialised = false;

/** @brief Stores the handle of the task that handles the stack.  The handle is used
 * (indirectly) by some utility function to determine if the utility function is
 * being called by a task (in which case it is ok to block) or by the IPCP task
 * itself (in which case it is not ok to block). */
static pthread_t xIpcpThread;

/** @brief The queue used to pass events into the IPCP-task for processing. */
RsQueue_t *xStackEventQueue = NULL;

static IPCPTimer_t xFATimer;

static IPCPTimer_t xN1FlowAllocatedTimer;

static IPCPTimer_t xRMTQueueTimer;

#ifdef ESP_PLATFORM
/* This saves the FreeRTOS current task handle. */
static TaskHandle_t xIpcpTaskHandle;
#endif

void prvIpcpFlowRequest(struct ipcpInstance_t *pxShimInstance, portId_t xN1PortId, name_t *pxIPCPName);

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
bool_t xIsCallingFromIpcpTask(void)
{
#ifdef ESP_PLATFORM
    /* On ESP32, we use the FreeRTOS API here because calling
     * pthread_self on a FreeRTOS task will crash the runtime with an
     * assertion failure. You can't call this method on a task that
     * has not been started with the pthread API. */
    return xTaskGetCurrentTaskHandle() == xIpcpTaskHandle;
#else
    return pthread_self() == xIpcpThread;
#endif
}

/**
 * @brief Check the RINA timers and if they are
 *        expired, send an event to the IPCP-Task.
 */
static void prvCheckNetworkTimers(void)
{
    /* Is it time for N1FlowAllocatedTimer processing? */
    // if (bIPCPTimerCheck(&xN1FlowAllocatedTimer) == false)
    //(void)xSendEventToIPCPTask(eShimFATimerEvent);
    //  LOGE(TAG_IPCPNORMAL, "Timer N1 FA run out");
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

    if (xFATimer.bActive && xFATimer.ulRemainingTimeUS < xMaximumSleepTimeUS)
        xMaximumSleepTimeUS = xFATimer.ulRemainingTimeUS;
    if (xN1FlowAllocatedTimer.bActive && xN1FlowAllocatedTimer.ulRemainingTimeUS < xMaximumSleepTimeUS)
        xMaximumSleepTimeUS = xN1FlowAllocatedTimer.ulRemainingTimeUS;

    return xMaximumSleepTimeUS;
}

/** @brief Check if task is ready */
bool_t xIpcpTaskReady(void)
{
    return xIpcpTaskInitialised;
}

/**
 * @brief Send an event (in form of struct) to the IP task to be processed.
 *
 * @param[in] pxEvent: The event to be sent.
 * @param[in] uxTimeout: Timeout for waiting in case the queue is full. 0 for non-blocking calls.
 *
 * @return pdPASS if the event was sent (or the desired effect was achieved). Else, pdFAIL.
 */
bool_t xSendEventStructToIPCPTask(const RINAStackEvent_t *pxEvent, useconds_t xTimeOutUS)
{
    bool_t xReturn, xSendMessage;
    useconds_t xCalculatedTimeOutUS = xTimeOutUS;

    if (!xIpcpTaskReady())
    {
        LOGE(TAG_IPCPMANAGER, "IPCP Task Not Ready, cannot send event");

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
            if ((xIsCallingFromIpcpTask()) && (xCalculatedTimeOutUS > 0))
                xCalculatedTimeOutUS = 0;

            xReturn = xRsQueueSendToBack(xStackEventQueue, pxEvent, sizeof(RINAStackEvent_t), xCalculatedTimeOutUS);

            if (!xReturn)
                LOGE(TAG_IPCPMANAGER, "Failed to add message to IPCP queue");
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
 * @brief Send an event to the IPCP task. It calls 'xSendEventStructToIPCPTask' internally.
 *
 * @param[in] eEvent: The event to be sent.
 *
 * @return pdPASS if the event was sent (or the desired effect was achieved). Else, pdFAIL.
 */
bool_t xSendEventToIPCPTask(eRINAEvent_t eEvent)
{
    RINAStackEvent_t xEventMessage;

    xEventMessage.eEventType = eEvent;
    xEventMessage.xData.PV = (void *)NULL;

    return xSendEventStructToIPCPTask(&xEventMessage, 0);
}

/***
 *  Create the IPCP Task
 * */
static void *prvIpcpTask(void *pvParameters)
{
    RINAStackEvent_t xReceivedEvent;
    struct timespec xNextIPCPSleep;
    useconds_t xSleepTimeUS;
    flowAllocateHandle_t *pxFlowAllocateHandle;

    /* Just to prevent compiler warnings about unused parameters. */
    (void)pvParameters;

#ifdef ESP_PLATFORM
    /* FIXME: Protect with mutex. */
    xIpcpTaskHandle = xTaskGetCurrentTaskHandle();
#endif

    /* Initialization is complete and events can now be processed. */
    xIpcpTaskInitialised = true;

    /* Generate a dummy message to say that the network connection has gone
     *  down.  This will cause this task to initialise the network interface.  After
     *  this it is the responsibility of the network interface hardware driver to
     *  send this message if a previously connected network is disconnected. */

    /* This is not necessary because the Manager task must request the enrollment
     * The enrollment is going to initialize the network interface and connect to the
     * Access Point.  */
    // NetworkDown();

    LOGI(TAG_IPCPMANAGER, "ENTER: IPCP Thread");

    /* Loop, processing IPCP events. */
    for (;;)
    {
        // ipconfigWATCHDOG_TIMER();

        /* Check the RINA timers to see if there is any periodic
         * or timeout processing to perform. */
        // prvCheckNetworkTimers();

        /* Calculate the acceptable maximum sleep time. */
        xSleepTimeUS = prvCalculateSleepTimeUS();

        LOGI(TAG_IPCPNORMAL, "TASK IPCP, %lu ", xSleepTimeUS);

        /* Wait until there is something to do. If the following call exits
         * due to a time out rather than a message being received, set a
         * 'NoEvent' value. */
        if (xRsQueueReceive(xStackEventQueue, (void *)&xReceivedEvent, sizeof(RINAStackEvent_t), xSleepTimeUS) == false)

            xReceivedEvent.eEventType = eNoEvent;

        switch (xReceivedEvent.eEventType)
        {
        case eShimEnrolledEvent:
            /* The IPCP manager sends this event after it receives the notification of the Shim was enrolled.
             * The IPCP manager must send the SHIM instance pòinter to register. Then, the Normal IPCP registers in the shim IPCP.
             * Then, the IPCP must allocate a N1 Port Id and request to shim allocate a flow and bind to that N1 Port. */

            pxShimInstance = (struct ipcpInstance_t *)xReceivedEvent.xData.PV;

            /* Registering into the shim */
            if (!xNormalRegistering(pxShimInstance, pxIpcpData->pxDifName, pxIpcpData->pxName))
            {
                LOGE(TAG_IPCPNORMAL, "IPCP not registered into the shim");
                break;
            } // should be void, the normal should control if there is an error.

            xN1PortId = xIpcMngrAllocatePortId(); // check this

            /* Normal IPCP request a Flow Allocation to the Shim */
            (void)prvIpcpFlowRequest(pxShimInstance, xN1PortId, pxIpcpData->pxName);
            // vIPCPTimerStart(&xN1FlowAllocatedTimer, 10000);

            break;

        case eShimFATimerEvent:

            LOGE(TAG_IPCPNORMAL, "Check if the shim flow was allocated: %d", xN1FlowAllocatedTimer.bActive);

            if (pxIpcpData->pxRmt->pxN1Port->eState == eN1_PORT_STATE_ALLOCATED)
            {
                LOGE(TAG_IPCPNORMAL, "ALLOCATED: %d", pxIpcpData->pxRmt->pxN1Port->eState);
                // xN1FlowAllocatedTimer.bActive = false;
                (void)xEnrollmentInit(pxIpcpData, xN1PortId);
            }
            else
            {
                // vIPCPTimerReload(&xN1FlowAllocatedTimer, 100000);
                LOGE(TAG_IPCPNORMAL, "NO ALLOCATED: %d", pxIpcpData->pxRmt->pxN1Port->eState);
            }

            /*Start the Enrollment Task*/
            //(void)xEnrollmentInit(pxIpcpData, xN1PortId);

            break;

        case eStackFlowAllocateEvent:

            /*Call the FlowAllocator module to handle the FARequest sended by the User APP*/
            LOGI(TAG_IPCPMANAGER, "Flow Allocate event received");
            pxFlowAllocateHandle = ((flowAllocateHandle_t *)xReceivedEvent.xData.PV);
            vFlowAllocatorFlowRequest(pxFlowAllocateHandle->xPortId, pxFlowAllocateHandle);

            break;

        case eFlowBindEvent:

            pxFlowAllocateHandle = (flowAllocateHandle_t *)xReceivedEvent.xData.PV;

            (void)xNormalFlowPrebind(pxIpcpData, pxFlowAllocateHandle);

#if 0
            pxFlowAllocateRequest->xEventBits |= (EventBits_t)eFLOW_BOUND;
            vRINA_WeakUpUser(pxFlowAllocateRequest);
#endif

            vRINA_WakeUpFlowRequest(pxFlowAllocateHandle, eFLOW_BOUND);
            break;

        case eStackRxEvent:

            pxMessagePDU = (struct du_t *)xReceivedEvent.xData.PV;

            if (!xNormalDuEnqueue(pxIpcpData, 1, pxMessagePDU)) // must change this
            {
                LOGI(TAG_IPCPMANAGER, "Drop frame because there is not enough memory space");
                xDuDestroy(pxMessagePDU);
            }

            break;

        case eStackTxEvent:
        {
            // call Efcp to write SDU.
            NetworkBufferDescriptor_t *pxNetBuffer = (NetworkBufferDescriptor_t *)xReceivedEvent.xData.PV;

            (void)xNormalDuWrite(pxIpcpData, pxNetBuffer->ulBoundPort, pxNetBuffer);
        }
        break;
        case eFATimerEvent:
            LOGI(TAG_IPCPMANAGER, "Setting FA timer to expired");
            // vIpcpSetFATimerExpiredState(true);

            break;

        case eNoEvent:
            /* xQueueReceive() returned because of a normal time-out. */
            break;

        default:
            /* Should not get here. */
            break;
        }
    }

    LOGI(TAG_IPCPNORMAL, "EXIT: IPC Thread");
    return NULL;
}

/**
 * @brief Initialize the RINA network stack and initialize the IPCP-task.
 *
 *
 * @return pdPASS if the task was successfully created and added to a ready
 * list, otherwise an error code defined in the file projdefs.h
 */
bool_t xIpcpInit(void)
{
    bool_t xReturn = false;

    LOGI(TAG_IPCPNORMAL, "************* INIT IPCP ***********");

    /* This function should only be called once. */
    RsAssert(xIpcpTaskReady() == false);
    RsAssert(xStackEventQueue == NULL);

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
    xStackEventQueue = pxRsQueueCreate("IPCPQueue",
                                       // EVENT_QUEUE_LENGTH,
                                       10,
                                       sizeof(RINAStackEvent_t));
    RsAssert(xStackEventQueue != NULL);

    if (xStackEventQueue != NULL)
    {

        pthread_attr_t attr;

        if (pthread_attr_init(&attr) != 0)
            return false;

        pthread_attr_setstacksize(&attr, IPCP_TASK_STACK_SIZE);

        if (pthread_create(&xIpcpThread, &attr, prvIpcpTask, NULL) != 0)
        {
            LOGE(TAG_IPCPNORMAL, "RINAInit: failed to start IPC process");
            xReturn = false;
        }
        else
            xReturn = true;

        pthread_attr_destroy(&attr);
    }
    else
        LOGE(TAG_IPCPNORMAL, "RINAInit: Network event queue could not be created\n");

    return xReturn;
}

void prvIpcpFlowRequest(struct ipcpInstance_t *pxShimInstance, portId_t xN1PortId, name_t *pxIPCPName)
{
    struct ipcpInstance_t *pxNormalIpcp;
    /*This should be proposed by the Flow Allocator?*/
    name_t *destinationInfo = pvRsMemAlloc(sizeof(*destinationInfo));
    destinationInfo->pcProcessName = REMOTE_ADDRESS_AP_NAME;
    destinationInfo->pcEntityName = "";
    destinationInfo->pcProcessInstance = REMOTE_ADDRESS_AP_INSTANCE;
    destinationInfo->pcEntityInstance = "";

    /*Query the IPCP instance to send*/

    pxNormalIpcp = pxIpcManagerActiveNormalInstance();

    if (pxShimInstance->pxOps->flowAllocateRequest == NULL)
    {
        LOGI(TAG_IPCPNORMAL, "There is not Flow Allocate Request API");
    }

    if (pxShimInstance->pxOps->flowAllocateRequest(pxShimInstance->pxData, pxNormalIpcp,
                                                   pxIPCPName,
                                                   destinationInfo,
                                                   xN1PortId))
        LOGI(TAG_IPCPNORMAL, "Flow Request processed by the Shim sucessfully");
}

struct rmt_t *pxIPCPGetRmt(void)
{
    return pxIpcpData->pxRmt;
}

struct efcpContainer_t *pxIPCPGetEfcpc(void)
{
    return pxIpcpData->pxEfcpc;
}
