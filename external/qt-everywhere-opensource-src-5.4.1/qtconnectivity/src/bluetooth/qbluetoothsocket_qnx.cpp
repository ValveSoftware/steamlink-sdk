/***************************************************************************
**
** Copyright (C) 2012 Research In Motion
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
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

#include "qbluetoothsocket.h"
#include "qbluetoothsocket_p.h"
#include "qbluetoothlocaldevice.h"
#include <sys/stat.h>
#ifdef QT_QNX_BT_BLUETOOTH
#include <errno.h>
#include <btapi/btspp.h>
#endif

QT_BEGIN_NAMESPACE
#ifdef QT_QNX_BT_BLUETOOTH
static int initCounter = 0;
#endif

QBluetoothSocketPrivate::QBluetoothSocketPrivate()
    : socket(-1),
      socketType(QBluetoothServiceInfo::UnknownProtocol),
      state(QBluetoothSocket::UnconnectedState),
      socketError(QBluetoothSocket::NoSocketError),
      readNotifier(0),
      connectWriteNotifier(0),
      connecting(false),
      discoveryAgent(0),
      isServerSocket(false)
{
#ifdef QT_QNX_BT_BLUETOOTH
    if (!initCounter && (bt_spp_init() == -1))
        qCDebug(QT_BT_QNX) << "Could not initialize Bluetooth library. "
                           << qt_error_string(errno);

    initCounter++;
#else
    ppsRegisterControl();
#endif
}

QBluetoothSocketPrivate::~QBluetoothSocketPrivate()
{
#ifdef QT_QNX_BT_BLUETOOTH
    if (initCounter == 1 && (bt_spp_deinit() == -1))
        qCDebug(QT_BT_QNX) << "Could not deinitialize Bluetooth library."
                              "SPP connection is still open.";

    initCounter--;
#else
    ppsUnregisterControl(this);
#endif
    close();
}

bool QBluetoothSocketPrivate::ensureNativeSocket(QBluetoothServiceInfo::Protocol type)
{
    socketType = type;
    if (socketType == QBluetoothServiceInfo::RfcommProtocol)
        return true;

    return false;
}

void QBluetoothSocketPrivate::connectToService(const QBluetoothAddress &address,
                                               const QBluetoothUuid &uuid,
                                               QIODevice::OpenMode openMode)
{
    Q_Q(QBluetoothSocket);
    Q_UNUSED(openMode);
    qCDebug(QT_BT_QNX) << "Connecting socket";

    m_peerAddress = address;
#ifdef QT_QNX_BT_BLUETOOTH
    QByteArray b_uuid = uuid.toByteArray();
    b_uuid = b_uuid.mid(1, b_uuid.length() - 2);
    socket = bt_spp_open(address.toString().toUtf8().data(), b_uuid.data(), false);
    if (socket == -1) {
        qCWarning(QT_BT_QNX) << "Could not connect to" << address.toString() << b_uuid <<  qt_error_string(errno);
        errorString = qt_error_string(errno);
        q->setSocketError(QBluetoothSocket::NetworkError);
        return;
    }

    delete readNotifier;
    delete connectWriteNotifier;

    readNotifier = new QSocketNotifier(socket, QSocketNotifier::Read);
    QObject::connect(readNotifier, SIGNAL(activated(int)), this, SLOT(_q_readNotify()));
    connectWriteNotifier = new QSocketNotifier(socket, QSocketNotifier::Write, q);
    QObject::connect(connectWriteNotifier, SIGNAL(activated(int)), this, SLOT(_q_writeNotify()));

    connecting = true;
    q->setOpenMode(openMode);
#else
    m_uuid = uuid;
    if (isServerSocket)
        return;

    if (state != QBluetoothSocket::UnconnectedState) {
        qCDebug(QT_BT_QNX) << "Socket already connected";
        return;
    }

    ppsSendControlMessage("connect_service", 0x1101, uuid, address.toString(), QString(), this, BT_SPP_CLIENT_SUBTYPE);
    ppsRegisterForEvent(QStringLiteral("service_connected"),this);
    ppsRegisterForEvent(QStringLiteral("get_mount_point_path"),this);
#endif
    q->setSocketState(QBluetoothSocket::ConnectingState);
}

void QBluetoothSocketPrivate::_q_writeNotify()
{
    Q_Q(QBluetoothSocket);
    if (connecting && state == QBluetoothSocket::ConnectingState){
        q->setSocketState(QBluetoothSocket::ConnectedState);
        emit q->connected();

        connectWriteNotifier->setEnabled(false);
        connecting = false;
    } else {
        if (txBuffer.size() == 0) {
            connectWriteNotifier->setEnabled(false);
            return;
        }

        char buf[1024];
        Q_Q(QBluetoothSocket);

        int size = txBuffer.read(buf, 1024);

        if (::write(socket, buf, size) != size) {
            errorString = QBluetoothSocket::tr("Network Error");
            q->setSocketError(QBluetoothSocket::NetworkError);
        }
        else {
            emit q->bytesWritten(size);
        }

        if (txBuffer.size()) {
            connectWriteNotifier->setEnabled(true);
        }
        else if (state == QBluetoothSocket::ClosingState) {
            connectWriteNotifier->setEnabled(false);
            this->close();
        }
    }
}

void QBluetoothSocketPrivate::_q_readNotify()
{
    Q_Q(QBluetoothSocket);
    char *writePointer = buffer.reserve(QPRIVATELINEARBUFFER_BUFFERSIZE);
    int readFromDevice = ::read(socket, writePointer, QPRIVATELINEARBUFFER_BUFFERSIZE);
    if (readFromDevice <= 0){
        int errsv = errno;
        readNotifier->setEnabled(false);
        connectWriteNotifier->setEnabled(false);
        qCWarning(QT_BT_QNX) << Q_FUNC_INFO << socket << " error:" << readFromDevice << errorString; //TODO Try if this actually works
        errorString = qt_error_string(errsv);
        q->setSocketError(QBluetoothSocket::UnknownSocketError);

        q->disconnectFromService();
        q->setSocketState(QBluetoothSocket::UnconnectedState);
    } else {
        buffer.chop(QPRIVATELINEARBUFFER_BUFFERSIZE - (readFromDevice < 0 ? 0 : readFromDevice));

        emit q->readyRead();
    }
}

void QBluetoothSocketPrivate::abort()
{
    Q_Q(QBluetoothSocket);
    qCDebug(QT_BT_QNX) << "Disconnecting service";
#ifdef QT_QNX_BT_BLUETOOTH
    if (isServerSocket)
        bt_spp_close_server(m_uuid.toString().toUtf8().data());
    else
        bt_spp_close(socket);
#else
    if (q->state() != QBluetoothSocket::ClosingState)
        ppsSendControlMessage("disconnect_service", 0x1101, m_uuid, m_peerAddress.toString(), QString(), 0,
                          isServerSocket ? BT_SPP_SERVER_SUBTYPE : BT_SPP_CLIENT_SUBTYPE);
#endif
    delete readNotifier;
    readNotifier = 0;
    delete connectWriteNotifier;
    connectWriteNotifier = 0;

    ::close(socket);

    isServerSocket = false;
}

QString QBluetoothSocketPrivate::localName() const
{
    QBluetoothLocalDevice ld;
    return ld.name();
}

QBluetoothAddress QBluetoothSocketPrivate::localAddress() const
{
    QBluetoothLocalDevice ld;
    return ld.address();
}

quint16 QBluetoothSocketPrivate::localPort() const
{
    return 0;
}

QString QBluetoothSocketPrivate::peerName() const
{
    return QString();
}

QBluetoothAddress QBluetoothSocketPrivate::peerAddress() const
{
    return m_peerAddress;
}

quint16 QBluetoothSocketPrivate::peerPort() const
{
    return 0;
}

qint64 QBluetoothSocketPrivate::writeData(const char *data, qint64 maxSize)
{
    Q_Q(QBluetoothSocket);

    if (state != QBluetoothSocket::ConnectedState) {
        errorString = QBluetoothSocket::tr("Cannot write while not connected");
        q->setSocketError(QBluetoothSocket::OperationError);
        return -1;
    }

    if (q->openMode() & QIODevice::Unbuffered) {
        if (::write(socket, data, maxSize) != maxSize) {
            errorString = QBluetoothSocket::tr("Network Error");
            q->setSocketError(QBluetoothSocket::NetworkError);
            qCWarning(QT_BT_QNX) << Q_FUNC_INFO << "Socket error";
            return -1;
        }

        emit q->bytesWritten(maxSize);

        return maxSize;
    } else {
        if (!connectWriteNotifier)
            return -1;

        if (txBuffer.size() == 0) {
            connectWriteNotifier->setEnabled(true);
            QMetaObject::invokeMethod(this, "_q_writeNotify", Qt::QueuedConnection);
        }

        char *txbuf = txBuffer.reserve(maxSize);
        memcpy(txbuf, data, maxSize);

        return maxSize;
    }
}

qint64 QBluetoothSocketPrivate::readData(char *data, qint64 maxSize)
{
    Q_Q(QBluetoothSocket);

    if (state != QBluetoothSocket::ConnectedState) {
        errorString = QBluetoothSocket::tr("Cannot read while not connected");
        q->setSocketError(QBluetoothSocket::OperationError);
        return -1;
    }

    if (!buffer.isEmpty()) {
        int i = buffer.read(data, maxSize);
        return i;
    }
    return 0;
}

void QBluetoothSocketPrivate::close()
{
    abort();
}

bool QBluetoothSocketPrivate::setSocketDescriptor(int socketDescriptor, QBluetoothServiceInfo::Protocol socketType_,
                                                  QBluetoothSocket::SocketState socketState, QBluetoothSocket::OpenMode openMode)
{
    Q_Q(QBluetoothSocket);
    delete readNotifier;
    readNotifier = 0;
    delete connectWriteNotifier;
    connectWriteNotifier = 0;

    socket = socketDescriptor;
    socketType = socketType_;

    readNotifier = new QSocketNotifier(socket, QSocketNotifier::Read);
    QObject::connect(readNotifier, SIGNAL(activated(int)), this, SLOT(_q_readNotify()));
    connectWriteNotifier = new QSocketNotifier(socket, QSocketNotifier::Write, q);
    QObject::connect(connectWriteNotifier, SIGNAL(activated(int)), this, SLOT(_q_writeNotify()));

    q->setSocketState(socketState);
    q->setOpenMode(openMode);

    if (openMode == QBluetoothSocket::ConnectedState)
        emit q->connected();

    isServerSocket = true;
#ifndef QT_QNX_BT_BLUETOOTH
    ppsRegisterForEvent(QStringLiteral("service_disconnected"),this);
#endif

    return true;
}

qint64 QBluetoothSocketPrivate::bytesAvailable() const
{
    return buffer.size();
}

void QBluetoothSocketPrivate::controlReply(ppsResult result)
{
#ifndef QT_QNX_BT_BLUETOOTH
    Q_Q(QBluetoothSocket);

    if (result.msg == QStringLiteral("connect_service")) {
        if (!result.errorMsg.isEmpty()) {
            qCWarning(QT_BT_QNX) << Q_FUNC_INFO << "Error connecting to service:" << result.errorMsg;
            errorString = result.errorMsg;
            q->setSocketError(QBluetoothSocket::UnknownSocketError);
            q->setSocketState(QBluetoothSocket::UnconnectedState);
            return;
        } else {
            qCDebug(QT_BT_QNX) << Q_FUNC_INFO << "Sending request for mount point";
            ppsSendControlMessage("get_mount_point_path", 0x1101, m_uuid, m_peerAddress.toString(), QString(), this, BT_SPP_CLIENT_SUBTYPE);
        }
    } else if (result.msg == QStringLiteral("get_mount_point_path")) {
        QString path;
        path = result.dat.first();
        qCDebug(QT_BT_QNX) << Q_FUNC_INFO << "PATH is" << path;

        if (!result.errorMsg.isEmpty()) {
            qCWarning(QT_BT_QNX) << Q_FUNC_INFO << result.errorMsg;
            errorString = result.errorMsg;
            q->setSocketError(QBluetoothSocket::UnknownSocketError);
            q->setSocketState(QBluetoothSocket::UnconnectedState);
            return;
        } else {
            qCDebug(QT_BT_QNX) << "Mount point path is:" << path;
            socket = ::open(path.toStdString().c_str(), O_RDWR);
            if (socket == -1) {
                errorString = qt_error_string(errno);
                q->setSocketError(QBluetoothSocket::UnknownSocketError);
                qCWarning(QT_BT_QNX) << Q_FUNC_INFO << socket << " error:" << errno << errorString; //TODO Try if this actually works

                q->disconnectFromService();
                q->setSocketState(QBluetoothSocket::UnconnectedState);
                return;
            }

            Q_Q(QBluetoothSocket);
            readNotifier = new QSocketNotifier(socket, QSocketNotifier::Read);
            QObject::connect(readNotifier, SIGNAL(activated(int)), this, SLOT(_q_readNotify()));
            connectWriteNotifier = new QSocketNotifier(socket, QSocketNotifier::Write, q);
            QObject::connect(connectWriteNotifier, SIGNAL(activated(int)), this, SLOT(_q_writeNotify()));

            connectWriteNotifier->setEnabled(true);
            readNotifier->setEnabled(true);
            q->setOpenMode(QIODevice::ReadWrite);
            state = QBluetoothSocket::ConnectedState;
            emit q->connected();
            ppsRegisterForEvent(QStringLiteral("service_disconnected"),this);
        }
    }
#endif
}

void QBluetoothSocketPrivate::controlEvent(ppsResult result)
{
#ifndef QT_QNX_BT_BLUETOOTH
    Q_Q(QBluetoothSocket);
    if (result.msg == QStringLiteral("service_disconnected")) {
        q->setSocketState(QBluetoothSocket::ClosingState);
        close();
    }
#endif
}

QT_END_NAMESPACE

