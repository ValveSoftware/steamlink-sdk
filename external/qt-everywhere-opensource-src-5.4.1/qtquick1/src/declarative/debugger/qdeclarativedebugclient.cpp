/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
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

#include "private/qdeclarativedebugclient_p.h"

#include "private/qpacketprotocol_p.h"

#include <QtCore/qdebug.h>
#include <QtCore/qstringlist.h>

#include <private/qobject_p.h>

QT_BEGIN_NAMESPACE

const int protocolVersion = 1;
const QString serverId = QLatin1String("QDeclarativeDebugServer");
const QString clientId = QLatin1String("QDeclarativeDebugClient");

class QDeclarativeDebugClientPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QDeclarativeDebugClient)
public:
    QDeclarativeDebugClientPrivate();

    QString name;
    QDeclarativeDebugConnection *connection;
};

class QDeclarativeDebugConnectionPrivate : public QObject
{
    Q_OBJECT
public:
    QDeclarativeDebugConnectionPrivate(QDeclarativeDebugConnection *c);
    QDeclarativeDebugConnection *q;
    QPacketProtocol *protocol;

    bool gotHello;
    QStringList serverPlugins;
    QHash<QString, QDeclarativeDebugClient *> plugins;

    void advertisePlugins();

public Q_SLOTS:
    void connected();
    void readyRead();
};

QDeclarativeDebugConnectionPrivate::QDeclarativeDebugConnectionPrivate(QDeclarativeDebugConnection *c)
: QObject(c), q(c), protocol(0), gotHello(false)
{
    protocol = new QPacketProtocol(q, this);
    QObject::connect(c, SIGNAL(connected()), this, SLOT(connected()));
    QObject::connect(protocol, SIGNAL(readyRead()), this, SLOT(readyRead()));
}

void QDeclarativeDebugConnectionPrivate::advertisePlugins()
{
    if (!q->isConnected())
        return;

    QPacket pack;
    pack << serverId << 1 << plugins.keys();
    protocol->send(pack);
    q->flush();
}

void QDeclarativeDebugConnectionPrivate::connected()
{
    QPacket pack;
    pack << serverId << 0 << protocolVersion << plugins.keys();
    protocol->send(pack);
    q->flush();
}

void QDeclarativeDebugConnectionPrivate::readyRead()
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
                    pack >> serverPlugins;
                    validHello = true;
                }
            }
        }

        if (!validHello) {
            qWarning("QDeclarativeDebugConnection: Invalid hello message");
            QObject::disconnect(protocol, SIGNAL(readyRead()), this, SLOT(readyRead()));
            return;
        }

        gotHello = true;

        QHash<QString, QDeclarativeDebugClient *>::Iterator iter = plugins.begin();
        for (; iter != plugins.end(); ++iter) {
            QDeclarativeDebugClient::Status newStatus = QDeclarativeDebugClient::Unavailable;
            if (serverPlugins.contains(iter.key()))
                newStatus = QDeclarativeDebugClient::Enabled;
            iter.value()->statusChanged(newStatus);
        }
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
                QStringList oldServerPlugins = serverPlugins;
                pack >> serverPlugins;

                QHash<QString, QDeclarativeDebugClient *>::Iterator iter = plugins.begin();
                for (; iter != plugins.end(); ++iter) {
                    const QString pluginName = iter.key();
                    QDeclarativeDebugClient::Status newStatus = QDeclarativeDebugClient::Unavailable;
                    if (serverPlugins.contains(pluginName))
                        newStatus = QDeclarativeDebugClient::Enabled;

                    if (oldServerPlugins.contains(pluginName)
                            != serverPlugins.contains(pluginName)) {
                        iter.value()->statusChanged(newStatus);
                    }
                }
            } else {
                qWarning() << "QDeclarativeDebugConnection: Unknown control message id" << op;
            }
        } else {
            QByteArray message;
            pack >> message;

            QHash<QString, QDeclarativeDebugClient *>::Iterator iter =
                plugins.find(name);
            if (iter == plugins.end()) {
                qWarning() << "QDeclarativeDebugConnection: Message received for missing plugin" << name;
            } else {
                (*iter)->messageReceived(message);
            }
        }
    }
}

QDeclarativeDebugConnection::QDeclarativeDebugConnection(QObject *parent)
: QTcpSocket(parent), d(new QDeclarativeDebugConnectionPrivate(this))
{
}

QDeclarativeDebugConnection::~QDeclarativeDebugConnection()
{
    QHash<QString, QDeclarativeDebugClient*>::iterator iter = d->plugins.begin();
    for (; iter != d->plugins.end(); ++iter) {
         iter.value()->d_func()->connection = 0;
         iter.value()->statusChanged(QDeclarativeDebugClient::NotConnected);
    }
}

bool QDeclarativeDebugConnection::isConnected() const
{
    return state() == ConnectedState;
}

QDeclarativeDebugClientPrivate::QDeclarativeDebugClientPrivate()
: connection(0)
{
}

QDeclarativeDebugClient::QDeclarativeDebugClient(const QString &name,
                                           QDeclarativeDebugConnection *parent)
: QObject(*(new QDeclarativeDebugClientPrivate), parent)
{
    Q_D(QDeclarativeDebugClient);
    d->name = name;
    d->connection = parent;

    if (!d->connection)
        return;

    if (d->connection->d->plugins.contains(name)) {
        qWarning() << "QDeclarativeDebugClient: Conflicting plugin name" << name;
        d->connection = 0;
    } else {
        d->connection->d->plugins.insert(name, this);
        d->connection->d->advertisePlugins();
    }
}

QDeclarativeDebugClient::~QDeclarativeDebugClient()
{
    Q_D(const QDeclarativeDebugClient);
    if (d->connection && d->connection->d) {
        d->connection->d->plugins.remove(d->name);
        d->connection->d->advertisePlugins();
    }
}

QString QDeclarativeDebugClient::name() const
{
    Q_D(const QDeclarativeDebugClient);
    return d->name;
}

QDeclarativeDebugClient::Status QDeclarativeDebugClient::status() const
{
    Q_D(const QDeclarativeDebugClient);
    if (!d->connection
            || !d->connection->isConnected()
            || !d->connection->d->gotHello)
        return NotConnected;

    if (d->connection->d->serverPlugins.contains(d->name))
        return Enabled;

    return Unavailable;
}

void QDeclarativeDebugClient::sendMessage(const QByteArray &message)
{
    Q_D(QDeclarativeDebugClient);

    if (status() != Enabled)
        return;

    QPacket pack;
    pack << d->name << message;
    d->connection->d->protocol->send(pack);
    d->connection->d->q->flush();
}

void QDeclarativeDebugClient::statusChanged(Status)
{
}

void QDeclarativeDebugClient::messageReceived(const QByteArray &)
{
}

QT_END_NAMESPACE

#include <qdeclarativedebugclient.moc>
