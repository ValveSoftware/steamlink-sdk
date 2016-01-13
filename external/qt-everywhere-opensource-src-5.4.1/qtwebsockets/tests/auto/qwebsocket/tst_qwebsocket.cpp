/****************************************************************************
**
** Copyright (C) 2014 Kurt Pattyn <pattyn.kurt@gmail.com>.
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
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
#include <QString>
#include <QtTest>
#include <QtWebSockets/QWebSocket>
#include <QtWebSockets/QWebSocketServer>
#include <QtWebSockets/qwebsocketprotocol.h>

QT_USE_NAMESPACE

Q_DECLARE_METATYPE(QWebSocketProtocol::Version)

class EchoServer : public QObject
{
    Q_OBJECT
public:
    explicit EchoServer(QObject *parent = Q_NULLPTR);
    ~EchoServer();

    QHostAddress hostAddress() const { return m_pWebSocketServer->serverAddress(); }
    quint16 port() const { return m_pWebSocketServer->serverPort(); }

Q_SIGNALS:
    void closed();

private Q_SLOTS:
    void onNewConnection();
    void processTextMessage(QString message);
    void processBinaryMessage(QByteArray message);
    void socketDisconnected();

private:
    QWebSocketServer *m_pWebSocketServer;
    QList<QWebSocket *> m_clients;
};

EchoServer::EchoServer(QObject *parent) :
    QObject(parent),
    m_pWebSocketServer(new QWebSocketServer(QStringLiteral("Echo Server"),
                                            QWebSocketServer::NonSecureMode, this)),
    m_clients()
{
    if (m_pWebSocketServer->listen()) {
        connect(m_pWebSocketServer, &QWebSocketServer::newConnection,
                this, &EchoServer::onNewConnection);
        connect(m_pWebSocketServer, &QWebSocketServer::closed, this, &EchoServer::closed);
    }
}

EchoServer::~EchoServer()
{
    m_pWebSocketServer->close();
    qDeleteAll(m_clients.begin(), m_clients.end());
}

void EchoServer::onNewConnection()
{
    QWebSocket *pSocket = m_pWebSocketServer->nextPendingConnection();

    connect(pSocket, &QWebSocket::textMessageReceived, this, &EchoServer::processTextMessage);
    connect(pSocket, &QWebSocket::binaryMessageReceived, this, &EchoServer::processBinaryMessage);
    connect(pSocket, &QWebSocket::disconnected, this, &EchoServer::socketDisconnected);

    m_clients << pSocket;
}

void EchoServer::processTextMessage(QString message)
{
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (pClient) {
        pClient->sendTextMessage(message);
    }
}

void EchoServer::processBinaryMessage(QByteArray message)
{
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (pClient) {
        pClient->sendBinaryMessage(message);
    }
}

void EchoServer::socketDisconnected()
{
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (pClient) {
        m_clients.removeAll(pClient);
        pClient->deleteLater();
    }
}

class tst_QWebSocket : public QObject
{
    Q_OBJECT

public:
    tst_QWebSocket();

private Q_SLOTS:
    void init();
    void initTestCase();
    void cleanupTestCase();
    void tst_initialisation_data();
    void tst_initialisation();
    void tst_settersAndGetters();
    void tst_invalidOpen_data();
    void tst_invalidOpen();
    void tst_invalidOrigin();
    void tst_sendTextMessage();
    void tst_sendBinaryMessage();
    void tst_errorString();
#ifndef QT_NO_NETWORKPROXY
    void tst_setProxy();
#endif
};

tst_QWebSocket::tst_QWebSocket()
{
}

void tst_QWebSocket::init()
{
    qRegisterMetaType<QWebSocketProtocol::Version>("QWebSocketProtocol::Version");
}

void tst_QWebSocket::initTestCase()
{
}

void tst_QWebSocket::cleanupTestCase()
{
}

void tst_QWebSocket::tst_initialisation_data()
{
    QTest::addColumn<QString>("origin");
    QTest::addColumn<QString>("expectedOrigin");
    QTest::addColumn<QWebSocketProtocol::Version>("version");
    QTest::addColumn<QWebSocketProtocol::Version>("expectedVersion");

    QTest::newRow("Default origin and version")
            << QString() << QString()
            << QWebSocketProtocol::VersionUnknown << QWebSocketProtocol::VersionLatest;
    QTest::newRow("Specific origin and default version")
            << QStringLiteral("qt-project.org") << QStringLiteral("qt-project.org")
            << QWebSocketProtocol::VersionUnknown << QWebSocketProtocol::VersionLatest;
    QTest::newRow("Specific origin and specific version")
            << QStringLiteral("qt-project.org") << QStringLiteral("qt-project.org")
            << QWebSocketProtocol::Version7 << QWebSocketProtocol::Version7;
}

void tst_QWebSocket::tst_initialisation()
{
    QFETCH(QString, origin);
    QFETCH(QString, expectedOrigin);
    QFETCH(QWebSocketProtocol::Version, version);
    QFETCH(QWebSocketProtocol::Version, expectedVersion);

    QScopedPointer<QWebSocket> socket;

    if (origin.isEmpty() && (version == QWebSocketProtocol::VersionUnknown))
        socket.reset(new QWebSocket);
    else if (!origin.isEmpty() && (version == QWebSocketProtocol::VersionUnknown))
        socket.reset(new QWebSocket(origin));
    else
        socket.reset(new QWebSocket(origin, version));

    QCOMPARE(socket->origin(), expectedOrigin);
    QCOMPARE(socket->version(), expectedVersion);
    QCOMPARE(socket->error(), QAbstractSocket::UnknownSocketError);
    QVERIFY(socket->errorString().isEmpty());
    QVERIFY(!socket->isValid());
    QVERIFY(socket->localAddress().isNull());
    QCOMPARE(socket->localPort(), quint16(0));
    QCOMPARE(socket->pauseMode(), QAbstractSocket::PauseNever);
    QVERIFY(socket->peerAddress().isNull());
    QCOMPARE(socket->peerPort(), quint16(0));
    QVERIFY(socket->peerName().isEmpty());
    QCOMPARE(socket->state(), QAbstractSocket::UnconnectedState);
    QCOMPARE(socket->readBufferSize(), 0);
    QVERIFY(socket->resourceName().isEmpty());
    QVERIFY(!socket->requestUrl().isValid());
    QCOMPARE(socket->closeCode(), QWebSocketProtocol::CloseCodeNormal);
    QVERIFY(socket->closeReason().isEmpty());
    QVERIFY(socket->flush());
    QCOMPARE(socket->sendTextMessage(QStringLiteral("A text message")), 0);
    QCOMPARE(socket->sendBinaryMessage(QByteArrayLiteral("A binary message")), 0);
}

void tst_QWebSocket::tst_settersAndGetters()
{
    QWebSocket socket;

    socket.setPauseMode(QAbstractSocket::PauseNever);
    QCOMPARE(socket.pauseMode(), QAbstractSocket::PauseNever);
    socket.setPauseMode(QAbstractSocket::PauseOnSslErrors);
    QCOMPARE(socket.pauseMode(), QAbstractSocket::PauseOnSslErrors);

    socket.setReadBufferSize(0);
    QCOMPARE(socket.readBufferSize(), 0);
    socket.setReadBufferSize(128);
    QCOMPARE(socket.readBufferSize(), 128);
    socket.setReadBufferSize(-1);
    QCOMPARE(socket.readBufferSize(), -1);
}

void tst_QWebSocket::tst_invalidOpen_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QString>("expectedUrl");
    QTest::addColumn<QString>("expectedPeerName");
    QTest::addColumn<QString>("expectedResourceName");
    QTest::addColumn<QAbstractSocket::SocketState>("stateAfterOpenCall");
    QTest::addColumn<int>("disconnectedCount");
    QTest::addColumn<int>("stateChangedCount");

    QTest::newRow("Illegal local address")
            << QStringLiteral("ws://127.0.0.1:1/") << QStringLiteral("ws://127.0.0.1:1/")
            << QStringLiteral("127.0.0.1")
            << QStringLiteral("/") << QAbstractSocket::ConnectingState
            << 1
            << 2;  //going from connecting to disconnected
    QTest::newRow("URL containing new line in the hostname")
            << QStringLiteral("ws://myhacky\r\nserver/") << QString()
            << QString()
            << QString() << QAbstractSocket::UnconnectedState
            << 0 << 0;
    QTest::newRow("URL containing new line in the resource name")
            << QStringLiteral("ws://127.0.0.1:1/tricky\r\npath") << QString()
            << QString()
            << QString()
            << QAbstractSocket::UnconnectedState
            << 0 << 0;
}

void tst_QWebSocket::tst_invalidOpen()
{
    QFETCH(QString, url);
    QFETCH(QString, expectedUrl);
    QFETCH(QString, expectedPeerName);
    QFETCH(QString, expectedResourceName);
    QFETCH(QAbstractSocket::SocketState, stateAfterOpenCall);
    QFETCH(int, disconnectedCount);
    QFETCH(int, stateChangedCount);
    QWebSocket socket;
    QSignalSpy errorSpy(&socket, SIGNAL(error(QAbstractSocket::SocketError)));
    QSignalSpy aboutToCloseSpy(&socket, SIGNAL(aboutToClose()));
    QSignalSpy connectedSpy(&socket, SIGNAL(connected()));
    QSignalSpy disconnectedSpy(&socket, SIGNAL(disconnected()));
    QSignalSpy stateChangedSpy(&socket, SIGNAL(stateChanged(QAbstractSocket::SocketState)));
    QSignalSpy readChannelFinishedSpy(&socket, SIGNAL(readChannelFinished()));
    QSignalSpy textFrameReceivedSpy(&socket, SIGNAL(textFrameReceived(QString,bool)));
    QSignalSpy binaryFrameReceivedSpy(&socket, SIGNAL(binaryFrameReceived(QByteArray,bool)));
    QSignalSpy textMessageReceivedSpy(&socket, SIGNAL(textMessageReceived(QString)));
    QSignalSpy binaryMessageReceivedSpy(&socket, SIGNAL(binaryMessageReceived(QByteArray)));
    QSignalSpy pongSpy(&socket, SIGNAL(pong(quint64,QByteArray)));
    QSignalSpy bytesWrittenSpy(&socket, SIGNAL(bytesWritten(qint64)));

    socket.open(QUrl(url));

    QVERIFY(socket.origin().isEmpty());
    QCOMPARE(socket.version(), QWebSocketProtocol::VersionLatest);
    //at this point the socket is in a connecting state
    //so, there should no error at this point
    QCOMPARE(socket.error(), QAbstractSocket::UnknownSocketError);
    QVERIFY(!socket.errorString().isEmpty());
    QVERIFY(!socket.isValid());
    QVERIFY(socket.localAddress().isNull());
    QCOMPARE(socket.localPort(), quint16(0));
    QCOMPARE(socket.pauseMode(), QAbstractSocket::PauseNever);
    QVERIFY(socket.peerAddress().isNull());
    QCOMPARE(socket.peerPort(), quint16(0));
    QCOMPARE(socket.peerName(), expectedPeerName);
    QCOMPARE(socket.state(), stateAfterOpenCall);
    QCOMPARE(socket.readBufferSize(), 0);
    QCOMPARE(socket.resourceName(), expectedResourceName);
    QCOMPARE(socket.requestUrl().toString(), expectedUrl);
    QCOMPARE(socket.closeCode(), QWebSocketProtocol::CloseCodeNormal);
    QVERIFY(socket.closeReason().isEmpty());
    QCOMPARE(socket.sendTextMessage(QStringLiteral("A text message")), 0);
    QCOMPARE(socket.sendBinaryMessage(QByteArrayLiteral("A text message")), 0);

    if (errorSpy.count() == 0)
        QVERIFY(errorSpy.wait());
    QCOMPARE(errorSpy.count(), 1);
    QList<QVariant> arguments = errorSpy.takeFirst();
    QAbstractSocket::SocketError socketError =
            qvariant_cast<QAbstractSocket::SocketError>(arguments.at(0));
    QCOMPARE(socketError, QAbstractSocket::ConnectionRefusedError);
    QCOMPARE(aboutToCloseSpy.count(), 0);
    QCOMPARE(connectedSpy.count(), 0);
    QCOMPARE(disconnectedSpy.count(), disconnectedCount);
    QCOMPARE(stateChangedSpy.count(), stateChangedCount);
    if (stateChangedCount == 2) {
        arguments = stateChangedSpy.takeFirst();
        QAbstractSocket::SocketState socketState =
                qvariant_cast<QAbstractSocket::SocketState>(arguments.at(0));
        arguments = stateChangedSpy.takeFirst();
        socketState = qvariant_cast<QAbstractSocket::SocketState>(arguments.at(0));
        QCOMPARE(socketState, QAbstractSocket::UnconnectedState);
    }
    QCOMPARE(readChannelFinishedSpy.count(), 0);
    QCOMPARE(textFrameReceivedSpy.count(), 0);
    QCOMPARE(binaryFrameReceivedSpy.count(), 0);
    QCOMPARE(textMessageReceivedSpy.count(), 0);
    QCOMPARE(binaryMessageReceivedSpy.count(), 0);
    QCOMPARE(pongSpy.count(), 0);
    QCOMPARE(bytesWrittenSpy.count(), 0);
}

void tst_QWebSocket::tst_invalidOrigin()
{
    QWebSocket socket(QStringLiteral("My server\r\nin the wild."));

    QSignalSpy errorSpy(&socket, SIGNAL(error(QAbstractSocket::SocketError)));
    QSignalSpy aboutToCloseSpy(&socket, SIGNAL(aboutToClose()));
    QSignalSpy connectedSpy(&socket, SIGNAL(connected()));
    QSignalSpy disconnectedSpy(&socket, SIGNAL(disconnected()));
    QSignalSpy stateChangedSpy(&socket, SIGNAL(stateChanged(QAbstractSocket::SocketState)));
    QSignalSpy readChannelFinishedSpy(&socket, SIGNAL(readChannelFinished()));
    QSignalSpy textFrameReceivedSpy(&socket, SIGNAL(textFrameReceived(QString,bool)));
    QSignalSpy binaryFrameReceivedSpy(&socket, SIGNAL(binaryFrameReceived(QByteArray,bool)));
    QSignalSpy textMessageReceivedSpy(&socket, SIGNAL(textMessageReceived(QString)));
    QSignalSpy binaryMessageReceivedSpy(&socket, SIGNAL(binaryMessageReceived(QByteArray)));
    QSignalSpy pongSpy(&socket, SIGNAL(pong(quint64,QByteArray)));
    QSignalSpy bytesWrittenSpy(&socket, SIGNAL(bytesWritten(qint64)));

    socket.open(QUrl(QStringLiteral("ws://127.0.0.1:1/")));

    //at this point the socket is in a connecting state
    //so, there should no error at this point
    QCOMPARE(socket.error(), QAbstractSocket::UnknownSocketError);
    QVERIFY(!socket.errorString().isEmpty());
    QVERIFY(!socket.isValid());
    QVERIFY(socket.localAddress().isNull());
    QCOMPARE(socket.localPort(), quint16(0));
    QCOMPARE(socket.pauseMode(), QAbstractSocket::PauseNever);
    QVERIFY(socket.peerAddress().isNull());
    QCOMPARE(socket.peerPort(), quint16(0));
    QCOMPARE(socket.peerName(), QStringLiteral("127.0.0.1"));
    QCOMPARE(socket.state(), QAbstractSocket::ConnectingState);
    QCOMPARE(socket.readBufferSize(), 0);
    QCOMPARE(socket.resourceName(), QStringLiteral("/"));
    QCOMPARE(socket.requestUrl(), QUrl(QStringLiteral("ws://127.0.0.1:1/")));
    QCOMPARE(socket.closeCode(), QWebSocketProtocol::CloseCodeNormal);

    QVERIFY(errorSpy.wait());

    QCOMPARE(errorSpy.count(), 1);
    QList<QVariant> arguments = errorSpy.takeFirst();
    QAbstractSocket::SocketError socketError =
            qvariant_cast<QAbstractSocket::SocketError>(arguments.at(0));
    QCOMPARE(socketError, QAbstractSocket::ConnectionRefusedError);
    QCOMPARE(aboutToCloseSpy.count(), 0);
    QCOMPARE(connectedSpy.count(), 0);
    QCOMPARE(disconnectedSpy.count(), 1);
    QCOMPARE(stateChangedSpy.count(), 2);   //connectingstate, unconnectedstate
    arguments = stateChangedSpy.takeFirst();
    QAbstractSocket::SocketState socketState =
            qvariant_cast<QAbstractSocket::SocketState>(arguments.at(0));
    arguments = stateChangedSpy.takeFirst();
    socketState = qvariant_cast<QAbstractSocket::SocketState>(arguments.at(0));
    QCOMPARE(socketState, QAbstractSocket::UnconnectedState);
    QCOMPARE(readChannelFinishedSpy.count(), 0);
    QCOMPARE(textFrameReceivedSpy.count(), 0);
    QCOMPARE(binaryFrameReceivedSpy.count(), 0);
    QCOMPARE(textMessageReceivedSpy.count(), 0);
    QCOMPARE(binaryMessageReceivedSpy.count(), 0);
    QCOMPARE(pongSpy.count(), 0);
    QCOMPARE(bytesWrittenSpy.count(), 0);
}

void tst_QWebSocket::tst_sendTextMessage()
{
    //TODO: will resolve in another commit
#ifndef Q_OS_WIN
    EchoServer echoServer;

    QWebSocket socket;

    //should return 0 because socket is not open yet
    QCOMPARE(socket.sendTextMessage(QStringLiteral("1234")), 0);

    QSignalSpy socketConnectedSpy(&socket, SIGNAL(connected()));
    QSignalSpy textMessageReceived(&socket, SIGNAL(textMessageReceived(QString)));
    QSignalSpy textFrameReceived(&socket, SIGNAL(textFrameReceived(QString,bool)));
    QSignalSpy binaryMessageReceived(&socket, SIGNAL(binaryMessageReceived(QByteArray)));
    QSignalSpy binaryFrameReceived(&socket, SIGNAL(binaryFrameReceived(QByteArray,bool)));

    socket.open(QUrl(QStringLiteral("ws://") + echoServer.hostAddress().toString() +
                     QStringLiteral(":") + QString::number(echoServer.port())));

    if (socketConnectedSpy.count() == 0)
        QVERIFY(socketConnectedSpy.wait(500));
    QCOMPARE(socket.state(), QAbstractSocket::ConnectedState);

    socket.sendTextMessage(QStringLiteral("Hello world!"));

    QVERIFY(textMessageReceived.wait(500));
    QCOMPARE(textMessageReceived.count(), 1);
    QCOMPARE(binaryMessageReceived.count(), 0);
    QCOMPARE(binaryFrameReceived.count(), 0);
    QList<QVariant> arguments = textMessageReceived.takeFirst();
    QString messageReceived = arguments.at(0).toString();
    QCOMPARE(messageReceived, QStringLiteral("Hello world!"));

    QCOMPARE(textFrameReceived.count(), 1);
    arguments = textFrameReceived.takeFirst();
    QString frameReceived = arguments.at(0).toString();
    bool isLastFrame = arguments.at(1).toBool();
    QCOMPARE(frameReceived, QStringLiteral("Hello world!"));
    QVERIFY(isLastFrame);

    socket.close();

    //QTBUG-36762: QWebSocket emits multiplied signals when socket was reopened
    socketConnectedSpy.clear();
    textMessageReceived.clear();
    textFrameReceived.clear();

    socket.open(QUrl(QStringLiteral("ws://") + echoServer.hostAddress().toString() +
                     QStringLiteral(":") + QString::number(echoServer.port())));

    if (socketConnectedSpy.count() == 0)
        QVERIFY(socketConnectedSpy.wait(500));
    QCOMPARE(socket.state(), QAbstractSocket::ConnectedState);

    socket.sendTextMessage(QStringLiteral("Hello world!"));

    QVERIFY(textMessageReceived.wait(500));
    QCOMPARE(textMessageReceived.count(), 1);
    QCOMPARE(binaryMessageReceived.count(), 0);
    QCOMPARE(binaryFrameReceived.count(), 0);
    arguments = textMessageReceived.takeFirst();
    messageReceived = arguments.at(0).toString();
    QCOMPARE(messageReceived, QStringLiteral("Hello world!"));

    QCOMPARE(textFrameReceived.count(), 1);
    arguments = textFrameReceived.takeFirst();
    frameReceived = arguments.at(0).toString();
    isLastFrame = arguments.at(1).toBool();
    QCOMPARE(frameReceived, QStringLiteral("Hello world!"));
    QVERIFY(isLastFrame);

    QString reason = QStringLiteral("going away");
    socket.close(QWebSocketProtocol::CloseCodeGoingAway, reason);
    QCOMPARE(socket.closeCode(), QWebSocketProtocol::CloseCodeGoingAway);
    QCOMPARE(socket.closeReason(), reason);
#endif
}

void tst_QWebSocket::tst_sendBinaryMessage()
{
    //TODO: will resolve in another commit
#ifndef Q_OS_WIN
    EchoServer echoServer;

    QWebSocket socket;

    //should return 0 because socket is not open yet
    QCOMPARE(socket.sendBinaryMessage(QByteArrayLiteral("1234")), 0);

    QSignalSpy socketConnectedSpy(&socket, SIGNAL(connected()));
    QSignalSpy textMessageReceived(&socket, SIGNAL(textMessageReceived(QString)));
    QSignalSpy textFrameReceived(&socket, SIGNAL(textFrameReceived(QString,bool)));
    QSignalSpy binaryMessageReceived(&socket, SIGNAL(binaryMessageReceived(QByteArray)));
    QSignalSpy binaryFrameReceived(&socket, SIGNAL(binaryFrameReceived(QByteArray,bool)));

    socket.open(QUrl(QStringLiteral("ws://") + echoServer.hostAddress().toString() +
                     QStringLiteral(":") + QString::number(echoServer.port())));

    if (socketConnectedSpy.count() == 0)
        QVERIFY(socketConnectedSpy.wait(500));
    QCOMPARE(socket.state(), QAbstractSocket::ConnectedState);

    socket.sendBinaryMessage(QByteArrayLiteral("Hello world!"));

    QVERIFY(binaryMessageReceived.wait(500));
    QCOMPARE(textMessageReceived.count(), 0);
    QCOMPARE(textFrameReceived.count(), 0);
    QCOMPARE(binaryMessageReceived.count(), 1);
    QList<QVariant> arguments = binaryMessageReceived.takeFirst();
    QByteArray messageReceived = arguments.at(0).toByteArray();
    QCOMPARE(messageReceived, QByteArrayLiteral("Hello world!"));

    QCOMPARE(binaryFrameReceived.count(), 1);
    arguments = binaryFrameReceived.takeFirst();
    QByteArray frameReceived = arguments.at(0).toByteArray();
    bool isLastFrame = arguments.at(1).toBool();
    QCOMPARE(frameReceived, QByteArrayLiteral("Hello world!"));
    QVERIFY(isLastFrame);

    socket.close();

    //QTBUG-36762: QWebSocket emits multiple signals when socket is reopened
    socketConnectedSpy.clear();
    binaryMessageReceived.clear();
    binaryFrameReceived.clear();

    socket.open(QUrl(QStringLiteral("ws://") + echoServer.hostAddress().toString() +
                     QStringLiteral(":") + QString::number(echoServer.port())));

    if (socketConnectedSpy.count() == 0)
        QVERIFY(socketConnectedSpy.wait(500));
    QCOMPARE(socket.state(), QAbstractSocket::ConnectedState);

    socket.sendBinaryMessage(QByteArrayLiteral("Hello world!"));

    QVERIFY(binaryMessageReceived.wait(500));
    QCOMPARE(textMessageReceived.count(), 0);
    QCOMPARE(textFrameReceived.count(), 0);
    QCOMPARE(binaryMessageReceived.count(), 1);
    arguments = binaryMessageReceived.takeFirst();
    messageReceived = arguments.at(0).toByteArray();
    QCOMPARE(messageReceived, QByteArrayLiteral("Hello world!"));

    QCOMPARE(binaryFrameReceived.count(), 1);
    arguments = binaryFrameReceived.takeFirst();
    frameReceived = arguments.at(0).toByteArray();
    isLastFrame = arguments.at(1).toBool();
    QCOMPARE(frameReceived, QByteArrayLiteral("Hello world!"));
    QVERIFY(isLastFrame);
#endif
}

void tst_QWebSocket::tst_errorString()
{
    //Check for QTBUG-37228: QWebSocket returns "Unknown Error" for known errors
    QWebSocket socket;

    //check that the default error string is empty
    QVERIFY(socket.errorString().isEmpty());

    QSignalSpy errorSpy(&socket, SIGNAL(error(QAbstractSocket::SocketError)));

    socket.open(QUrl(QStringLiteral("ws://someserver.on.mars:9999")));

    if (errorSpy.count() == 0)
        errorSpy.wait();
    QCOMPARE(errorSpy.count(), 1);
    QList<QVariant> arguments = errorSpy.takeFirst();
    QAbstractSocket::SocketError socketError =
            qvariant_cast<QAbstractSocket::SocketError>(arguments.at(0));
    QCOMPARE(socketError, QAbstractSocket::HostNotFoundError);
    QCOMPARE(socket.errorString(), QStringLiteral("Host not found"));
}

#ifndef QT_NO_NETWORKPROXY
void tst_QWebSocket::tst_setProxy()
{
    // check if property assignment works as expected.
    QWebSocket socket;
    QCOMPARE(socket.proxy(), QNetworkProxy(QNetworkProxy::DefaultProxy));

    QNetworkProxy proxy;
    proxy.setPort(123);
    socket.setProxy(proxy);
    QCOMPARE(socket.proxy(), proxy);

    proxy.setPort(321);
    QCOMPARE(socket.proxy().port(), quint16(123));
    socket.setProxy(proxy);
    QCOMPARE(socket.proxy(), proxy);
}
#endif // QT_NO_NETWORKPROXY

QTEST_MAIN(tst_QWebSocket)

#include "tst_qwebsocket.moc"
