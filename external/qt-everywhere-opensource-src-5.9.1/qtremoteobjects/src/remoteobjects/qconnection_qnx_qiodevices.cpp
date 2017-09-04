/****************************************************************************
**
** Copyright (C) 2017-2016 Ford Motor Company
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtRemoteObjects module of the Qt Toolkit.
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

#include "qconnection_qnx_global_p.h"
#include "qconnection_qnx_qiodevices.h"
#include "qconnection_qnx_server.h"
#include "qconnection_qnx_qiodevices_p.h"

QT_BEGIN_NAMESPACE

QQnxNativeIoPrivate::QQnxNativeIoPrivate()
    : QIODevicePrivate()
    , serverId(-1)
    , channelId(-1)
    , connectionId(-1)
    , state(QAbstractSocket::UnconnectedState)
    , obuffer(new QRingBuffer)
    , thread(this, QStringLiteral("NativeIo"))
{
    //This lets us set msgType before any MsgSend
    //and have the value sent as the header/type.
    SETIOV(tx_iov + 0, &msgType, sizeof(msgType));
}

QQnxNativeIoPrivate::~QQnxNativeIoPrivate()
{
    teardownConnection();
}

bool QQnxNativeIoPrivate::establishConnection()
{
    //On the client side, we need to create the channel/connection
    //to listen for the server's send pulse.
    const int channel = ChannelCreate(0);
    if (channel == -1) {
        WARNING(ChannelCreate)
        return false;
    }
    channelId = channel;

    const int connection = ConnectAttach(ND_LOCAL_NODE, 0, channel, _NTO_SIDE_CHANNEL, 0);
    if (connection == -1) {
        WARNING(ConnectAttach)
        teardownConnection();
        return false;
    }
    connectionId = connection;

    sigevent tx_pulse;
    SIGEV_PULSE_INIT(&tx_pulse, connection, SIGEV_PULSE_PRIO_INHERIT, SOURCE_TX_RQ, 0);
    qCDebug(QT_REMOTEOBJECT) << "in establish" << tx_pulse.sigev_code << SOURCE_TX_RQ;

    const int id = name_open(qPrintable(serverName), 0);
    if (id == -1) {
        qCWarning(QT_REMOTEOBJECT) << "Unable to connect to server" << serverName;
        teardownConnection();
        return false;
    }
    serverId = id;

    thread.start();

    Q_Q(QQnxNativeIo);
    qCDebug(QT_REMOTEOBJECT) << "Before INIT: ServerId" << id << "name" << serverName << q;
    SETIOV(tx_iov + 1, &tx_pulse, sizeof(tx_pulse));
    SETIOV(tx_iov + 2, &channelId, sizeof(channelId));

    //Send our registration message to the server
    msgType = MsgType::REPLICA_INIT;
    //We send tx_pulse along.  When the server want to
    //transmit data, it sends the pulse back to us.
    //When we see that (in receive_thread) we send a
    //message and get the server's tx data in the reply.
    //
    //We transmit the channelId as well - it is only used
    //if HAM is enabled, but this will prevent a possible
    //mismatch between code compiled with vs. without HAM
    //(which could happen if we ever use QCONN between
    //devices)
    if (MsgSendv(serverId, tx_iov, 3, NULL, 0) == -1) {
        WARNING(MsgSendv)
        teardownConnection();
        return false;
    }

    state = QAbstractSocket::ConnectedState;
    emit q->stateChanged(state);

    return true;
}

void QQnxNativeIoPrivate::teardownConnection()
{
    Q_Q(QQnxNativeIo);
    state = QAbstractSocket::ClosingState;
    emit q->stateChanged(state);

    stopThread();

    state = QAbstractSocket::UnconnectedState;
    emit q->stateChanged(state);
}

void QQnxNativeIoPrivate::stopThread()
{
    if (thread.isRunning()) {
        for (int count = 0; count < MAX_RETRIES; ++count) {
            if (MsgSendPulse(connectionId, -1, TERMINATE, 0) == -1) {
                if (errno == EAGAIN) {
                    usleep(5000 + (rand() % 10) * 10000); //5 to 95 msec
                    qCWarning(QT_REMOTEOBJECT) << "Retrying terminate pulse";
                    continue;
                }
                qFatal("MsgSendPulse failed on terminate pulse.  Error = %s", strerror(errno));
            }
        }
        if (errno == EAGAIN)
            qFatal("MsgSendPulse failed on terminate pulse (max retries)");
        thread.wait();
    }
}

// method (run in a thread) to watch for connections and handle receiving data
void QQnxNativeIoPrivate::thread_func()
{
    Q_Q(QQnxNativeIo);

    _pulse pulse;

    qCDebug(QT_REMOTEOBJECT) << "Client thread_func running";

    bool running = true;
    int nTxRequestToIgnore = 0;
    while (running) {
        int rcvid = MsgReceivePulse(channelId, &pulse, sizeof(pulse), NULL);
        if (rcvid == -1)
           continue;

        qCDebug(QT_REMOTEOBJECT) << "MsgReceivePulse unblocked, code =" << pulse.code;

        switch (pulse.code) {
        case SOURCE_TX_RQ: //The Source object wants to send us data
        {
            quint32 len = pulse.value.sival_int;
            qCDebug(QT_REMOTEOBJECT, "TX request with len = %d, tx ignore = %d", len, nTxRequestToIgnore);
            if (nTxRequestToIgnore) {
                --nTxRequestToIgnore;
                break;
            }

            int bytesLeft;

            msgType = MsgType::SOURCE_TX_RESP;
            ibLock.lockForWrite();
            iov_t reply_vector[2];
            SETIOV(reply_vector, &bytesLeft, sizeof(bytesLeft));
            SETIOV(reply_vector+1, buffer.reserve(len), len);
            const int res = MsgSendv(serverId, tx_iov, 1, reply_vector, 2);

            if (res == -1) {
                buffer.chop(len);
                ibLock.unlock();
                WARNING(MsgSendv);
                break;
            }
            ibLock.unlock();

            qCDebug(QT_REMOTEOBJECT) << "Reply said bytesLeft =" << bytesLeft;

            if (bytesLeft) {
                msgType = MsgType::SOURCE_TX_RESP_REPEAT;
                ibLock.lockForWrite();
                SETIOV(reply_vector, &nTxRequestToIgnore, sizeof(nTxRequestToIgnore));
                SETIOV(reply_vector+1, buffer.reserve(bytesLeft), bytesLeft);
                const int res = MsgSendv(serverId, tx_iov, 1, reply_vector, 2);
                if (res == -1) {
                    buffer.chop(bytesLeft);
                    ibLock.unlock();
                    WARNING(MsgSendv);
                    break;
                }
                ibLock.unlock();
                qCDebug(QT_REMOTEOBJECT) << "Reply2 said nTxRequestToIgnore =" << nTxRequestToIgnore;
            }

            QMetaObject::invokeMethod(q, "readyRead", Qt::QueuedConnection);
        }
            break;
        case REPLICA_WRITE: //Our node has data to send
        {
            const int len = pulse.value.sival_int;
            obLock.lockForWrite(); //NAR (Not-An-Error)
            const QByteArray payload = obuffer->read();
            obLock.unlock();
            Q_ASSERT(len == payload.length());

            msgType = MsgType::REPLICA_TX_RECV;
            SETIOV(tx_iov + 1, payload.constData(), len);
            if (MsgSendvs(serverId, tx_iov, 2, NULL, 0) == -1) {
                WARNING(MsgSendvs);
                obLock.lockForWrite();
                if (obuffer->isEmpty()) {
                    obuffer->append(payload);
                } else {
                    //Since QRingBuffer just holds a QList of
                    //QByteArray, copying the QByteArrays to
                    //another container is cheap.
                    QRingBuffer *newBuffer = new QRingBuffer;
                    newBuffer->append(payload);
                    while (!obuffer->isEmpty())
                        newBuffer->append(obuffer->read());
                    obuffer.reset(newBuffer);
                }
                obLock.unlock();
                WARNING(MsgSendvs);
            }
        }
            break;
        case TERMINATE:
            running = false;
            continue;
        case NODE_DEATH:
            qCWarning(QT_REMOTEOBJECT) << "Host node died";
            running = false;
            emit q->error(QAbstractSocket::RemoteHostClosedError);
            continue;
        default:
          /* some other unexpected message */
          qCWarning(QT_REMOTEOBJECT) << "unexpected pulse type:" << pulse.type << __FILE__ << __LINE__;
          WARN_ON_ERROR(MsgError, rcvid, ENOSYS)
          break;
        }
    }

    if (serverId >= 0) {
        WARN_ON_ERROR(name_close, serverId)
        serverId = -1;
    }

    if (connectionId >= 0) {
        WARN_ON_ERROR(ConnectDetach, connectionId)
        connectionId = -1;
    }

    if (channelId >= 0) {
        WARN_ON_ERROR(ChannelDestroy, channelId)
        channelId = -1;
    }

    qCDebug(QT_REMOTEOBJECT) << "Client thread_func stopped";
}

QQnxNativeIo::QQnxNativeIo(QObject *parent)
    : QIODevice(*new QQnxNativeIoPrivate, parent)
{
}

QQnxNativeIo::~QQnxNativeIo()
{
    close();
}

bool QQnxNativeIo::connectToServer(QIODevice::OpenMode openMode)
{
    Q_D(QQnxNativeIo);
    Q_UNUSED(openMode);

    if (state() == QAbstractSocket::ConnectedState ||
        state() == QAbstractSocket::ConnectingState) {
        setErrorString(QStringLiteral("Already connected"));
        emit error(QAbstractSocket::OperationError);
        return false;
    }

    const int omMask = QIODevice::Append | QIODevice::Truncate | QIODevice::Text;
    if (openMode & omMask)
        qCWarning(QT_REMOTEOBJECT, "Tried to open using unsupported open mode flags.");

    d->errorString.clear();
    d->state = QAbstractSocket::ConnectingState;
    emit stateChanged(d->state);

    if (d->serverName.isEmpty()) {
        setErrorString(QStringLiteral("serverName not set"));
        emit error(QAbstractSocket::HostNotFoundError);
        return false;
    }

    QIODevice::open(openMode & (~omMask));

    if (!d->establishConnection()) {
        QIODevice::close();
        qCWarning(QT_REMOTEOBJECT, "Failed to connect to server");
        emit error(QAbstractSocket::UnknownSocketError);
        return false;
    }

    emit stateChanged(d->state);

    return true;
}

bool QQnxNativeIo::connectToServer(const QString &name, QIODevice::OpenMode openMode)
{
    setServerName(name);
    return connectToServer(openMode);
}

void QQnxNativeIo::disconnectFromServer()
{
    close();
}

void QQnxNativeIo::setServerName(const QString &name)
{
    Q_D(QQnxNativeIo);

    if (d->state != QAbstractSocket::UnconnectedState) {
        qCWarning(QT_REMOTEOBJECT) << "QQnxNativeIo::setServerName() called while not unconnected";
        return;
    }

    d->serverName = name;
}

QString QQnxNativeIo::serverName() const
{
    Q_D(const QQnxNativeIo);
    return d->serverName;
}

void QQnxNativeIo::abort()
{
    Q_D(QQnxNativeIo);

    d->stopThread();
    //Don't need mutex since thread is stopped
    d->obuffer->clear();
    d->buffer.clear();

    d->state = QAbstractSocket::UnconnectedState;
    emit stateChanged(d->state);
    QIODevice::close();
}

bool QQnxNativeIo::isSequential() const
{
    return true;
}

qint64 QQnxNativeIo::bytesAvailable() const
{
    Q_D(const QQnxNativeIo);
    quint64 size;

    d->ibLock.lockForRead();
    size = d->buffer.size();
    d->ibLock.unlock();

    return size;
}

qint64 QQnxNativeIo::bytesToWrite() const
{
    Q_D(const QQnxNativeIo);
    quint64 size;

    d->obLock.lockForRead();
    size = d->obuffer->size();
    d->obLock.unlock();

    return size;
}

bool QQnxNativeIo::open(QIODevice::OpenMode openMode)
{
    const int omMask = QIODevice::Append | QIODevice::Truncate | QIODevice::Text;
    if (openMode & omMask)
        qCWarning(QT_REMOTEOBJECT, "Tried to open using unsupported open mode flags.");

    return connectToServer(openMode & (~omMask));
}

void QQnxNativeIo::close()
{
    Q_D(QQnxNativeIo);

    if (!isOpen())
        return;

    d->teardownConnection();

    d->obuffer->clear();
    d->buffer.clear();
    QIODevice::close();
}

QAbstractSocket::SocketState QQnxNativeIo::state() const
{
    Q_D(const QQnxNativeIo);
    return d->state;
}

bool QQnxNativeIo::waitForBytesWritten(int msecs)
{
    //TODO - This method isn't used by Qt Remote Objects, but would
    //need to be implemented before this class could be used as a
    //generic QIODevice.
    Q_UNUSED(msecs);
    Q_ASSERT(false);
    return false;
}

bool QQnxNativeIo::waitForReadyRead(int msecs)
{
    //TODO - This method isn't used by Qt Remote Objects, but would
    //need to be implemented before this class could be used as a
    //generic QIODevice.
    Q_UNUSED(msecs);
    Q_ASSERT(false);
    return false;
}

qint64 QQnxNativeIo::readData(char *data, qint64 size)
{
    Q_D(QQnxNativeIo);
    qint64 read;

    if (!isReadable())
        return 0;

    d->ibLock.lockForWrite(); //NAR (Not-An-Error)
    read = d->buffer.read(data, size);
    d->ibLock.unlock();

    return read;
}

qint64 QQnxNativeIo::writeData(const char *data, qint64 size)
{
    Q_D(QQnxNativeIo);

    if (!isWritable())
        return 0;

    d->obLock.lockForWrite();
    d->obuffer->append(QByteArray(data, size));
    d->obLock.unlock();

    WARN_AND_RETURN_ON_ERROR(MsgSendPulse, -1, d->connectionId, -1, PulseType::REPLICA_WRITE, size)

    return size;
}

/* QIOQnxSource ***************************************************************/

QIOQnxSourcePrivate::QIOQnxSourcePrivate(int _rcvid)
    : QIODevicePrivate()
    , rcvid(_rcvid)
    , state(QAbstractSocket::ConnectedState)
{
}

QIOQnxSource::QIOQnxSource(int rcvid, QObject *parent)
    : QIODevice(*new QIOQnxSourcePrivate(rcvid), parent)
{
    setOpenMode(QIODevice::ReadWrite);
}

QIOQnxSource::~QIOQnxSource()
{
    close();
}

bool QIOQnxSource::isSequential() const
{
    return true;
}

qint64 QIOQnxSource::bytesAvailable() const
{
    Q_D(const QIOQnxSource);
    quint64 size;

    d->ibLock.lockForRead();
    size = d->buffer.size();
    d->ibLock.unlock();

    return size;
}

qint64 QIOQnxSource::bytesToWrite() const
{
    Q_D(const QIOQnxSource);
    quint64 size;

    d->obLock.lockForRead();
    size = d->obuffer.size();
    d->obLock.unlock();

    return size;
}

bool QIOQnxSource::open(QIODevice::OpenMode openMode)
{
    Q_UNUSED(openMode);
    return false;
}

void QIOQnxSource::onDisconnected()
{
    close();
    emit disconnected();
}

void QIOQnxSource::close()
{
    Q_D(QIOQnxSource);

    if (!isOpen())
        return;

    d->state = QAbstractSocket::ClosingState;
    emit stateChanged(d->state);

    d->state = QAbstractSocket::UnconnectedState;
    emit stateChanged(d->state);

    d->obuffer.clear();
    d->buffer.clear();
    QIODevice::close();
}

QAbstractSocket::SocketState QIOQnxSource::state() const
{
    Q_D(const QIOQnxSource);
    return d->state;
}

bool QIOQnxSource::waitForBytesWritten(int msecs)
{
    //TODO - This method isn't used by Qt Remote Objects, but would
    //need to be implemented before this class could be used as a
    //generic QIODevice.
    Q_UNUSED(msecs);
    Q_ASSERT(false);
    return false;
}

bool QIOQnxSource::waitForReadyRead(int msecs)
{
    //TODO - This method isn't used by Qt Remote Objects, but would
    //need to be implemented before this class could be used as a
    //generic QIODevice.
    Q_UNUSED(msecs);
    Q_ASSERT(false);
    return false;
}

qint64 QIOQnxSource::readData(char *data, qint64 size)
{
    Q_D(QIOQnxSource);
    qint64 read;

    if (!isReadable())
        return 0;

    d->ibLock.lockForWrite(); //NAR (Not-An-Error)
    read = d->buffer.read(data, size);
    d->ibLock.unlock();

    return read;
}

qint64 QIOQnxSource::writeData(const char *data, qint64 size)
{
    Q_D(QIOQnxSource);

    if (!isWritable())
        return 0;

    d->obLock.lockForWrite();
    d->obuffer.append(QByteArray(data, size));
    d->obLock.unlock();

    if (!d->m_serverClosing.load()) {
        d->m_event.sigev_value.sival_int = size;
        WARN_ON_ERROR(MsgDeliverEvent, d->rcvid, &(d->m_event))
    }

    return size;
}

QT_END_NAMESPACE
