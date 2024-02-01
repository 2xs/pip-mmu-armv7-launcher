/*******************************************************************************/
/*  © Université de Lille, The Pip Development Team (2015-2024)                */
/*                                                                             */
/*  This software is a computer program whose purpose is to run a minimal,     */
/*  hypervisor relying on proven properties such as memory isolation.          */
/*                                                                             */
/*  This software is governed by the CeCILL license under French law and       */
/*  abiding by the rules of distribution of free software.  You can  use,      */
/*  modify and/ or redistribute the software under the terms of the CeCILL     */
/*  license as circulated by CEA, CNRS and INRIA at the following URL          */
/*  "http://www.cecill.info".                                                  */
/*                                                                             */
/*  As a counterpart to the access to the source code and  rights to copy,     */
/*  modify and redistribute granted by the license, users are provided only    */
/*  with a limited warranty  and the software's author,  the holder of the     */
/*  economic rights,  and the successive licensors  have only  limited         */
/*  liability.                                                                 */
/*                                                                             */
/*  In this respect, the user's attention is drawn to the risks associated     */
/*  with loading,  using,  modifying and/or developing or reproducing the      */
/*  software by the user in light of its specific status of free software,     */
/*  that may mean  that it is complicated to manipulate,  and  that  also      */
/*  therefore means  that it is reserved for developers  and  experienced      */
/*  professionals having in-depth computer knowledge. Users are therefore      */
/*  encouraged to load and test the software's suitability as regards their    */
/*  requirements in conditions enabling the security of their systems and/or   */
/*  data to be ensured and,  more generally, to use and operate it in the      */
/*  same conditions as regards security.                                       */
/*                                                                             */
/*  The fact that you are presently reading this means that you have had       */
/*  knowledge of the CeCILL license and that you accept its terms.             */
/*******************************************************************************/

/*!
 * \mainpage
 * The purpose of this project is to illustrate, using a simple example, how to
 * transfer the execution flow from the root partition to a child partition.
 */

/*!
 * \file
 * This file contains the root partition source code
 */

#include <stdint.h>

#include <pip/stdio.h>
#include <pip/fpinfo.h>
#include <pip/paging.h>
#include <pip/vidt.h>
#include <pip/api.h>
#include <pip/wrappers.h>

#include "launcher.h"

/*!
 * \brief Start address of the child partition
 * \note This symbol is defined in the link.ld file
 */
extern void *__startChildAddress;

/*!
 * \brief End address of the child partition
 * \note This symbol is defined in the link.ld file
 */
extern void *__endChildAddress;

/*!
 * \brief The child partition descriptor
 */
uint32_t descChild;

/*
 * Function prototypes
 */
static uint32_t bootstrapPartition(uint32_t base, uint32_t size,
		uint32_t loadAddress);
static void doBootstrap(void);
static void doYield(void);

/*!
 * \brief Handler for the timer interrupt
 */
void timerHandler(void)
{
	printf("A timer interrupt was triggered ...\n");

	// Yield to the child partition
	doYield();

	// Should never be reached
	PANIC();
}

/*!
 * \fn void _main(pip_fpinfo* bootInformations)
 * \brief The root partition entry point called by the 0boot.S file
 * \param bootInformation The boot information address from the ebx register
 * \warning Do not name the entry point "main" because gcc generates an
 *          erroneous machine code: it tries to retrieve the arguments argc
 *          and argv even with the parameters --freestanding and -nostdlib
 */
void _main(pip_fpinfo* bootInformations)
{
	printf("The root partition is booting ...\n");

	// Retrieve the root partition context from the stack top
	user_ctx_t *rootPartitionContext = (user_ctx_t*) (STACK_TOP_VADDR + PAGE_SIZE -
			sizeof(user_ctx_t));

	// Save the context pointer of the root partition into the VIDT
	VIDT[48] = rootPartitionContext;
	VIDT[49] = rootPartitionContext;

	printf("Initializing the memory pages ...\n");
	if (!Pip_InitPaging(bootInformations->membegin,
			bootInformations->memend))
	{
		PANIC();
	}

	// Allocate two interrupt contexts
	user_ctx_t *timerHandlerContext    = Pip_AllocContext();

	// Allocate a page for the handler stack
	uint32_t handlerStackAddress = (uint32_t) Pip_AllocPage();

	// Registration of the interrupt handler
	Pip_RegisterInterrupt(timerHandlerContext, 11, (uint32_t) timerHandler,
			handlerStackAddress, 0);

	printf("Bootstraping the minimal partition ...");
	doBootstrap();

	printf("Yielding to the child partition ...\n");
	doYield();

	// Should never be reached.
	PANIC();
}

/*!
 * \fn static void bootstrapPartition(uint32_t base, uint32_t size,
 *		uint32_t loadAddress)
 * \brief Bootstraping a new child partition at a specific address
 * \param base The start address of the first memory page
 * \param size The size of the child partition to map
 * \param laddr The address where to load the partition
 * \return 0 in the case of a success, greater than zero otherwise
 */
static uint32_t bootstrapPartition(uint32_t base, uint32_t size,
		uint32_t loadAddress)
{
	enum map_page_wrapper_ret_e map_page_rcode;

	// Allocate 5 memory pages in order to create a child partition
	descChild                = (uint32_t) Pip_AllocPage();
	uint32_t pdChild         = (uint32_t) Pip_AllocPage();
	uint32_t shadow1Child    = (uint32_t) Pip_AllocPage();
	uint32_t shadow2Child    = (uint32_t) Pip_AllocPage();
	uint32_t configPagesList = (uint32_t) Pip_AllocPage();

	// Create the child partition
	if (!Pip_CreatePartition(descChild, pdChild, shadow1Child, shadow2Child,
			configPagesList))
	{
		return FAIL_CREATE_PARTITION;
	}

	// Map each page of the child partition to the newly created partition
	for (uint32_t offset = 0; offset < size; offset += PAGE_SIZE)
	{
		map_page_rcode = Pip_MapPageWrapper(base + offset, descChild,
                                                    loadAddress + offset);
		switch (map_page_rcode) {
			case FAIL_ALLOC_PAGE:
				printf("MapPageWrapper failed while allocating a page\n");
				return FAIL_MAP_CHILD_PAGE;
			case FAIL_PREPARE:
				printf("MapPageWrapper failed while trying to give pages to the kernel for memory data structures\n");
				return FAIL_MAP_CHILD_PAGE;
			case FAIL_ADD_VADDR:
				printf("MapPageWrapper failed while trying to add a memory page to the child\n");
				return FAIL_MAP_CHILD_PAGE;
			case SUCCESS :
				break;
			default:
				printf("Unknown MapPageWrapper return code\n");
		}
	}

	// Allocate a page for the child's stack
	uint32_t stackPage = (uint32_t) Pip_AllocPage();

	// Compute the physical address of the child context
	user_ctx_t *contextPAddr = (user_ctx_t*) (stackPage + PAGE_SIZE -
			sizeof(user_ctx_t));

	// Compute the virtual address of the child context
	user_ctx_t *contextVAddr = (user_ctx_t*) (STACK_TOP_VADDR + PAGE_SIZE -
			sizeof(user_ctx_t));

	// Create the child's context
	contextPAddr->valid       = 0;
	contextPAddr->reg[CTX_PC] = loadAddress;
	contextPAddr->reg[CTX_SP] = STACK_TOP_VADDR + PAGE_SIZE - sizeof(user_ctx_t);
	contextPAddr->pipflags    = 0;
	contextPAddr->valid       = 1;

	// Map the stack page to the newly created partition
        map_page_rcode = Pip_MapPageWrapper(stackPage, descChild, STACK_TOP_VADDR);
        switch (map_page_rcode) {
                case FAIL_ALLOC_PAGE:
                        printf("MapPageWrapper failed while allocating a page\n");
                        return FAIL_MAP_STACK_PAGE;
                case FAIL_PREPARE:
                        printf("MapPageWrapper failed while trying to give pages to the kernel for memory data structures\n");
                        return FAIL_MAP_STACK_PAGE;
                case FAIL_ADD_VADDR:
                        printf("MapPageWrapper failed while trying to add a memory page to the child\n");
                        return FAIL_MAP_STACK_PAGE;
                case SUCCESS :
                        break;
                default:
                        printf("Unknown MapPageWrapper return code\n");
        }

	// Allocate a memory page for the child's VIDT
	user_ctx_t **vidtPage = (user_ctx_t**) Pip_AllocPage();

	// Save the child's context into the child's VIDT
	vidtPage[ 0] = contextVAddr;
	vidtPage[48] = contextVAddr;
	vidtPage[49] = contextVAddr;

	// Map the VIDT page to the newly created partition
        map_page_rcode = Pip_MapPageWrapper((uint32_t) vidtPage, descChild, VIDT_VADDR);
        switch (map_page_rcode) {
                case FAIL_ALLOC_PAGE:
                        printf("MapPageWrapper failed while allocating a page\n");
                        return FAIL_MAP_VIDT_PAGE;
                case FAIL_PREPARE:
                        printf("MapPageWrapper failed while trying to give pages to the kernel for memory data structures\n");
                        return FAIL_MAP_VIDT_PAGE;
                case FAIL_ADD_VADDR:
                        printf("MapPageWrapper failed while trying to add a memory page to the child\n");
                        return FAIL_MAP_VIDT_PAGE;
                case SUCCESS :
                        break;
                default:
                        printf("Unknown MapPageWrapper return code\n");
        }

	return 0;
}

/*!
 * \fn static void doBootstrap(void)
 * \brief Do the bootstrap of the child partition and abort if an error occured.
 */
static void doBootstrap(void)
{
	// Retrieve the start and end address of the child partition
	uint32_t startChildAddress = (uint32_t) &__startChildAddress;
	uint32_t endChildAddress   = (uint32_t) &__endChildAddress;

	// Bootstrap the child partition
	uint32_t ret = bootstrapPartition(startChildAddress, endChildAddress -
			startChildAddress, LOAD_VADDRESS);

	switch (ret)
	{
		case 0: return;
		case FAIL_CREATE_PARTITION:
			printf("bootstrapPartition returned "
					"FAIL_CREATE_PARTITION ...\n");
			break;
		case FAIL_MAP_CHILD_PAGE:
			printf("bootstrapPartition returned "
					"FAIL_MAP_CHILD_PAGE ...\n");
			break;
		case FAIL_MAP_STACK_PAGE:
			printf("bootstrapPartition returned "
					"FAIL_MAP_STACK_PAGE ...\n");
			break;
		case FAIL_MAP_VIDT_PAGE:
			printf("bootstrapPartition returned "
					"FAIL_MAP_VIDT_PAGE ...\n");
			break;
		default:
			printf("bootstrapPartition returned "
				"an unexpected value: %d ...\n", ret);
	}

	PANIC();
}

/*!
 * \fn static void doYield(void)
 * \brief Do the yield to the child partition and abort if an error occured.
 */
static void doYield(void)
{
	uint32_t ret = Pip_Yield(descChild, 0, 49, 0, 0);

	switch (ret)
	{
		case 0: return;
		case FAIL_INVALID_INT_LEVEL:
			printf("Pip_Yield returned "
					"FAIL_INVALID_INT_LEVEL ...\n");
			break;
		case FAIL_INVALID_CTX_SAVE_INDEX:
			printf("Pip_Yield returned "
					"FAIL_INVALID_CTX_SAVE_INDEX ...\n");
			break;
		case FAIL_ROOT_CALLER:
			printf("Pip_Yield returned "
					"FAIL_ROOT_CALLER ...\n");
			break;
		case FAIL_INVALID_CHILD:
			printf("Pip_Yield returned "
					"FAIL_INVALID_CHILD ...\n");
			break;
		case FAIL_UNAVAILABLE_TARGET_VIDT:
			printf("Pip_Yield returned "
					"FAIL_UNAVAILABLE_TARGET_VIDT ...\n");
			break;
		case FAIL_UNAVAILABLE_CALLER_VIDT:
			printf("Pip_Yield returned "
					"FAIL_UNAVAILABLE_CALLER_VIDT ...\n");
			break;
		case FAIL_MASKED_INTERRUPT:
			printf("Pip_Yield returned "
					"FAIL_MASKED_INTERRUPT ...\n");
			break;
		case FAIL_UNAVAILABLE_TARGET_CTX:
			printf("Pip_Yield returned "
					"FAIL_UNAVAILABLE_TARGET_CTX ...\n");
			break;
		case FAIL_CALLER_CONTEXT_SAVE:
			printf("Pip_Yield returned "
					"FAIL_CALLER_CONTEXT_SAVE ...\n");
			break;
		default:
			printf("Pip_Yield returned an unexpected value: "
					"0x%x ...\n", ret);
	}

	PANIC();
}
