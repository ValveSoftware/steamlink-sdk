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

#include "tinycanbackend.h"
#include "tinycanbackend_p.h"

#include "tinycan_symbols_p.h"

#include <QtSerialBus/qcanbusdevice.h>

#include <QtCore/qtimer.h>
#include <QtCore/qmutex.h>
#include <QtCore/qcoreevent.h>

#include <algorithm>

QT_BEGIN_NAMESPACE

#ifndef LINK_LIBMHSTCAN
Q_GLOBAL_STATIC(QLibrary, mhstcanLibrary)
#endif

bool TinyCanBackend::canCreate(QString *errorReason)
{
#ifdef LINK_LIBMHSTCAN
    return true;
#else
    static bool symbolsResolved = resolveSymbols(mhstcanLibrary());
    if (!symbolsResolved) {
        *errorReason = mhstcanLibrary()->errorString();
        return false;
    }
    return true;
#endif
}

Q_GLOBAL_STATIC(QList<TinyCanBackendPrivate *>, qChannels)

static QMutex channelsGuard(QMutex::NonRecursive);

class WriteNotifier : public QTimer
{
    // no Q_OBJECT macro!
public:
    WriteNotifier(TinyCanBackendPrivate *d, QObject *parent)
        : QTimer(parent)
        , dptr(d)
    {
    }

protected:
    void timerEvent(QTimerEvent *e) override
    {
        if (e->timerId() == timerId()) {
            dptr->startWrite();
            return;
        }
        QTimer::timerEvent(e);
    }

private:
    TinyCanBackendPrivate *dptr;
};

static int driverRefCount = 0;

static void DRV_CALLBACK_TYPE canRxEventCallback(quint32 index, TCanMsg *frame, qint32 count)
{
    Q_UNUSED(frame);
    Q_UNUSED(count);

    QMutexLocker lock(&channelsGuard);
    foreach (TinyCanBackendPrivate *p, *qChannels()) {
        if (p->channelIndex == int(index)) {
            p->startRead();
            return;
        }
    }
}

TinyCanBackendPrivate::TinyCanBackendPrivate(TinyCanBackend *q)
    : q_ptr(q)
    , isOpen(false)
    , channelIndex(INDEX_INVALID)
    , writeNotifier(nullptr)
{
    startupDriver();

    QMutexLocker lock(&channelsGuard);
    qChannels()->append(this);
}

TinyCanBackendPrivate::~TinyCanBackendPrivate()
{
    cleanupDriver();

    QMutexLocker lock(&channelsGuard);
    qChannels()->removeAll(this);
}

struct BitrateItem
{
    int bitrate;
    int code;
};

struct BitrateLessFunctor
{
    bool operator()( const BitrateItem &item1, const BitrateItem &item2) const
    {
        return item1.bitrate < item2.bitrate;
    }
};

static int bitrateCodeFromBitrate(int bitrate)
{
    static const BitrateItem bitratetable[] = {
        { 10000, CAN_10K_BIT },
        { 20000, CAN_20K_BIT },
        { 50000, CAN_50K_BIT },
        { 100000, CAN_100K_BIT },
        { 125000, CAN_125K_BIT },
        { 250000, CAN_250K_BIT },
        { 500000, CAN_500K_BIT },
        { 800000, CAN_800K_BIT },
        { 1000000, CAN_1M_BIT }
    };

    static const BitrateItem *endtable = bitratetable + (sizeof(bitratetable) / sizeof(*bitratetable));

    const BitrateItem item = { bitrate , 0 };
    const BitrateItem *where = std::lower_bound(bitratetable, endtable, item, BitrateLessFunctor());
    return where != endtable ? where->code : -1;
}

bool TinyCanBackendPrivate::open()
{
    Q_Q(TinyCanBackend);

    {
        char options[] = "AutoConnect=1;AutoReopen=0";
        const int ret = ::CanSetOptions(options);
        if (ret < 0) {
            q->setError(systemErrorString(ret), QCanBusDevice::CanBusError::ConnectionError);
            return false;
        }
    }

    {
        const int ret = ::CanDeviceOpen(channelIndex, nullptr);
        if (ret < 0) {
            q->setError(systemErrorString(ret), QCanBusDevice::CanBusError::ConnectionError);
            return false;
        }
    }

    {
        const int ret = ::CanSetMode(channelIndex, OP_CAN_START, CAN_CMD_ALL_CLEAR);
        if (ret < 0) {
            q->setError(systemErrorString(ret), QCanBusDevice::CanBusError::ConnectionError);
            ::CanDeviceClose(channelIndex);
            return false;
        }
    }

    writeNotifier = new WriteNotifier(this, q);
    writeNotifier->setInterval(0);

    isOpen = true;
    return true;
}

void TinyCanBackendPrivate::close()
{
    Q_Q(TinyCanBackend);

    delete writeNotifier;
    writeNotifier = nullptr;

    const int ret = ::CanDeviceClose(channelIndex);
    if (ret < 0)
        q->setError(systemErrorString(ret), QCanBusDevice::CanBusError::ConnectionError);

    isOpen = false;
}

bool TinyCanBackendPrivate::setConfigurationParameter(int key, const QVariant &value)
{
    Q_Q(TinyCanBackend);

    switch (key) {
    case QCanBusDevice::BitRateKey:
        return setBitRate(value.toInt());
    default:
        q->setError(TinyCanBackend::tr("Unsupported configuration key: %1").arg(key),
                    QCanBusDevice::ConfigurationError);
        return false;
    }
}

// These error codes taked from the errors.h file, which
// exists only in linux sources.
QString TinyCanBackendPrivate::systemErrorString(int errorCode)
{
    switch (errorCode) {
    case 0:
        return TinyCanBackend::tr("No error");
    case -1:
        return TinyCanBackend::tr("Driver not initialized");
    case -2:
        return TinyCanBackend::tr("Invalid parameters values were passed");
    case -3:
        return TinyCanBackend::tr("Invalid index value");
    case -4:
        return TinyCanBackend::tr("More invalid CAN-channel");
    case -5:
        return TinyCanBackend::tr("General error");
    case -6:
        return TinyCanBackend::tr("The FIFO cannot be written");
    case -7:
        return TinyCanBackend::tr("The buffer cannot be written");
    case -8:
        return TinyCanBackend::tr("The FIFO cannot be read");
    case -9:
        return TinyCanBackend::tr("The buffer cannot be read");
    case -10:
        return TinyCanBackend::tr("Variable not found");
    case -11:
        return TinyCanBackend::tr("Reading of the variable does not permit");
    case -12:
        return TinyCanBackend::tr("Reading buffer for variable too small");
    case -13:
        return TinyCanBackend::tr("Writing of the variable does not permit");
    case -14:
        return TinyCanBackend::tr("The string/stream to be written is to majority");
    case -15:
        return TinyCanBackend::tr("Fell short min of value");
    case -16:
        return TinyCanBackend::tr("Max value crossed");
    case -17:
        return TinyCanBackend::tr("Access refuses");
    case -18:
        return TinyCanBackend::tr("Invalid value of CAN speed");
    case -19:
        return TinyCanBackend::tr("Invalid value of baud rate");
    case -20:
        return TinyCanBackend::tr("Value not put");
    case -21:
        return TinyCanBackend::tr("No connection to the hardware");
    case -22:
        return TinyCanBackend::tr("Communication error to the hardware");
    case -23:
        return TinyCanBackend::tr("Hardware sends wrong number of parameters");
    case -24:
        return TinyCanBackend::tr("Not enough main memory");
    case -25:
        return TinyCanBackend::tr("The system cannot provide the enough resources");
    case -26:
        return TinyCanBackend::tr("A system call returns with an error");
    case -27:
        return TinyCanBackend::tr("The main thread is occupied");
    case -28:
        return TinyCanBackend::tr("User allocated memory not found");
    case -29:
        return TinyCanBackend::tr("The main thread cannot be launched");
        // the codes -30...-33 are skipped, as they belongs to sockets
    default:
        return TinyCanBackend::tr("Unknown error");
    }
}

static int channelIndexFromName(const QString &interfaceName)
{
    if (interfaceName == QStringLiteral("can0.0"))
        return INDEX_CAN_KANAL_A;
    else if (interfaceName == QStringLiteral("can0.1"))
        return INDEX_CAN_KANAL_B;
    else
        return INDEX_INVALID;
}

void TinyCanBackendPrivate::setupChannel(const QString &interfaceName)
{
    channelIndex = channelIndexFromName(interfaceName);
}

// Calls only in constructor
void TinyCanBackendPrivate::setupDefaultConfigurations()
{
    Q_Q(TinyCanBackend);

    q->setConfigurationParameter(QCanBusDevice::BitRateKey, 500000);
}

void TinyCanBackendPrivate::startWrite()
{
    Q_Q(TinyCanBackend);

    if (!q->hasOutgoingFrames()) {
        writeNotifier->stop();
        return;
    }

    const QCanBusFrame frame = q->dequeueOutgoingFrame();
    const QByteArray payload = frame.payload();

    TCanMsg message;
    ::memset(&message, 0, sizeof(message));

    if (payload.size() > int(sizeof(message.Data.Bytes))) {
        qWarning("Impossible to write the message with unacceptable data size %d, ignored", payload.size());
    } else {
        message.Id = frame.frameId();
        message.Flags.Flag.Len = payload.size();
        message.Flags.Flag.Error = (frame.frameType() == QCanBusFrame::ErrorFrame);
        message.Flags.Flag.RTR = (frame.frameType() == QCanBusFrame::RemoteRequestFrame);
        message.Flags.Flag.TxD = 1;
        message.Flags.Flag.EFF = frame.hasExtendedFrameFormat();

        const qint32 messagesToWrite = 1;
        ::memcpy(message.Data.Bytes, payload.constData(), sizeof(message.Data.Bytes));
        const int ret = ::CanTransmit(channelIndex, &message, messagesToWrite);
        if (ret < 0)
            q->setError(systemErrorString(ret), QCanBusDevice::CanBusError::WriteError);
        else
            emit q->framesWritten(messagesToWrite);
    }

    if (q->hasOutgoingFrames() && !writeNotifier->isActive())
        writeNotifier->start();
}

// this method is called from the different thread!
void TinyCanBackendPrivate::startRead()
{
    Q_Q(TinyCanBackend);

    QVector<QCanBusFrame> newFrames;

    forever {
        if (!::CanReceiveGetCount(channelIndex))
            break;

        TCanMsg message;
        ::memset(&message, 0, sizeof(message));

        const int messagesToRead = 1;
        const int ret = ::CanReceive(channelIndex, &message, messagesToRead);
        if (ret < 0) {
            q->setError(systemErrorString(ret), QCanBusDevice::CanBusError::ReadError);

            TDeviceStatus status;
            ::memset(&status, 0, sizeof(status));

            if (::CanGetDeviceStatus(channelIndex, &status) < 0) {
                q->setError(systemErrorString(ret), QCanBusDevice::CanBusError::ReadError);
            } else {
                if (status.CanStatus == CAN_STATUS_BUS_OFF) {
                    qWarning("CAN bus is in off state, trying to reset the bus");
                    if (::CanSetMode(channelIndex, OP_CAN_RESET, CAN_CMD_NONE) < 0)
                        q->setError(systemErrorString(ret), QCanBusDevice::CanBusError::ReadError);
                }
            }

            continue;
        }

        QCanBusFrame frame(message.Id, QByteArray(reinterpret_cast<char *>(message.Data.Bytes),
                                                  int(message.Flags.Flag.Len)));
        frame.setTimeStamp(QCanBusFrame::TimeStamp(message.Time.Sec, message.Time.USec));
        frame.setExtendedFrameFormat(message.Flags.Flag.EFF);

        if (message.Flags.Flag.Error)
            frame.setFrameType(QCanBusFrame::ErrorFrame);
        else if (message.Flags.Flag.RTR)
            frame.setFrameType(QCanBusFrame::RemoteRequestFrame);
        else
            frame.setFrameType(QCanBusFrame::DataFrame);

        newFrames.append(std::move(frame));
    }

    q->enqueueReceivedFrames(newFrames);
}

void TinyCanBackendPrivate::startupDriver()
{
    Q_Q(TinyCanBackend);

    if (driverRefCount == 0) {
        const int ret = ::CanInitDriver(nullptr);
        if (ret < 0) {
            q->setError(systemErrorString(ret), QCanBusDevice::CanBusError::ConnectionError);
            return;
        }

        ::CanSetRxEventCallback(&canRxEventCallback);
        ::CanSetEvents(EVENT_ENABLE_RX_MESSAGES);

    } else if (driverRefCount < 0) {
        qCritical("Wrong reference counter: %d", driverRefCount);
        return;
    }

    ++driverRefCount;
}

void TinyCanBackendPrivate::cleanupDriver()
{
    --driverRefCount;

    if (driverRefCount < 0) {
        qCritical("Wrong reference counter: %d", driverRefCount);
        driverRefCount = 0;
    } else if (driverRefCount == 0) {
        ::CanSetEvents(EVENT_DISABLE_ALL);
        ::CanDownDriver();
    }
}

bool TinyCanBackendPrivate::setBitRate(int bitrate)
{
    Q_Q(TinyCanBackend);

    const int bitrateCode = bitrateCodeFromBitrate(bitrate);
    if (bitrateCode == -1) {
        q->setError(TinyCanBackend::tr("Unsupported bitrate value"),
                    QCanBusDevice::ConfigurationError);
        return false;
    }

    if (isOpen) {
        const int ret = ::CanSetSpeed(channelIndex, bitrateCode);
        if (ret < 0) {
            q->setError(systemErrorString(ret), QCanBusDevice::CanBusError::ConfigurationError);
            return false;
        }
    }

    return true;
}

TinyCanBackend::TinyCanBackend(const QString &name, QObject *parent)
    : QCanBusDevice(parent)
    , d_ptr(new TinyCanBackendPrivate(this))
{
    Q_D(TinyCanBackend);

    d->setupChannel(name);
    d->setupDefaultConfigurations();
}

TinyCanBackend::~TinyCanBackend()
{
    close();
    delete d_ptr;
}

bool TinyCanBackend::open()
{
    Q_D(TinyCanBackend);

    if (!d->isOpen) {
        if (!d->open()) {
            close(); // sets UnconnectedState
            return false;
        }

        // apply all stored configurations
        foreach (int key, configurationKeys()) {
            const QVariant param = configurationParameter(key);
            const bool success = d->setConfigurationParameter(key, param);
            if (!success) {
                qWarning() << "Cannot apply parameter:" << key
                           << "with value:" << param;
            }
        }
    }

    setState(QCanBusDevice::ConnectedState);
    return true;
}

void TinyCanBackend::close()
{
    Q_D(TinyCanBackend);

    d->close();

    setState(QCanBusDevice::UnconnectedState);
}

void TinyCanBackend::setConfigurationParameter(int key, const QVariant &value)
{
    Q_D(TinyCanBackend);

    if (d->setConfigurationParameter(key, value))
        QCanBusDevice::setConfigurationParameter(key, value);
}

bool TinyCanBackend::writeFrame(const QCanBusFrame &newData)
{
    Q_D(TinyCanBackend);

    if (state() != QCanBusDevice::ConnectedState)
        return false;

    if (!newData.isValid()) {
        setError(tr("Cannot write invalid QCanBusFrame"), QCanBusDevice::WriteError);
        return false;
    }

    if (newData.frameType() != QCanBusFrame::DataFrame
            && newData.frameType() != QCanBusFrame::RemoteRequestFrame
            && newData.frameType() != QCanBusFrame::ErrorFrame) {
        setError(tr("Unable to write a frame with unacceptable type"),
                 QCanBusDevice::WriteError);
        return false;
    }

    // CAN FD frame format not supported at this stage
    if (newData.payload().size() > 8) {
        setError(tr("CAN FD frame format not supported."), QCanBusDevice::WriteError);
        return false;
    }

    enqueueOutgoingFrame(newData);

    if (!d->writeNotifier->isActive())
        d->writeNotifier->start();

    return true;
}

// TODO: Implement me
QString TinyCanBackend::interpretErrorFrame(const QCanBusFrame &errorFrame)
{
    Q_UNUSED(errorFrame);

    return QString();
}

QT_END_NAMESPACE
