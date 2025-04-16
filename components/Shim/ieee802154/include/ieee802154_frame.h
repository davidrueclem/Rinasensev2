#ifndef _COMPONENTS_SHIM_IEEE802154_INCLUDE_IEEE802154_FRAME_H
#define _COMPONENTS_SHIM_IEEE802154_INCLUDE_IEEE802154_FRAME_H

#include "common/rina_ids.h"
#include "common/rina_gpha.h"
#include <stdint.h>
#include "common/rina_common_port.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define FRAME_VERSION_STD_2003 0
#define FRAME_VERSION_STD_2006 1
#define FRAME_VERSION_STD_2015 2

#define ADDR_MODE_NONE (0)     // PAN ID and address fields are not present
#define ADDR_MODE_RESERVED (1) // Reseved
#define ADDR_MODE_SHORT (2)    // Short address (16-bit)
#define ADDR_MODE_LONG (3)     // Extended address (64-bit)
                               /*
                               #define FRAME_TYPE_BEACON (0)
                               #define FRAME_TYPE_DATA (1)
                               #define FRAME_TYPE_ACK (2)
                               #define FRAME_TYPE_MAC_COMMAND (3)
                               #define FRAME_TYPE_RESERVED (4)
                               #define FRAME_TYPE_MULTIPURPOSE (5)
                               #define FRAME_TYPE_FRAGMENT (6)
                               #define FRAME_TYPE_EXTENDED (7)*/

    typedef enum
    {
        eFRAME_TYPE_BEACON = 0,
        eFRAME_TYPE_DATA = 1,
        eFRAME_TYPE_ACK = 2,
        eFRAME_TYPE_MAC_COMMAND = 3
    } ieee802154_frame_type_t;

    typedef struct
    {
        uint8_t mode; // ADDR_MODE_NONE || ADDR_MODE_SHORT || ADDR_MODE_LONG
        union
        {
            uint16_t short_address;
            uint8_t long_address[8];
        };
    } ieee802154_address_t;

    typedef struct __attribute__((packed))
    {
        uint8_t ucFrameType : 3;
        uint8_t ucSecure : 1;
        uint8_t ucFramePending : 1;
        uint8_t ucAckReqd : 1;
        uint8_t ucPanIdCompressed : 1;
        uint8_t ucRfu1 : 1;
        uint8_t ucSequenceNumberSuppression : 1;
        uint8_t ucInformationElementsPresent : 1;
        uint8_t ucDestAddrType : 2;
        uint8_t ucFrameVer : 2;
        uint8_t ucSrcAddrType : 2;
    } ieee802154Header_t;

    typedef struct mac_fcs
    {
        uint8_t frameType : 3;
        uint8_t secure : 1;
        uint8_t framePending : 1;
        uint8_t ackReqd : 1;
        uint8_t panIdCompressed : 1;
        uint8_t rfu1 : 1;
        uint8_t sequenceNumberSuppression : 1;
        uint8_t informationElementsPresent : 1;
        uint8_t destAddrType : 2;
        uint8_t frameVer : 2;
        uint8_t srcAddrType : 2;
    } mac_fcs_t;

    uint8_t ieee802154_header(const uint16_t *src_pan, ieee802154_address_t *src, const uint16_t *dst_pan,
                              ieee802154_address_t *dst, uint8_t ack, uint8_t *header, uint8_t header_length);

    typedef enum IEEE802154_FRAMES_PROCESSING
    {
        eIeee802154ReleaseBuffer = 0,   /* Processing the frame did not find anything to do - just release the buffer. */
        eIeee802154ProcessBuffer,       /* An Ethernet frame has a valid address - continue process its contents. */
        eIeee802154ReturnEthernetFrame, /* The Ethernet frame contains an ARP826 packet that can be returned to its source. */
        eIeee802154FrameConsumed        /* Processing the Ethernet packet contents resulted in the payload being sent to the stack. */
    } eFrameResult_t;
    void vHandleIEEE802154Frame(NetworkBufferDescriptor_t *pxNetworkBuffer);
    void vIeee802154FrameSend(uint8_t *pucBuffer, uint16_t usLength);
    esp_err_t xProcessIEEE802154Packet(NetworkBufferDescriptor_t *pxNetworkBuffer);

    ieee802154_address_t *vCastPointerTo_Ieee802154Header_t(void *pvArgument);
#ifdef __cplusplus
}
#endif

#endif /* _COMPONENTS_SHIM_IEEE802154_INCLUDE_IEEE802154_IPCP_H*/