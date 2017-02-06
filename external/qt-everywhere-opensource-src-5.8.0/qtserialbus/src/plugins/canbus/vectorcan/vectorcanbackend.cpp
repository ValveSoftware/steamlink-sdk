/****************************************************************************
**
** Copyright (C) 2016 Denis Shienkov <denis.shienkov@gmail.com>
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

#include "vectorcanbackend.h"
#include "vectorcanbackend_p.h"
#include "vectorcan_symbols_p.h"

#include <QtSerialBus/qcanbusdevice.h>

#include <QtCore/qtimer.h>
#include <QtCore/qcoreevent.h>
#include <QtCore/qwineventnotifier.h>
#include <QtCore/qcoreapplication.h>

#include <algorithm>

QT_BEGIN_NAMESPACE

#ifndef LINK_LIBVECTORCAN
Q_GLOBAL_STATIC(QLibrary, vectorcanLibrary)
#endif

bool VectorCanBackend::canCreate(QString *errorReason)
{
#ifdef LINK_LIBVECTORCAN
    return true;
#else
    static bool symbolsResolved = resolveSymbols(vectorcanLibrary());
    if (!symbolsResolved) {
        *errorReason = vectorcanLibrary()->errorString();
        return false;
    }
    return true;
#endif
}

static int driverRefCount = 0;

class ReadNotifier : public QWinEventNotifier
{
    // no Q_OBJECT macro!
public:
    explicit ReadNotifier(VectorCanBackendPrivate *d, QObject *parent)
        : QWinEventNotifier(parent)
        , dptr(d)
    {
        setHandle(dptr->readHandle);
    }

protected:
    bool event(QEvent *e) override
    {
        if (e->type() == QEvent::WinEventAct) {
            dptr->startRead();
            return true;
        }
        return QWinEventNotifier::event(e);
    }

private:
    VectorCanBackendPrivate *dptr;
};

class WriteNotifier : public QTimer
{
    // no Q_OBJECT macro!
public:
    WriteNotifier(VectorCanBackendPrivate *d, QObject *parent)
        : QTimer(parent)
        , dptr(d)
    {
        setInterval(0);
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
    VectorCanBackendPrivate *dptr;
};


VectorCanBackendPrivate::VectorCanBackendPrivate(VectorCanBackend *q)
    : q_ptr(q)
    , portHandle(XL_INVALID_PORTHANDLE)
{
    startupDriver();
}

VectorCanBackendPrivate::~VectorCanBackendPrivate()
{
    cleanupDriver();
}

bool VectorCanBackendPrivate::open()
{
    Q_Q(VectorCanBackend);

    {
        XLaccess permissionMask = channelMask;
        const quint32 queueSize = 256;
        const XLstatus status = ::xlOpenPort(&portHandle,
                                             const_cast<char *>(qPrintable(qApp->applicationName())),
                                             channelMask, &permissionMask, queueSize,
                                             XL_INTERFACE_VERSION, XL_BUS_TYPE_CAN);

        if (status != XL_SUCCESS || portHandle == XL_INVALID_PORTHANDLE) {
            q->setError(systemErrorString(status),
                        QCanBusDevice::ConnectionError);
            return false;
        }
    }

    {
        const XLstatus status = ::xlActivateChannel(portHandle, channelMask,
                                                    XL_BUS_TYPE_CAN, XL_ACTIVATE_RESET_CLOCK);
        if (status != XL_SUCCESS) {
            q->setError(systemErrorString(status),
                        QCanBusDevice::CanBusError::ConnectionError);
            return false;
        }
    }

    {
        const int queueLevel = 1;
        const XLstatus status = ::xlSetNotification(portHandle, &readHandle, queueLevel);
        if (status != XL_SUCCESS) {
            q->setError(systemErrorString(status),
                        QCanBusDevice::ConnectionError);
            return false;
        }
    }

    readNotifier = new ReadNotifier(this, q);
    readNotifier->setEnabled(true);

    writeNotifier = new WriteNotifier(this, q);

    return true;
}

void VectorCanBackendPrivate::close()
{
    Q_Q(VectorCanBackend);

    delete readNotifier;
    readNotifier = nullptr;

    delete writeNotifier;
    writeNotifier = nullptr;

    {
        const XLstatus status = ::xlDeactivateChannel(portHandle, channelMask);
        if (status != XL_SUCCESS) {
            q->setError(systemErrorString(status),
                        QCanBusDevice::CanBusError::ConfigurationError);
        }
    }

    {
        const XLstatus status = ::xlClosePort(portHandle);
        if (status != XL_SUCCESS) {
            q->setError(systemErrorString(status),
                        QCanBusDevice::ConnectionError);
        }
    }

    portHandle = XL_INVALID_PORTHANDLE;
}

bool VectorCanBackendPrivate::setConfigurationParameter(int key, const QVariant &value)
{
    Q_Q(VectorCanBackend);

    switch (key) {
    case QCanBusDevice::BitRateKey:
        return setBitRate(value.toUInt());
    default:
        q->setError(VectorCanBackend::tr("Unsupported configuration key"),
                    QCanBusDevice::ConfigurationError);
        return false;
    }
}

void VectorCanBackendPrivate::setupChannel(const QString &interfaceName)
{
    if (interfaceName.startsWith(QStringLiteral("can"))) {
        const QStringRef ref = interfaceName.midRef(3);
        bool ok = false;
        const int channelIndex = ref.toInt(&ok);
        if (ok && (channelIndex >= 0 && channelIndex < XL_CONFIG_MAX_CHANNELS)) {
            channelMask = XL_CHANNEL_MASK((channelIndex));
            return;
        }
    }

    qCritical() << "Unable to parse the channel from an interface";
}

void VectorCanBackendPrivate::setupDefaultConfigurations()
{
    Q_Q(VectorCanBackend);

    q->setConfigurationParameter(QCanBusDevice::BitRateKey, 500000);
}

QString VectorCanBackendPrivate::systemErrorString(int errorCode) const
{
    if (const char *string = ::xlGetErrorString(errorCode))
        return QString::fromUtf8(string);
    return VectorCanBackend::tr("Unable to retrieve an error string");
}

void VectorCanBackendPrivate::startWrite()
{
    Q_Q(VectorCanBackend);

    if (!q->hasOutgoingFrames()) {
        writeNotifier->stop();
        return;
    }

    const QCanBusFrame frame = q->dequeueOutgoingFrame();
    const QByteArray payload = frame.payload();

    XLevent event;
    ::memset(&event, 0, sizeof(event));

    event.tag = XL_TRANSMIT_MSG;

    s_xl_can_msg &msg = event.tagData.msg;

    msg.id = frame.frameId();
    if (frame.hasExtendedFrameFormat())
        msg.id |= XL_CAN_EXT_MSG_ID;

    msg.dlc = payload.size();

    if (frame.frameType() == QCanBusFrame::RemoteRequestFrame)
        msg.flags |= XL_CAN_MSG_FLAG_REMOTE_FRAME; // we do not care about the payload
    else if (frame.frameType() == QCanBusFrame::ErrorFrame)
        msg.flags |= XL_CAN_MSG_FLAG_ERROR_FRAME; // we do not care about the payload
    else
        ::memcpy(msg.data, payload.constData(), sizeof(msg.data));

    quint32 eventCount = 1;
    const XLstatus status = ::xlCanTransmit(portHandle, channelMask,
                                            &eventCount, &event);
    if (status != XL_SUCCESS) {
        q->setError(systemErrorString(status),
                    QCanBusDevice::WriteError);
    } else {
        emit q->framesWritten(qint64(eventCount));
    }

    if (q->hasOutgoingFrames())
        writeNotifier->start();
}

void VectorCanBackendPrivate::startRead()
{
    Q_Q(VectorCanBackend);

    QVector<QCanBusFrame> newFrames;

    for (;;) {
        quint32 eventCount = 1;
        XLevent event;
        ::memset(&event, 0, sizeof(event));

        const XLstatus status = ::xlReceive(portHandle, &eventCount, &event);
        if (status != XL_SUCCESS) {
            if (status != XL_ERR_QUEUE_IS_EMPTY) {
                q->setError(systemErrorString(status),
                            QCanBusDevice::ReadError);
            }
            break;
        }

        if (event.tag != XL_RECEIVE_MSG || event.portHandle != portHandle)
            continue;

        const s_xl_can_msg &msg = event.tagData.msg;

        QCanBusFrame frame(msg.id, QByteArray(reinterpret_cast<const char *>(msg.data),
                                              int(msg.dlc)));
        frame.setTimeStamp(QCanBusFrame::TimeStamp::fromMicroSeconds(event.timeStamp / 1000));
        frame.setExtendedFrameFormat(msg.id & XL_CAN_EXT_MSG_ID);
        frame.setFrameType((msg.flags & XL_CAN_MSG_FLAG_REMOTE_FRAME)
                           ? QCanBusFrame::RemoteRequestFrame
                           : (msg.flags & XL_CAN_MSG_FLAG_ERROR_FRAME)
                             ? QCanBusFrame::ErrorFrame
                             : QCanBusFrame::DataFrame);

        newFrames.append(std::move(frame));
    }

    q->enqueueReceivedFrames(newFrames);
}

void VectorCanBackendPrivate::startupDriver()
{
    Q_Q(VectorCanBackend);

    if (driverRefCount == 0) {
        const XLstatus status = ::xlOpenDriver();
        if (status != XL_SUCCESS) {
            q->setError(systemErrorString(status),
                        QCanBusDevice::CanBusError::ConnectionError);
            return;
        }
    } else if (driverRefCount < 0) {
        qCritical("Wrong reference counter: %d", driverRefCount);
        return;
    }

    ++driverRefCount;
}

void VectorCanBackendPrivate::cleanupDriver()
{
    --driverRefCount;

    if (driverRefCount < 0) {
        qCritical("Wrong reference counter: %d", driverRefCount);
        driverRefCount = 0;
    } else if (driverRefCount == 0) {
        ::xlCloseDriver();
    }
}

bool VectorCanBackendPrivate::setBitRate(quint32 bitrate)
{
    Q_Q(VectorCanBackend);

    if (q->state() != QCanBusDevice::UnconnectedState) {
        const XLstatus status = ::xlCanSetChannelBitrate(portHandle, channelMask, bitrate);
        if (status != XL_SUCCESS) {
            q->setError(systemErrorString(status),
                        QCanBusDevice::CanBusError::ConfigurationError);
            return false;
        }
    }

    return true;
}

VectorCanBackend::VectorCanBackend(const QString &name, QObject *parent)
    : QCanBusDevice(parent)
    , d_ptr(new VectorCanBackendPrivate(this))
{
    Q_D(VectorCanBackend);

    d->setupChannel(name);
    d->setupDefaultConfigurations();
}

VectorCanBackend::~VectorCanBackend()
{
    if (state() == ConnectedState)
        close();

    delete d_ptr;
}

bool VectorCanBackend::open()
{
    Q_D(VectorCanBackend);

    if (!d->open()) {
        close(); // sets UnconnectedState
        return false;
    }

    const auto keys = configurationKeys();
    for (int key : keys) {
        const QVariant param = configurationParameter(key);
        const bool success = d->setConfigurationParameter(key, param);
        if (!success) {
            qWarning() << "Cannot apply parameter:" << key
                       << "with value:" << param;
        }
    }

    setState(QCanBusDevice::ConnectedState);
    return true;
}

void VectorCanBackend::close()
{
    Q_D(VectorCanBackend);

    d->close();

    setState(QCanBusDevice::UnconnectedState);
}

void VectorCanBackend::setConfigurationParameter(int key, const QVariant &value)
{
    Q_D(VectorCanBackend);

    if (d->setConfigurationParameter(key, value))
        QCanBusDevice::setConfigurationParameter(key, value);
}

bool VectorCanBackend::writeFrame(const QCanBusFrame &newData)
{
    Q_D(VectorCanBackend);

    if (state() != QCanBusDevice::ConnectedState)
        return false;

    if (!newData.isValid()) {
        setError(tr("Cannot write invalid QCanBusFrame"),
                 QCanBusDevice::WriteError);
        return false;
    }

    if (newData.frameType() != QCanBusFrame::DataFrame
            && newData.frameType() != QCanBusFrame::RemoteRequestFrame
            && newData.frameType() != QCanBusFrame::ErrorFrame) {
        setError(tr("Unable to write a frame with unacceptable type"),
                 QCanBusDevice::WriteError);
        return false;
    }

    // CAN FD frame format not implemented at this stage
    if (newData.payload().size() > MAX_MSG_LEN) {
        setError(tr("CAN FD frame format not supported."),
                 QCanBusDevice::WriteError);
        return false;
    }

    enqueueOutgoingFrame(newData);

    if (!d->writeNotifier->isActive())
        d->writeNotifier->start();

    return true;
}

// TODO: Implement me
QString VectorCanBackend::interpretErrorFrame(const QCanBusFrame &errorFrame)
{
    Q_UNUSED(errorFrame);

    return QString();
}

QT_END_NAMESPACE
