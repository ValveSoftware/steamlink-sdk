/******************************************************************************* 
* Copyright (C) Marvell International Ltd. and its affiliates 
* 
* Marvell GPL License Option 
* 
* If you received this File from Marvell, you may opt to use, redistribute and/or 
* modify this File in accordance with the terms and conditions of the General 
* Public License Version 2, June 1991 (the "GPL License"), a copy of which is 
* available along with the File in the license.txt file or by writing to the Free 
* Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 or 
* on the worldwide web at http://www.gnu.org/licenses/gpl.txt.  
* 
* THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED 
* WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY 
* DISCLAIMED.  The GPL License provides additional details about this warranty 
* disclaimer.  
********************************************************************************/

#ifndef __gfx_linux_common_h_
#define __gfx_linux_common_h_

#ifdef __cplusplus
extern "C"
{
#endif //def __cplusplus

#define IN
#define OUT

#ifndef NULL
#define NULL 0
#endif
#define SIZEOF(x) (sizeof(x))

#define sysOutput printf

#define gcvFALSE				0
#define gcvTRUE					1

#define gcvINFINITE				((gctUINT32) ~0)

    typedef int						gctBOOL;
    typedef gctBOOL *				gctBOOL_PTR;

    typedef int						gctINT;
    typedef signed char				gctINT8;
    typedef signed short			gctINT16;
    typedef signed int				gctINT32;
    typedef signed long long		gctINT64;

    typedef gctINT *				gctINT_PTR;
    typedef gctINT8 *				gctINT8_PTR;
    typedef gctINT16 *				gctINT16_PTR;
    typedef gctINT32 *				gctINT32_PTR;
    typedef gctINT64 *				gctINT64_PTR;

    typedef unsigned int			gctUINT;
    typedef unsigned char			gctUINT8;
    typedef unsigned short			gctUINT16;
    typedef unsigned int			gctUINT32;
    typedef unsigned long long		gctUINT64;

    typedef gctUINT *				gctUINT_PTR;
    typedef gctUINT8 *				gctUINT8_PTR;
    typedef gctUINT16 *				gctUINT16_PTR;
    typedef gctUINT32 *				gctUINT32_PTR;
    typedef gctUINT64 *				gctUINT64_PTR;

    typedef unsigned long			gctSIZE_T;
    typedef gctSIZE_T *				gctSIZE_T_PTR;

#ifdef __cplusplus
#	define gcvNULL				0
#else
#	define gcvNULL				((void *) 0)
#endif

    typedef float					gctFLOAT;
    typedef signed int				gctFIXED_POINT;
    typedef float *					gctFLOAT_PTR;

    typedef void *					gctPHYS_ADDR;
    typedef void *					gctHANDLE;
    typedef void *					gctFILE;
    typedef void *					gctSIGNAL;
    typedef void *					gctWINDOW;
    typedef void *					gctIMAGE;

    typedef void *					gctPOINTER;
    typedef const void *			gctCONST_POINTER;

    typedef char					gctCHAR;
    typedef char *					gctSTRING;
    typedef const char *			gctCONST_STRING;



    /******************************************************************************\
    ********************************* Status Macros ********************************
    \******************************************************************************/
#	define gcmVERIFY_ARGUMENT(arg) \
			do \
			{ \
				if (!(arg)) \
				{ \
					return gcvSTATUS_INVALID_ARGUMENT; \
				} \
			} \
			while (gcvFALSE)

#define gcmIS_ERROR(status)			(status < 0)
#define gcmNO_ERROR(status)			(status >= 0)
#define gcmIS_SUCCESS(status)		(status == gcvSTATUS_OK)

    typedef enum _gceSTATUS
    {
        gcvSTATUS_OK					= 	0,
        gcvSTATUS_FALSE					= 	0,
        gcvSTATUS_TRUE					= 	1,
        gcvSTATUS_NO_MORE_DATA 			= 	2,
        gcvSTATUS_CACHED				= 	3,
        gcvSTATUS_MIPMAP_TOO_LARGE		= 	4,
        gcvSTATUS_NAME_NOT_FOUND		=	5,
        gcvSTATUS_NOT_OUR_INTERRUPT		=	6,
        gcvSTATUS_MISMATCH				=	7,
        gcvSTATUS_MIPMAP_TOO_SMALL		=	8,
        gcvSTATUS_LARGER				=	9,
        gcvSTATUS_SMALLER				=	10,
        gcvSTATUS_CHIP_NOT_READY		= 	11,
        gcvSTATUS_NEED_CONVERSION		=	12,
        gcvSTATUS_SKIP					=	13,
        gcvSTATUS_DATA_TOO_LARGE		=	14,
        gcvSTATUS_INVALID_CONFIG		=	15,

        gcvSTATUS_INVALID_ARGUMENT		= 	-1,
        gcvSTATUS_INVALID_OBJECT 		= 	-2,
        gcvSTATUS_OUT_OF_MEMORY 		= 	-3,
        gcvSTATUS_MEMORY_LOCKED			= 	-4,
        gcvSTATUS_MEMORY_UNLOCKED		= 	-5,
        gcvSTATUS_HEAP_CORRUPTED		= 	-6,
        gcvSTATUS_GENERIC_IO			= 	-7,
        gcvSTATUS_INVALID_ADDRESS		= 	-8,
        gcvSTATUS_CONTEXT_LOSSED		= 	-9,
        gcvSTATUS_TOO_COMPLEX			= 	-10,
        gcvSTATUS_BUFFER_TOO_SMALL		= 	-11,
        gcvSTATUS_INTERFACE_ERROR		= 	-12,
        gcvSTATUS_NOT_SUPPORTED			= 	-13,
        gcvSTATUS_MORE_DATA				= 	-14,
        gcvSTATUS_TIMEOUT				= 	-15,
        gcvSTATUS_OUT_OF_RESOURCES		= 	-16,
        gcvSTATUS_INVALID_DATA			= 	-17,
        gcvSTATUS_INVALID_MIPMAP		= 	-18,
        gcvSTATUS_NOT_FOUND				=	-19,
        gcvSTATUS_NOT_ALIGNED			=	-20,
        gcvSTATUS_INVALID_REQUEST		=	-21,

        /* Linker errors. */
        gcvSTATUS_GLOBAL_TYPE_MISMATCH	=	-1000,
        gcvSTATUS_TOO_MANY_ATTRIBUTES	=	-1001,
        gcvSTATUS_TOO_MANY_UNIFORMS		=	-1002,
        gcvSTATUS_TOO_MANY_VARYINGS		=	-1003,
        gcvSTATUS_UNDECLARED_VARYING	=	-1004,
        gcvSTATUS_VARYING_TYPE_MISMATCH	=	-1005,
        gcvSTATUS_MISSING_MAIN			=	-1006,
        gcvSTATUS_NAME_MISMATCH			=	-1007,
        gcvSTATUS_INVALID_INDEX			=	-1008,
    }
    gceSTATUS;

    /******************************************************************************\
    ******************************* I/O Control Codes ******************************
    \******************************************************************************/

#define gcvHAL_CLASS					"galcore"
#define IOCTL_GCHAL_INTERFACE			30000
#define IOCTL_GCHAL_KERNEL_INTERFACE	30001

    /******************************************************************************\
    ********************************* Command Codes ********************************
    \******************************************************************************/

    typedef enum _gceHAL_COMMAND_CODES
    {
        gcvHAL_GET_INTERRUPT,
        gcvHAL_SET_INTERRUPT,
        gcvHAL_READ_REGISTER,
        gcvHAL_WRITE_REGISTER,
        gcvHAL_GET_BASE_ADDRESS,
    }
    gceHAL_COMMAND_CODES;


    typedef struct _gcsHAL_INTERFACE
    {
        /* Command code. */
        gceHAL_COMMAND_CODES		command;

        /* Status value. */
        gceSTATUS					status;

        /* Union of command structures. */
        union _u
        {
            /* gcvHAL_GET_BASE_ADDRESS */
            struct _gcsHAL_GET_BASE_ADDRESS
            {
                /* Physical memory address of internal memory. */
                OUT gctUINT32				baseAddress;
            }
            GetBaseAddress;

            /* gcvHAL_COMMIT */
            struct _gcsHAL_COMMIT
            {
                /* result. */
                OUT gctUINT32 result;
            }
            GetInterrupt;

            /* gcvHAL_SetInt */
            struct _gcsHAL_SET_INT
            {
                /* result. */
                IN gctUINT32 data;
            }
            SetInterrupt;

            /* gcvHAL_READ_REGISTER */
            struct _gcsHAL_READ_REGISTER
            {
                /* Logical address of memory to write data to. */
                IN gctUINT32			address;

                /* Data read. */
                OUT gctUINT32			data;
            }
            ReadRegisterData;

            /* gcvHAL_WRITE_REGISTER */
            struct _gcsHAL_WRITE_REGISTER
            {
                /* Logical address of memory to write data to. */
                IN gctUINT32			address;

                /* Data read. */
                IN gctUINT32			data;
            }
            WriteRegisterData;
        }
        u;
    }
    gcsHAL_INTERFACE;

    typedef struct _gcsHAL_INTERFACE *	gcsHAL_INTERFACE_PTR;

#endif

