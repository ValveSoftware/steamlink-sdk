/****************************************************************************
**
** Copyright (C) 2012 Denis Shienkov <denis.shienkov@gmail.com>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtSerialPort module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qserialport_symbian_p.h"

#include <QtCore/qmap.h>

#include <e32base.h>
//#include <e32test.h>
#include <f32file.h>

QT_BEGIN_NAMESPACE

// Physical device driver.
#ifdef __WINS__
_LIT(KPddName, "ECDRV");
#else // defined (__EPOC32__)
_LIT(KPddName, "EUART");
#endif

// Logical  device driver.
_LIT(KLddName,"ECOMM");

// Modules names.
_LIT(KRS232ModuleName, "ECUART");
_LIT(KBluetoothModuleName, "BTCOMM");
_LIT(KInfraRedModuleName, "IRCOMM");
_LIT(KACMModuleName, "ECACM");

// Return false on error load.
static bool loadDevices()
{
    TInt r = KErrNone;
#ifdef __WINS__
    RFs fileServer;
    r = User::LeaveIfError(fileServer.Connect());
    if (r != KErrNone)
        return false;
    fileServer.Close ();
#endif

    r = User::LoadPhysicalDevice(KPddName);
    if (r != KErrNone && r != KErrAlreadyExists)
        return false; //User::Leave(r);

    r = User::LoadLogicalDevice(KLddName);
    if (r != KErrNone && r != KErrAlreadyExists)
        return false; //User::Leave(r);

#ifndef __WINS__
    r = StartC32();
    if (r != KErrNone && r != KErrAlreadyExists)
        return false; //User::Leave(r);
#endif

    return true;
}

QSerialPortPrivate::QSerialPortPrivate(QSerialPort *q)
    : QSerialPortPrivateData(q)
    , errnum(KErrNone)
{
}

bool QSerialPortPrivate::open(QIODevice::OpenMode mode)
{
    Q_Q(QSerialPort);

    // FIXME: Maybe need added check an ReadWrite open mode?
    Q_UNUSED(mode)

    if (!loadDevices()) {
        q->setError(QSerialPort::UnknownError);
        return false;
    }

    RCommServ server;
    errnum = server.Connect();
    if (errnum != KErrNone) {
        q->setError(decodeSystemError());
        return false;
    }

    if (systemLocation.contains("BTCOMM"))
        errnum = server.LoadCommModule(KBluetoothModuleName);
    else if (systemLocation.contains("IRCOMM"))
        errnum = server.LoadCommModule(KInfraRedModuleName);
    else if (systemLocation.contains("ACM"))
        errnum = server.LoadCommModule(KACMModuleName);
    else
        errnum = server.LoadCommModule(KRS232ModuleName);

    if (errnum != KErrNone) {
        q->setError(decodeSystemError());
        return false;
    }

    // In Symbian OS port opening only in R/W mode?
    TPtrC portName(static_cast<const TUint16*>(systemLocation.utf16()), systemLocation.length());
    errnum = descriptor.Open(server, portName, ECommExclusive);

    if (errnum != KErrNone) {
        q->setError(decodeSystemError());
        return false;
    }

    // Save current port settings.
    errnum = descriptor.Config(restoredSettings);
    if (errnum != KErrNone) {
        q->setError(decodeSystemError());
        return false;
    }

    return true;
}

void QSerialPortPrivate::close()
{
    if (settingsRestoredOnClose)
        descriptor.SetConfig(restoredSettings);
    descriptor.Close();
}

QSerialPort::PinoutSignals QSerialPortPrivate::pinoutSignals()
{
    QSerialPort::PinoutSignals ret = QSerialPort::NoSignal;

    TUint signalMask = 0;
    descriptor.Signals(signalMask);

    if (signalMask & KSignalCTS)
        ret |= QSerialPort::ClearToSendSignal;
    if (signalMask & KSignalDSR)
        ret |= QSerialPort::DataSetReadySignal;
    if (signalMask & KSignalDCD)
        ret |= QSerialPort::DataCarrierDetectSignal;
    if (signalMask & KSignalRNG)
        ret |= QSerialPort::RingIndicatorSignal;
    if (signalMask & KSignalRTS)
        ret |= QSerialPort::RequestToSendSignal;
    if (signalMask & KSignalDTR)
        ret |= QSerialPort::DataTerminalReadySignal;

    //if (signalMask & KSignalBreak)
    //  ret |=
    return ret;
}

bool QSerialPortPrivate::setDataTerminalReady(bool set)
{
    TInt r;
    if (set)
        r = descriptor.SetSignalsToMark(KSignalDTR);
    else
        r = descriptor.SetSignalsToSpace(KSignalDTR);

    return r == KErrNone;
}

bool QSerialPortPrivate::setRequestToSend(bool set)
{
    TInt r;
    if (set)
        r = descriptor.SetSignalsToMark(KSignalRTS);
    else
        r = descriptor.SetSignalsToSpace(KSignalRTS);

    return r == KErrNone;
}

bool QSerialPortPrivate::flush()
{
    // TODO: Implement me
    return false;
}

bool QSerialPortPrivate::clear(QSerialPort::Directions directions)
{
    TUint flags = 0;
    if (directions & QSerialPort::Input)
        flags |= KCommResetRx;
    if (directions & QSerialPort::Output)
        flags |= KCommResetTx;
    TInt r = descriptor.ResetBuffers(flags);
    return r == KErrNone;
}

bool QSerialPortPrivate::sendBreak(int duration)
{
    TRequestStatus status;
    descriptor.Break(status, TTimeIntervalMicroSeconds32(duration * 1000));
    return false;
}

bool QSerialPortPrivate::setBreakEnabled(bool set)
{
    // TODO: Implement me
    return false;
}

void QSerialPortPrivate::startWriting()
{
    // TODO: Implement me
}

bool QSerialPortPrivate::waitForReadyRead(int msec)
{
    // TODO: Implement me
    return false;
}

bool QSerialPortPrivate::waitForBytesWritten(int msec)
{
    // TODO: Implement me
    return false;
}

bool QSerialPortPrivate::setBaudRate()
{
    return setBaudRate(inputBaudRate, QSerialPort::AllDirections);
}

bool QSerialPortPrivate::setBaudRate(qint32 baudRate, QSerialPort::Directions directions)
{
    Q_Q(QSerialPort);

    if (directions != QSerialPort::AllDirections) {
        q->setError(QSerialPort::UnsupportedOperationError);
        return false;
    }

    baudRate = settingFromBaudRate(baudRate);
    if (baudRate)
        currentSettings().iRate = static_cast<TBps>(baudRate);
    else {
        q->setError(QSerialPort::UnsupportedOperationError);
        return false;
    }

    return updateCommConfig();
}

bool QSerialPortPrivate::setDataBits(QSerialPort::DataBits dataBits)
{
    switch (dataBits) {
    case QSerialPort::Data5:
        currentSettings().iDataBits = EData5;
        break;
    case QSerialPort::Data6:
        currentSettings().iDataBits = EData6;
        break;
    case QSerialPort::Data7:
        currentSettings().iDataBits = EData7;
        break;
    case QSerialPort::Data8:
        currentSettings().iDataBits = EData8;
        break;
    default:
        currentSettings().iDataBits = EData8;
        break;
    }

    return updateCommConfig();
}

bool QSerialPortPrivate::setParity(QSerialPort::Parity parity)
{
    switch (parity) {
    case QSerialPort::NoParity:
        currentSettings().iParity = EParityNone;
        break;
    case QSerialPort::EvenParity:
        currentSettings().iParity = EParityEven;
        break;
    case QSerialPort::OddParity:
        currentSettings().iParity = EParityOdd;
        break;
    case QSerialPort::MarkParity:
        currentSettings().iParity = EParityMark;
        break;
    case QSerialPort::SpaceParity:
        currentSettings().iParity = EParitySpace;
        break;
    default:
        currentSettings().iParity = EParityNone;
        break;
    }

    return updateCommConfig();
}

bool QSerialPortPrivate::setStopBits(QSerialPort::StopBits stopBits)
{
    switch (stopBits) {
    case QSerialPort::OneStop:
        currentSettings().iStopBits = EStop1;
        break;
    case QSerialPort::TwoStop:
        currentSettings().iStopBits = EStop2;
        break;
    default:
        currentSettings().iStopBits = EStop1;
        break;
    }

    return updateCommConfig();
}

bool QSerialPortPrivate::setFlowControl(QSerialPort::FlowControl flowControl)
{
    switch (flowControl) {
    case QSerialPort::NoFlowControl:
        currentSettings().iHandshake = KConfigFailDSR;
        break;
    case QSerialPort::HardwareControl:
        currentSettings().iHandshake = KConfigObeyCTS | KConfigFreeRTS;
        break;
    case QSerialPort::SoftwareControl:
        currentSettings().iHandshake = KConfigObeyXoff | KConfigSendXoff;
        break;
    default:
        currentSettings().iHandshake = KConfigFailDSR;
        break;
    }

    return updateCommConfig();
}

bool QSerialPortPrivate::setDataErrorPolicy(QSerialPort::DataErrorPolicy policy)
{
    // TODO: Implement me
    return false;
}

bool QSerialPortPrivate::notifyRead()
{
    // TODO: Implement me
    return false;
}

bool QSerialPortPrivate::notifyWrite()
{
    // TODO: Implement me
    return false;
}

bool QSerialPortPrivate::updateCommConfig()
{
    Q_Q(QSerialPort);

    if (descriptor.SetConfig(currentSettings) != KErrNone) {
        q->setError(QSerialPort::UnsupportedOperationError);
        return false;
    }
    return true;
}

QSerialPort::SerialPortError QSerialPortPrivate::decodeSystemError() const
{
    QSerialPort::SerialPortError error;
    switch (errnum) {
    case KErrPermissionDenied:
        error = QSerialPort::DeviceNotFoundError;
        break;
    case KErrLocked:
        error = QSerialPort::PermissionError;
        break;
    case KErrAccessDenied:
        error = QSerialPort::PermissionError;
        break;
    default:
        error = QSerialPort::UnknownError;
        break;
    }
    return error;
}

bool QSerialPortPrivate::waitForReadOrWrite(bool *selectForRead, bool *selectForWrite,
                                           bool checkRead, bool checkWrite,
                                           int msecs, bool *timedOut)
{

    // FIXME: I'm not sure in implementation this method.
    // Someone needs to check and correct.

    TRequestStatus timerStatus;
    TRequestStatus readStatus;
    TRequestStatus writeStatus;

    if (msecs > 0)  {
        if (!selectTimer.Handle()) {
            if (selectTimer.CreateLocal() != KErrNone)
                return false;
        }
        selectTimer.HighRes(timerStatus, msecs * 1000);
    }

    if (checkRead)
        descriptor.NotifyDataAvailable(readStatus);

    if (checkWrite)
        descriptor.NotifyOutputEmpty(writeStatus);

    enum { STATUSES_COUNT = 3 };
    TRequestStatus *statuses[STATUSES_COUNT];
    TInt num = 0;
    statuses[num++] = &timerStatus;
    statuses[num++] = &readStatus;
    statuses[num++] = &writeStatus;

    User::WaitForNRequest(statuses, num);

    bool result = false;

    // By timeout?
    if (timerStatus != KRequestPending) {
        Q_ASSERT(selectForRead);
        *selectForRead = false;
        Q_ASSERT(selectForWrite);
        *selectForWrite = false;
    } else {
        selectTimer.Cancel();
        User::WaitForRequest(timerStatus);

        // By read?
        if (readStatus != KRequestPending) {
            Q_ASSERT(selectForRead);
            *selectForRead = true;
        }

        // By write?
        if (writeStatus != KRequestPending) {
            Q_ASSERT(selectForWrite);
            *selectForWrite = true;
        }

        if (checkRead)
            descriptor.NotifyDataAvailableCancel();
        if (checkWrite)
            descriptor.NotifyOutputEmptyCancel();

        result = true;
    }
    return result;
}

QString QSerialPortPrivate::portNameToSystemLocation(const QString &port)
{
    // Port name is equval to port systemLocation.
    return port;
}

QString QSerialPortPrivate::portNameFromSystemLocation(const QString &location)
{
    // Port name is equval to port systemLocation.
    return location;
}

typedef QMap<qint32, qint32> BaudRateMap;

// This table contains correspondences standard pairs values of
// baud rates that are defined in files
// - d32comm.h for Symbian^3
// - d32public.h for Symbian SR1

static const BaudRateMap createStandardBaudRateMap()
{
    BaudRateMap baudRateMap;

    baudRateMap.insert(50, EBps50)
    baudRateMap.insert(75, EBps75)
    baudRateMap.insert(110, EBps110)
    baudRateMap.insert(134, EBps134)
    baudRateMap.insert(150, EBps150)
    baudRateMap.insert(300, EBps300)
    baudRateMap.insert(600, EBps600)
    baudRateMap.insert(1200, EBps1200)
    baudRateMap.insert(1800, EBps1800)
    baudRateMap.insert(2000, EBps2000)
    baudRateMap.insert(2400, EBps2400)
    baudRateMap.insert(3600, EBps3600)
    baudRateMap.insert(4800, EBps4800)
    baudRateMap.insert(7200, EBps7200)
    baudRateMap.insert(9600, EBps9600)
    baudRateMap.insert(19200, EBps19200)
    baudRateMap.insert(38400, EBps38400)
    baudRateMap.insert(57600, EBps57600)
    baudRateMap.insert(115200, EBps115200)
    baudRateMap.insert(230400, EBps230400)
    baudRateMap.insert(460800, EBps460800)
    baudRateMap.insert(576000, EBps576000)
    baudRateMap.insert(921600, EBps921600)
    baudRateMap.insert(1152000, EBps1152000)
        // << baudRateMap.insert(1843200, EBps1843200) only for Symbian SR1
    baudRateMap.insert(4000000, EBps4000000);

    return baudRateMap;
}

static const BaudRateMap& standardBaudRateMap()
{
    static const BaudRateMap baudRateMap = createStandardBaudRateMap();
    return baudRateMap;
}

qint32 QSerialPortPrivate::baudRateFromSetting(qint32 setting)
{
    return standardBaudRateMap().key(setting);
}

qint32 QSerialPortPrivate::settingFromBaudRate(qint32 baudRate)
{
    return standardBaudRateMap().value(baudRate);
}

QList<qint32> QSerialPortPrivate::standardBaudRates()
{
    return standardBaudRateMap().keys();
}

QSerialPort::Handle QSerialPort::handle() const
{
    return -1;
}

QT_END_NAMESPACE
