#include <string.h>

#include "common/mac.h"
#include "common/rina_gpha.h"
#include "portability/port.h"

/* No used check if this is necessary */
char *xGPAAddressToString(const gpa_t *pxGpa)
{
	char *tmp, *p;

	if (!xIsGPAOK(pxGpa))
	{
		LOGE(TAG_ARP, "Bad input parameter, "
					  "cannot get a meaningful address from GPA");
		return NULL;
	}

	tmp = pvRsMemAlloc(pxGpa->uxLength + 1);
	if (!tmp)
		return NULL;

	memcpy(tmp, pxGpa->pucAddress, pxGpa->uxLength);

	p = tmp + pxGpa->uxLength;
	*(p) = '\0';

	return tmp;
}

gpa_t *pxNameToGPA(const name_t *pcName)
{
	gpa_t *pxGpa;
	string_t pcTmp;

	pcTmp = pcNameToString(pcName);

	if (!pcTmp)
	{
		LOGI(TAG_ARP, "Name to String not correct");
		return NULL;
	}

	// Convert the IPCPAddress Concatenated to bits
	pxGpa = pxCreateGPA((buffer_t)pcTmp, strlen(pcTmp)); // considering the null terminated

	if (!pxGpa)
	{
		LOGI(TAG_ARP, "GPA was not created correct");
		vRsMemFree(pcTmp);
		return NULL;
	}

	vRsMemFree(pcTmp);

	return pxGpa;
}

buffer_t pucCreateAddress(size_t uxLength)
{
	buffer_t pucAddress;

	pucAddress = pvRsMemAlloc(uxLength);
	memset(pucAddress, 0, uxLength);

	return pucAddress;
}

/** Create a GPA object from a buffer
 *
 * @param pucAddress Address of the buffer
 *
 */
gpa_t *pxCreateGPA(const buffer_t pucAddress, size_t uxLength)
{
	gpa_t *pxGPA;

	if (!pucAddress || uxLength == 0)
	{
		LOGI(TAG_ARP, "Bad input parameters, cannot create GPA");
		return NULL;
	}

	pxGPA = pvRsMemAlloc(sizeof(*pxGPA));

	if (!pxGPA)
		return NULL;

	pxGPA->uxLength = uxLength;							// strlen of the address without '\0'
	pxGPA->pucAddress = pucCreateAddress(uxLength + 1); // Create an address an include the '\0'

	if (!pxGPA->pucAddress)
	{
		vRsMemFree(pxGPA);
		return NULL;
	}

	memcpy(pxGPA->pucAddress, pucAddress, pxGPA->uxLength);

	LOGI(TAG_ARP, "CREATE GPA address: %s", pxGPA->pucAddress);
	LOGI(TAG_ARP, "CREATE GPA size: %zu", pxGPA->uxLength);

	return pxGPA;
}

gha_t *pxCreateGHA(eGHAType_t xType, const MACAddress_t *pxAddress) // Changes to uint8_t
{
	gha_t *pxGha;

	if (xType == MAC_ADDR_802_3 || xType == MAC_ADDR_802_15_4)
	{
		pxGha = pvRsMemAlloc(sizeof(*pxGha));
		if (!pxGha)
			return NULL;

		pxGha->xType = xType;
		memcpy(pxGha->xAddress.ucBytes, pxAddress->ucBytes, sizeof(pxGha->xAddress));
		if(xType == MAC_ADDR_802_15_4)
		{
			
		}

		return pxGha;
	}

	LOGE(TAG_ARP, "Wrong input parameters, cannot create GHA");
	return NULL;
}

bool_t xIsGPAOK(const gpa_t *pxGpa)
{
	if (!pxGpa)
	{
		LOGI(TAG_ARP, " !Gpa");
		return false;
	}

	if (pxGpa->pucAddress == NULL)
	{
		LOGI(TAG_ARP, "xIsGPAOK Address is NULL");
		return false;
	}

	if (pxGpa->uxLength == 0)
	{
		LOGI(TAG_ARP, "Length = 0");
		return false;
	}
	return true;
}

bool_t xIsGHAOK(const gha_t *pxGha)
{
	if (!pxGha)
	{
		LOGI(TAG_ARP, "No Valid GHA");
		return false;
	}

	if (pxGha->xType == MAC_ADDR_802_3 || pxGha->xType == MAC_ADDR_802_15_4)
	{
		return true;
	}

	return false;
}

bool_t xGPACmp(const gpa_t *gpa1, const gpa_t *gpa2)
{
	return xIsGPAOK(gpa1) && xIsGPAOK(gpa2) && gpa1->uxLength == gpa2->uxLength && memcmp(gpa1->pucAddress, gpa2->pucAddress, gpa1->uxLength) == 0;
}

void vGPADestroy(gpa_t *pxGpa)
{
	if (!xIsGPAOK(pxGpa))
	{
		return;
	}

	vRsMemFree(pxGpa->pucAddress);

	/* Invalidate the GPA so as to not accidentally reuse one that was
	 * freed. */
	/* FIXME: Make this part of debug builds only. */
	memset(pxGpa, 0, sizeof(gpa_t));

	vRsMemFree(pxGpa);

	return;
}

void vGHADestroy(gha_t *pxGha)
{
	if (!xIsGHAOK(pxGha))
	{
		return;
	}

	/* Invalid the GHA so as to not accidentally reuse one that was
	 * freed. */
	/* FIXME: Make part of debug builds only. */
	memset(pxGha, 0, sizeof(gha_t));

	vRsMemFree(pxGha);

	return;
}
