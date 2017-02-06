/****************************************************************************
**
** Copyright (C) 2015 Denis Shienkov <denis.shienkov@gmail.com>
** Copyright (C) 2015 The Qt Company Ltd.
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

#ifndef TINYCAN_SYMBOLS_P_H
#define TINYCAN_SYMBOLS_P_H

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

#ifdef LINK_LIBMHSTCAN

extern "C"
{
#include <can_types.h>
#include <can_drv.h>
#include <can_drv_ex.h>
}

#else

#include <QtCore/qlibrary.h>
#include <QtCore/qstring.h>
#include <QtCore/qdebug.h>

#ifdef Q_OS_WIN32
#include <windows.h>
#define DRV_CALLBACK_TYPE WINAPI
#else
#define DRV_CALLBACK_TYPE
#endif

#define INDEX_INVALID          0xFFFFFFFF

#define INDEX_FIFO_PUFFER_MASK 0x0000FFFF
#define INDEX_SOFT_FLAG        0x02000000
#define INDEX_RXD_TXT_FLAG     0x01000000
#define INDEX_CAN_KANAL_MASK   0x000F0000
#define INDEX_CAN_DEVICE_MASK  0x00F00000

#define INDEX_CAN_KANAL_A      0x00000000
#define INDEX_CAN_KANAL_B      0x00010000

// CAN Message Type
#define MsgFlags Flags.Long
#define MsgLen Flags.Flag.Len
#define MsgRTR Flags.Flag.RTR
#define MsgEFF Flags.Flag.EFF
#define MsgTxD Flags.Flag.TxD
#define MsgErr Flags.Flag.Error
#define MsgSource Flags.Flag.Source
#define MsgData Data.Bytes

struct TCanFlagsBits
{
    unsigned Len:4;     // DLC -> data length 0 - 8 byte
    unsigned TxD:1;     // TxD -> 1 = Tx CAN Message, 0 = Rx CAN Message
    unsigned Error:1;   // Error -> 1 = bus CAN error message
    unsigned RTR:1;     // Remote Transmission Request bit -> If message RTR marks
    unsigned EFF:1;     // Extended Frame Format bit -> 1 = 29 Bit Id's, 0 = 11 Bit Id's
    unsigned Source:8;  // Source of the message (Device)
};

#pragma pack(push, 1)
union TCanFlags
{
    struct TCanFlagsBits Flag;
    quint32 Long;
};
#pragma pack(pop)

#pragma pack(push, 1)
union TCanData
{
    char Chars[8];
    quint8 Bytes[8];
    quint16 Words[4];
    quint32 Longs[2];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct TTime
{
    quint32 Sec;
    quint32 USec;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct TCanMsg
{
    quint32 Id;
    union TCanFlags Flags;
    union TCanData Data;
    struct TTime Time;
};
#pragma pack(pop)

// CAN Message Filter Type
#define FilFlags Flags.Long
#define FilRTR Flags.Flag.RTR
#define FilEFF Flags.Flag.EFF
#define FilMode Flags.Flag.Mode
#define FilIdMode Flags.Flag.IdMode
#define FilEnable Flags.Flag.Enable

struct TMsgFilterFlagsBits
{
    // 1. byte
    unsigned Len:4;         // DLC
    unsigned Res:2;         // Reserved
    unsigned RTR:1;         // Remote Transmit Request bit
    unsigned EFF:1;         // Extended Frame Format bit
    // 2. byte
    unsigned IdMode:2;      // 0 = Mask & Code; 1 = Start & Sstop; 2 = Single Id
    unsigned DLCCheck:1;
    unsigned DataCheck:1;
    unsigned Res1:4;
    // 3. byte
    unsigned Res2:8;
    // 4. byte
    unsigned Type:4;        // 0 = Single buffer
    unsigned Res3:2;
    unsigned Mode:1;        // 0 = Delete message; 1 = Don't delete message
    unsigned Enable:1;      // 0 = Close filter; 1 = Release filter
};

#pragma pack(push, 1)
union TMsgFilterFlags
{
    struct TMsgFilterFlagsBits Flag;
    quint32  Long;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct TMsgFilter
{
    quint32  Maske;
    quint32  Code;
    union TMsgFilterFlags Flags;
    union TCanData Data;
};
#pragma pack(pop)

// CAN speed
#define CAN_10K_BIT   10    // 10 kBit/s
#define CAN_20K_BIT   20    // 20 kBit/s
#define CAN_50K_BIT   50    // 50 kBit/s
#define CAN_100K_BIT  100   // 100 kBit/s
#define CAN_125K_BIT  125   // 125 kBit/s
#define CAN_250K_BIT  250   // 250 kBit/s
#define CAN_500K_BIT  500   // 500 kBit/s
#define CAN_800K_BIT  800   // 800 kBit/s
#define CAN_1M_BIT    1000  // 1 MBit/s

// CAN bus mode
#define OP_CAN_NO_CHANGE         0  // Current state no change
#define OP_CAN_START             1  // Starts CAN-Bus
#define OP_CAN_STOP              2  // Stops CAN-Bus
#define OP_CAN_RESET             3  // Reset CAN Controller (clearing BusOff)
#define OP_CAN_START_LOM         4  // Starts the CAN-Bus in Silent Mode (Listen Only Mode)
#define OP_CAN_START_NO_RETRANS  5  // Starts the CAN-Bus in Automatic Retransmission disable Mode


#define CAN_CMD_NONE                0x0000
#define CAN_CMD_RXD_OVERRUN_CLEAR   0x0001
#define CAN_CMD_RXD_FIFOS_CLEAR     0x0002
#define CAN_CMD_TXD_OVERRUN_CLEAR   0x0004
#define CAN_CMD_TXD_FIFOS_CLEAR     0x0008
#define CAN_CMD_HW_FILTER_CLEAR     0x0010
#define CAN_CMD_SW_FILTER_CLEAR     0x0020
#define CAN_CMD_TXD_PUFFERS_CLEAR   0x0040

#define CAN_CMD_ALL_CLEAR           0x0FFF

// Driver status
#define DRV_NOT_LOAD              0  // Driver DLL was not loaded
#define DRV_STATUS_NOT_INIT       1  // Driver was not initialised
#define DRV_STATUS_INIT           2  // Driver was successfully initialised
#define DRV_STATUS_PORT_NOT_OPEN  3  // The port is not open
#define DRV_STATUS_PORT_OPEN      4  // The port was opened
#define DRV_STATUS_DEVICE_FOUND   5  // The connection to the hardware was established
#define DRV_STATUS_CAN_OPEN       6  // The device was opened and initialised
#define DRV_STATUS_CAN_RUN_TX     7  // CAN Bus RUN Transmitter (not used)
#define DRV_STATUS_CAN_RUN        8  // CAN Bus RUN

// CAN status
#define CAN_STATUS_OK          0     // CAN-Controller: Ok
#define CAN_STATUS_ERROR       1     // CAN-Controller: CAN Error
#define CAN_STATUS_WARNING     2     // CAN-Controller: Error warning
#define CAN_STATUS_PASSIV      3     // CAN-Controller: Error passive
#define CAN_STATUS_BUS_OFF     4     // CAN-Controller: Bus Off
#define CAN_STATUS_UNBEKANNT   5     // CAN-Controller: State unknown

// FIFO status
#define FIFO_OK                 0 // Fifo-Status: Ok
#define FIFO_HW_OVERRUN         1 // Fifo-Status: Hardware Fifo overflow
#define FIFO_SW_OVERRUN         2 // Fifo-Status: Software Fifo overflow
#define FIFO_HW_SW_OVERRUN      3 // Fifo-Status: Hardware & Software Fifo overflow
#define FIFO_STATUS_UNBEKANNT   4 // Fifo-Status: Unknown

// Macro for SetEvent
#define EVENT_ENABLE_PNP_CHANGE          0x0001
#define EVENT_ENABLE_STATUS_CHANGE       0x0002
#define EVENT_ENABLE_RX_FILTER_MESSAGES  0x0004
#define EVENT_ENABLE_RX_MESSAGES         0x0008
#define EVENT_ENABLE_ALL                 0x00FF

#define EVENT_DISABLE_PNP_CHANGE         0x0100
#define EVENT_DISABLE_STATUS_CHANGE      0x0200
#define EVENT_DISABLE_RX_FILTER_MESSAGES 0x0400
#define EVENT_DISABLE_RX_MESSAGES        0x0800
#define EVENT_DISABLE_ALL                0xFF00

#pragma pack(push, 1)
struct TDeviceStatus
{
    quint32 DrvStatus;
    quint8 CanStatus;
    quint8 FifoStatus;
};
#pragma pack(pop)

typedef void (DRV_CALLBACK_TYPE *CanPnPEventCallback)(
    quint32 index,
    qint32 status
);

typedef void (DRV_CALLBACK_TYPE *CanStatusEventCallback)(
    quint32 index,
    TDeviceStatus *device_status
);

typedef void (DRV_CALLBACK_TYPE *CanRxEventCallback)(
    quint32 index,
    TCanMsg *msg,
    qint32 count
);

#define GENERATE_SYMBOL_VARIABLE(returnType, symbolName, ...) \
    typedef returnType (DRV_CALLBACK_TYPE *fp_##symbolName)(__VA_ARGS__); \
    static fp_##symbolName symbolName;

#define RESOLVE_SYMBOL(symbolName) \
    symbolName = (fp_##symbolName)mhstcanLibrary->resolve(#symbolName); \
    if (!symbolName) \
        return false;

GENERATE_SYMBOL_VARIABLE(qint32, CanInitDriver, char *)
GENERATE_SYMBOL_VARIABLE(void, CanDownDriver, void)
GENERATE_SYMBOL_VARIABLE(qint32, CanSetOptions, char *)
GENERATE_SYMBOL_VARIABLE(qint32, CanDeviceOpen, quint32, char *)
GENERATE_SYMBOL_VARIABLE(qint32, CanDeviceClose, quint32)
GENERATE_SYMBOL_VARIABLE(qint32, CanApplaySettings, quint32)
GENERATE_SYMBOL_VARIABLE(qint32, CanSetMode, quint32, quint8, quint16)
GENERATE_SYMBOL_VARIABLE(qint32, CanSet, quint32, quint16, quint16, void *, qint32)
GENERATE_SYMBOL_VARIABLE(qint32, CanGet, quint32, quint16, quint16, void *, qint32)
GENERATE_SYMBOL_VARIABLE(qint32, CanTransmit, quint32, TCanMsg *, qint32)
GENERATE_SYMBOL_VARIABLE(void, CanTransmitClear, quint32)
GENERATE_SYMBOL_VARIABLE(quint32, CanTransmitGetCount, quint32)
GENERATE_SYMBOL_VARIABLE(qint32, CanTransmitSet, quint32, quint16, quint32)
GENERATE_SYMBOL_VARIABLE(qint32, CanReceive, quint32, TCanMsg *, qint32)
GENERATE_SYMBOL_VARIABLE(void, CanReceiveClear, quint32)
GENERATE_SYMBOL_VARIABLE(quint32, CanReceiveGetCount, quint32)
GENERATE_SYMBOL_VARIABLE(qint32, CanSetSpeed, quint32, quint16)
GENERATE_SYMBOL_VARIABLE(qint32, CanSetSpeedUser, quint32, quint32)
GENERATE_SYMBOL_VARIABLE(char *, CanDrvInfo, void)
GENERATE_SYMBOL_VARIABLE(char *, CanDrvHwInfo, quint32)
GENERATE_SYMBOL_VARIABLE(qint32, CanSetFilter, quint32, TMsgFilter *)
GENERATE_SYMBOL_VARIABLE(qint32, CanGetDeviceStatus, quint32, TDeviceStatus *)
GENERATE_SYMBOL_VARIABLE(void, CanSetPnPEventCallback, CanPnPEventCallback)
GENERATE_SYMBOL_VARIABLE(void, CanSetStatusEventCallback, CanStatusEventCallback)
GENERATE_SYMBOL_VARIABLE(void, CanSetRxEventCallback, CanRxEventCallback)
GENERATE_SYMBOL_VARIABLE(void, CanSetEvents, quint16)
GENERATE_SYMBOL_VARIABLE(quint32, CanEventStatus, void)

inline bool resolveSymbols(QLibrary *mhstcanLibrary)
{
    if (!mhstcanLibrary->isLoaded()) {
        mhstcanLibrary->setFileName(QStringLiteral("mhstcan"));
        if (!mhstcanLibrary->load())
            return false;
    }

    RESOLVE_SYMBOL(CanInitDriver)
    RESOLVE_SYMBOL(CanDownDriver)
    RESOLVE_SYMBOL(CanSetOptions)
    RESOLVE_SYMBOL(CanDeviceOpen)
    RESOLVE_SYMBOL(CanDeviceClose)
    RESOLVE_SYMBOL(CanApplaySettings)
    RESOLVE_SYMBOL(CanSetMode)
    RESOLVE_SYMBOL(CanSet)
    RESOLVE_SYMBOL(CanGet)
    RESOLVE_SYMBOL(CanTransmit)
    RESOLVE_SYMBOL(CanTransmitClear)
    RESOLVE_SYMBOL(CanTransmitGetCount)
    RESOLVE_SYMBOL(CanTransmitSet)
    RESOLVE_SYMBOL(CanReceive)
    RESOLVE_SYMBOL(CanReceiveClear)
    RESOLVE_SYMBOL(CanReceiveGetCount)
    RESOLVE_SYMBOL(CanSetSpeed)
    RESOLVE_SYMBOL(CanSetSpeedUser)
    RESOLVE_SYMBOL(CanDrvInfo)
    RESOLVE_SYMBOL(CanDrvHwInfo)
    RESOLVE_SYMBOL(CanSetFilter)
    RESOLVE_SYMBOL(CanGetDeviceStatus)
    RESOLVE_SYMBOL(CanSetPnPEventCallback)
    RESOLVE_SYMBOL(CanSetStatusEventCallback)
    RESOLVE_SYMBOL(CanSetRxEventCallback)
    RESOLVE_SYMBOL(CanSetEvents)
    RESOLVE_SYMBOL(CanEventStatus)

    return true;
}

#endif

#endif // TINYCAN_SYMBOLS_P_H
