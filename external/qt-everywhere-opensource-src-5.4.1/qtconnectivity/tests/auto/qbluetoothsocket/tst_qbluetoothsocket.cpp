/****************************************************************************
**
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

#include <QtTest/QtTest>

#include <QDebug>

#include <qbluetoothsocket.h>
#include <qbluetoothdeviceinfo.h>
#include <qbluetoothserviceinfo.h>
#include <qbluetoothservicediscoveryagent.h>
#include <qbluetoothlocaldevice.h>

QT_USE_NAMESPACE

Q_DECLARE_METATYPE(QBluetoothSocket::SocketState)
Q_DECLARE_METATYPE(QBluetoothSocket::SocketError)
Q_DECLARE_METATYPE(QBluetoothServiceInfo::Protocol)

//same uuid as tests/bttestui
#define TEST_SERVICE_UUID "e8e10f95-1a70-4b27-9ccf-02010264e9c8"

// Max time to wait for connection

static const int MaxConnectTime = 60 * 1000;   // 1 minute in ms
static const int MaxReadWriteTime = 60 * 1000; // 1 minute in ms

class tst_QBluetoothSocket : public QObject
{
    Q_OBJECT

public:
    enum ClientConnectionShutdown {
        Error,
        Disconnect,
        Close,
        Abort,
    };

    tst_QBluetoothSocket();
    ~tst_QBluetoothSocket();

private slots:
    void initTestCase();

    void tst_construction_data();
    void tst_construction();

    void tst_serviceConnection();

    void tst_clientCommunication_data();
    void tst_clientCommunication();

    void tst_error();

public slots:
    void serviceDiscovered(const QBluetoothServiceInfo &info);
    void finished();
    void error(QBluetoothServiceDiscoveryAgent::Error error);
private:
    bool done_discovery;
    bool localDeviceFound;
    QBluetoothDeviceInfo remoteDevice;
    QBluetoothHostInfo localDevice;
    QBluetoothServiceInfo remoteServiceInfo;

};

Q_DECLARE_METATYPE(tst_QBluetoothSocket::ClientConnectionShutdown)

tst_QBluetoothSocket::tst_QBluetoothSocket()
{
    qRegisterMetaType<QBluetoothSocket::SocketState>("QBluetoothSocket::SocketState");
    qRegisterMetaType<QBluetoothSocket::SocketError>("QBluetoothSocket::SocketError");

    localDeviceFound = false; // true if we have a local adapter
    done_discovery = false; //true if we found remote device

    //Enable logging to facilitate debugging in case of error
    QLoggingCategory::setFilterRules(QStringLiteral("qt.bluetooth* = true"));
}

tst_QBluetoothSocket::~tst_QBluetoothSocket()
{
}

void tst_QBluetoothSocket::initTestCase()
{
    // setup Bluetooth env
    const QList<QBluetoothHostInfo> foundDevices = QBluetoothLocalDevice::allDevices();
    if (!foundDevices.count()) {
        qWarning() << "Missing local device";
        return;
    } else {
        localDevice = foundDevices.at(0);
        qDebug() << "Local device" << localDevice.name() << localDevice.address().toString();
    }

    localDeviceFound = true;

    //start the device
    QBluetoothLocalDevice device(localDevice.address());
    device.powerOn();


    //find the remote test server
    //the remote test server is tests/bttestui

    // Go find the server
    QBluetoothServiceDiscoveryAgent *sda = new QBluetoothServiceDiscoveryAgent(this);
    connect(sda, SIGNAL(serviceDiscovered(QBluetoothServiceInfo)), this, SLOT(serviceDiscovered(QBluetoothServiceInfo)));
    connect(sda, SIGNAL(error(QBluetoothServiceDiscoveryAgent::Error)), this, SLOT(error(QBluetoothServiceDiscoveryAgent::Error)));
    connect(sda, SIGNAL(finished()), this, SLOT(finished()));

    qDebug() << "Starting discovery";

    sda->setUuidFilter(QBluetoothUuid(QString(TEST_SERVICE_UUID)));
    sda->start(QBluetoothServiceDiscoveryAgent::MinimalDiscovery);

    for (int connectTime = MaxConnectTime; !done_discovery && connectTime > 0; connectTime -= 1000)
        QTest::qWait(1000);

    sda->stop();

    if (!remoteDevice.isValid()) {
        qWarning() << "#########################";
        qWarning() << "Unable to find test service";
        qWarning() << "Remote device may have to be changed into Discoverable mode";
        qWarning() << "#########################";
    }

    delete sda;
}

void tst_QBluetoothSocket::error(QBluetoothServiceDiscoveryAgent::Error error)
{
    qDebug() << "Received error" << error;
    done_discovery = true;
}

void tst_QBluetoothSocket::finished()
{
    qDebug() << "Finished";
    done_discovery = true;
}

void tst_QBluetoothSocket::serviceDiscovered(const QBluetoothServiceInfo &info)
{
    qDebug() << "Found test service on:" << info.device().name() << info.device().address().toString();
    remoteDevice = info.device();
    remoteServiceInfo = info;
    done_discovery = true;
}

void tst_QBluetoothSocket::tst_construction_data()
{
    QTest::addColumn<QBluetoothServiceInfo::Protocol>("socketType");

    QTest::newRow("unknown protocol") << QBluetoothServiceInfo::UnknownProtocol;
    QTest::newRow("rfcomm socket") << QBluetoothServiceInfo::RfcommProtocol;
    QTest::newRow("l2cap socket") << QBluetoothServiceInfo::L2capProtocol;
}

void tst_QBluetoothSocket::tst_construction()
{
    QFETCH(QBluetoothServiceInfo::Protocol, socketType);

    {
        QBluetoothSocket socket;

        QCOMPARE(socket.socketType(), QBluetoothServiceInfo::UnknownProtocol);
        QCOMPARE(socket.error(), QBluetoothSocket::NoSocketError);
        QCOMPARE(socket.errorString(), QString());
        QCOMPARE(socket.peerAddress(), QBluetoothAddress());
        QCOMPARE(socket.peerName(), QString());
        QCOMPARE(socket.peerPort(), quint16(0));
        QCOMPARE(socket.state(), QBluetoothSocket::UnconnectedState);
        QCOMPARE(socket.socketDescriptor(), -1);
        QCOMPARE(socket.bytesAvailable(), 0);
        QCOMPARE(socket.bytesToWrite(), 0);
        QCOMPARE(socket.canReadLine(), false);
        QCOMPARE(socket.isSequential(), true);
        QCOMPARE(socket.atEnd(), true);
        QCOMPARE(socket.pos(), 0);
        QCOMPARE(socket.size(), 0);
        QCOMPARE(socket.isOpen(), false);
        QCOMPARE(socket.isReadable(), false);
        QCOMPARE(socket.isWritable(), false);
        QCOMPARE(socket.openMode(), QIODevice::NotOpen);

        QByteArray array = socket.readAll();
        QVERIFY(array.isEmpty());

        char buffer[10];
        int returnValue = socket.read((char*)&buffer, 10);
        QCOMPARE(returnValue, -1);
    }

    {
        QBluetoothSocket socket(socketType);
        QCOMPARE(socket.socketType(), socketType);
    }
}

void tst_QBluetoothSocket::tst_serviceConnection()
{
    if (!remoteServiceInfo.isValid())
        QSKIP("Remote service not found");

    /* Construction */
    QBluetoothSocket socket;

    QSignalSpy stateSpy(&socket, SIGNAL(stateChanged(QBluetoothSocket::SocketState)));

    QCOMPARE(socket.socketType(), QBluetoothServiceInfo::UnknownProtocol);
    QCOMPARE(socket.state(), QBluetoothSocket::UnconnectedState);

    /* Connection */
    QSignalSpy connectedSpy(&socket, SIGNAL(connected()));
    QSignalSpy errorSpy(&socket, SIGNAL(error(QBluetoothSocket::SocketError)));

    QCOMPARE(socket.openMode(), QIODevice::NotOpen);
    QCOMPARE(socket.isWritable(), false);
    QCOMPARE(socket.isReadable(), false);
    QCOMPARE(socket.isOpen(), false);

    socket.connectToService(remoteServiceInfo);

    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(stateSpy.takeFirst().at(0).value<QBluetoothSocket::SocketState>(), QBluetoothSocket::ConnectingState);
    QCOMPARE(socket.state(), QBluetoothSocket::ConnectingState);

    stateSpy.clear();

    int connectTime = MaxConnectTime;
    while (connectedSpy.count() == 0 && errorSpy.count() == 0 && connectTime > 0) {
        QTest::qWait(1000);
        connectTime -= 1000;
    }

    if (errorSpy.count() != 0) {
        qDebug() << errorSpy.takeFirst().at(0).toInt();
        QSKIP("Connection error");
    }
    QCOMPARE(connectedSpy.count(), 1);
    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(stateSpy.takeFirst().at(0).value<QBluetoothSocket::SocketState>(), QBluetoothSocket::ConnectedState);
    QCOMPARE(socket.state(), QBluetoothSocket::ConnectedState);

    QCOMPARE(socket.isWritable(), true);
    QCOMPARE(socket.isReadable(), true);
    QCOMPARE(socket.isOpen(), true);

    stateSpy.clear();

    //check the peer & local info
    QCOMPARE(socket.localAddress(), localDevice.address());
    QCOMPARE(socket.localName(), localDevice.name());
    QCOMPARE(socket.peerName(), remoteDevice.name());
    QCOMPARE(socket.peerAddress(), remoteDevice.address());

    /* Disconnection */
    QSignalSpy disconnectedSpy(&socket, SIGNAL(disconnected()));

    socket.abort(); // close() tested by other functions
    QCOMPARE(socket.isWritable(), false);
    QCOMPARE(socket.isReadable(), false);
    QCOMPARE(socket.isOpen(), false);
    QCOMPARE(socket.openMode(), QIODevice::NotOpen);

    QVERIFY(stateSpy.count() >= 1);
    QCOMPARE(stateSpy.takeFirst().at(0).value<QBluetoothSocket::SocketState>(), QBluetoothSocket::ClosingState);

    int disconnectTime = MaxConnectTime;
    while (disconnectedSpy.count() == 0 && disconnectTime > 0) {
        QTest::qWait(1000);
        disconnectTime -= 1000;
    }

    QCOMPARE(disconnectedSpy.count(), 1);
    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(stateSpy.takeFirst().at(0).value<QBluetoothSocket::SocketState>(), QBluetoothSocket::UnconnectedState);

    // The remote service needs time to close the connection and resume listening
    QTest::qSleep(100);
}

void tst_QBluetoothSocket::tst_clientCommunication_data()
{
    QTest::addColumn<QStringList>("data");

    {
        QStringList data;
        data << QStringLiteral("Echo: Test line one.\n");
        data << QStringLiteral("Echo: Test line two, with longer data.\n");

        QTest::newRow("two line test") << data;
    }
}

void tst_QBluetoothSocket::tst_clientCommunication()
{
    if (!remoteServiceInfo.isValid())
        QSKIP("Remote service not found");
    QFETCH(QStringList, data);

    /* Construction */
    QBluetoothSocket socket(QBluetoothServiceInfo::RfcommProtocol);

    QSignalSpy stateSpy(&socket, SIGNAL(stateChanged(QBluetoothSocket::SocketState)));

    QCOMPARE(socket.socketType(), QBluetoothServiceInfo::RfcommProtocol);
    QCOMPARE(socket.state(), QBluetoothSocket::UnconnectedState);

    /* Connection */
    QSignalSpy connectedSpy(&socket, SIGNAL(connected()));

    QCOMPARE(socket.isWritable(), false);
    QCOMPARE(socket.isReadable(), false);
    QCOMPARE(socket.isOpen(), false);
    QCOMPARE(socket.openMode(), QIODevice::NotOpen);
    socket.connectToService(remoteServiceInfo);

    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(qvariant_cast<QBluetoothSocket::SocketState>(stateSpy.takeFirst().at(0)), QBluetoothSocket::ConnectingState);
    QCOMPARE(socket.state(), QBluetoothSocket::ConnectingState);

    stateSpy.clear();

    int connectTime = MaxConnectTime;
    while (connectedSpy.count() == 0 && connectTime > 0) {
        QTest::qWait(1000);
        connectTime -= 1000;
    }

    QCOMPARE(socket.isWritable(), true);
    QCOMPARE(socket.isReadable(), true);
    QCOMPARE(socket.isOpen(), true);

    QCOMPARE(connectedSpy.count(), 1);
    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(qvariant_cast<QBluetoothSocket::SocketState>(stateSpy.takeFirst().at(0)), QBluetoothSocket::ConnectedState);
    QCOMPARE(socket.state(), QBluetoothSocket::ConnectedState);

    stateSpy.clear();

    /* Read / Write */
    QCOMPARE(socket.bytesToWrite(), qint64(0));
    QCOMPARE(socket.bytesAvailable(), qint64(0));

    {
        /* Send line by line with event loop */
        foreach (const QString &line, data) {
            QSignalSpy readyReadSpy(&socket, SIGNAL(readyRead()));
            QSignalSpy bytesWrittenSpy(&socket, SIGNAL(bytesWritten(qint64)));

            qint64 dataWritten = socket.write(line.toUtf8());

            if (socket.openMode() & QIODevice::Unbuffered)
                QCOMPARE(socket.bytesToWrite(), qint64(0));
            else
                QCOMPARE(socket.bytesToWrite(), qint64(line.length()));

            QCOMPARE(dataWritten, qint64(line.length()));

            int readWriteTime = MaxReadWriteTime;
            while ((bytesWrittenSpy.count() == 0 || readyReadSpy.count() == 0) && readWriteTime > 0) {
                QTest::qWait(1000);
                readWriteTime -= 1000;
            }

            QCOMPARE(bytesWrittenSpy.count(), 1);
            QCOMPARE(bytesWrittenSpy.at(0).at(0).toLongLong(), qint64(line.length()));

            readWriteTime = MaxReadWriteTime;
            while ((readyReadSpy.count() == 0) && readWriteTime > 0) {
                QTest::qWait(1000);
                readWriteTime -= 1000;
            }

            QCOMPARE(readyReadSpy.count(), 1);

            if (socket.openMode() & QIODevice::Unbuffered)
                QVERIFY(socket.bytesAvailable() <= qint64(line.length()));
            else
                QCOMPARE(socket.bytesAvailable(), qint64(line.length()));

            QVERIFY(socket.canReadLine());

            QByteArray echoed = socket.readAll();

            QCOMPARE(line.toUtf8(), echoed);
        }
    }

    QCOMPARE(socket.isWritable(), true);
    QCOMPARE(socket.isReadable(), true);
    QCOMPARE(socket.isOpen(), true);

    {
        /* Send all at once */
        QSignalSpy readyReadSpy(&socket, SIGNAL(readyRead()));
        QSignalSpy bytesWrittenSpy(&socket, SIGNAL(bytesWritten(qint64)));

        QString joined = data.join(QString());
        qint64 dataWritten = socket.write(joined.toUtf8());

        if (socket.openMode() & QIODevice::Unbuffered)
            QCOMPARE(socket.bytesToWrite(), qint64(0));
        else
            QCOMPARE(socket.bytesToWrite(), qint64(joined.length()));

        QCOMPARE(dataWritten, qint64(joined.length()));

        int readWriteTime = MaxReadWriteTime;
        while ((bytesWrittenSpy.count() == 0 || readyReadSpy.count() == 0) && readWriteTime > 0) {
            QTest::qWait(1000);
            readWriteTime -= 1000;
        }

        QCOMPARE(bytesWrittenSpy.count(), 1);
        QCOMPARE(bytesWrittenSpy.at(0).at(0).toLongLong(), qint64(joined.length()));
        QVERIFY(readyReadSpy.count() > 0);

        if (socket.openMode() & QIODevice::Unbuffered)
            QVERIFY(socket.bytesAvailable() <= qint64(joined.length()));
        else
            QCOMPARE(socket.bytesAvailable(), qint64(joined.length()));

        QVERIFY(socket.canReadLine());

        QByteArray echoed = socket.readAll();

        QCOMPARE(joined.toUtf8(), echoed);
    }

    /* Disconnection */
    QSignalSpy disconnectedSpy(&socket, SIGNAL(disconnected()));

    socket.disconnectFromService();

    QCOMPARE(socket.isWritable(), false);
    QCOMPARE(socket.isReadable(), false);
    QCOMPARE(socket.isOpen(), false);
    QCOMPARE(socket.openMode(), QIODevice::NotOpen);

    int disconnectTime = MaxConnectTime;
    while (disconnectedSpy.count() == 0 && disconnectTime > 0) {
        QTest::qWait(1000);
        disconnectTime -= 1000;
    }

    QCOMPARE(disconnectedSpy.count(), 1);
    QCOMPARE(stateSpy.count(), 2);
    QCOMPARE(qvariant_cast<QBluetoothSocket::SocketState>(stateSpy.takeFirst().at(0)), QBluetoothSocket::ClosingState);
    QCOMPARE(qvariant_cast<QBluetoothSocket::SocketState>(stateSpy.takeFirst().at(0)), QBluetoothSocket::UnconnectedState);

    // The remote service needs time to close the connection and resume listening
    QTest::qSleep(100);
}

void tst_QBluetoothSocket::tst_error()
{
    QBluetoothSocket socket;
    QSignalSpy errorSpy(&socket, SIGNAL(error(QBluetoothSocket::SocketError)));
    QCOMPARE(errorSpy.count(), 0);
    const QBluetoothSocket::SocketError e = socket.error();

    QVERIFY(e == QBluetoothSocket::NoSocketError);

    QVERIFY(socket.errorString() == QString());
}

QTEST_MAIN(tst_QBluetoothSocket)

#include "tst_qbluetoothsocket.moc"

