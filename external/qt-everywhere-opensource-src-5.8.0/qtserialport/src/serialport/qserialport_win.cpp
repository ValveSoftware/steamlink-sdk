/****************************************************************************
**
** Copyright (C) 2012 Denis Shienkov <denis.shienkov@gmail.com>
** Copyright (C) 2012 Laszlo Papp <lpapp@kde.org>
** Copyright (C) 2012 Andre Hartmann <aha_1980@gmx.de>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtSerialPort module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qserialport_p.h"

#include <QtCore/qcoreevent.h>
#include <QtCore/qelapsedtimer.h>
#include <QtCore/qvector.h>
#include <QtCore/qtimer.h>
#include <private/qwinoverlappedionotifier_p.h>
#include <algorithm>

#ifndef CTL_CODE
#  define CTL_CODE(DeviceType, Function, Method, Access) ( \
    ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method) \
    )
#endif

#ifndef FILE_DEVICE_SERIAL_PORT
#  define FILE_DEVICE_SERIAL_PORT  27
#endif

#ifndef METHOD_BUFFERED
#  define METHOD_BUFFERED  0
#endif

#ifndef FILE_ANY_ACCESS
#  define FILE_ANY_ACCESS  0x00000000
#endif

#ifndef IOCTL_SERIAL_GET_DTRRTS
#  define IOCTL_SERIAL_GET_DTRRTS \
    CTL_CODE(FILE_DEVICE_SERIAL_PORT, 30, METHOD_BUFFERED, FILE_ANY_ACCESS)
#endif

#ifndef SERIAL_DTR_STATE
#  define SERIAL_DTR_STATE  0x00000001
#endif

#ifndef SERIAL_RTS_STATE
#  define SERIAL_RTS_STATE  0x00000002
#endif

QT_BEGIN_NAMESPACE

static inline void qt_set_common_props(DCB *dcb)
{
    dcb->fBinary = TRUE;
    dcb->fAbortOnError = FALSE;
    dcb->fNull = FALSE;
    dcb->fErrorChar = FALSE;

    if (dcb->fDtrControl == DTR_CONTROL_HANDSHAKE)
        dcb->fDtrControl = DTR_CONTROL_DISABLE;

    if (dcb->fRtsControl != RTS_CONTROL_HANDSHAKE)
        dcb->fRtsControl = RTS_CONTROL_DISABLE;
}

static inline void qt_set_baudrate(DCB *dcb, qint32 baudrate)
{
    dcb->BaudRate = baudrate;
}

static inline void qt_set_databits(DCB *dcb, QSerialPort::DataBits databits)
{
    dcb->ByteSize = databits;
}

static inline void qt_set_parity(DCB *dcb, QSerialPort::Parity parity)
{
    dcb->fParity = TRUE;
    switch (parity) {
    case QSerialPort::NoParity:
        dcb->Parity = NOPARITY;
        dcb->fParity = FALSE;
        break;
    case QSerialPort::OddParity:
        dcb->Parity = ODDPARITY;
        break;
    case QSerialPort::EvenParity:
        dcb->Parity = EVENPARITY;
        break;
    case QSerialPort::MarkParity:
        dcb->Parity = MARKPARITY;
        break;
    case QSerialPort::SpaceParity:
        dcb->Parity = SPACEPARITY;
        break;
    default:
        dcb->Parity = NOPARITY;
        dcb->fParity = FALSE;
        break;
    }
}

static inline void qt_set_stopbits(DCB *dcb, QSerialPort::StopBits stopbits)
{
    switch (stopbits) {
    case QSerialPort::OneStop:
        dcb->StopBits = ONESTOPBIT;
        break;
    case QSerialPort::OneAndHalfStop:
        dcb->StopBits = ONE5STOPBITS;
        break;
    case QSerialPort::TwoStop:
        dcb->StopBits = TWOSTOPBITS;
        break;
    default:
        dcb->StopBits = ONESTOPBIT;
        break;
    }
}

static inline void qt_set_flowcontrol(DCB *dcb, QSerialPort::FlowControl flowcontrol)
{
    dcb->fInX = FALSE;
    dcb->fOutX = FALSE;
    dcb->fOutxCtsFlow = FALSE;
    if (dcb->fRtsControl == RTS_CONTROL_HANDSHAKE)
        dcb->fRtsControl = RTS_CONTROL_DISABLE;
    switch (flowcontrol) {
    case QSerialPort::NoFlowControl:
        break;
    case QSerialPort::SoftwareControl:
        dcb->fInX = TRUE;
        dcb->fOutX = TRUE;
        break;
    case QSerialPort::HardwareControl:
        dcb->fOutxCtsFlow = TRUE;
        dcb->fRtsControl = RTS_CONTROL_HANDSHAKE;
        break;
    default:
        break;
    }
}

bool QSerialPortPrivate::open(QIODevice::OpenMode mode)
{
    DWORD desiredAccess = 0;
    originalEventMask = 0;

    if (mode & QIODevice::ReadOnly) {
        desiredAccess |= GENERIC_READ;
        originalEventMask |= EV_RXCHAR;
    }
    if (mode & QIODevice::WriteOnly)
        desiredAccess |= GENERIC_WRITE;

    handle = ::CreateFile(reinterpret_cast<const wchar_t*>(systemLocation.utf16()),
                              desiredAccess, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);

    if (handle == INVALID_HANDLE_VALUE) {
        setError(getSystemError());
        return false;
    }

    if (initialize())
        return true;

    ::CloseHandle(handle);
    return false;
}

void QSerialPortPrivate::close()
{
    ::CancelIo(handle);

    delete notifier;
    notifier = nullptr;

    delete startAsyncWriteTimer;
    startAsyncWriteTimer = nullptr;

    communicationStarted = false;
    readStarted = false;
    writeStarted = false;
    writeBuffer.clear();

    if (settingsRestoredOnClose) {
        ::SetCommState(handle, &restoredDcb);
        ::SetCommTimeouts(handle, &restoredCommTimeouts);
    }

    ::CloseHandle(handle);
    handle = INVALID_HANDLE_VALUE;
}

QSerialPort::PinoutSignals QSerialPortPrivate::pinoutSignals()
{
    DWORD modemStat = 0;

    if (!::GetCommModemStatus(handle, &modemStat)) {
        setError(getSystemError());
        return QSerialPort::NoSignal;
    }

    QSerialPort::PinoutSignals ret = QSerialPort::NoSignal;

    if (modemStat & MS_CTS_ON)
        ret |= QSerialPort::ClearToSendSignal;
    if (modemStat & MS_DSR_ON)
        ret |= QSerialPort::DataSetReadySignal;
    if (modemStat & MS_RING_ON)
        ret |= QSerialPort::RingIndicatorSignal;
    if (modemStat & MS_RLSD_ON)
        ret |= QSerialPort::DataCarrierDetectSignal;

    DWORD bytesReturned = 0;
    if (!::DeviceIoControl(handle, IOCTL_SERIAL_GET_DTRRTS, nullptr, 0,
                          &modemStat, sizeof(modemStat),
                          &bytesReturned, nullptr)) {
        setError(getSystemError());
        return ret;
    }

    if (modemStat & SERIAL_DTR_STATE)
        ret |= QSerialPort::DataTerminalReadySignal;
    if (modemStat & SERIAL_RTS_STATE)
        ret |= QSerialPort::RequestToSendSignal;

    return ret;
}

bool QSerialPortPrivate::setDataTerminalReady(bool set)
{
    if (!::EscapeCommFunction(handle, set ? SETDTR : CLRDTR)) {
        setError(getSystemError());
        return false;
    }

    DCB dcb;
    if (!getDcb(&dcb))
        return false;

    dcb.fDtrControl = set ? DTR_CONTROL_ENABLE : DTR_CONTROL_DISABLE;
    return setDcb(&dcb);
}

bool QSerialPortPrivate::setRequestToSend(bool set)
{
    if (!::EscapeCommFunction(handle, set ? SETRTS : CLRRTS)) {
        setError(getSystemError());
        return false;
    }

    DCB dcb;
    if (!getDcb(&dcb))
        return false;

    dcb.fRtsControl = set ? RTS_CONTROL_ENABLE : RTS_CONTROL_DISABLE;
    return setDcb(&dcb);
}

bool QSerialPortPrivate::flush()
{
    return _q_startAsyncWrite();
}

bool QSerialPortPrivate::clear(QSerialPort::Directions directions)
{
    DWORD flags = 0;
    if (directions & QSerialPort::Input)
        flags |= PURGE_RXABORT | PURGE_RXCLEAR;
    if (directions & QSerialPort::Output)
        flags |= PURGE_TXABORT | PURGE_TXCLEAR;
    if (!::PurgeComm(handle, flags)) {
        setError(getSystemError());
        return false;
    }

    // We need start async read because a reading can be stalled. Since the
    // PurgeComm can abort of current reading sequence, or a port is in hardware
    // flow control mode, or a port has a limited read buffer size.
    if (directions & QSerialPort::Input)
        startAsyncCommunication();

    return true;
}

bool QSerialPortPrivate::sendBreak(int duration)
{
    if (!setBreakEnabled(true))
        return false;

    ::Sleep(duration);

    if (!setBreakEnabled(false))
        return false;

    return true;
}

bool QSerialPortPrivate::setBreakEnabled(bool set)
{
    if (set ? !::SetCommBreak(handle) : !::ClearCommBreak(handle)) {
        setError(getSystemError());
        return false;
    }

    return true;
}

bool QSerialPortPrivate::waitForReadyRead(int msecs)
{
    if (!writeStarted && !_q_startAsyncWrite())
        return false;

    const qint64 initialReadBufferSize = buffer.size();
    qint64 currentReadBufferSize = initialReadBufferSize;

    QElapsedTimer stopWatch;
    stopWatch.start();

    do {
        const OVERLAPPED *overlapped = waitForNotified(
                    qt_subtract_from_timeout(msecs, stopWatch.elapsed()));
        if (!overlapped)
            return false;

        if (overlapped == &readCompletionOverlapped) {
            const qint64 readBytesForOneReadOperation = qint64(buffer.size()) - currentReadBufferSize;
            if (readBytesForOneReadOperation == ReadChunkSize) {
                currentReadBufferSize = buffer.size();
            } else if (readBytesForOneReadOperation == 0) {
                if (initialReadBufferSize != currentReadBufferSize)
                    return true;
            } else {
                return true;
            }
        }

    } while (msecs == -1 || qt_subtract_from_timeout(msecs, stopWatch.elapsed()) > 0);

    return false;
}

bool QSerialPortPrivate::waitForBytesWritten(int msecs)
{
    if (writeBuffer.isEmpty() && writeChunkBuffer.isEmpty())
        return false;

    if (!writeStarted && !_q_startAsyncWrite())
        return false;

    QElapsedTimer stopWatch;
    stopWatch.start();

    for (;;) {
        const OVERLAPPED *overlapped = waitForNotified(
                    qt_subtract_from_timeout(msecs, stopWatch.elapsed()));
        if (!overlapped)
            return false;

         if (overlapped == &writeCompletionOverlapped)
            return true;
    }

    return false;
}

bool QSerialPortPrivate::setBaudRate()
{
    return setBaudRate(inputBaudRate, QSerialPort::AllDirections);
}

bool QSerialPortPrivate::setBaudRate(qint32 baudRate, QSerialPort::Directions directions)
{
    if (directions != QSerialPort::AllDirections) {
        setError(QSerialPortErrorInfo(QSerialPort::UnsupportedOperationError, QSerialPort::tr("Custom baud rate direction is unsupported")));
        return false;
    }

    DCB dcb;
    if (!getDcb(&dcb))
        return false;

    qt_set_baudrate(&dcb, baudRate);

    return setDcb(&dcb);
}

bool QSerialPortPrivate::setDataBits(QSerialPort::DataBits dataBits)
{
    DCB dcb;
    if (!getDcb(&dcb))
        return false;

    qt_set_databits(&dcb, dataBits);

    return setDcb(&dcb);
}

bool QSerialPortPrivate::setParity(QSerialPort::Parity parity)
{
    DCB dcb;
    if (!getDcb(&dcb))
        return false;

    qt_set_parity(&dcb, parity);

    return setDcb(&dcb);
}

bool QSerialPortPrivate::setStopBits(QSerialPort::StopBits stopBits)
{
    DCB dcb;
    if (!getDcb(&dcb))
        return false;

    qt_set_stopbits(&dcb, stopBits);

    return setDcb(&dcb);
}

bool QSerialPortPrivate::setFlowControl(QSerialPort::FlowControl flowControl)
{
    DCB dcb;
    if (!getDcb(&dcb))
        return false;

    qt_set_flowcontrol(&dcb, flowControl);

    return setDcb(&dcb);
}

bool QSerialPortPrivate::completeAsyncCommunication(qint64 bytesTransferred)
{
    communicationStarted = false;

    if (bytesTransferred == qint64(-1))
        return false;

    return startAsyncRead();
}

bool QSerialPortPrivate::completeAsyncRead(qint64 bytesTransferred)
{
    if (bytesTransferred == qint64(-1)) {
        readStarted = false;
        return false;
    }
    if (bytesTransferred > 0)
        buffer.append(readChunkBuffer.constData(), bytesTransferred);

    readStarted = false;

    bool result = true;
    if (bytesTransferred == ReadChunkSize
            || queuedBytesCount(QSerialPort::Input) > 0) {
        result = startAsyncRead();
    } else if (readBufferMaxSize == 0
               || readBufferMaxSize > buffer.size()) {
        result = startAsyncCommunication();
    }

    if (bytesTransferred > 0)
        emitReadyRead();

    return result;
}

bool QSerialPortPrivate::completeAsyncWrite(qint64 bytesTransferred)
{
    Q_Q(QSerialPort);

    if (writeStarted) {
        if (bytesTransferred == qint64(-1)) {
            writeChunkBuffer.clear();
            writeStarted = false;
            return false;
        }
        Q_ASSERT(bytesTransferred == writeChunkBuffer.size());
        writeChunkBuffer.clear();
        emit q->bytesWritten(bytesTransferred);
        writeStarted = false;
    }

    return _q_startAsyncWrite();
}

bool QSerialPortPrivate::startAsyncCommunication()
{
    if (communicationStarted)
        return true;

    ::ZeroMemory(&communicationOverlapped, sizeof(communicationOverlapped));
    if (!::WaitCommEvent(handle, &triggeredEventMask, &communicationOverlapped)) {
        QSerialPortErrorInfo error = getSystemError();
        if (error.errorCode != QSerialPort::NoError) {
            if (error.errorCode == QSerialPort::PermissionError)
                error.errorCode = QSerialPort::ResourceError;
            setError(error);
            return false;
        }
    }
    communicationStarted = true;
    return true;
}

bool QSerialPortPrivate::startAsyncRead()
{
    if (readStarted)
        return true;

    DWORD bytesToRead = ReadChunkSize;

    if (readBufferMaxSize && bytesToRead > (readBufferMaxSize - buffer.size())) {
        bytesToRead = readBufferMaxSize - buffer.size();
        if (bytesToRead == 0) {
            // Buffer is full. User must read data from the buffer
            // before we can read more from the port.
            return false;
        }
    }

    ::ZeroMemory(&readCompletionOverlapped, sizeof(readCompletionOverlapped));
    if (::ReadFile(handle, readChunkBuffer.data(), bytesToRead, nullptr, &readCompletionOverlapped)) {
        readStarted = true;
        return true;
    }

    QSerialPortErrorInfo error = getSystemError();
    if (error.errorCode != QSerialPort::NoError) {
        if (error.errorCode == QSerialPort::PermissionError)
            error.errorCode = QSerialPort::ResourceError;
        if (error.errorCode != QSerialPort::ResourceError)
            error.errorCode = QSerialPort::ReadError;
        setError(error);
        return false;
    }

    readStarted = true;
    return true;
}

bool QSerialPortPrivate::_q_startAsyncWrite()
{
    if (writeBuffer.isEmpty() || writeStarted)
        return true;

    writeChunkBuffer = writeBuffer.read();
    ::ZeroMemory(&writeCompletionOverlapped, sizeof(writeCompletionOverlapped));
    if (!::WriteFile(handle, writeChunkBuffer.constData(),
                     writeChunkBuffer.size(), nullptr, &writeCompletionOverlapped)) {

        QSerialPortErrorInfo error = getSystemError();
        if (error.errorCode != QSerialPort::NoError) {
            if (error.errorCode != QSerialPort::ResourceError)
                error.errorCode = QSerialPort::WriteError;
            setError(error);
            return false;
        }
    }

    writeStarted = true;
    return true;
}

void QSerialPortPrivate::_q_notified(DWORD numberOfBytes, DWORD errorCode, OVERLAPPED *overlapped)
{
    const QSerialPortErrorInfo error = getSystemError(errorCode);
    if (error.errorCode != QSerialPort::NoError) {
        setError(error);
        return;
    }

    if (overlapped == &communicationOverlapped)
        completeAsyncCommunication(numberOfBytes);
    else if (overlapped == &readCompletionOverlapped)
        completeAsyncRead(numberOfBytes);
    else if (overlapped == &writeCompletionOverlapped)
        completeAsyncWrite(numberOfBytes);
    else
        Q_ASSERT(!"Unknown OVERLAPPED activated");
}

void QSerialPortPrivate::emitReadyRead()
{
    Q_Q(QSerialPort);

    emit q->readyRead();
}

qint64 QSerialPortPrivate::writeData(const char *data, qint64 maxSize)
{
    Q_Q(QSerialPort);

    writeBuffer.append(data, maxSize);

    if (!writeBuffer.isEmpty() && !writeStarted) {
        if (!startAsyncWriteTimer) {
            startAsyncWriteTimer = new QTimer(q);
            QObjectPrivate::connect(startAsyncWriteTimer, &QTimer::timeout, this, &QSerialPortPrivate::_q_startAsyncWrite);
            startAsyncWriteTimer->setSingleShot(true);
        }
        if (!startAsyncWriteTimer->isActive())
            startAsyncWriteTimer->start();
    }
    return maxSize;
}

OVERLAPPED *QSerialPortPrivate::waitForNotified(int msecs)
{
    OVERLAPPED *overlapped = notifier->waitForAnyNotified(msecs);
    if (!overlapped) {
        setError(getSystemError(WAIT_TIMEOUT));
        return nullptr;
    }
    return overlapped;
}

qint64 QSerialPortPrivate::queuedBytesCount(QSerialPort::Direction direction) const
{
    COMSTAT comstat;
    if (::ClearCommError(handle, nullptr, &comstat) == 0)
        return -1;
    return (direction == QSerialPort::Input)
            ? comstat.cbInQue
            : ((direction == QSerialPort::Output) ? comstat.cbOutQue : -1);
}

inline bool QSerialPortPrivate::initialize()
{
    Q_Q(QSerialPort);

    DCB dcb;
    if (!getDcb(&dcb))
        return false;

    restoredDcb = dcb;

    qt_set_common_props(&dcb);
    qt_set_baudrate(&dcb, inputBaudRate);
    qt_set_databits(&dcb, dataBits);
    qt_set_parity(&dcb, parity);
    qt_set_stopbits(&dcb, stopBits);
    qt_set_flowcontrol(&dcb, flowControl);

    if (!setDcb(&dcb))
        return false;

    if (!::GetCommTimeouts(handle, &restoredCommTimeouts)) {
        setError(getSystemError());
        return false;
    }

    ::ZeroMemory(&currentCommTimeouts, sizeof(currentCommTimeouts));
    currentCommTimeouts.ReadIntervalTimeout = MAXDWORD;

    if (!::SetCommTimeouts(handle, &currentCommTimeouts)) {
        setError(getSystemError());
        return false;
    }

    if (!::SetCommMask(handle, originalEventMask)) {
        setError(getSystemError());
        return false;
    }

    notifier = new QWinOverlappedIoNotifier(q);
    QObjectPrivate::connect(notifier, &QWinOverlappedIoNotifier::notified,
               this, &QSerialPortPrivate::_q_notified);
    notifier->setHandle(handle);
    notifier->setEnabled(true);

    if ((originalEventMask & EV_RXCHAR) && !startAsyncCommunication())
        return false;

    return true;
}

bool QSerialPortPrivate::setDcb(DCB *dcb)
{
    if (!::SetCommState(handle, dcb)) {
        setError(getSystemError());
        return false;
    }
    return true;
}

bool QSerialPortPrivate::getDcb(DCB *dcb)
{
    ::ZeroMemory(dcb, sizeof(DCB));
    dcb->DCBlength = sizeof(DCB);

    if (!::GetCommState(handle, dcb)) {
        setError(getSystemError());
        return false;
    }
    return true;
}

QSerialPortErrorInfo QSerialPortPrivate::getSystemError(int systemErrorCode) const
{
    if (systemErrorCode == -1)
        systemErrorCode = ::GetLastError();

    QSerialPortErrorInfo error;
    error.errorString = qt_error_string(systemErrorCode);

    switch (systemErrorCode) {
    case ERROR_SUCCESS:
        error.errorCode = QSerialPort::NoError;
        break;
    case ERROR_IO_PENDING:
        error.errorCode = QSerialPort::NoError;
        break;
    case ERROR_MORE_DATA:
        error.errorCode = QSerialPort::NoError;
        break;
    case ERROR_FILE_NOT_FOUND:
        error.errorCode = QSerialPort::DeviceNotFoundError;
        break;
    case ERROR_PATH_NOT_FOUND:
        error.errorCode = QSerialPort::DeviceNotFoundError;
        break;
    case ERROR_INVALID_NAME:
        error.errorCode = QSerialPort::DeviceNotFoundError;
        break;
    case ERROR_ACCESS_DENIED:
        error.errorCode = QSerialPort::PermissionError;
        break;
    case ERROR_INVALID_HANDLE:
        error.errorCode = QSerialPort::ResourceError;
        break;
    case ERROR_INVALID_PARAMETER:
        error.errorCode = QSerialPort::UnsupportedOperationError;
        break;
    case ERROR_BAD_COMMAND:
        error.errorCode = QSerialPort::ResourceError;
        break;
    case ERROR_DEVICE_REMOVED:
        error.errorCode = QSerialPort::ResourceError;
        break;
    case ERROR_OPERATION_ABORTED:
        error.errorCode = QSerialPort::ResourceError;
        break;
    case WAIT_TIMEOUT:
        error.errorCode = QSerialPort::TimeoutError;
        break;
    default:
        error.errorCode = QSerialPort::UnknownError;
        break;
    }
    return error;
}

// This table contains standard values of baud rates that
// are defined in MSDN and/or in Win SDK file winbase.h

static const QList<qint32> standardBaudRatePairList()
{

    static const QList<qint32> standardBaudRatesTable = QList<qint32>()

        #ifdef CBR_110
            << CBR_110
        #endif

        #ifdef CBR_300
            << CBR_300
        #endif

        #ifdef CBR_600
            << CBR_600
        #endif

        #ifdef CBR_1200
            << CBR_1200
        #endif

        #ifdef CBR_2400
            << CBR_2400
        #endif

        #ifdef CBR_4800
            << CBR_4800
        #endif

        #ifdef CBR_9600
            << CBR_9600
        #endif

        #ifdef CBR_14400
            << CBR_14400
        #endif

        #ifdef CBR_19200
            << CBR_19200
        #endif

        #ifdef CBR_38400
            << CBR_38400
        #endif

        #ifdef CBR_56000
            << CBR_56000
        #endif

        #ifdef CBR_57600
            << CBR_57600
        #endif

        #ifdef CBR_115200
            << CBR_115200
        #endif

        #ifdef CBR_128000
            << CBR_128000
        #endif

        #ifdef CBR_256000
            << CBR_256000
        #endif
    ;

    return standardBaudRatesTable;
};

QList<qint32> QSerialPortPrivate::standardBaudRates()
{
    return standardBaudRatePairList();
}

QSerialPort::Handle QSerialPort::handle() const
{
    Q_D(const QSerialPort);
    return d->handle;
}

QT_END_NAMESPACE
