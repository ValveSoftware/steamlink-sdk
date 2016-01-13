/****************************************************************************
**
** Copyright (C) 2013 Lauri Laanmets (Proekspert AS) <lauri.laanmets@eesti.ee>
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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
#include "qbluetoothaddress.h"
#include <QtCore/QLoggingCategory>
#include <QtCore/QTime>
#include <QtConcurrent/QtConcurrentRun>
#include <QtAndroidExtras/QAndroidJniEnvironment>


QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_BT_ANDROID)

QBluetoothSocketPrivate::QBluetoothSocketPrivate()
  : socket(-1),
    socketType(QBluetoothServiceInfo::UnknownProtocol),
    state(QBluetoothSocket::UnconnectedState),
    socketError(QBluetoothSocket::NoSocketError),
    connecting(false),
    discoveryAgent(0),
    inputThread(0)
{
    adapter = QAndroidJniObject::callStaticObjectMethod("android/bluetooth/BluetoothAdapter",
                                                        "getDefaultAdapter",
                                                        "()Landroid/bluetooth/BluetoothAdapter;");
    qRegisterMetaType<QBluetoothSocket::SocketError>("QBluetoothSocket::SocketError");
    qRegisterMetaType<QBluetoothSocket::SocketState>("QBluetoothSocket::SocketState");
}

QBluetoothSocketPrivate::~QBluetoothSocketPrivate()
{
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
                                               QIODevice::OpenMode openMode,
                                               int fallbackServiceChannel)
{
    Q_Q(QBluetoothSocket);

    q->setSocketState(QBluetoothSocket::ConnectingState);
    QtConcurrent::run(this, &QBluetoothSocketPrivate::connectToServiceConc,
                      address, uuid, openMode, fallbackServiceChannel);
}

bool QBluetoothSocketPrivate::fallBackConnect(QAndroidJniObject uuid, int channel)
{
    qCWarning(QT_BT_ANDROID) << "Falling back to workaround.";

    QAndroidJniEnvironment env;
    jclass remoteDeviceClazz = env->GetObjectClass(remoteDevice.object());
    jmethodID getClassMethod = env->GetMethodID(remoteDeviceClazz, "getClass", "()Ljava/lang/Class;");
    if (!getClassMethod) {
        qCWarning(QT_BT_ANDROID) << "BluetoothDevice.getClass method could not be found.";
        return false;
    }


    QAndroidJniObject remoteDeviceClass = QAndroidJniObject(env->CallObjectMethod(remoteDevice.object(), getClassMethod));
    if (!remoteDeviceClass.isValid()) {
        qCWarning(QT_BT_ANDROID) << "Could not invoke BluetoothDevice.getClass.";
        return false;
    }

    jclass classClass = env->FindClass("java/lang/Class");
    jclass integerClass = env->FindClass("java/lang/Integer");
    jfieldID integerType = env->GetStaticFieldID(integerClass, "TYPE", "Ljava/lang/Class;");
    jobject integerObject = env->GetStaticObjectField(integerClass, integerType);
    if (!integerObject) {
        qCWarning(QT_BT_ANDROID) << "Could not get Integer.TYPE";
        return false;
    }

    jobjectArray paramTypes = env->NewObjectArray(1, classClass, integerObject);
    if (!paramTypes) {
        qCWarning(QT_BT_ANDROID) << "Could not create new Class[]{Integer.TYPE}";
        return false;
    }

    QAndroidJniObject parcelUuid("android/os/ParcelUuid", "(Ljava/util/UUID;)V",
                                 uuid.object());
    if (parcelUuid.isValid()) {
        jint socketChannel = remoteDevice.callMethod<jint>("getServiceChannel",
                                                           "(Landroid/os/ParcelUuid;)I",
                                                           parcelUuid.object());
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }

        if (socketChannel
                == remoteDevice.getStaticField<jint>("android/bluetooth/BluetoothDevice", "ERROR")) {
            qCWarning(QT_BT_ANDROID) << "Cannot determine RFCOMM service channel.";
        } else {
            qCWarning(QT_BT_ANDROID) << "Using found rfcomm channel" << socketChannel;
            channel = socketChannel;
        }
    }

    QAndroidJniObject method = remoteDeviceClass.callObjectMethod(
                "getMethod",
                "(Ljava/lang/String;[Ljava/lang/Class;)Ljava/lang/reflect/Method;",
                QAndroidJniObject::fromString(QLatin1String("createRfcommSocket")).object<jstring>(),
                paramTypes);
    if (!method.isValid() || env->ExceptionCheck()) {
        qCWarning(QT_BT_ANDROID) << "Could not invoke getMethod";
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        return false;
    }

    jclass methodClass = env->GetObjectClass(method.object());
    jmethodID invokeMethodId = env->GetMethodID(
                methodClass, "invoke",
                "(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;");
    if (!invokeMethodId) {
        qCWarning(QT_BT_ANDROID) << "Could not invoke method.";
        return false;
    }

    jmethodID valueOfMethodId = env->GetStaticMethodID(integerClass, "valueOf", "(I)Ljava/lang/Integer;");
    jclass objectClass = env->FindClass("java/lang/Object");
    jobjectArray invokeParams = env->NewObjectArray(1, objectClass, env->CallStaticObjectMethod(integerClass, valueOfMethodId, channel));


    jobject invokeResult = env->CallObjectMethod(method.object(), invokeMethodId,
                                                 remoteDevice.object(), invokeParams);
    if (!invokeResult)
    {
        qCWarning(QT_BT_ANDROID) << "Invoke Resulted with error.";
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        return false;
    }

    socketObject = QAndroidJniObject(invokeResult);
    socketObject.callMethod<void>("connect");
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();

        qCWarning(QT_BT_ANDROID) << "Socket connect via workaround failed.";

        return false;
    }

    qCWarning(QT_BT_ANDROID) << "Workaround invoked.";
    return true;
}

void QBluetoothSocketPrivate::connectToServiceConc(const QBluetoothAddress &address,
                                                   const QBluetoothUuid &uuid, QIODevice::OpenMode openMode, int fallbackServiceChannel)
{
    Q_Q(QBluetoothSocket);
    Q_UNUSED(openMode);

    qCDebug(QT_BT_ANDROID) << "connectToServiceConc()" << address.toString() << uuid.toString();

    if (!adapter.isValid()) {
        qCWarning(QT_BT_ANDROID) << "Device does not support Bluetooth";
        errorString = QBluetoothSocket::tr("Device does not support Bluetooth");
        q->setSocketError(QBluetoothSocket::NetworkError);
        q->setSocketState(QBluetoothSocket::UnconnectedState);
        return;
    }

    const int state = adapter.callMethod<jint>("getState");
    if (state != 12 ) { //BluetoothAdapter.STATE_ON
        qCWarning(QT_BT_ANDROID) << "Bt device offline";
        errorString = QBluetoothSocket::tr("Device is powered off");
        q->setSocketError(QBluetoothSocket::NetworkError);
        q->setSocketState(QBluetoothSocket::UnconnectedState);
        return;
    }

    QAndroidJniEnvironment env;
    QAndroidJniObject inputString = QAndroidJniObject::fromString(address.toString());
    remoteDevice = adapter.callObjectMethod("getRemoteDevice",
                                            "(Ljava/lang/String;)Landroid/bluetooth/BluetoothDevice;",
                                            inputString.object<jstring>());
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();

        errorString = QBluetoothSocket::tr("Cannot access address %1", "%1 = Bt address e.g. 11:22:33:44:55:66").arg(address.toString());
        q->setSocketError(QBluetoothSocket::HostNotFoundError);
        q->setSocketState(QBluetoothSocket::UnconnectedState);
        return;
    }

    //cut leading { and trailing } {xxx-xxx}
    QString tempUuid = uuid.toString();
    tempUuid.chop(1); //remove trailing '}'
    tempUuid.remove(0, 1); //remove first '{'

    inputString = QAndroidJniObject::fromString(tempUuid);
    QAndroidJniObject uuidObject = QAndroidJniObject::callStaticObjectMethod("java/util/UUID", "fromString",
                                                                       "(Ljava/lang/String;)Ljava/util/UUID;",
                                                                       inputString.object<jstring>());

    socketObject = remoteDevice.callObjectMethod("createRfcommSocketToServiceRecord",
                                                 "(Ljava/util/UUID;)Landroid/bluetooth/BluetoothSocket;",
                                                 uuidObject.object<jobject>());

    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();

        socketObject = remoteDevice = QAndroidJniObject();
        errorString = QBluetoothSocket::tr("Cannot connect to %1 on %2",
                                           "%1 = uuid, %2 = Bt address").arg(uuid.toString()).arg(address.toString());
        q->setSocketError(QBluetoothSocket::ServiceNotFoundError);
        q->setSocketState(QBluetoothSocket::UnconnectedState);
        return;
    }

    socketObject.callMethod<void>("connect");
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();

        bool success = fallBackConnect(uuidObject, fallbackServiceChannel);
        if (!success) {
            errorString = QBluetoothSocket::tr("Connection to service failed");
            socketObject = remoteDevice = QAndroidJniObject();
            q->setSocketError(QBluetoothSocket::ServiceNotFoundError);
            q->setSocketState(QBluetoothSocket::UnconnectedState);

            env->ExceptionClear(); //just in case
            return;
        }
    }

    if (inputThread) {
        inputThread->deleteLater();
        inputThread = 0;
    }

    inputStream = socketObject.callObjectMethod("getInputStream", "()Ljava/io/InputStream;");
    outputStream = socketObject.callObjectMethod("getOutputStream", "()Ljava/io/OutputStream;");

    if (env->ExceptionCheck() || !inputStream.isValid() || !outputStream.isValid()) {
        env->ExceptionDescribe();
        env->ExceptionClear();

        //close socket again
        socketObject.callMethod<void>("close");
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }

        socketObject = inputStream = outputStream = remoteDevice = QAndroidJniObject();


        errorString = QBluetoothSocket::tr("Obtaining streams for service failed");
        q->setSocketError(QBluetoothSocket::NetworkError);
        q->setSocketState(QBluetoothSocket::UnconnectedState);
        return;
    }

    inputThread = new InputStreamThread(this);
    QObject::connect(inputThread, SIGNAL(dataAvailable()),
                     q, SIGNAL(readyRead()), Qt::QueuedConnection);
    QObject::connect(inputThread, SIGNAL(error(int)),
                     this, SLOT(inputThreadError(int)), Qt::QueuedConnection);

    if (!inputThread->run()) {
        //close socket again
        socketObject.callMethod<void>("close");
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }

        socketObject = inputStream = outputStream = remoteDevice = QAndroidJniObject();

        delete inputThread;
        inputThread = 0;

        errorString = QBluetoothSocket::tr("Input stream thread cannot be started");
        q->setSocketError(QBluetoothSocket::NetworkError);
        q->setSocketState(QBluetoothSocket::UnconnectedState);
        return;
    }

    // only unbuffered behavior supported at this stage
    q->setOpenMode(QIODevice::ReadWrite|QIODevice::Unbuffered);

    q->setSocketState(QBluetoothSocket::ConnectedState);
    emit q->connected();
}

void QBluetoothSocketPrivate::abort()
{
    if (state == QBluetoothSocket::UnconnectedState)
        return;

    if (socketObject.isValid()) {
        QAndroidJniEnvironment env;

        /*
         * BluetoothSocket.close() triggers an abort of the input stream
         * thread because inputStream.read() throws IOException
         * In turn the thread stops and throws an error which sets
         * new state, error and emits relevant signals.
         * See QBluetoothSocketPrivate::inputThreadError() for details
         */

        if (inputThread)
            inputThread->prepareForClosure();

        //triggers abort of input thread as well
        socketObject.callMethod<void>("close");
        if (env->ExceptionCheck()) {

            qCWarning(QT_BT_ANDROID) << "Error during closure of socket";
            env->ExceptionDescribe();
            env->ExceptionClear();
        }

        if (inputThread) {
            //don't delete here as signals caused by Java Thread are still
            //going to be emitted
            //delete occurs in inputThreadError()
            inputThread = 0;
        }

        inputStream = outputStream = socketObject = remoteDevice = QAndroidJniObject();
    }
}

QString QBluetoothSocketPrivate::localName() const
{
    if (adapter.isValid())
        return adapter.callObjectMethod<jstring>("getName").toString();

    return QString();
}

QBluetoothAddress QBluetoothSocketPrivate::localAddress() const
{
    QString result;
    if (adapter.isValid())
        result = adapter.callObjectMethod("getAddress", "()Ljava/lang/String;").toString();

    return QBluetoothAddress(result);
}

quint16 QBluetoothSocketPrivate::localPort() const
{
    // Impossible to get channel number with current Android API (Levels 5 to 19)
    return 0;
}

QString QBluetoothSocketPrivate::peerName() const
{
    if (!remoteDevice.isValid())
        return QString();

    return remoteDevice.callObjectMethod("getName", "()Ljava/lang/String;").toString();
}

QBluetoothAddress QBluetoothSocketPrivate::peerAddress() const
{
    if (!remoteDevice.isValid())
        return QBluetoothAddress();

    const QString address = remoteDevice.callObjectMethod("getAddress",
                                                          "()Ljava/lang/String;").toString();

    return QBluetoothAddress(address);
}

quint16 QBluetoothSocketPrivate::peerPort() const
{
    // Impossible to get channel number with current Android API (Levels 5 to 13)
    return 0;
}

qint64 QBluetoothSocketPrivate::writeData(const char *data, qint64 maxSize)
{
    //TODO implement buffered behavior (so far only unbuffered)
    Q_Q(QBluetoothSocket);
    if (state != QBluetoothSocket::ConnectedState || !outputStream.isValid()) {
        qCWarning(QT_BT_ANDROID) << "Socket::writeData: " << state << outputStream.isValid();
        errorString = QBluetoothSocket::tr("Cannot write while not connected");
        q->setSocketError(QBluetoothSocket::OperationError);
        return -1;
    }

    QAndroidJniEnvironment env;
    jbyteArray nativeData = env->NewByteArray((qint32)maxSize);
    env->SetByteArrayRegion(nativeData, 0, (qint32)maxSize, reinterpret_cast<const jbyte*>(data));
    outputStream.callMethod<void>("write", "([BII)V", nativeData, 0, (qint32)maxSize);
    env->DeleteLocalRef(nativeData);
    emit q->bytesWritten(maxSize);

    if (env->ExceptionCheck()) {
        qCWarning(QT_BT_ANDROID) << "Error while writing";
        env->ExceptionDescribe();
        env->ExceptionClear();
        errorString = QBluetoothSocket::tr("Error during write on socket.");
        q->setSocketError(QBluetoothSocket::NetworkError);
        return -1;
    }

    return maxSize;
}

qint64 QBluetoothSocketPrivate::readData(char *data, qint64 maxSize)
{
    Q_Q(QBluetoothSocket);
    if (state != QBluetoothSocket::ConnectedState || !inputThread) {
        qCWarning(QT_BT_ANDROID) << "Socket::readData: " << state << inputThread ;
        errorString = QBluetoothSocket::tr("Cannot read while not connected");
        q->setSocketError(QBluetoothSocket::OperationError);
        return -1;
    }

    return inputThread->readData(data, maxSize);
}

void QBluetoothSocketPrivate::inputThreadError(int errorCode)
{
    Q_Q(QBluetoothSocket);

    if (errorCode != -1) { //magic error which is expected and can be ignored
        errorString = QBluetoothSocket::tr("Network error during read");
        q->setSocketError(QBluetoothSocket::NetworkError);
    }

    //finally we can delete the InputStreamThread
    InputStreamThread *client = qobject_cast<InputStreamThread *>(sender());
    if (client)
        client->deleteLater();

    if (socketObject.isValid()) {
        //triggered when remote side closed the socket
        //cleanup internal objects
        //if it was call to local close()/abort() the objects are cleaned up already

        QAndroidJniEnvironment env;
        socketObject.callMethod<void>("close");
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }

        inputStream = outputStream = remoteDevice = socketObject = QAndroidJniObject();
    }

    q->setSocketState(QBluetoothSocket::UnconnectedState);
    q->setOpenMode(QIODevice::NotOpen);
    emit q->disconnected();
}

void QBluetoothSocketPrivate::close()
{
    /* This function is called by QBluetoothSocket::close and softer version
       QBluetoothSocket::disconnectFromService() which difference I do not quite fully understand.
       Anyways we end up in Android "close" function call.
     */
    abort();
}

bool QBluetoothSocketPrivate::setSocketDescriptor(int socketDescriptor, QBluetoothServiceInfo::Protocol socketType,
                         QBluetoothSocket::SocketState socketState, QBluetoothSocket::OpenMode openMode)
{
    Q_UNUSED(socketDescriptor);
    Q_UNUSED(socketType)
    Q_UNUSED(socketState);
    Q_UNUSED(openMode);
    qCWarning(QT_BT_ANDROID) << "No socket descriptor support on Android.";
    return false;
}

bool QBluetoothSocketPrivate::setSocketDescriptor(const QAndroidJniObject &socket, QBluetoothServiceInfo::Protocol socketType_,
                         QBluetoothSocket::SocketState socketState, QBluetoothSocket::OpenMode openMode)
{
    Q_Q(QBluetoothSocket);

    if (q->state() != QBluetoothSocket::UnconnectedState || !socket.isValid())
        return false;

    if (!ensureNativeSocket(socketType_))
        return false;

    socketObject = socket;

    QAndroidJniEnvironment env;
    inputStream = socketObject.callObjectMethod("getInputStream", "()Ljava/io/InputStream;");
    outputStream = socketObject.callObjectMethod("getOutputStream", "()Ljava/io/OutputStream;");

    if (env->ExceptionCheck() || !inputStream.isValid() || !outputStream.isValid()) {
        env->ExceptionDescribe();
        env->ExceptionClear();

        //close socket again
        socketObject.callMethod<void>("close");
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }

        socketObject = inputStream = outputStream = remoteDevice = QAndroidJniObject();


        errorString = QBluetoothSocket::tr("Obtaining streams for service failed");
        q->setSocketError(QBluetoothSocket::NetworkError);
        q->setSocketState(QBluetoothSocket::UnconnectedState);
        return false;
    }

    remoteDevice = socketObject.callObjectMethod("getRemoteDevice", "()Landroid/bluetooth/BluetoothDevice;");

    if (inputThread) {
        inputThread->deleteLater();
        inputThread = 0;
    }
    inputThread = new InputStreamThread(this);
    QObject::connect(inputThread, SIGNAL(dataAvailable()),
                     q, SIGNAL(readyRead()), Qt::QueuedConnection);
    QObject::connect(inputThread, SIGNAL(error(int)),
                     this, SLOT(inputThreadError(int)), Qt::QueuedConnection);
    inputThread->run();


    q->setSocketState(socketState);
    q->setOpenMode(openMode | QIODevice::Unbuffered);

    if (openMode == QBluetoothSocket::ConnectedState)
        emit q->connected();

    return true;
}

qint64 QBluetoothSocketPrivate::bytesAvailable() const
{
    //We cannot access buffer directly as it is part of different thread
    if (inputThread)
        return inputThread->bytesAvailable();

    return 0;
}

QT_END_NAMESPACE
