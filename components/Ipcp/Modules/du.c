/*
 * du.c
 *
 *  Created on: 30 sept. 2021
 *      Author: i2CAT
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "BufferManagement.h"

#include "du.h"
#include "pci.h"

#define TAG_DU "[DTP]"

bool_t xDuDestroy(struct du_t *pxDu)
{
	/* If there is an NetworkBuffer then release and release memory */
	if (pxDu->pxNetworkBuffer)
	{
		LOGD(TAG_DU, "Destroying du struct and releasing Buffer");
		vReleaseNetworkBufferAndDescriptor(pxDu->pxNetworkBuffer);
	}
	vRsMemFree(pxDu);

	return true;
}

bool_t xDuDecap(struct du_t *pxDu)
{
	LOGD(TAG_DU, "Decapsulating the DU");
	pduType_t xType;
	pci_t *pxPciTmp;
	size_t uxPciLen;
	NetworkBufferDescriptor_t *pxNewBuffer;
	uint8_t *pucData;
	size_t uxBufferSize;

	/* Extract PCI from buffer*/
	pxPciTmp = vCastPointerTo_pci_t(pxDu->pxNetworkBuffer->pucRinaBuffer);

	xType = pxPciTmp->xType;
	if (!pdu_type_is_ok(xType))
	{
		LOGE(TAG_DU, "Could not decap DU. Type is not ok");
		return true;
	}

	uxPciLen = (size_t)(14); /* PCI defined static for this initial stage = 14Bytes*/

	uxBufferSize = pxDu->pxNetworkBuffer->xRinaDataLength - uxPciLen;

	// LOGE(TAG_ARP, "Taking Buffer to copy the SDU from the RINA PDU: DuDecap");
	pxNewBuffer = pxGetNetworkBufferWithDescriptor(uxBufferSize, 1000);
	if (pxNewBuffer == NULL)
	{
		LOGE(TAG_DU, "NO buffer was allocated to do the Decap");
		return false;
	}
	pxNewBuffer->xDataLength = uxBufferSize;

	pucData = (uint8_t *)pxPciTmp + 14;
	pxDu->pxNetworkBuffer->xDataLength = uxBufferSize;
	pxDu->pxNetworkBuffer->pucDataBuffer = pucData;

	// memcpy(pxNewBuffer->pucEthernetBuffer, pucPtr, xBufferSize);

	pxDu->pxPci = pxPciTmp;

	LOGD(TAG_DU, "------------ PCI DT DECAP-----------");
	LOGD(TAG_DU, "PCI Version: 0x%04x", pxDu->pxPci->ucVersion);
	LOGD(TAG_DU, "PCI SourceAddress: 0x%04x", pxDu->pxPci->xSource);
	LOGD(TAG_DU, "PCI DestinationAddress: 0x%04x", pxDu->pxPci->xDestination);
	LOGD(TAG_DU, "PCI QoS: 0x%04x", pxDu->pxPci->connectionId_t.xQosId);
	LOGD(TAG_DU, "PCI CEP Source: 0x%04x", pxDu->pxPci->connectionId_t.xSource);
	LOGD(TAG_DU, "PCI CEP Destination: 0x%04x", pxDu->pxPci->connectionId_t.xDestination);
	LOGD(TAG_DU, "PCI FLAG: 0x%04x", pxDu->pxPci->xFlags);
	LOGD(TAG_DU, "PCI Type: 0x%04x", pxDu->pxPci->xType);
	LOGD(TAG_DU, "PCI SequenceNumber: 0x%08x", pxDu->pxPci->xSequenceNumber);
	LOGD(TAG_DU, "PCI xPDULEN: 0x%04x", pxDu->pxPci->xPduLen);

	/*
		pxDu->pxPci->ucVersion = pxPciTmp->ucVersion;
		pxDu->pxPci->xSource = pxPciTmp->xSource;
		pxDu->pxPci->xDestination = pxPciTmp->xDestination;
		pxDu->pxPci->xFlags = pxPciTmp->xFlags;
		pxDu->pxPci->xPduLen = pxPciTmp->xPduLen;
		pxDu->pxPci->xSequenceNumber = pxPciTmp->xSequenceNumber;
		pxDu->pxPci->xType = pxPciTmp->xType;
		pxDu->pxPci->connectionId_t = pxPciTmp->connectionId_t;*/

	// LOGE(TAG_DU, "Releasing Buffer after copy the SDU from the RINA PDU:DuDcap");
	// vReleaseNetworkBufferAndDescriptor(pxDu->pxNetworkBuffer);
	// pxDu->pxNetworkBuffer = pxNewBuffer;

	return false;
}

ssize_t xDuDataLen(const struct du_t *pxDu)
{
	if (pxDu->pxPci->xPduLen != pxDu->pxNetworkBuffer->xRinaDataLength) /* up direction */
		return pxDu->pxNetworkBuffer->xRinaDataLength;
	return (pxDu->pxNetworkBuffer->xDataLength - pxDu->pxPci->xPduLen); /* down direction */
}

size_t xDuLen(const struct du_t *pxDu)
{
	return pxDu->pxNetworkBuffer->xRinaDataLength;
}

bool_t xDuEncap(struct du_t *pxDu, pduType_t xType)
{
	size_t uxPciLen;
	NetworkBufferDescriptor_t *pxNewBuffer;
	uint8_t *pucDataPtr;
	size_t xBufferSize;
	pci_t *pxPciTmp;
	
	uxPciLen = (size_t)(14); /* Default PCI length */

	/* New Size = Data Size more the PCI size defined by default. */
	xBufferSize = pxDu->pxNetworkBuffer->xDataLength + uxPciLen;

	// LOGE(TAG_DU, "Taking Buffer to encap PDU");
	pxNewBuffer = pxGetNetworkBufferWithDescriptor(xBufferSize, 1000);

	if (!pxNewBuffer)
	{
		LOGE(TAG_DU, "Buffer was not allocated properly");
		return false;
	}

	pucDataPtr = (uint8_t *)(pxNewBuffer->pucEthernetBuffer + 14);

	memcpy(pucDataPtr, pxDu->pxNetworkBuffer->pucEthernetBuffer,
		   pxDu->pxNetworkBuffer->xDataLength);

	vReleaseNetworkBufferAndDescriptor(pxDu->pxNetworkBuffer);

	pxNewBuffer->xDataLength = xBufferSize;

	pxDu->pxNetworkBuffer = pxNewBuffer;

	pxPciTmp = vCastPointerTo_pci_t(pxDu->pxNetworkBuffer->pucEthernetBuffer);

	pxDu->pxPci = pxPciTmp;

	return true;
}

bool_t xDuIsOk(const struct du_t *pxDu)
{
	return (pxDu && pxDu->pxNetworkBuffer ? true : false);
}
