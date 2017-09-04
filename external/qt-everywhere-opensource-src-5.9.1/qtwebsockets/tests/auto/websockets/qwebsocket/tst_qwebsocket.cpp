/****************************************************************************
**
** Copyright (C) 2016 Kurt Pattyn <pattyn.kurt@gmail.com>.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
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
    void newConnection(QUrl requestUrl);
    void newConnection(QNetworkRequest request);

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
    if (m_pWebSocketServer->listen(QHostAddress(QStringLiteral("127.0.0.1")))) {
        connect(m_pWebSocketServer, SIGNAL(newConnection()),
                this, SLOT(onNewConnection()));
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

    Q_EMIT newConnection(pSocket->requestUrl());
    Q_EMIT newConnection(pSocket->request());

    connect(pSocket, SIGNAL(textMessageReceived(QString)), this, SLOT(processTextMessage(QString)));
    connect(pSocket, SIGNAL(binaryMessageReceived(QByteArray)), this, SLOT(processBinaryMessage(QByteArray)));
    connect(pSocket, SIGNAL(disconnected()), this, SLOT(socketDisconnected()));

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
    void tst_openRequest();
    void tst_moveToThread();
    void tst_moveToThreadNoWarning();
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
    EchoServer echoServer;

    QWebSocket socket;

    //should return 0 because socket is not open yet
    QCOMPARE(socket.sendTextMessage(QStringLiteral("1234")), 0);

    QSignalSpy socketConnectedSpy(&socket, SIGNAL(connected()));
    QSignalSpy serverConnectedSpy(&echoServer, SIGNAL(newConnection(QUrl)));
    QSignalSpy textMessageReceived(&socket, SIGNAL(textMessageReceived(QString)));
    QSignalSpy textFrameReceived(&socket, SIGNAL(textFrameReceived(QString,bool)));
    QSignalSpy binaryMessageReceived(&socket, SIGNAL(binaryMessageReceived(QByteArray)));
    QSignalSpy binaryFrameReceived(&socket, SIGNAL(binaryFrameReceived(QByteArray,bool)));
    QSignalSpy socketError(&socket, SIGNAL(error(QAbstractSocket::SocketError)));

    QUrl url = QUrl(QStringLiteral("ws://") + echoServer.hostAddress().toString() +
                    QStringLiteral(":") + QString::number(echoServer.port()));
    url.setPath("/segment/with spaces");
    QUrlQuery query;
    query.addQueryItem("queryitem", "with encoded characters");
    url.setQuery(query);

    socket.open(url);

    QTRY_COMPARE(socketConnectedSpy.count(), 1);
    QCOMPARE(socketError.count(), 0);
    QCOMPARE(socket.state(), QAbstractSocket::ConnectedState);
    QList<QVariant> arguments = serverConnectedSpy.takeFirst();
    QUrl urlConnected = arguments.at(0).toUrl();
    QCOMPARE(urlConnected, url);

    socket.sendTextMessage(QStringLiteral("Hello world!"));

    QVERIFY(textMessageReceived.wait(500));
    QCOMPARE(textMessageReceived.count(), 1);
    QCOMPARE(binaryMessageReceived.count(), 0);
    QCOMPARE(binaryFrameReceived.count(), 0);
    arguments = textMessageReceived.takeFirst();
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

    QTRY_COMPARE(socketConnectedSpy.count(), 1);
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
}

void tst_QWebSocket::tst_sendBinaryMessage()
{
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

    QTRY_COMPARE(socketConnectedSpy.count(), 1);
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

    QTRY_COMPARE(socketConnectedSpy.count(), 1);
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
}

void tst_QWebSocket::tst_errorString()
{
    //Check for QTBUG-37228: QWebSocket returns "Unknown Error" for known errors
    QWebSocket socket;

    //check that the default error string is empty
    QVERIFY(socket.errorString().isEmpty());

    QSignalSpy errorSpy(&socket, SIGNAL(error(QAbstractSocket::SocketError)));

    socket.open(QUrl(QStringLiteral("ws://someserver.on.mars:9999")));

    QTRY_COMPARE(errorSpy.count(), 1);
    QList<QVariant> arguments = errorSpy.takeFirst();
    QAbstractSocket::SocketError socketError =
            qvariant_cast<QAbstractSocket::SocketError>(arguments.at(0));
    QCOMPARE(socketError, QAbstractSocket::HostNotFoundError);
    QCOMPARE(socket.errorString(), QStringLiteral("Host not found"));
}

void tst_QWebSocket::tst_openRequest()
{
    EchoServer echoServer;

    QWebSocket socket;

    QSignalSpy socketConnectedSpy(&socket, SIGNAL(connected()));
    QSignalSpy serverRequestSpy(&echoServer, SIGNAL(newConnection(QNetworkRequest)));

    QUrl url = QUrl(QStringLiteral("ws://") + echoServer.hostAddress().toString() +
                    QLatin1Char(':') + QString::number(echoServer.port()));
    QUrlQuery query;
    query.addQueryItem("queryitem", "with encoded characters");
    url.setQuery(query);
    QNetworkRequest req(url);
    req.setRawHeader("X-Custom-Header", "A custom header");
    socket.open(req);

    QTRY_COMPARE(socketConnectedSpy.count(), 1);
    QTRY_COMPARE(serverRequestSpy.count(), 1);
    QCOMPARE(socket.state(), QAbstractSocket::ConnectedState);
    QList<QVariant> arguments = serverRequestSpy.takeFirst();
    QNetworkRequest requestConnected = arguments.at(0).value<QNetworkRequest>();
    QCOMPARE(requestConnected.url(), req.url());
    QCOMPARE(requestConnected.rawHeader("X-Custom-Header"), req.rawHeader("X-Custom-Header"));
    socket.close();
}

class WebSocket : public QWebSocket
{
    Q_OBJECT

public:
    explicit WebSocket()
    {
        connect(this, SIGNAL(triggerClose()), SLOT(onClose()), Qt::QueuedConnection);
        connect(this, SIGNAL(triggerOpen(QUrl)), SLOT(onOpen(QUrl)), Qt::QueuedConnection);
        connect(this, SIGNAL(triggerSendTextMessage(QString)), SLOT(onSendTextMessage(QString)), Qt::QueuedConnection);
        connect(this, SIGNAL(textMessageReceived(QString)), this, SLOT(onTextMessageReceived(QString)), Qt::QueuedConnection);
    }

    void asyncClose() { triggerClose(); }
    void asyncOpen(const QUrl &url) { triggerOpen(url); }
    void asyncSendTextMessage(const QString &msg) { triggerSendTextMessage(msg); }

    QString receivedMessage;

Q_SIGNALS:
    void triggerClose();
    void triggerOpen(const QUrl &);
    void triggerSendTextMessage(const QString &);
    void done();

private Q_SLOTS:
    void onClose() { close(); }
    void onOpen(const QUrl &url) { open(url); }
    void onSendTextMessage(const QString &msg) { sendTextMessage(msg); }
    void onTextMessageReceived(const QString &msg) { receivedMessage = msg; done(); }
};

struct Warned
{
    static QtMessageHandler origHandler;
    static bool warned;
    static void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& str)
    {
        if (type == QtWarningMsg) {
            warned = true;
        }
        if (origHandler)
            origHandler(type, context, str);
    }
};
QtMessageHandler Warned::origHandler = 0;
bool Warned::warned = false;


void tst_QWebSocket::tst_moveToThread()
{
    Warned::origHandler = qInstallMessageHandler(&Warned::messageHandler);

    EchoServer echoServer;

    QThread* thread = new QThread;
    thread->start();

    WebSocket* socket = new WebSocket;
    socket->moveToThread(thread);

    const QString textMessage = QStringLiteral("Hello world!");
    QSignalSpy socketConnectedSpy(socket, SIGNAL(connected()));
    QUrl url = QUrl(QStringLiteral("ws://") + echoServer.hostAddress().toString() +
                    QStringLiteral(":") + QString::number(echoServer.port()));
    url.setPath("/segment/with spaces");
    QUrlQuery query;
    query.addQueryItem("queryitem", "with encoded characters");
    url.setQuery(query);

    socket->asyncOpen(url);
    if (socketConnectedSpy.count() == 0)
        QVERIFY(socketConnectedSpy.wait(500));

    socket->asyncSendTextMessage(textMessage);

    QTimer timer;
    timer.setInterval(1000);
    timer.start();
    QEventLoop loop;
    connect(socket, SIGNAL(done()), &loop, SLOT(quit()));
    connect(socket, SIGNAL(done()), &timer, SLOT(stop()));
    connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
    loop.exec();

    socket->asyncClose();

    QTRY_COMPARE_WITH_TIMEOUT(loop.isRunning(), false, 200);
    QCOMPARE(socket->receivedMessage, textMessage);

    socket->deleteLater();
    thread->quit();
    thread->deleteLater();
}

void tst_QWebSocket::tst_moveToThreadNoWarning()
{
    // check for warnings in tst_moveToThread()
    // couldn't done there because warnings are processed after the test run
    QCOMPARE(Warned::warned, false);
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
