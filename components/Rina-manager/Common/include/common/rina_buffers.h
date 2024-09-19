#ifndef _COMMON_RINA_BUFFERS_H_
#define _COMMON_RINA_BUFFERS_H_

#include "portability/port.h"
#include "common/list.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct xNETWORK_BUFFER
    {
        RsListItem_t xBufferListItem; /**< Used to reference the buffer form the free buffer list. */
        uint8_t ulGpa;                /**< Source or destination Protocol address, depending on usage scenario. */
        uint8_t *pucEthernetBuffer;   /**< Pointer to the start of the Ethernet frame. */
        uint8_t *pucRinaBuffer;       /**< Pointer to the start of the Rina packet. */
        uint8_t *pucDataBuffer;       /**< Pointer to the start of the User Data Unit. */
        size_t xEthernetDataLength;   /**< Starts by holding the total Ethernet frame length, then the Rina payload length. */
        size_t xRinaDataLength;       /**< Starts by holding the total Rina packet length, then the User Data payload length. */
        size_t xDataLength;           /**< Starts by holding the total User Data Unit length. */
        uint32_t ulPort;              /**< Source or destination port, depending on usage scenario. */
        uint32_t ulBoundPort;         /**< The N-1 port to transmite. */

    } NetworkBufferDescriptor_t;

#ifdef __cplusplus
}
#endif

#endif // _COMMON_RINA_BUFFERS_H_