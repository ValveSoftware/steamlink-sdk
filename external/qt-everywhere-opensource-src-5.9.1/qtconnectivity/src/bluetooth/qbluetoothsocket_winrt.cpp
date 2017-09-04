/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
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

#include "qbluetoothsocket.h"
#include "qbluetoothsocket_p.h"

#include <qfunctions_winrt.h>

#include <private/qeventdispatcher_winrt_p.h>

#include <QtBluetooth/QBluetoothLocalDevice>
#include <QtCore/qloggingcategory.h>

#include <robuffer.h>
#include <windows.devices.bluetooth.h>
#include <windows.networking.sockets.h>
#include <windows.storage.streams.h>
#include <wrl.h>

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::Devices::Bluetooth;
using namespace ABI::Windows::Devices::Bluetooth::Rfcomm;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;
using namespace ABI::Windows::Networking;
using namespace ABI::Windows::Networking::Sockets;
using namespace ABI::Windows::Storage::Streams;

typedef IAsyncOperationWithProgressCompletedHandler<IBuffer *, UINT32> SocketReadCompletedHandler;
typedef IAsyncOperationWithProgress<IBuffer *, UINT32> IAsyncBufferOperation;

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_BT_WINRT)

struct SocketGlobal
{
    SocketGlobal()
    {
        HRESULT hr;
        hr = GetActivationFactory(HString::MakeReference(RuntimeClass_Windows_Storage_Streams_Buffer).Get(),
            &bufferFactory);
        Q_ASSERT_SUCCEEDED(hr);
    }

    ComPtr<IBufferFactory> bufferFactory;
};
Q_GLOBAL_STATIC(SocketGlobal, g)

#define READ_BUFFER_SIZE 65536

static inline QString qt_QStringFromHString(const HString &string)
{
    UINT32 length;
    PCWSTR rawString = string.GetRawBuffer(&length);
    return QString::fromWCharArray(rawString, length);
}

static qint64 writeIOStream(ComPtr<IOutputStream> stream, const char *data, qint64 len)
{
    ComPtr<IBuffer> buffer;
    HRESULT hr = g->bufferFactory->Create(len, &buffer);
    Q_ASSERT_SUCCEEDED(hr);
    hr = buffer->put_Length(len);
    Q_ASSERT_SUCCEEDED(hr);
    ComPtr<Windows::Storage::Streams::IBufferByteAccess> byteArrayAccess;
    hr = buffer.As(&byteArrayAccess);
    Q_ASSERT_SUCCEEDED(hr);
    byte *bytes;
    hr = byteArrayAccess->Buffer(&bytes);
    Q_ASSERT_SUCCEEDED(hr);
    memcpy(bytes, data, len);
    ComPtr<IAsyncOperationWithProgress<UINT32, UINT32>> op;
    hr = stream->WriteAsync(buffer.Get(), &op);
    RETURN_IF_FAILED("Failed to write to stream", return -1);
    UINT32 bytesWritten;
    hr = QWinRTFunctions::await(op, &bytesWritten);
    RETURN_IF_FAILED("Failed to write to stream", return -1);
    return bytesWritten;
}

class SocketWorker : public QObject
{
    Q_OBJECT
public:
    SocketWorker()
    {
    }

    ~SocketWorker()
    {
        if (Q_UNLIKELY(m_initialReadOp)) {
            ComPtr<IAsyncInfo> info;
            HRESULT hr = m_initialReadOp.As(&info);
            Q_ASSERT_SUCCEEDED(hr);
            if (info) {
                hr = info->Cancel();
                Q_ASSERT_SUCCEEDED(hr);
                hr = info->Close();
                Q_ASSERT_SUCCEEDED(hr);
            }
        }

        if (m_readOp) {
            ComPtr<IAsyncInfo> info;
            HRESULT hr = m_readOp.As(&info);
            Q_ASSERT_SUCCEEDED(hr);
            if (info) {
                hr = info->Cancel();
                Q_ASSERT_SUCCEEDED(hr);
                hr = info->Close();
                Q_ASSERT_SUCCEEDED(hr);
            }
        }
    }

signals:
    void newDataReceived(const QVector<QByteArray> &data);
    void socketErrorOccured(QBluetoothSocket::SocketError error);

public slots:
    Q_INVOKABLE void notifyAboutNewData()
    {
        QMutexLocker locker(&m_mutex);
        const QVector<QByteArray> newData = std::move(m_pendingData);
        m_pendingData.clear();
        emit newDataReceived(newData);
    }

public:
    void startReading()
    {
        HRESULT hr;
        hr = QEventDispatcherWinRT::runOnXamlThread([this]()
        {
            ComPtr<IBuffer> buffer;
            HRESULT hr = g->bufferFactory->Create(READ_BUFFER_SIZE, &buffer);
            Q_ASSERT_SUCCEEDED(hr);
            ComPtr<IInputStream> stream;
            hr = m_socket->get_InputStream(&stream);
            Q_ASSERT_SUCCEEDED(hr);
            hr = stream->ReadAsync(buffer.Get(), READ_BUFFER_SIZE, InputStreamOptions_Partial, m_initialReadOp.GetAddressOf());
            Q_ASSERT_SUCCEEDED(hr);
            hr = m_initialReadOp->put_Completed(Callback<SocketReadCompletedHandler>(this, &SocketWorker::onReadyRead).Get());
            Q_ASSERT_SUCCEEDED(hr);
            return S_OK;
        });
        Q_ASSERT_SUCCEEDED(hr);
    }

    HRESULT onReadyRead(IAsyncBufferOperation *asyncInfo, AsyncStatus status)
    {
        if (asyncInfo == m_initialReadOp.Get()) {
            m_initialReadOp.Reset();
        } else if (asyncInfo == m_readOp.Get()) {
            m_readOp.Reset();
        } else {
            Q_ASSERT(false);
        }

        // A read in UnconnectedState will close the socket and return -1 and thus tell the caller,
        // that the connection was closed. The socket cannot be closed here, as the subsequent read
        // might fail then.
        if (status == Error || status == Canceled) {
            emit socketErrorOccured(QBluetoothSocket::NetworkError);
            return S_OK;
        }

        ComPtr<IBuffer> buffer;
        HRESULT hr = asyncInfo->GetResults(&buffer);
        if (FAILED(hr)) {
            qErrnoWarning(hr, "Failed to get read results buffer");
            emit socketErrorOccured(QBluetoothSocket::UnknownSocketError);
            return S_OK;
        }

        UINT32 bufferLength;
        hr = buffer->get_Length(&bufferLength);
        if (FAILED(hr)) {
            qErrnoWarning(hr, "Failed to get buffer length");
            emit socketErrorOccured(QBluetoothSocket::UnknownSocketError);
            return S_OK;
        }
        // A zero sized buffer length signals, that the remote host closed the connection. The socket
        // cannot be closed though, as the following read might have socket descriptor -1 and thus and
        // the closing of the socket won't be communicated to the caller. So only the error is set. The
        // actual socket close happens inside of read.
        if (!bufferLength) {
            emit socketErrorOccured(QBluetoothSocket::NetworkError);
            return S_OK;
        }

        ComPtr<Windows::Storage::Streams::IBufferByteAccess> byteArrayAccess;
        hr = buffer.As(&byteArrayAccess);
        if (FAILED(hr)) {
            qErrnoWarning(hr, "Failed to get cast buffer");
            emit socketErrorOccured(QBluetoothSocket::UnknownSocketError);
            return S_OK;
        }
        byte *data;
        hr = byteArrayAccess->Buffer(&data);
        if (FAILED(hr)) {
            qErrnoWarning(hr, "Failed to access buffer data");
            emit socketErrorOccured(QBluetoothSocket::UnknownSocketError);
            return S_OK;
        }

        QByteArray newData(reinterpret_cast<const char*>(data), qint64(bufferLength));
        QMutexLocker readLocker(&m_mutex);
        if (m_pendingData.isEmpty())
            QMetaObject::invokeMethod(this, "notifyAboutNewData", Qt::QueuedConnection);
        m_pendingData << newData;
        readLocker.unlock();

        hr = QEventDispatcherWinRT::runOnXamlThread([buffer, this]() {
            UINT32 readBufferLength;
            ComPtr<IInputStream> stream;
            HRESULT hr = m_socket->get_InputStream(&stream);
            if (FAILED(hr)) {
                qErrnoWarning(hr, "Failed to obtain input stream");
                emit socketErrorOccured(QBluetoothSocket::UnknownSocketError);
                return S_OK;
            }

            // Reuse the stream buffer
            hr = buffer->get_Capacity(&readBufferLength);
            if (FAILED(hr)) {
                qErrnoWarning(hr, "Failed to get buffer capacity");
                emit socketErrorOccured(QBluetoothSocket::UnknownSocketError);
                return S_OK;
            }
            hr = buffer->put_Length(0);
            if (FAILED(hr)) {
                qErrnoWarning(hr, "Failed to set buffer length");
                emit socketErrorOccured(QBluetoothSocket::UnknownSocketError);
                return S_OK;
            }

            hr = stream->ReadAsync(buffer.Get(), readBufferLength, InputStreamOptions_Partial, &m_readOp);
            if (FAILED(hr)) {
                qErrnoWarning(hr, "onReadyRead(): Could not read into socket stream buffer.");
                emit socketErrorOccured(QBluetoothSocket::UnknownSocketError);
                return S_OK;
            }
            hr = m_readOp->put_Completed(Callback<SocketReadCompletedHandler>(this, &SocketWorker::onReadyRead).Get());
            if (FAILED(hr)) {
                qErrnoWarning(hr, "onReadyRead(): Failed to set socket read callback.");
                emit socketErrorOccured(QBluetoothSocket::UnknownSocketError);
                return S_OK;
            }
            return S_OK;
        });
        Q_ASSERT_SUCCEEDED(hr);
        return S_OK;
    }

    void setSocket(ComPtr<IStreamSocket> socket) { m_socket = socket; }

private:
    ComPtr<IStreamSocket> m_socket;
    QVector<QByteArray> m_pendingData;

    // Protects pendingData/pendingDatagrams which are accessed from native callbacks
    QMutex m_mutex;

    ComPtr<IAsyncOperationWithProgress<IBuffer *, UINT32>> m_initialReadOp;
    ComPtr<IAsyncOperationWithProgress<IBuffer *, UINT32>> m_readOp;
};

QBluetoothSocketPrivate::QBluetoothSocketPrivate()
    : socket(-1),
      socketType(QBluetoothServiceInfo::UnknownProtocol),
      state(QBluetoothSocket::UnconnectedState),
      socketError(QBluetoothSocket::NoSocketError),
      secFlags(QBluetooth::NoSecurity),
      m_worker(new SocketWorker())
{
    connect(m_worker, &SocketWorker::newDataReceived,
            this, &QBluetoothSocketPrivate::handleNewData, Qt::QueuedConnection);
    connect(m_worker, &SocketWorker::socketErrorOccured,
            this, &QBluetoothSocketPrivate::handleError, Qt::QueuedConnection);
}

QBluetoothSocketPrivate::~QBluetoothSocketPrivate()
{
    abort();
}

bool QBluetoothSocketPrivate::ensureNativeSocket(QBluetoothServiceInfo::Protocol type)
{
    if (socket != -1) {
        if (type == socketType)
            return true;
        m_socketObject = nullptr;
        socket = -1;
    }
    socketType = type;
    if (socketType != QBluetoothServiceInfo::RfcommProtocol)
        return false;

    HRESULT hr;
    hr = RoActivateInstance(HString::MakeReference(RuntimeClass_Windows_Networking_Sockets_StreamSocket).Get(), &m_socketObject);
    if (FAILED(hr) || !m_socketObject) {
        qErrnoWarning(hr, "ensureNativeSocket: Could not create socket instance");
        return false;
    }
    socket = qintptr(m_socketObject.Get());
    m_worker->setSocket(m_socketObject);

    return true;
}

void QBluetoothSocketPrivate::connectToService(const QBluetoothAddress &address, quint16 port, QIODevice::OpenMode openMode)
{
    Q_Q(QBluetoothSocket);
    Q_UNUSED(openMode);

    if (socket == -1 && !ensureNativeSocket(socketType)) {
        errorString = QBluetoothSocket::tr("Unknown socket error");
        q->setSocketError(QBluetoothSocket::UnknownSocketError);
        return;
    }

    const QString addressString = address.toString();
    HStringReference hostNameRef(reinterpret_cast<LPCWSTR>(addressString.utf16()));
    ComPtr<IHostNameFactory> hostNameFactory;
    HRESULT hr = GetActivationFactory(HString::MakeReference(RuntimeClass_Windows_Networking_HostName).Get(),
                                      &hostNameFactory);
    Q_ASSERT_SUCCEEDED(hr);
    ComPtr<IHostName> remoteHost;
    hr = hostNameFactory->CreateHostName(hostNameRef.Get(), &remoteHost);
    RETURN_VOID_IF_FAILED("QBluetoothSocketPrivate::connectToService: Could not create hostname.");

    const QString portString = QString::number(port);
    HStringReference portReference(reinterpret_cast<LPCWSTR>(portString.utf16()));

    hr = m_socketObject->ConnectAsync(remoteHost.Get(), portReference.Get(), &m_connectOp);
    if (hr == E_ACCESSDENIED) {
        qErrnoWarning(hr, "QBluetoothSocketPrivate::connectToService: Unable to connect to bluetooth socket."
            "Please check your manifest capabilities.");
        q->setSocketState(QBluetoothSocket::UnconnectedState);
        return;
    }
    Q_ASSERT_SUCCEEDED(hr);

    q->setSocketState(QBluetoothSocket::ConnectingState);
    q->setOpenMode(openMode);
    QEventDispatcherWinRT::runOnXamlThread([this]() {
        HRESULT hr;
        hr = m_connectOp->put_Completed(Callback<IAsyncActionCompletedHandler>(
                                         this, &QBluetoothSocketPrivate::handleConnectOpFinished).Get());
        RETURN_HR_IF_FAILED("connectToHostByName: Could not register \"connectOp\" callback");
        return S_OK;
    });
}

void QBluetoothSocketPrivate::abort()
{
    Q_Q(QBluetoothSocket);
    if (state == QBluetoothSocket::UnconnectedState)
        return;

    disconnect(m_worker, &SocketWorker::newDataReceived,
        this, &QBluetoothSocketPrivate::handleNewData);
    disconnect(m_worker, &SocketWorker::socketErrorOccured,
        this, &QBluetoothSocketPrivate::handleError);
    m_worker->deleteLater();

    if (socket != -1) {
        m_socketObject = nullptr;
        socket = -1;
    }
    q->setSocketState(QBluetoothSocket::UnconnectedState);
}

QString QBluetoothSocketPrivate::localName() const
{
    const QBluetoothAddress address = localAddress();
    if (address.isNull())
        return QString();

    QBluetoothLocalDevice device(address);
    return device.name();
}

QBluetoothAddress QBluetoothSocketPrivate::localAddress() const
{
    HRESULT hr;
    ComPtr<IStreamSocketInformation> info;
    hr = m_socketObject->get_Information(&info);
    Q_ASSERT_SUCCEEDED(hr);
    ComPtr<IHostName> localHost;
    hr = info->get_LocalAddress(&localHost);
    Q_ASSERT_SUCCEEDED(hr);
    HString localAddress;
    hr = localHost->get_CanonicalName(localAddress.GetAddressOf());
    Q_ASSERT_SUCCEEDED(hr);
    return QBluetoothAddress(qt_QStringFromHString(localAddress));
}

quint16 QBluetoothSocketPrivate::localPort() const
{
    HRESULT hr;
    ComPtr<IStreamSocketInformation> info;
    hr = m_socketObject->get_Information(&info);
    Q_ASSERT_SUCCEEDED(hr);
    HString localPortString;
    hr = info->get_LocalPort(localPortString.GetAddressOf());
    Q_ASSERT_SUCCEEDED(hr);
    return qt_QStringFromHString(localPortString).toInt();
}

QString QBluetoothSocketPrivate::peerName() const
{
    HRESULT hr;
    ComPtr<IStreamSocketInformation> info;
    hr = m_socketObject->get_Information(&info);
    Q_ASSERT_SUCCEEDED(hr);
    ComPtr<IHostName> remoteHost;
    hr = info->get_RemoteHostName(&remoteHost);
    Q_ASSERT_SUCCEEDED(hr);
    HString remoteHostName;
    hr = remoteHost->get_DisplayName(remoteHostName.GetAddressOf());
    Q_ASSERT_SUCCEEDED(hr);
    return qt_QStringFromHString(remoteHostName);
}

QBluetoothAddress QBluetoothSocketPrivate::peerAddress() const
{
    HRESULT hr;
    ComPtr<IStreamSocketInformation> info;
    hr = m_socketObject->get_Information(&info);
    Q_ASSERT_SUCCEEDED(hr);
    ComPtr<IHostName> remoteHost;
    hr = info->get_RemoteAddress(&remoteHost);
    Q_ASSERT_SUCCEEDED(hr);
    HString remoteAddress;
    hr = remoteHost->get_CanonicalName(remoteAddress.GetAddressOf());
    Q_ASSERT_SUCCEEDED(hr);
    return QBluetoothAddress(qt_QStringFromHString(remoteAddress));
}

quint16 QBluetoothSocketPrivate::peerPort() const
{
    HRESULT hr;
    ComPtr<IStreamSocketInformation> info;
    hr = m_socketObject->get_Information(&info);
    Q_ASSERT_SUCCEEDED(hr);
    HString remotePortString;
    hr = info->get_LocalPort(remotePortString.GetAddressOf());
    Q_ASSERT_SUCCEEDED(hr);
    return qt_QStringFromHString(remotePortString).toInt();
}

qint64 QBluetoothSocketPrivate::writeData(const char *data, qint64 maxSize)
{
    Q_Q(QBluetoothSocket);

    if (state != QBluetoothSocket::ConnectedState) {
        errorString = QBluetoothSocket::tr("Cannot write while not connected");
        q->setSocketError(QBluetoothSocket::OperationError);
        return -1;
    }

    ComPtr<IOutputStream> stream;
    HRESULT hr;
    hr = m_socketObject->get_OutputStream(&stream);
    Q_ASSERT_SUCCEEDED(hr);

    qint64 bytesWritten = writeIOStream(stream, data, maxSize);
    if (bytesWritten < 0) {
        qCWarning(QT_BT_WINRT) << "Socket::writeData: " << state;
        errorString = QBluetoothSocket::tr("Cannot read while not connected");
        q->setSocketError(QBluetoothSocket::OperationError);
    }

    emit q->bytesWritten(bytesWritten);
    return bytesWritten;
}

qint64 QBluetoothSocketPrivate::readData(char *data, qint64 maxSize)
{
    Q_Q(QBluetoothSocket);

    if (state != QBluetoothSocket::ConnectedState) {
        errorString = QBluetoothSocket::tr("Cannot read while not connected");
        q->setSocketError(QBluetoothSocket::OperationError);
        return -1;
    }

    if (!buffer.isEmpty())
        return buffer.read(data, maxSize);

    return 0;
}

void QBluetoothSocketPrivate::close()
{
    abort();
}

bool QBluetoothSocketPrivate::setSocketDescriptor(int socketDescriptor, QBluetoothServiceInfo::Protocol socketType,
                                           QBluetoothSocket::SocketState socketState, QBluetoothSocket::OpenMode openMode)
{
    Q_UNUSED(socketDescriptor);
    Q_UNUSED(socketType)
    Q_UNUSED(socketState);
    Q_UNUSED(openMode);
    qCWarning(QT_BT_WINRT) << "No socket descriptor support on WinRT.";
    return false;
}

bool QBluetoothSocketPrivate::setSocketDescriptor(ComPtr<IStreamSocket> socketPtr, QBluetoothServiceInfo::Protocol socketType,
                                           QBluetoothSocket::SocketState socketState, QBluetoothSocket::OpenMode openMode)
{
    Q_Q(QBluetoothSocket);
    if (socketType != QBluetoothServiceInfo::RfcommProtocol || !socketPtr)
        return false;

    m_socketObject = socketPtr;
    socket = qintptr(m_socketObject.Get());
    m_worker->setSocket(m_socketObject);
    q->setSocketState(socketState);
    if (socketState == QBluetoothSocket::ConnectedState)
        m_worker->startReading();
    q->setOpenMode(openMode);
    return true;
}

qint64 QBluetoothSocketPrivate::bytesAvailable() const
{
    return buffer.size();
}

void QBluetoothSocketPrivate::handleNewData(const QVector<QByteArray> &data)
{
    // Defer putting the data into the list until the next event loop iteration
    // (where the readyRead signal is emitted as well)
    QMetaObject::invokeMethod(this, "addToPendingData", Qt::QueuedConnection,
                              Q_ARG(QVector<QByteArray>, data));
}

void QBluetoothSocketPrivate::handleError(QBluetoothSocket::SocketError error)
{
    Q_Q(QBluetoothSocket);
    switch (error) {
    case QBluetoothSocket::NetworkError:
        errorString = QBluetoothSocket::tr("Network error");
        break;
    default:
        errorString = QBluetoothSocket::tr("Unknown socket error");
    }

    q->setSocketError(error);
    q->setSocketState(QBluetoothSocket::UnconnectedState);
}

void QBluetoothSocketPrivate::addToPendingData(const QVector<QByteArray> &data)
{
    Q_Q(QBluetoothSocket);
    QMutexLocker locker(&m_readMutex);
    m_pendingData.append(data);
    for (const QByteArray &newData : data) {
        char *writePointer = buffer.reserve(newData.length());
        memcpy(writePointer, newData.data(), newData.length());
    }
    locker.unlock();
    emit q->readyRead();
}

HRESULT QBluetoothSocketPrivate::handleConnectOpFinished(ABI::Windows::Foundation::IAsyncAction *action, ABI::Windows::Foundation::AsyncStatus status)
{
    Q_Q(QBluetoothSocket);
    if (status != Completed || !m_connectOp) { // Protect against a late callback
        errorString = QBluetoothSocket::tr("Unknown socket error");
        q->setSocketError(QBluetoothSocket::UnknownSocketError);
        q->setSocketState(QBluetoothSocket::UnconnectedState);
        return S_OK;
    }

    HRESULT hr = action->GetResults();
    switch (hr) {
    case 0x8007274c: // A connection attempt failed because the connected party did not properly respond after a period of time, or established connection failed because connected host has failed to respond.
        errorString = QBluetoothSocket::tr("Connection timed out");
        q->setSocketError(QBluetoothSocket::NetworkError);
        q->setSocketState(QBluetoothSocket::UnconnectedState);
        return S_OK;
    case 0x80072751: // A socket operation was attempted to an unreachable host.
        errorString = QBluetoothSocket::tr("Host not reachable");
        q->setSocketError(QBluetoothSocket::HostNotFoundError);
        q->setSocketState(QBluetoothSocket::UnconnectedState);
        return S_OK;
    case 0x8007274d: // No connection could be made because the target machine actively refused it.
        errorString = QBluetoothSocket::tr("Host refused connection");
        q->setSocketError(QBluetoothSocket::HostNotFoundError);
        q->setSocketState(QBluetoothSocket::UnconnectedState);
        return S_OK;
    default:
        if (FAILED(hr)) {
            errorString = QBluetoothSocket::tr("Unknown socket error");
            q->setSocketError(QBluetoothSocket::UnknownSocketError);
            q->setSocketState(QBluetoothSocket::UnconnectedState);
            return S_OK;
        }
    }

    // The callback might be triggered several times if we do not cancel/reset it here
    if (m_connectOp) {
        ComPtr<IAsyncInfo> info;
        hr = m_connectOp.As(&info);
        Q_ASSERT_SUCCEEDED(hr);
        if (info) {
            hr = info->Cancel();
            Q_ASSERT_SUCCEEDED(hr);
            hr = info->Close();
            Q_ASSERT_SUCCEEDED(hr);
        }
        hr = m_connectOp.Reset();
        Q_ASSERT_SUCCEEDED(hr);
    }

    q->setSocketState(QBluetoothSocket::ConnectedState);
    m_worker->startReading();
    emit q->connected();

    return S_OK;
}

QT_END_NAMESPACE

#include "qbluetoothsocket_winrt.moc"
