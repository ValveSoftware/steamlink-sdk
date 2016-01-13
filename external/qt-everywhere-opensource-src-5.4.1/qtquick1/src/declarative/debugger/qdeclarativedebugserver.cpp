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

#include "private/qdeclarativedebugserver_p.h"
#include "private/qdeclarativedebugservice_p.h"
#include "private/qdeclarativedebugservice_p_p.h"
#include "private/qdeclarativeengine_p.h"

#include <QtCore/QDir>
#include <QtCore/QPluginLoader>
#include <QtCore/QStringList>

#include <private/qobject_p.h>
#include <private/qapplication_p.h>

QT_BEGIN_NAMESPACE

/*
  QDeclarativeDebug Protocol (Version 1):

  handshake:
    1. Client sends
         "QDeclarativeDebugServer" 0 version pluginNames
       version: an int representing the highest protocol version the client knows
       pluginNames: plugins available on client side
    2. Server sends
         "QDeclarativeDebugClient" 0 version pluginNames
       version: an int representing the highest protocol version the client & server know
       pluginNames: plugins available on server side. plugins both in the client and server message are enabled.
  client plugin advertisement
    1. Client sends
         "QDeclarativeDebugServer" 1 pluginNames
  server plugin advertisement
    1. Server sends
         "QDeclarativeDebugClient" 1 pluginNames
  plugin communication:
       Everything send with a header different to "QDeclarativeDebugServer" is sent to the appropriate plugin.
  */

const int protocolVersion = 1;


class QDeclarativeDebugServerPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QDeclarativeDebugServer)
public:
    QDeclarativeDebugServerPrivate();

    void advertisePlugins();

    QDeclarativeDebugServerConnection *connection;
    QHash<QString, QDeclarativeDebugService *> plugins;
    QStringList clientPlugins;
    bool gotHello;
    QString waitingForMsgFromService;
    bool waitingForMsgSucceeded;

private:
    // private slot
    void _q_deliverMessage(const QString &serviceName, const QByteArray &message);
    static QDeclarativeDebugServerConnection *loadConnectionPlugin(QPluginLoader *loader, const QString &pluginName);
};

QDeclarativeDebugServerPrivate::QDeclarativeDebugServerPrivate() :
    connection(0),
    gotHello(false),
    waitingForMsgSucceeded(false)
{
}

void QDeclarativeDebugServerPrivate::advertisePlugins()
{
    if (!gotHello)
        return;

    QByteArray message;
    {
        QDataStream out(&message, QIODevice::WriteOnly);
        out << QString(QLatin1String("QDeclarativeDebugClient")) << 1 << plugins.keys();
    }
    connection->send(QList<QByteArray>() << message);
}

QDeclarativeDebugServerConnection *QDeclarativeDebugServerPrivate::loadConnectionPlugin(
        QPluginLoader *loader, const QString &pluginName)
{
#ifndef QT_NO_LIBRARY
    QStringList pluginCandidates;
    const QStringList paths = QCoreApplication::libraryPaths();
    foreach (const QString &libPath, paths) {
        const QDir dir(libPath + QLatin1String("/qml1tooling"));
        if (dir.exists()) {
            QStringList plugins(dir.entryList(QDir::Files));
            foreach (const QString &pluginPath, plugins) {
                if (QFileInfo(pluginPath).fileName().contains(pluginName))
                    pluginCandidates << dir.absoluteFilePath(pluginPath);
            }
        }
    }

    foreach (const QString &pluginPath, pluginCandidates) {
        loader->setFileName(pluginPath);
        if (!loader->load()) {
            continue;
        }
        QDeclarativeDebugServerConnection *connection = 0;
        if (QObject *instance = loader->instance())
            connection = qobject_cast<QDeclarativeDebugServerConnection*>(instance);

        if (connection)
            return connection;
        loader->unload();
    }
#endif
    return 0;
}

bool QDeclarativeDebugServer::hasDebuggingClient() const
{
    Q_D(const QDeclarativeDebugServer);
    return d->connection
            && d->connection->isConnected()
            && d->gotHello;
}

QDeclarativeDebugServer *QDeclarativeDebugServer::instance()
{
    static bool commandLineTested = false;
    static QDeclarativeDebugServer *server = 0;

    if (!commandLineTested) {
        commandLineTested = true;

        QApplicationPrivate *appD = static_cast<QApplicationPrivate*>(QObjectPrivate::get(qApp));
#ifndef QDECLARATIVE_NO_DEBUG_PROTOCOL
        // ### remove port definition when protocol is changed
        int port = 0;
        bool block = false;
        bool ok = false;

        // format: qmljsdebugger=port:3768[,block] OR qmljsdebugger=ost[,block]
        if (!appD->qmljsDebugArgumentsString().isEmpty()) {
            if (!QDeclarativeEnginePrivate::qml_debugging_enabled) {
                qWarning() << QString::fromLatin1(
                                  "QDeclarativeDebugServer: Ignoring \"-qmljsdebugger=%1\". "
                                  "Debugging has not been enabled.").arg(
                                  appD->qmljsDebugArgumentsString());
                return 0;
            }

            QString pluginName;
            if (appD->qmljsDebugArgumentsString().indexOf(QLatin1String("port:")) == 0) {
                int separatorIndex = appD->qmljsDebugArgumentsString().indexOf(QLatin1Char(','));
                port = appD->qmljsDebugArgumentsString().mid(5, separatorIndex - 5).toInt(&ok);
                pluginName = QLatin1String("qmldbg_tcp");
            } else if (appD->qmljsDebugArgumentsString().contains(QLatin1String("ost"))) {
                pluginName = QLatin1String("qmldbg_ost");
                ok = true;
            }

            block = appD->qmljsDebugArgumentsString().contains(QLatin1String("block"));

            if (ok) {
                server = new QDeclarativeDebugServer();

                QPluginLoader *loader = new QPluginLoader(server);
                QDeclarativeDebugServerConnection *connection
                        = QDeclarativeDebugServerPrivate::loadConnectionPlugin(loader, pluginName);
                if (connection) {
                    server->d_func()->connection = connection;

                    connection->setServer(server);
                    connection->setPort(port, block);
                } else {
                    qWarning() << QString::fromLatin1(
                                      "QDeclarativeDebugServer: Ignoring \"-qmljsdebugger=%1\". "
                                      "Remote debugger plugin has not been found.").arg(
                                      appD->qmljsDebugArgumentsString());
                }

            } else {
                qWarning() << QString::fromLatin1(
                                  "QDeclarativeDebugServer: Ignoring \"-qmljsdebugger=%1\". "
                                  "Format is -qmljsdebugger=port:<port>[,block]").arg(
                                  appD->qmljsDebugArgumentsString());
            }
        }
#else
        if (!appD->qmljsDebugArgumentsString().isEmpty()) {
            qWarning() << QString::fromLatin1(
                              "QDeclarativeDebugServer: Ignoring \"-qmljsdebugger=%1\". "
                              "QtDeclarative is not configured for debugging.").arg(
                              appD->qmljsDebugArgumentsString());
        }
#endif
    }

    return server;
}

QDeclarativeDebugServer::QDeclarativeDebugServer()
: QObject(*(new QDeclarativeDebugServerPrivate))
{
}

void QDeclarativeDebugServer::receiveMessage(const QByteArray &message)
{
    Q_D(QDeclarativeDebugServer);

    QDataStream in(message);
    if (!d->gotHello) {
        QString name;
        int op;
        in >> name >> op;

        if (name != QLatin1String("QDeclarativeDebugServer")
                || op != 0) {
            qWarning("QDeclarativeDebugServer: Invalid hello message");
            d->connection->disconnect();
            return;
        }

        int version;
        in >> version >> d->clientPlugins;

        // Send the hello answer immediately, since it needs to arrive before
        // the plugins below start sending messages.
        QByteArray helloAnswer;
        {
            QDataStream out(&helloAnswer, QIODevice::WriteOnly);
            QList<float> fakeVersions;
            for (int i = 0; i < d->plugins.size(); ++i)
                fakeVersions << 1.0;
            out << QString(QLatin1String("QDeclarativeDebugClient")) << 0
                << protocolVersion << d->plugins.keys()
                << fakeVersions << QDataStream().version();
        }
        d->connection->send(QList<QByteArray>() << helloAnswer);

        d->gotHello = true;

        QHash<QString, QDeclarativeDebugService*>::Iterator iter = d->plugins.begin();
        for (; iter != d->plugins.end(); ++iter) {
            QDeclarativeDebugService::Status newStatus = QDeclarativeDebugService::Unavailable;
            if (d->clientPlugins.contains(iter.key()))
                newStatus = QDeclarativeDebugService::Enabled;
            iter.value()->d_func()->status = newStatus;
            iter.value()->statusChanged(newStatus);
        }

        qDebug("QDeclarativeDebugServer: Connection established");
    } else {

        QString debugServer(QLatin1String("QDeclarativeDebugServer"));

        QString name;
        in >> name;

        if (name == debugServer) {
            int op = -1;
            in >> op;

            if (op == 1) {
                // Service Discovery
                QStringList oldClientPlugins = d->clientPlugins;
                in >> d->clientPlugins;

                QHash<QString, QDeclarativeDebugService*>::Iterator iter = d->plugins.begin();
                for (; iter != d->plugins.end(); ++iter) {
                    const QString pluginName = iter.key();
                    QDeclarativeDebugService::Status newStatus = QDeclarativeDebugService::Unavailable;
                    if (d->clientPlugins.contains(pluginName))
                        newStatus = QDeclarativeDebugService::Enabled;

                    if (oldClientPlugins.contains(pluginName)
                            != d->clientPlugins.contains(pluginName)) {
                        iter.value()->d_func()->status = newStatus;
                        iter.value()->statusChanged(newStatus);
                    }
                }
            } else {
                qWarning("QDeclarativeDebugServer: Invalid control message %d", op);
            }
        } else {
            QByteArray message;
            in >> message;

            if (d->waitingForMsgFromService == name) {
                // deliver directly so that it is delivered before waitForMessage is returning.
                d->_q_deliverMessage(name, message);
                d->waitingForMsgSucceeded = true;
            } else {
                // deliver message in next event loop run.
                // Fixes the case that the service does start it's own event loop ...,
                // but the networking code doesn't deliver any new messages because readyRead
                // hasn't returned.
                QMetaObject::invokeMethod(this, "_q_deliverMessage", Qt::QueuedConnection,
                                          Q_ARG(QString, name),
                                          Q_ARG(QByteArray, message));
            }
        }
    }
}

void QDeclarativeDebugServerPrivate::_q_deliverMessage(const QString &serviceName, const QByteArray &message)
{
    QHash<QString, QDeclarativeDebugService *>::Iterator iter = plugins.find(serviceName);
    if (iter == plugins.end()) {
        qWarning() << "QDeclarativeDebugServer: Message received for missing plugin" << serviceName;
    } else {
        (*iter)->messageReceived(message);
    }
}

QList<QDeclarativeDebugService*> QDeclarativeDebugServer::services() const
{
    const Q_D(QDeclarativeDebugServer);
    return d->plugins.values();
}

QStringList QDeclarativeDebugServer::serviceNames() const
{
    const Q_D(QDeclarativeDebugServer);
    return d->plugins.keys();
}

bool QDeclarativeDebugServer::addService(QDeclarativeDebugService *service)
{
    Q_D(QDeclarativeDebugServer);
    if (!service || d->plugins.contains(service->name()))
        return false;

    d->plugins.insert(service->name(), service);
    d->advertisePlugins();

    QDeclarativeDebugService::Status newStatus = QDeclarativeDebugService::Unavailable;
    if (d->clientPlugins.contains(service->name()))
        newStatus = QDeclarativeDebugService::Enabled;
    service->d_func()->status = newStatus;
    service->statusChanged(newStatus);
    return true;
}

bool QDeclarativeDebugServer::removeService(QDeclarativeDebugService *service)
{
    Q_D(QDeclarativeDebugServer);
    if (!service || !d->plugins.contains(service->name()))
        return false;

    d->plugins.remove(service->name());
    d->advertisePlugins();

    QDeclarativeDebugService::Status newStatus = QDeclarativeDebugService::NotConnected;
    service->d_func()->server = 0;
    service->d_func()->status = newStatus;
    service->statusChanged(newStatus);
    return true;
}

void QDeclarativeDebugServer::sendMessage(QDeclarativeDebugService *service,
                                          const QByteArray &message)
{
    Q_D(QDeclarativeDebugServer);
    QByteArray msg;
    {
        QDataStream out(&msg, QIODevice::WriteOnly);
        out << service->name() << message;
    }
    d->connection->send(QList<QByteArray>() << msg);
}

bool QDeclarativeDebugServer::waitForMessage(QDeclarativeDebugService *service)
{
    Q_D(QDeclarativeDebugServer);

    if (!service
            || !d->plugins.contains(service->name())
            || !d->waitingForMsgFromService.isEmpty())
        return false;

    d->waitingForMsgFromService = service->name();

    do {
        d->connection->waitForMessage();
    } while (!d->waitingForMsgSucceeded);
    d->waitingForMsgSucceeded = false;
    d->waitingForMsgFromService.clear();
    return true;
}

QT_END_NAMESPACE

#include "moc_qdeclarativedebugserver_p.cpp"
