/****************************************************************************
**
** Copyright (C) 2017 Andre Hartmann <aha_1980@gmx.de>
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSerialBus module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef SYSTECCAN_SYMBOLS_P_H
#define SYSTECCAN_SYMBOLS_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/qlibrary.h>
#include <QtCore/qstring.h>

#include <windows.h>
#define DRV_CALLBACK_TYPE WINAPI

typedef quint8 UCANRET;
typedef quint8 tUcanHandle;

#define GENERATE_SYMBOL_VARIABLE(returnType, symbolName, ...) \
    typedef returnType (DRV_CALLBACK_TYPE *fp_##symbolName)(__VA_ARGS__); \
    static fp_##symbolName symbolName;

#define RESOLVE_SYMBOL(symbolName) \
    symbolName = (fp_##symbolName)systecLibrary->resolve(#symbolName); \
    if (!symbolName) \
        return false;

typedef void (DRV_CALLBACK_TYPE *tCallbackFktEx) (tUcanHandle handle, quint32 event, quint8 channel, void *args);

// The Callback function is called, if certain events did occur.
// These Defines specify the event.
#define USBCAN_EVENT_INITHW                 0    // the USB-CANmodul has been initialized
#define USBCAN_EVENT_INITCAN                1    // the CAN interface has been initialized
#define USBCAN_EVENT_RECEIVE                2    // a new CAN message has been received
#define USBCAN_EVENT_STATUS                 3    // the error state in the module has changed
#define USBCAN_EVENT_DEINITCAN              4    // the CAN interface has been deinitialized (UcanDeinitCan() was called)
#define USBCAN_EVENT_DEINITHW               5    // the USB-CANmodul has been deinitialized (UcanDeinitHardware() was called)
#define USBCAN_EVENT_CONNECT                6    // a new USB-CANmodul has been connected
#define USBCAN_EVENT_DISCONNECT             7    // a USB-CANmodul has been disconnected
#define USBCAN_EVENT_FATALDISCON            8    // a USB-CANmodul has been disconnected during operation
#define USBCAN_EVENT_RESERVED1              0x80

#define kUcanModeNormal         0x00        // normal mode (send and receive)
#define kUcanModeListenOnly     0x01        // listen only mode (only receive)
#define kUcanModeTxEcho         0x02        // CAN messages which was sent will be received at UcanReadCanMsg..
#define kUcanModeRxOrderCh      0x04        // reserved (not implemented in this version)
#define kUcanModeHighResTimer   0x08        // high resolution time stamps in received CAN messages (only available with STM derivates)

// ABR and ACR for mode "receive all CAN messages"
#define USBCAN_AMR_ALL                      (quint32) 0xffffffff
#define USBCAN_ACR_ALL                      (quint32) 0x00000000

#define USBCAN_OCR_DEFAULT                  0x1A  // default OCR for standard GW-002
#define USBCAN_OCR_RS485_ISOLATED           0x1E  // OCR for RS485 interface and galvanic isolation
#define USBCAN_OCR_RS485_NOT_ISOLATED       0x0A  // OCR for RS485 interface without galvanic isolation
#define USBCAN_DEFAULT_BUFFER_ENTRIES       4096

// Structure with init parameters for function UcanInitCanEx() and UcanInitCanEx2()
#pragma pack(push, 1)
typedef struct _tUcanInitCanParam {
    quint32     m_dwSize;                       // [IN] size of this structure
    quint8      m_bMode;                        // [IN] selects the mode of CAN controller (see kUcanMode...)

    // Baudrate Registers for GW-001 or GW-002
    quint8      m_bBTR0;                        // [IN] Bus Timing Register 0 (SJA1000 - use high byte USBCAN_BAUD_...)
    quint8      m_bBTR1;                        // [IN] Bus Timing Register 1 (SJA1000 - use low  byte USBCAN_BAUD_...)

    quint8      m_bOCR;                         // [IN] Output Control Register of SJA1000 (should be 0x1A)
    quint32     m_dwAMR;                        // [IN] Acceptance Mask Register (SJA1000)
    quint32     m_dwACR;                        // [IN] Acceptance Code Register (SJA1000)

    // since version V3.00 - is ignored from function UcanInitCanEx() and until m_dwSize < 20
    quint32     m_dwBaudrate;                   // [IN] Baudrate Register for Multiport 3004006, USB-CANmodul1 3204000 or USB-CANmodul2 3204002
                                                //      (use USBCAN_BAUDEX_...)

    // since version V3.05 - is ignored until m_dwSize < 24
    quint16     m_wNrOfRxBufferEntries;         // [IN] number of receive buffer entries (default is 4096)
    quint16     m_wNrOfTxBufferEntries;         // [IN] number of transmit buffer entries (default is 4096)
} tUcanInitCanParam;

#define USBCAN_BAUDEX_1MBit                 0x00020354          // = 1000 kBit/s    Sample Point: 68,75%
#define USBCAN_BAUDEX_800kBit               0x00030254          // =  800 kBit/s    Sample Point: 66,67%
#define USBCAN_BAUDEX_500kBit               0x00050354          // =  500 kBit/s    Sample Point: 68,75%
#define USBCAN_BAUDEX_250kBit               0x000B0354          // =  250 kBit/s    Sample Point: 68,75%
#define USBCAN_BAUDEX_125kBit               0x00170354          // =  125 kBit/s    Sample Point: 68,75%
#define USBCAN_BAUDEX_100kBit               0x00171466          // =  100 kBit/s    Sample Point: 65,00%
#define USBCAN_BAUDEX_50kBit                0x002F1466          // =   50 kBit/s    Sample Point: 65,00%
#define USBCAN_BAUDEX_20kBit                0x00771466          // =   20 kBit/s    Sample Point: 65,00%
#define USBCAN_BAUDEX_10kBit                0x80771466          // =   10 kBit/s    Sample Point: 65,00% (CLK = 1, see L-487 since version 15)

// Frame format for a CAN message (bit oriented)
#define USBCAN_MSG_FF_STD                   0x00                // Standard Frame (11-Bit-ID)
#define USBCAN_MSG_FF_ECHO                  0x20                // Tx echo (message received from UcanReadCanMsg.. was previously sent by UcanWriteCanMsg..)
#define USBCAN_MSG_FF_RTR                   0x40                // Remote Transmission Request Frame
#define USBCAN_MSG_FF_EXT                   0x80                // Extended Frame (29-Bit-ID)

typedef struct _tCanMsgStruct {
    quint32 m_dwID;                         // CAN Identifier
    quint8  m_bFF;                          // CAN Frame format (BIT7=1: 29BitID / BIT6=1: RTR-Frame / BIT5=1: Tx echo)
    quint8  m_bDLC;                         // CAN Data Length Code
    quint8  m_bData[8];                     // CAN Data
    quint32 m_dwTime;                       // Time in ms
} tCanMsgStruct;

// Function return codes (encoding)
#define USBCAN_SUCCESSFUL                   0x00                // no error
#define USBCAN_ERR                          0x01                // error in library; function has not been executed
#define USBCAN_ERRCMD                       0x40                // error in module; function has not been executed
#define USBCAN_WARNING                      0x80                // Warning; function has been executed anyway
#define USBCAN_RESERVED                     0xc0                // reserved return codes (up to 255)

// Error messages, that can occur in the library
#define USBCAN_ERR_RESOURCE                 0x01                // could not create a resource (memory, Handle, ...)
#define USBCAN_ERR_MAXMODULES               0x02                // the maximum number of open modules is exceeded
#define USBCAN_ERR_HWINUSE                  0x03                // a module is already in use
#define USBCAN_ERR_ILLVERSION               0x04                // the software versions of the module and library are incompatible
#define USBCAN_ERR_ILLHW                    0x05                // the module with the corresponding device number is not connected
#define USBCAN_ERR_ILLHANDLE                0x06                // wrong USB-CAN-Handle handed over to the function
#define USBCAN_ERR_ILLPARAM                 0x07                // wrong parameter handed over to the function
#define USBCAN_ERR_BUSY                     0x08                // instruction can not be processed at this time
#define USBCAN_ERR_TIMEOUT                  0x09                // no answer from the module
#define USBCAN_ERR_IOFAILED                 0x0a                // a request for the driver failed
#define USBCAN_ERR_DLL_TXFULL               0x0b                // the message did not fit into the transmission queue
#define USBCAN_ERR_MAXINSTANCES             0x0c                // maximum number of applications is reached
#define USBCAN_ERR_CANNOTINIT               0x0d                // CAN-interface is not yet initialized
#define USBCAN_ERR_DISCONNECT               0x0e                // USB-CANmodul was disconnected
#define USBCAN_ERR_DISCONECT            USBCAN_ERR_DISCONNECT   // renamed (still defined for compatibility reason)
#define USBCAN_ERR_NOHWCLASS                0x0f                // the needed device class does not exist
#define USBCAN_ERR_ILLCHANNEL               0x10                // illegal CAN channel for GW-001/GW-002
#define USBCAN_ERR_RESERVED1                0x11
#define USBCAN_ERR_ILLHWTYPE                0x12                // the API function can not be used with this hardware
#define USBCAN_ERR_SERVER_TIMEOUT           0x13                // the command server does not send an reply of an command

// Error messages, that the module returns during the command sequence
#define USBCAN_ERRCMD_NOTEQU                0x40                // the received response does not match with the transmitted command
#define USBCAN_ERRCMD_REGTST                0x41                // no access to the CAN controller possible
#define USBCAN_ERRCMD_ILLCMD                0x42                // the module could not interpret the command
#define USBCAN_ERRCMD_EEPROM                0x43                // error while reading the EEPROM occurred
#define USBCAN_ERRCMD_RESERVED1             0x44
#define USBCAN_ERRCMD_RESERVED2             0x45
#define USBCAN_ERRCMD_RESERVED3             0x46
#define USBCAN_ERRCMD_ILLBDR                0x47                // illegal baudrate values for Multiport 3004006, USB-CANmodul1 3204000 or USB-CANmodul2 3204002 in BTR0/BTR1
#define USBCAN_ERRCMD_NOTINIT               0x48                // CAN channel was not initialized
#define USBCAN_ERRCMD_ALREADYINIT           0x49                // CAN channel was already initialized
#define USBCAN_ERRCMD_ILLSUBCMD             0x4A                // illegal sub-command specified
#define USBCAN_ERRCMD_ILLIDX                0x4B                // illegal index specified (e.g. index for cyclic CAN message)
#define USBCAN_ERRCMD_RUNNING               0x4C                // cyclic CAN message(s) can not be defined because transmission of cyclic CAN messages is already running

// Warning messages, that can occur in library
// NOTE: These messages are only warnings. The function has been executed anyway.
#define USBCAN_WARN_NODATA                  0x80                // no CAN messages received
#define USBCAN_WARN_SYS_RXOVERRUN           0x81                // overrun in the receive queue of the driver (but this CAN message is successfuly read)
#define USBCAN_WARN_DLL_RXOVERRUN           0x82                // overrun in the receive queue of the library (but this CAN message is successfuly read)
#define USBCAN_WARN_RESERVED1               0x83
#define USBCAN_WARN_RESERVED2               0x84
#define USBCAN_WARN_FW_TXOVERRUN            0x85                // overrun in the transmit queue of the firmware (but this CAN message was successfully stored in buffer)
#define USBCAN_WARN_FW_RXOVERRUN            0x86                // overrun in the receive queue of the firmware (but this CAN message was successfully read)
#define USBCAN_WARN_FW_TXMSGLOST            0x87                // (not implemented yet)
#define USBCAN_WARN_NULL_PTR                0x90                // pointer to address is NULL (function will not work correctly)
#define USBCAN_WARN_TXLIMIT                 0x91                // function UcanWriteCanMsgEx() was called for sending more CAN messages than one
                                                                //      But not all of them could be sent because the buffer is full.
                                                                //      Variable pointed by pdwCount_p received the number of successfully sent CAN messages.
#define USBCAN_WARN_BUSY                    0x92                // place holder (only for internaly use)

typedef struct _tUcanHardwareInfoEx {
    DWORD       m_dwSize;                       // [IN]  size of this structure
    tUcanHandle m_UcanHandle;                   // [OUT] USB-CAN-Handle assigned by the DLL
    BYTE        m_bDeviceNr;                    // [OUT] device number of the USB-CANmodul
    DWORD       m_dwSerialNr;                   // [OUT] serial number from USB-CANmodul
    DWORD       m_dwFwVersionEx;                // [OUT] version of firmware
    DWORD       m_dwProductCode;                // [OUT] product code (for differentiate between different hardware modules)
                                                //       see constants USBCAN_PRODCODE_...

    DWORD       m_adwUniqueId[4];               // [OUT] unique ID (available since V5.01) !!! m_dwSize must be >= USBCAN_HWINFO_SIZE_V2
    DWORD       m_dwFlags;                      // [OUT] additional flags
} tUcanHardwareInfoEx;

#define USBCAN_HWINFO_SIZE_V1           0x12    // size without m_adwDeviceId[]
#define USBCAN_HWINFO_SIZE_V2           0x22    // size with m_adwDeviceId[]
#define USBCAN_HWINFO_SIZE_V3           0x26    // size with m_adwDeviceId[] and m_dwFlags

// definitions for product code in structure tUcanHardwareInfoEx
#define USBCAN_PRODCODE_MASK_DID            0xFFFF0000L
#define USBCAN_PRODCODE_MASK_MFU            0x00008000L
#define USBCAN_PRODCODE_PID_TWO_CHA         0x00000001L
#define USBCAN_PRODCODE_PID_TERM            0x00000001L
#define USBCAN_PRODCODE_PID_RBUSER          0x00000001L
#define USBCAN_PRODCODE_PID_RBCAN           0x00000001L
#define USBCAN_PRODCODE_PID_G4              0x00000020L
#define USBCAN_PRODCODE_PID_RESVD           0x00000040L
#define USBCAN_PRODCODE_MASK_PID            0x00007FFFL
#define USBCAN_PRODCODE_MASK_PIDG3          (USBCAN_PRODCODE_MASK_PID & ~USBCAN_PRODCODE_PID_RESVD)

#define USBCAN_PRODCODE_PID_GW001           0x00001100L     // order code GW-001 "USB-CANmodul" outdated
#define USBCAN_PRODCODE_PID_GW002           0x00001102L     // order code GW-002 "USB-CANmodul" outdated
#define USBCAN_PRODCODE_PID_MULTIPORT       0x00001103L     // order code 3004006/3404000/3404001 "Multiport CAN-to-USB"
#define USBCAN_PRODCODE_PID_BASIC           0x00001104L     // order code 3204000/3204001 "USB-CANmodul1"
#define USBCAN_PRODCODE_PID_ADVANCED        0x00001105L     // order code 3204002/3204003 "USB-CANmodul2"
#define USBCAN_PRODCODE_PID_USBCAN8         0x00001107L     // order code 3404000 "USB-CANmodul8"
#define USBCAN_PRODCODE_PID_USBCAN16        0x00001109L     // order code 3404001 "USB-CANmodul16"
#define USBCAN_PRODCODE_PID_RESERVED3       0x00001110L
#define USBCAN_PRODCODE_PID_ADVANCED_G4     0x00001121L     // order code ------- "USB-CANmodul2" 4th generation
#define USBCAN_PRODCODE_PID_BASIC_G4        0x00001122L     // order code 3204000 "USB-CANmodul1" 4th generation
#define USBCAN_PRODCODE_PID_RESERVED1       0x00001144L
#define USBCAN_PRODCODE_PID_RESERVED2       0x00001145L
#define USBCAN_PRODCODE_PID_RESERVED4       0x00001162L

// checks if the module supports two CAN channels (at logical device)
#define USBCAN_CHECK_SUPPORT_TWO_CHANNEL(pHwInfoEx) \
    ((((pHwInfoEx)->m_dwProductCode & USBCAN_PRODCODE_MASK_PID) >= USBCAN_PRODCODE_PID_MULTIPORT) && \
     (((pHwInfoEx)->m_dwProductCode & USBCAN_PRODCODE_PID_TWO_CHA) != 0)                             )

typedef struct _tUcanHardwareInitInfo {
    DWORD           m_dwSize;                   // [IN]  size of this structure
    BOOL            m_fDoInitialize;            // [IN]  specifies if the found module should be initialized by the DLL
    tUcanHandle    *m_pUcanHandle;              // [IN]  pointer to variable receiving the USB-CAN-Handle
    tCallbackFktEx  m_fpCallbackFktEx;          // [IN]  pointer to callback function
    void           *m_pCallbackArg;             // [IN]  pointer to user defined parameter for callback function
    BOOL            m_fTryNext;                 // [IN]  specifies if a further module should be found
} tUcanHardwareInitInfo;

typedef void (DRV_CALLBACK_TYPE *tUcanEnumCallback) (
    DWORD                  dwIndex_p,           // [IN]  gives a sequential number of the enumerated module
    BOOL                   fIsUsed_p,           // [IN]  set to TRUE if the module is used by another application
    tUcanHardwareInfoEx   *pHwInfoEx_p,         // [IN]  pointer to the hardware info structure identifying the enumerated module
    tUcanHardwareInitInfo *pInitInfo_p,         // [IN]  pointer to an init structure for initializing the module
    void                  *pArg_p);             // [IN]  user argument which was overhand with UcanEnumerateHardware()

#pragma pack(pop)

GENERATE_SYMBOL_VARIABLE(quint32, UcanEnumerateHardware,
                         tUcanEnumCallback /* callback */, void * /* args */, BOOL /* used */,
                         quint8, quint8 /* device number low and high */,
                         quint32, quint32, /* serial low and high */
                         quint32, quint32 /* product code low and high */)
GENERATE_SYMBOL_VARIABLE(UCANRET, UcanInitHardwareEx, tUcanHandle *, quint8 /* device */,
                         tCallbackFktEx /* callback */, void *)
GENERATE_SYMBOL_VARIABLE(UCANRET, UcanDeinitHardware, tUcanHandle)
GENERATE_SYMBOL_VARIABLE(UCANRET, UcanInitCanEx2, tUcanHandle, quint8 /* channel */, tUcanInitCanParam *)
GENERATE_SYMBOL_VARIABLE(UCANRET, UcanDeinitCanEx, tUcanHandle, quint8 /* channel */)
GENERATE_SYMBOL_VARIABLE(UCANRET, UcanReadCanMsgEx, tUcanHandle, quint8 *, tCanMsgStruct *, quint32 *)
GENERATE_SYMBOL_VARIABLE(UCANRET, UcanWriteCanMsgEx, tUcanHandle, quint8, tCanMsgStruct *, quint32 *)

inline bool resolveSymbols(QLibrary *systecLibrary)
{
    if (!systecLibrary->isLoaded()) {
#ifdef Q_PROCESSOR_X86_64
        systecLibrary->setFileName(QStringLiteral("usbcan64"));
#else
        systecLibrary->setFileName(QStringLiteral("usbcan32"));
#endif
        if (!systecLibrary->load())
            return false;
    }

    RESOLVE_SYMBOL(UcanEnumerateHardware);
    RESOLVE_SYMBOL(UcanInitHardwareEx);
    RESOLVE_SYMBOL(UcanDeinitHardware);
    RESOLVE_SYMBOL(UcanInitCanEx2);
    RESOLVE_SYMBOL(UcanDeinitCanEx);
    RESOLVE_SYMBOL(UcanReadCanMsgEx);
    RESOLVE_SYMBOL(UcanWriteCanMsgEx);

    return true;
}

#endif // SYSTECCAN_SYMBOLS_P_H
