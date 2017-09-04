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

#include "systeccanbackend.h"
#include "systeccanbackend_p.h"
#include "systeccan_symbols_p.h"

#include <QtSerialBus/qcanbusdevice.h>

#include <QtCore/qcoreapplication.h>
#include <QtCore/qcoreevent.h>
#include <QtCore/qdebug.h>
#include <QtCore/qregularexpression.h>
#include <QtCore/qtimer.h>

QT_BEGIN_NAMESPACE

Q_GLOBAL_STATIC(QLibrary, systecLibrary)

bool SystecCanBackend::canCreate(QString *errorReason)
{
    static bool symbolsResolved = resolveSymbols(systecLibrary());
    if (Q_UNLIKELY(!symbolsResolved)) {
        *errorReason = systecLibrary()->errorString();
        return false;
    }
    return true;
}

static void DRV_CALLBACK_TYPE ucanEnumCallback(DWORD index, BOOL isUsed,
                                               tUcanHardwareInfoEx *hardwareInfo,
                                               tUcanHardwareInitInfo *initInfo,
                                               void *args)
{
    auto result = static_cast<QStringList *>(args);

    Q_UNUSED(isUsed);
    Q_UNUSED(hardwareInfo);
    Q_UNUSED(initInfo);

    result->append(QString::fromLatin1("can%1.0").arg(index));
    if (USBCAN_CHECK_SUPPORT_TWO_CHANNEL(hardwareInfo))
        result->append(QString::fromLatin1("can%1.1").arg(index));
}

QList<QCanBusDeviceInfo> SystecCanBackend::interfaces()
{
    QList<QCanBusDeviceInfo> result;

    QStringList devices;
    ::UcanEnumerateHardware(&ucanEnumCallback, &devices, false,
                                                0, ~0, 0, ~0, 0, ~0);

    for (const QString &s : qAsConst(devices))
        result.append(createDeviceInfo(s, false, false));
    return result;
}

class OutgoingEventNotifier : public QTimer
{
public:
    OutgoingEventNotifier(SystecCanBackendPrivate *d, QObject *parent) :
        QTimer(parent),
        dptr(d)
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
    SystecCanBackendPrivate *dptr;
};

SystecCanBackendPrivate::SystecCanBackendPrivate(SystecCanBackend *q) :
    q_ptr(q),
    incomingEventHandler(new IncomingEventHandler(this, q))
{
}

static uint bitrateCodeFromBitrate(int bitrate)
{
    struct BitrateItem {
        int bitrate;
        uint code;
    } bitrateTable[] = {
        {   10000, USBCAN_BAUDEX_10kBit  },
        {   20000, USBCAN_BAUDEX_20kBit  },
        {   50000, USBCAN_BAUDEX_50kBit  },
        {  100000, USBCAN_BAUDEX_100kBit },
        {  125000, USBCAN_BAUDEX_125kBit },
        {  250000, USBCAN_BAUDEX_250kBit },
        {  500000, USBCAN_BAUDEX_500kBit },
        {  800000, USBCAN_BAUDEX_800kBit },
        { 1000000, USBCAN_BAUDEX_1MBit   }
    };

    const int entries = (sizeof(bitrateTable) / sizeof(*bitrateTable));
    for (int i = 0; i < entries; ++i)
        if (bitrateTable[i].bitrate == bitrate)
            return bitrateTable[i].code;

    return 0;
}

void IncomingEventHandler::customEvent(QEvent *event)
{
    dptr->eventHandler(event);
}

/*
 * Do not call functions of USBCAN32.DLL directly from this callback handler.
 * Use events or windows messages to notify the event to the application.
 */
static void DRV_CALLBACK_TYPE ucanCallback(tUcanHandle, quint32 event, quint8, void *args)
{
    QEvent::Type type = static_cast<QEvent::Type>(QEvent::User + event);
    IncomingEventHandler *handler = static_cast<IncomingEventHandler *>(args);
    qApp->postEvent(handler, new QEvent(type));
}

bool SystecCanBackendPrivate::open()
{
    Q_Q(SystecCanBackend);

    const UCANRET initHardwareRes = ::UcanInitHardwareEx(&handle, device, ucanCallback, incomingEventHandler);
    if (Q_UNLIKELY(initHardwareRes != USBCAN_SUCCESSFUL)) {
        q->setError(systemErrorString(initHardwareRes), QCanBusDevice::ConnectionError);
        return false;
    }

    const int bitrate = q->configurationParameter(QCanBusDevice::BitRateKey).toInt();
    const bool receiveOwn = q->configurationParameter(QCanBusDevice::ReceiveOwnKey).toBool();

    tUcanInitCanParam param;
    ::memset(&param, 0, sizeof(param));
    param.m_dwSize = sizeof(param);
    param.m_bMode  = receiveOwn ? kUcanModeTxEcho : kUcanModeNormal;
    param.m_bOCR   = USBCAN_OCR_DEFAULT;
    param.m_dwACR  = USBCAN_ACR_ALL;
    param.m_dwAMR  = USBCAN_AMR_ALL;
    param.m_dwBaudrate = bitrateCodeFromBitrate(bitrate);
    param.m_wNrOfRxBufferEntries = USBCAN_DEFAULT_BUFFER_ENTRIES;
    param.m_wNrOfTxBufferEntries = USBCAN_DEFAULT_BUFFER_ENTRIES;

    const UCANRET initCanResult = ::UcanInitCanEx2(handle, channel, &param);
    if (Q_UNLIKELY(initCanResult != USBCAN_SUCCESSFUL)) {
        ::UcanDeinitHardware(handle);
        q->setError(systemErrorString(initCanResult), QCanBusDevice::ConnectionError);
        return false;
    }

    return true;
}

void SystecCanBackendPrivate::close()
{
    Q_Q(SystecCanBackend);

    enableWriteNotification(false);

    if (outgoingEventNotifier) {
        delete outgoingEventNotifier;
        outgoingEventNotifier = nullptr;
    }

    const UCANRET deinitCanRes = UcanDeinitCanEx(handle, channel);
    if (Q_UNLIKELY(deinitCanRes != USBCAN_SUCCESSFUL))
        q->setError(systemErrorString(deinitCanRes), QCanBusDevice::ConfigurationError);

    // TODO: other channel keeps working?
    const UCANRET deinitHardwareRes = UcanDeinitHardware(handle);
    if (Q_UNLIKELY(deinitHardwareRes != USBCAN_SUCCESSFUL))
        emit q->setError(systemErrorString(deinitHardwareRes), QCanBusDevice::ConnectionError);
}

void SystecCanBackendPrivate::eventHandler(QEvent *event)
{
    const int code = event->type() - QEvent::User;

    if (code == USBCAN_EVENT_RECEIVE)
        readAllReceivedMessages();
}

bool SystecCanBackendPrivate::setConfigurationParameter(int key, const QVariant &value)
{
    Q_Q(SystecCanBackend);

    switch (key) {
    case QCanBusDevice::BitRateKey:
        return verifyBitRate(value.toInt());
    case QCanBusDevice::ReceiveOwnKey:
        if (Q_UNLIKELY(q->state() != QCanBusDevice::UnconnectedState)) {
            q->setError(SystecCanBackend::tr("Cannot configure TxEcho for open device"),
                        QCanBusDevice::ConfigurationError);
            return false;
        }
        return true;
    default:
        q->setError(SystecCanBackend::tr("Unsupported configuration key: %1").arg(key),
                    QCanBusDevice::ConfigurationError);
        return false;
    }
}

bool SystecCanBackendPrivate::setupChannel(const QString &interfaceName)
{
    Q_Q(SystecCanBackend);

    const QRegularExpression re(QStringLiteral("can(\\d)\\.(\\d)"));
    const QRegularExpressionMatch match = re.match(interfaceName);

    if (Q_LIKELY(match.hasMatch())) {
        device = match.captured(1).toInt();
        channel = match.captured(2).toInt();
    } else {
        q->setError(SystecCanBackend::tr("Invalid interface '%1'.")
                    .arg(interfaceName), QCanBusDevice::ConnectionError);
        return false;
    }

    return true;
}

void SystecCanBackendPrivate::setupDefaultConfigurations()
{
    Q_Q(SystecCanBackend);

    q->setConfigurationParameter(QCanBusDevice::BitRateKey, 500000);
    q->setConfigurationParameter(QCanBusDevice::ReceiveOwnKey, false);
}

QString SystecCanBackendPrivate::systemErrorString(int errorCode)
{
    switch (errorCode) {
    case USBCAN_ERR_RESOURCE:
        return SystecCanBackend::tr("Could not create a resource (memory, handle, ...)");
    case USBCAN_ERR_MAXMODULES:
        return SystecCanBackend::tr("The maximum number of open modules is exceeded");
    case USBCAN_ERR_HWINUSE:
        return SystecCanBackend::tr("The module is already in use");
    case USBCAN_ERR_ILLVERSION:
        return SystecCanBackend::tr("The software versions of the module and library are incompatible");
    case USBCAN_ERR_ILLHW:
        return SystecCanBackend::tr("The module with the corresponding device number is not connected");
    case USBCAN_ERR_ILLHANDLE:
        return SystecCanBackend::tr("Wrong USB-CAN-Handle handed over to the function");
    case USBCAN_ERR_ILLPARAM:
        return SystecCanBackend::tr("Wrong parameter handed over to the function");
    case USBCAN_ERR_BUSY:
        return SystecCanBackend::tr("Instruction can not be processed at this time");
    case USBCAN_ERR_TIMEOUT:
        return SystecCanBackend::tr("No answer from the module");
    case USBCAN_ERR_IOFAILED:
        return SystecCanBackend::tr("A request for the driver failed");
    case USBCAN_ERR_DLL_TXFULL:
        return SystecCanBackend::tr("The message did not fit into the transmission queue");
    case USBCAN_ERR_MAXINSTANCES:
        return SystecCanBackend::tr("Maximum number of applications is reached");
    case USBCAN_ERR_CANNOTINIT:
        return SystecCanBackend::tr("CAN-interface is not yet initialized");
    case USBCAN_ERR_DISCONNECT:
        return SystecCanBackend::tr("USB-CANmodul was disconnected");
    case USBCAN_ERR_NOHWCLASS:
        return SystecCanBackend::tr("The needed device class does not exist");
    case USBCAN_ERR_ILLCHANNEL:
        return SystecCanBackend::tr("Illegal CAN channel for GW-001/GW-002");
    case USBCAN_ERR_ILLHWTYPE:
        return SystecCanBackend::tr("The API function can not be used with this hardware");
    case USBCAN_ERR_SERVER_TIMEOUT:
        return SystecCanBackend::tr("The command server did not send a reply to a command");
    default:
        return SystecCanBackend::tr("Unknown error code '%1'.").arg(errorCode);
    }
}

void SystecCanBackendPrivate::enableWriteNotification(bool enable)
{
    Q_Q(SystecCanBackend);

    if (outgoingEventNotifier) {
        if (enable) {
            if (!outgoingEventNotifier->isActive())
                outgoingEventNotifier->start();
        } else {
            outgoingEventNotifier->stop();
        }
    } else if (enable) {
        outgoingEventNotifier = new OutgoingEventNotifier(this, q);
        outgoingEventNotifier->start(0);
    }
}

void SystecCanBackendPrivate::startWrite()
{
    Q_Q(SystecCanBackend);

    if (!q->hasOutgoingFrames()) {
        enableWriteNotification(false);
        return;
    }

    const QCanBusFrame frame = q->dequeueOutgoingFrame();
    const QByteArray payload = frame.payload();

    tCanMsgStruct message;
    ::memset(&message, 0, sizeof(message));

    message.m_dwID = frame.frameId();
    message.m_bDLC = payload.size();

    message.m_bFF = frame.hasExtendedFrameFormat() ? USBCAN_MSG_FF_EXT : USBCAN_MSG_FF_STD;

    if (frame.frameType() == QCanBusFrame::RemoteRequestFrame)
        message.m_bFF |= USBCAN_MSG_FF_RTR; // remote request frame without payload
    else
        ::memcpy(message.m_bData, payload.constData(), sizeof(message.m_bData));

    const UCANRET result = ::UcanWriteCanMsgEx(handle, channel, &message, nullptr);
    if (Q_UNLIKELY(result != USBCAN_SUCCESSFUL))
        q->setError(systemErrorString(result), QCanBusDevice::WriteError);
    else
        emit q->framesWritten(qint64(1));

    if (q->hasOutgoingFrames())
        enableWriteNotification(true);
}

void SystecCanBackendPrivate::readAllReceivedMessages()
{
    Q_Q(SystecCanBackend);

    QVector<QCanBusFrame> newFrames;

    for (;;) {
        tCanMsgStruct message;
        ::memset(&message, 0, sizeof(message));

        const UCANRET result = ::UcanReadCanMsgEx(handle, &channel, &message, nullptr);
        if (result == USBCAN_WARN_NODATA)
            break;

        if (Q_UNLIKELY(result != USBCAN_SUCCESSFUL)) {
            // handle errors

            q->setError(systemErrorString(result), QCanBusDevice::ReadError);
            break;
        }

        QCanBusFrame frame(message.m_dwID,
                           QByteArray(reinterpret_cast<const char *>(message.m_bData),
                                      int(message.m_bDLC)));

        // TODO: Timestamp can also be set to 100 us resolution with kUcanModeHighResTimer
        frame.setTimeStamp(QCanBusFrame::TimeStamp::fromMicroSeconds(message.m_dwTime * 1000));
        frame.setExtendedFrameFormat(message.m_bFF & USBCAN_MSG_FF_EXT);
        frame.setFrameType((message.m_bFF & USBCAN_MSG_FF_RTR)
                           ? QCanBusFrame::RemoteRequestFrame
                           : QCanBusFrame::DataFrame);

        newFrames.append(frame);
    }

    q->enqueueReceivedFrames(newFrames);
}

bool SystecCanBackendPrivate::verifyBitRate(int bitrate)
{
    Q_Q(SystecCanBackend);

    if (Q_UNLIKELY(q->state() != QCanBusDevice::UnconnectedState)) {
        q->setError(SystecCanBackend::tr("Cannot configure bitrate for open device"),
                    QCanBusDevice::ConfigurationError);
        return false;
    }

    if (Q_UNLIKELY(bitrateCodeFromBitrate(bitrate) == 0)) {
        q->setError(SystecCanBackend::tr("Unsupported bitrate %1.").arg(bitrate),
                    QCanBusDevice::ConfigurationError);
        return false;
    }

    return true;
}

SystecCanBackend::SystecCanBackend(const QString &name, QObject *parent) :
    QCanBusDevice(parent),
    d_ptr(new SystecCanBackendPrivate(this))
{
    Q_D(SystecCanBackend);

    d->setupChannel(name);
    d->setupDefaultConfigurations();
}

SystecCanBackend::~SystecCanBackend()
{
    if (state() == QCanBusDevice::ConnectedState)
        close();

    delete d_ptr;
}

bool SystecCanBackend::open()
{
    Q_D(SystecCanBackend);

    if (!d->open())
        return false;

    // Apply all stored configurations except bitrate, because
    // the bitrate can not be applied after opening the device
    const QVector<int> keys = configurationKeys();
    for (int key : keys) {
        if (key == QCanBusDevice::BitRateKey)
            continue;
        const QVariant param = configurationParameter(key);
        const bool success = d->setConfigurationParameter(key, param);
        if (Q_UNLIKELY(!success)) {
            qWarning("Cannot apply parameter %d with value %ls.",
                     key, qUtf16Printable(param.toString()));
        }
    }

    setState(QCanBusDevice::ConnectedState);
    return true;
}

void SystecCanBackend::close()
{
    Q_D(SystecCanBackend);

    d->close();

    setState(QCanBusDevice::UnconnectedState);
}

void SystecCanBackend::setConfigurationParameter(int key, const QVariant &value)
{
    Q_D(SystecCanBackend);

    if (d->setConfigurationParameter(key, value))
        QCanBusDevice::setConfigurationParameter(key, value);
}

bool SystecCanBackend::writeFrame(const QCanBusFrame &newData)
{
    Q_D(SystecCanBackend);

    if (Q_UNLIKELY(state() != QCanBusDevice::ConnectedState))
        return false;

    if (Q_UNLIKELY(!newData.isValid())) {
        setError(tr("Cannot write invalid QCanBusFrame"), QCanBusDevice::WriteError);
        return false;
    }

    const QCanBusFrame::FrameType type = newData.frameType();
    if (Q_UNLIKELY(type != QCanBusFrame::DataFrame && type != QCanBusFrame::RemoteRequestFrame)) {
        setError(tr("Unable to write a frame with unacceptable type"),
                 QCanBusDevice::WriteError);
        return false;
    }

    // CAN FD frame format is not supported by the hardware yet
    if (Q_UNLIKELY(newData.payload().size() > 8)) {
        setError(tr("CAN FD frame format not supported"), QCanBusDevice::WriteError);
        return false;
    }

    enqueueOutgoingFrame(newData);
    d->enableWriteNotification(true);

    return true;
}

// TODO: Implement me
QString SystecCanBackend::interpretErrorFrame(const QCanBusFrame &errorFrame)
{
    Q_UNUSED(errorFrame);

    return QString();
}

QT_END_NAMESPACE
