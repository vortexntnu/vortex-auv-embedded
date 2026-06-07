/*
 * fast_mdma.c
 *
 *  Created on: 23. mar. 2026
 *      Author: vikin
 */

#include "embedded_macros.h"
#include "arm_math_types.h"
#include "fast_mdma.h"


static void fast_MDMA_set_config(MDMA_HandleTypeDef *hmdma, uint32_t SrcAddress, uint32_t DstAddress, uint32_t BlockDataLength, uint32_t BlockCount)
{
	  uint32_t addressMask;

	/* Configure the MDMA Channel data length */
	  MODIFY_REG(hmdma->Instance->CBNDTR ,MDMA_CBNDTR_BNDT, (BlockDataLength & MDMA_CBNDTR_BNDT));

	  /* Configure the MDMA block repeat count */
	  MODIFY_REG(hmdma->Instance->CBNDTR , MDMA_CBNDTR_BRC , ((BlockCount - 1U) << MDMA_CBNDTR_BRC_Pos) & MDMA_CBNDTR_BRC);

	  /* Clear all interrupt flags */
	  __HAL_MDMA_CLEAR_FLAG(hmdma, MDMA_FLAG_TE | MDMA_FLAG_CTC | MDMA_CISR_BRTIF | MDMA_CISR_BTIF | MDMA_CISR_TCIF);

	  /* Configure MDMA Channel destination address */
	  hmdma->Instance->CDAR = DstAddress;

	  /* Configure MDMA Channel Source address */
	  hmdma->Instance->CSAR = SrcAddress;

    addressMask = SrcAddress & 0xFF000000U;
    if(unlikely((addressMask == 0x20000000U) || (addressMask == 0x00000000U)))
    {
      /*The AHBSbus is used as source (read operation) on channel x */
      hmdma->Instance->CTBR |= MDMA_CTBR_SBUS;
    }
    else
    {
      /*The AXI bus is used as source (read operation) on channel x */
      hmdma->Instance->CTBR &= (~MDMA_CTBR_SBUS);
    }

    addressMask = DstAddress & 0xFF000000U;
    if(likely((addressMask == 0x20000000U) || (addressMask == 0x00000000U)))
    {
      /*The AHB bus is used as destination (write operation) on channel x */
      hmdma->Instance->CTBR |= MDMA_CTBR_DBUS;
    }
    else
    {
      /*The AXI bus is used as destination (write operation) on channel x */
      hmdma->Instance->CTBR &= (~MDMA_CTBR_DBUS);
    }

    /* Set the linked list register to the first node of the list */
    hmdma->Instance->CLAR = (uint32_t)hmdma->FirstLinkedListNodeAddress;
    // copy any other register writes from HAL source
}

HAL_StatusTypeDef fast_MDMA_copy_block(q15_t *src, q15_t *dst, MDMA_HandleTypeDef* hdma)
{
		/* Process locked */
		__HAL_LOCK(hdma);

		/* Change MDMA peripheral state */
		hdma->State = HAL_MDMA_STATE_BUSY;

	    /* Initialize the error code */
	    hdma->ErrorCode = HAL_MDMA_ERROR_NONE;

	    /* Disable the peripheral */
	    __HAL_MDMA_DISABLE(hdma);

	    /* Configure the source, destination address and the data length */
	    fast_MDMA_set_config(hdma, (uint32_t)src, (uint32_t)dst, MDMA_BLOCK_SIZE*sizeof(q15_t), 1);

	    /* Enable Common interrupts i.e Transfer Error IT and Channel Transfer Complete IT*/
	    __HAL_MDMA_ENABLE_IT(hdma, (MDMA_IT_TE | MDMA_IT_CTC));


	    /* Enable the Peripheral */
	    __HAL_MDMA_ENABLE(hdma);

	    /* activate If SW request mode*/
	    hdma->Instance->CCR |=  MDMA_CCR_SWRQ;

	    return HAL_OK;
}

void fast_MDMA_wait_complete(void) {
    while (!(MDMA_Channel0->CISR & MDMA_CISR_CTCIF));      // wait for transfer complete
    MDMA_Channel0->CIFCR = MDMA_CIFCR_CCTCIF;              // clear the flag
}
