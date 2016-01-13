/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qqmldebugclient.h"
#include "../../../../../src/plugins/qmltooling/shared/qpacketprotocol.h"

#include <QtCore/qdebug.h>
#include <QtCore/qeventloop.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qtimer.h>
#include <QtNetwork/qnetworkproxy.h>

const int protocolVersion = 1;
const QString serverId = QLatin1String("QDeclarativeDebugServer");
const QString clientId = QLatin1String("QDeclarativeDebugClient");

class QQmlDebugClientPrivate
{
public:
    QQmlDebugClientPrivate();

    QString name;
    QQmlDebugConnection *connection;
};

class QQmlDebugConnectionPrivate : public QObject
{
    Q_OBJECT
public:
    QQmlDebugConnectionPrivate(QQmlDebugConnection *c);
    QQmlDebugConnection *q;
    QPacketProtocol *protocol;
    QIODevice *device;
    QEventLoop handshakeEventLoop;
    QTimer handshakeTimer;

    bool gotHello;
    QHash <QString, float> serverPlugins;
    QHash<QString, QQmlDebugClient *> plugins;

    void advertisePlugins();
    void connectDeviceSignals();

public Q_SLOTS:
    void connected();
    void readyRead();
    void deviceAboutToClose();
    void handshakeTimeout();
};

QQmlDebugConnectionPrivate::QQmlDebugConnectionPrivate(QQmlDebugConnection *c)
    : QObject(c), q(c), protocol(0), device(0), gotHello(false)
{
    protocol = new QPacketProtocol(q, this);
    QObject::connect(c, SIGNAL(connected()), this, SLOT(connected()));
    QObject::connect(protocol, SIGNAL(readyRead()), this, SLOT(readyRead()));

    handshakeTimer.setSingleShot(true);
    handshakeTimer.setInterval(3000);
    connect(&handshakeTimer, SIGNAL(timeout()), SLOT(handshakeTimeout()));
}

void QQmlDebugConnectionPrivate::advertisePlugins()
{
    if (!q->isConnected())
        return;

    QPacket pack;
    pack << serverId << 1 << plugins.keys();
    protocol->send(pack);
    q->flush();
}

void QQmlDebugConnectionPrivate::connected()
{
    QPacket pack;
    pack << serverId << 0 << protocolVersion << plugins.keys()
         << q->m_dataStreamVersion;
    protocol->send(pack);
    q->flush();
}

void QQmlDebugConnectionPrivate::readyRead()
{
    if (!gotHello) {
        QPacket pack = protocol->read();
        QString name;

        pack >> name;

        bool validHello = false;
        if (name == clientId) {
            int op = -1;
            pack >> op;
            if (op == 0) {
                int version = -1;
                pack >> version;
                if (version == protocolVersion) {
                    QStringList pluginNames;
                    QList<float> pluginVersions;
                    pack >> pluginNames;
                    if (!pack.isEmpty())
                        pack >> pluginVersions;

                    const int pluginNamesSize = pluginNames.size();
                    const int pluginVersionsSize = pluginVersions.size();
                    for (int i = 0; i < pluginNamesSize; ++i) {
                        float pluginVersion = 1.0;
                        if (i < pluginVersionsSize)
                            pluginVersion = pluginVersions.at(i);
                        serverPlugins.insert(pluginNames.at(i), pluginVersion);
                    }

                    pack >> q->m_dataStreamVersion;
                    validHello = true;
                }
            }
        }

        if (!validHello) {
            qWarning("QQmlDebugConnection: Invalid hello message");
            QObject::disconnect(protocol, SIGNAL(readyRead()), this, SLOT(readyRead()));
            return;
        }
        gotHello = true;

        QHash<QString, QQmlDebugClient *>::Iterator iter = plugins.begin();
        for (; iter != plugins.end(); ++iter) {
            QQmlDebugClient::State newState = QQmlDebugClient::Unavailable;
            if (serverPlugins.contains(iter.key()))
                newState = QQmlDebugClient::Enabled;
            iter.value()->stateChanged(newState);
        }

        handshakeTimer.stop();
        handshakeEventLoop.quit();
    }

    while (protocol->packetsAvailable()) {
        QPacket pack = protocol->read();
        QString name;
        pack >> name;

        if (name == clientId) {
            int op = -1;
            pack >> op;

            if (op == 1) {
                // Service Discovery
                QHash<QString, float> oldServerPlugins = serverPlugins;
                serverPlugins.clear();

                QStringList pluginNames;
                QList<float> pluginVersions;
                pack >> pluginNames;
                if (!pack.isEmpty())
                    pack >> pluginVersions;

                const int pluginNamesSize = pluginNames.size();
                const int pluginVersionsSize = pluginVersions.size();
                for (int i = 0; i < pluginNamesSize; ++i) {
                    float pluginVersion = 1.0;
                    if (i < pluginVersionsSize)
                        pluginVersion = pluginVersions.at(i);
                    serverPlugins.insert(pluginNames.at(i), pluginVersion);
                }

                QHash<QString, QQmlDebugClient *>::Iterator iter = plugins.begin();
                for (; iter != plugins.end(); ++iter) {
                    const QString pluginName = iter.key();
                    QQmlDebugClient::State newSate = QQmlDebugClient::Unavailable;
                    if (serverPlugins.contains(pluginName))
                        newSate = QQmlDebugClient::Enabled;

                    if (oldServerPlugins.contains(pluginName)
                            != serverPlugins.contains(pluginName)) {
                        iter.value()->stateChanged(newSate);
                    }
                }
            } else {
                qWarning() << "QQmlDebugConnection: Unknown control message id" << op;
            }
        } else {
            QByteArray message;
            pack >> message;

            QHash<QString, QQmlDebugClient *>::Iterator iter =
                    plugins.find(name);
            if (iter == plugins.end()) {
                qWarning() << "QQmlDebugConnection: Message received for missing plugin" << name;
            } else {
                (*iter)->messageReceived(message);
            }
        }
    }
}

void QQmlDebugConnectionPrivate::deviceAboutToClose()
{
    // This is nasty syntax but we want to emit our own aboutToClose signal (by calling QIODevice::close())
    // without calling the underlying device close fn as that would cause an infinite loop
    q->QIODevice::close();
}

void QQmlDebugConnectionPrivate::handshakeTimeout()
{
    if (!gotHello) {
        qWarning() << "Qml Debug Client: Did not get handshake answer in time";
        handshakeEventLoop.quit();
    }
}

QQmlDebugConnection::QQmlDebugConnection(QObject *parent)
    : QIODevice(parent), d(new QQmlDebugConnectionPrivate(this)),
      m_dataStreamVersion(QDataStream::Qt_5_0)
{
}

QQmlDebugConnection::~QQmlDebugConnection()
{
    QHash<QString, QQmlDebugClient*>::iterator iter = d->plugins.begin();
    for (; iter != d->plugins.end(); ++iter) {
        iter.value()->d->connection = 0;
        iter.value()->stateChanged(QQmlDebugClient::NotConnected);
    }
}

void QQmlDebugConnection::setDataStreamVersion(int dataStreamVersion)
{
    m_dataStreamVersion = dataStreamVersion;
}

int QQmlDebugConnection::dataStreamVersion()
{
    return m_dataStreamVersion;
}

bool QQmlDebugConnection::isConnected() const
{
    return state() == QAbstractSocket::ConnectedState;
}

qint64 QQmlDebugConnection::readData(char *data, qint64 maxSize)
{
    return d->device->read(data, maxSize);
}

qint64 QQmlDebugConnection::writeData(const char *data, qint64 maxSize)
{
    return d->device->write(data, maxSize);
}

qint64 QQmlDebugConnection::bytesAvailable() const
{
    return d->device->bytesAvailable();
}

bool QQmlDebugConnection::isSequential() const
{
    return true;
}

void QQmlDebugConnection::close()
{
    if (isOpen()) {
        QIODevice::close();
        d->device->close();
        emit stateChanged(QAbstractSocket::UnconnectedState);

        QHash<QString, QQmlDebugClient*>::iterator iter = d->plugins.begin();
        for (; iter != d->plugins.end(); ++iter) {
            iter.value()->stateChanged(QQmlDebugClient::NotConnected);
        }
    }
}

bool QQmlDebugConnection::waitForConnected(int msecs)
{
    QAbstractSocket *socket = qobject_cast<QAbstractSocket*>(d->device);
    if (!socket)
        return false;
    if (!socket->waitForConnected(msecs))
        return false;
    // wait for handshake
    d->handshakeTimer.start();
    d->handshakeEventLoop.exec();
    return d->gotHello;
}

QString QQmlDebugConnection::stateString() const
{
   QString state;

   if (isConnected())
       state = "Connected";
   else
       state = "Not connected";

   if (d->gotHello)
       state += ", got hello";
   else
       state += ", did not get hello!";

   return state;
}

QAbstractSocket::SocketState QQmlDebugConnection::state() const
{
    QAbstractSocket *socket = qobject_cast<QAbstractSocket*>(d->device);
    if (socket)
        return socket->state();

    return QAbstractSocket::UnconnectedState;
}

void QQmlDebugConnection::flush()
{
    QAbstractSocket *socket = qobject_cast<QAbstractSocket*>(d->device);
    if (socket) {
        socket->flush();
        return;
    }
}

void QQmlDebugConnection::connectToHost(const QString &hostName, quint16 port)
{
    QTcpSocket *socket = new QTcpSocket(d);
    socket->setProxy(QNetworkProxy::NoProxy);
    d->device = socket;
    d->connectDeviceSignals();
    d->gotHello = false;
    connect(socket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SIGNAL(stateChanged(QAbstractSocket::SocketState)));
    connect(socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SIGNAL(error(QAbstractSocket::SocketError)));
    connect(socket, SIGNAL(connected()), this, SIGNAL(connected()));
    socket->connectToHost(hostName, port);
    QIODevice::open(ReadWrite | Unbuffered);
}

void QQmlDebugConnectionPrivate::connectDeviceSignals()
{
    connect(device, SIGNAL(bytesWritten(qint64)), q, SIGNAL(bytesWritten(qint64)));
    connect(device, SIGNAL(readyRead()), q, SIGNAL(readyRead()));
    connect(device, SIGNAL(aboutToClose()), this, SLOT(deviceAboutToClose()));
}

//

QQmlDebugClientPrivate::QQmlDebugClientPrivate()
    : connection(0)
{
}

QQmlDebugClient::QQmlDebugClient(const QString &name,
                                                 QQmlDebugConnection *parent)
    : QObject(parent),
      d(new QQmlDebugClientPrivate)
{
    d->name = name;
    d->connection = parent;

    if (!d->connection)
        return;

    if (d->connection->d->plugins.contains(name)) {
        qWarning() << "QQmlDebugClient: Conflicting plugin name" << name;
        d->connection = 0;
    } else {
        d->connection->d->plugins.insert(name, this);
        d->connection->d->advertisePlugins();
    }
}

QQmlDebugClient::~QQmlDebugClient()
{
    if (d->connection && d->connection->d) {
        d->connection->d->plugins.remove(d->name);
        d->connection->d->advertisePlugins();
    }
    delete d;
}

QString QQmlDebugClient::name() const
{
    return d->name;
}

float QQmlDebugClient::serviceVersion() const
{
    if (d->connection->d->serverPlugins.contains(d->name))
        return d->connection->d->serverPlugins.value(d->name);
    return -1;
}

QQmlDebugClient::State QQmlDebugClient::state() const
{
    if (!d->connection
            || !d->connection->isConnected()
            || !d->connection->d->gotHello)
        return NotConnected;

    if (d->connection->d->serverPlugins.contains(d->name))
        return Enabled;

    return Unavailable;
}

QString QQmlDebugClient::stateString() const
{
    switch (state()) {
    case NotConnected: return QLatin1String("Not connected");
    case Unavailable: return QLatin1String("Unavailable");
    case Enabled: return QLatin1String("Enabled");
    }
    return QLatin1String("Invalid");
}

void QQmlDebugClient::sendMessage(const QByteArray &message)
{
    if (state() != Enabled)
        return;

    QPacket pack;
    pack << d->name << message;
    d->connection->d->protocol->send(pack);
    d->connection->flush();
}

void QQmlDebugClient::stateChanged(State)
{
}

void QQmlDebugClient::messageReceived(const QByteArray &)
{
}

#include <qqmldebugclient.moc>
