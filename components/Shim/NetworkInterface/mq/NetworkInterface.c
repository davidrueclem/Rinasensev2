#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>

#include "configSensor.h"
#include "portability/port.h"
#include "common/rina_gpha.h"

#include "IPCP_api.h"
#include "IPCP_events.h"
#include "BufferManagement.h"
#include "NetworkInterface.h"
#include "NetworkInterface_mq.h"

/* The 'mq' NetworkInterface variant uses POSIX messages queues to
 * emulate a network interface. This is to be used for testing
 * only. */

/* Incoming queue. */
static mqd_t mqIn;

/* Outgoing queue */
static mqd_t mqOut;

/* MAC address + 3 bytes for 'OUT' + 1 (-) + 1 (\0) */
#define QUEUE_NAME_MIN_BUFSZ MAC2STR_MIN_BUFSZ + 6

/* MAC address. */
static char sMac[MAC2STR_MIN_BUFSZ] = {0};

/* Queue names */
static char sInQueueName[QUEUE_NAME_MIN_BUFSZ] = {0};
static char sOutQueueName[QUEUE_NAME_MIN_BUFSZ] = {0};

static struct mq_attr mqInitAttr = {
    .mq_flags = 0,
    .mq_maxmsg = 6,
    .mq_msgsize = 1500,
    .mq_curmsgs = 0};

/* Test API */

#ifdef ESP_PLATFORM
pthread_t xNotifyThreadID;

void *xNotifyThread()
{
    struct mq_attr mqattr;
    char *buf;

    if (mq_getattr(mqIn, &mqattr) < 0)
        LOGE(TAG_WIFI, "Failed to get size inbound packet");

    while (true)
    {
        buf = pvRsMemAlloc(mqattr.mq_msgsize);

        if (mq_receive(mqIn, buf, mqattr.mq_msgsize, NULL) < 0)
            LOGE(TAG_WIFI, "Failed to read inbound packet");

        LOGD(TAG_WIFI, "Received %lu bytes for the RINA stack", mqattr.mq_msgsize);

        if (!xNetworkInterfaceInput(buf, mqattr.mq_msgsize, NULL))
            LOGE(TAG_WIFI, "RX processing failed");

        vRsMemFree(buf);
    }
}

void vCreateNotifyThread()
{
    pthread_attr_t attr;

    /* We're running on a tiny ESP32 device so we'll skimp on the
     * thoughful error handling. If anything below doesn't work,
     * nothing else will, so we want it to crash ASAP. */

    RsAssert(!pthread_attr_init(&attr));
    RsAssert(!pthread_create(&xNotifyThreadID, &attr, xNotifyThread, NULL));

    LOGI(TAG_WIFI, "Started notification thread for %s", sInQueueName);

    pthread_attr_destroy(&attr);
}
#endif

long xMqNetworkInterfaceOutputCount()
{
    struct mq_attr attr;
    int n;

    n = mq_getattr(mqOut, &attr);

    RsAssert(mqOut != (mqd_t)-1);
    RsAssert(errno != EBADF);
    RsAssert(n == 0);

    return attr.mq_curmsgs;
}

/*
 * Read a message from the message queue.
 *
 * Use this to check what was written by xNetworkInterfaceOutput.
 */
bool_t xMqNetworkInterfaceReadOutput(string_t data, size_t sz, ssize_t *pnBytesRead)
{
    ssize_t n;

    if ((n = mq_receive(mqOut, data, sz, NULL)) == -1)
    {
        LOGE(TAG_WIFI, "xMqNetworkInterfaceReadOutput failed (errno: %d)", errno);
        return false;
    }
    if (pnBytesRead != NULL)
        *pnBytesRead = n;

    return true;
}

/*
 * Remove all the messages waiting in the outgoing queue.
 */
void xMqNetworkInterfaceOutputDiscard()
{
    long n;
    char *psBuf;

    n = xMqNetworkInterfaceOutputCount();

    psBuf = pvRsMemAlloc(mqInitAttr.mq_msgsize);

    for (long i = 0; i < n; i++)
        xMqNetworkInterfaceReadOutput(psBuf, mqInitAttr.mq_msgsize, NULL);
}

bool_t xMqNetworkInterfaceWriteInput(string_t data, size_t sz)
{
    RsAssert((int)sz >= mqInitAttr.mq_msgsize);

    if (mq_send(mqIn, data, sz, 0) < 0)
        return false;

    return true;
}

/* Event handler */

#ifndef ESP_PLATFORM
static void event_handler(union sigval sv)
{
    struct mq_attr mqattr;
    char *buf;

    if (mq_getattr(mqIn, &mqattr) < 0)
        LOGE(TAG_WIFI, "Failed to get size inbound packet");

    buf = pvRsMemAlloc(mqattr.mq_msgsize);

    if (mq_receive(mqIn, buf, mqattr.mq_msgsize, NULL) < 0)
        LOGE(TAG_WIFI, "Failed to read inbound packet");

    LOGD(TAG_WIFI, "Received %lu bytes for the RINA stack", mqattr.mq_msgsize);

    if (!xNetworkInterfaceInput(buf, mqattr.mq_msgsize, NULL))
        LOGE(TAG_WIFI, "RX processing failed");

    vRsMemFree(buf);
}
#endif // ESP_PLATFORM

/* Public interface */

bool_t xNetworkInterfaceInitialise(MACAddress_t *pxPhyDev)
{
    /* Save the MAC address to use as queue names. */
    mac2str(pxPhyDev, sMac, sizeof(sMac));
    return true;
}

/* Format the queue name according to $MAC-[IN|OUT] */
void prvFormatQueueName(bool_t isIn, const char *sMac, char *pxBuf, const size_t nBufSz)
{
    const char fmt[] = "/%s-%s";

    RsAssert(nBufSz >= QUEUE_NAME_MIN_BUFSZ);

    if (isIn)
        snprintf(pxBuf, nBufSz, fmt, "IN", sMac);
    else
        snprintf(pxBuf, nBufSz, fmt, "OUT", sMac);
}

bool_t xNetworkInterfaceConnect(void)
{
#ifndef ESP_PLATFORM
    struct sigevent se;
#endif

    prvFormatQueueName(true, sMac, sInQueueName, sizeof(sInQueueName));
    prvFormatQueueName(false, sMac, sOutQueueName, sizeof(sOutQueueName));

    LOGI(TAG_WIFI, "Creating IN queue: %s", sInQueueName);

    mqIn = mq_open(sInQueueName, O_RDWR | O_CREAT, 0644, &mqInitAttr);
    if (mqIn == (mqd_t)-1)
    {
        LOGE(TAG_WIFI, "Failed to create inbound queue");
        goto err;
    }

    LOGI(TAG_WIFI, "Creating OUT queue: %s", sOutQueueName);

    mqOut = mq_open(sOutQueueName, O_RDWR | O_CREAT, 0644, &mqInitAttr);
    if (mqOut == (mqd_t)-1)
    {
        LOGE(TAG_WIFI, "Failed to create outbound queue");
        goto err;
    }

#ifdef ESP_PLATFORM
    /* mq_notify isn't support on ESP32 so we'll create a thread to do
     * the same job. */
    vCreateNotifyThread();
#else
    /* Use mq_notify on POSIX because it's certainly better tested
     * than xCreateNotifyThread. */

#ifndef NDEBUG
    memset(&se, 0, sizeof(se));
#endif

    se.sigev_notify = SIGEV_THREAD;
    se.sigev_notify_function = event_handler;
    se.sigev_notify_attributes = NULL;
    se.sigev_value.sival_ptr = &mqIn;

    /* Get notified of INCOMING messages. */
    if (mq_notify(mqIn, &se) == (mqd_t)-1)
    {
        LOGE(TAG_WIFI, "Failed to setup notification for incoming messages");
        goto err;
    }
#endif // ESP_PLATFORM

    LOGI(TAG_WIFI, "MQ-NetworkInterface initialized");

    return true;

err:
    if (mqIn != (mqd_t)-1)
        mq_close(mqIn);
    if (mqOut != (mqd_t)-1)
        mq_close(mqOut);

    return false;
}

bool_t xNetworkInterfaceDisconnect(void)
{
    int r1 = 0, r2 = 0;

    if (mqIn != (mqd_t)-1)
    {
        r1 = mq_close(mqIn);
        mq_unlink(sInQueueName);
    }
    if (mqOut != (mqd_t)-1)
    {
        r2 = mq_close(mqOut);
        mq_unlink(sOutQueueName);
    }

    mqIn = 0;
    mqOut = 0;

    return r1 == 0 && r2 == 0;
}

bool_t xNetworkInterfaceOutput(NetworkBufferDescriptor_t *const pxNetworkBuffer,
                               bool_t xReleaseAfterSend)
{
    if (mq_send(mqOut, (const char *)pxNetworkBuffer->pucEthernetBuffer,
                pxNetworkBuffer->xDataLength, 0) != 0)
    {
        LOGE(TAG_WIFI, "Error writing %zu bytes to network interface (errno: %d)",
             pxNetworkBuffer->xDataLength, errno);
        return false;
    }

    LOGI(TAG_WIFI, "Wrote %zu bytes to network interface", pxNetworkBuffer->xDataLength);

    return true;
}

bool_t xNetworkInterfaceInput(void *buffer, uint16_t len, void *eb)
{
    NetworkBufferDescriptor_t *pxNetworkBuffer;
    RINAStackEvent_t xRxEvent;

    pxNetworkBuffer = pxGetNetworkBufferWithDescriptor(len, 0);
    xRxEvent.eEventType = eNetworkRxEvent;

    if (pxNetworkBuffer != NULL)
    {
        /* Set the packet size, in case a larger buffer was returned. */
        pxNetworkBuffer->xDataLength = len;

        /* Copy the packet data. */
        memcpy(pxNetworkBuffer->pucEthernetBuffer, buffer, len);
        xRxEvent.xData.PV = (void *)pxNetworkBuffer;

        if (xSendEventStructToIPCPTask(&xRxEvent, 0) == false)
        {
            LOGE(TAG_WIFI, "Failed to enqueue packet to network stack %p, len %d", buffer, len);
            vReleaseNetworkBufferAndDescriptor(pxNetworkBuffer);
            return false;
        }

        return true;
    }

    else
    {
        LOGE(TAG_WIFI, "Failed to get buffer descriptor");
        vReleaseNetworkBufferAndDescriptor(pxNetworkBuffer);
        return false;
    }
}

void vNetworkNotifyIFDown()
{
}

void vNetworkNotifyIFUp()
{
}
