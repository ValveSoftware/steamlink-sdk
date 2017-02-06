/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qqmldebugserver.h"
#include "qqmldebugserverfactory.h"
#include "qqmldebugserverconnection.h"
#include "qqmldebugpacket.h"

#include <private/qqmldebugservice_p.h>
#include <private/qjsengine_p.h>
#include <private/qqmlglobal_p.h>
#include <private/qqmldebugpluginmanager_p.h>
#include <private/qqmldebugserviceinterfaces_p.h>
#include <private/qpacketprotocol_p.h>

#include <QtCore/QAtomicInt>
#include <QtCore/QDir>
#include <QtCore/QPluginLoader>
#include <QtCore/QStringList>
#include <QtCore/QVector>
#include <QtCore/qwaitcondition.h>

QT_BEGIN_NAMESPACE

/*
  QQmlDebug Protocol (Version 1):

  handshake:
    1. Client sends
         "QDeclarativeDebugServer" 0 version pluginNames [QDataStream version]
       version: an int representing the highest protocol version the client knows
       pluginNames: plugins available on client side
    2. Server sends
         "QDeclarativeDebugClient" 0 version pluginNames pluginVersions [QDataStream version]
       version: an int representing the highest protocol version the client & server know
       pluginNames: plugins available on server side. plugins both in the client and server message are enabled.
  client plugin advertisement
    1. Client sends
         "QDeclarativeDebugServer" 1 pluginNames
  server plugin advertisement (not implemented: all services are required to register before open())
    1. Server sends
         "QDeclarativeDebugClient" 1 pluginNames pluginVersions
  plugin communication:
       Everything send with a header different to "QDeclarativeDebugServer" is sent to the appropriate plugin.
  */

Q_QML_DEBUG_PLUGIN_LOADER(QQmlDebugServerConnection)

const int protocolVersion = 1;

class QQmlDebugServerImpl;
class QQmlDebugServerThread : public QThread
{
public:
    QQmlDebugServerThread() : m_server(0), m_portFrom(-1), m_portTo(-1) {}

    void setServer(QQmlDebugServerImpl *server)
    {
        m_server = server;
    }

    void setPortRange(int portFrom, int portTo, const QString &hostAddress)
    {
        m_pluginName = QLatin1String("QTcpServerConnection");
        m_portFrom = portFrom;
        m_portTo = portTo;
        m_hostAddress = hostAddress;
    }

    void setFileName(const QString &fileName)
    {
        m_pluginName = QLatin1String("QLocalClientConnection");
        m_fileName = fileName;
    }

    const QString &pluginName() const
    {
        return m_pluginName;
    }

    void run();

private:
    QQmlDebugServerImpl *m_server;
    QString m_pluginName;
    int m_portFrom;
    int m_portTo;
    QString m_hostAddress;
    QString m_fileName;
};

class QQmlDebugServerImpl : public QQmlDebugServer
{
    Q_OBJECT
public:
    QQmlDebugServerImpl();

    bool blockingMode() const Q_DECL_OVERRIDE;

    QQmlDebugService *service(const QString &name) const Q_DECL_OVERRIDE;

    void addEngine(QJSEngine *engine) Q_DECL_OVERRIDE;
    void removeEngine(QJSEngine *engine) Q_DECL_OVERRIDE;
    bool hasEngine(QJSEngine *engine) const Q_DECL_OVERRIDE;

    bool addService(const QString &name, QQmlDebugService *service) Q_DECL_OVERRIDE;
    bool removeService(const QString &name) Q_DECL_OVERRIDE;

    bool open(const QVariantHash &configuration) Q_DECL_OVERRIDE;
    void setDevice(QIODevice *socket) Q_DECL_OVERRIDE;

    void parseArguments();

    static void cleanup();

private:
    friend class QQmlDebugServerThread;
    friend class QQmlDebugServerFactory;

    class EngineCondition {
    public:
        EngineCondition() : numServices(0), condition(new QWaitCondition) {}

        bool waitForServices(QMutex *locked, int numEngines);
        bool isWaiting() const { return numServices > 0; }

        void wake();
    private:
        int numServices;

        // shared pointer to allow for QHash-inflicted copying.
        QSharedPointer<QWaitCondition> condition;
    };

    bool canSendMessage(const QString &name);
    void doSendMessage(const QString &name, const QByteArray &message);
    void wakeEngine(QJSEngine *engine);
    void sendMessage(const QString &name, const QByteArray &message);
    void sendMessages(const QString &name, const QList<QByteArray> &messages);
    void changeServiceState(const QString &serviceName, QQmlDebugService::State state);
    void removeThread();
    void receiveMessage();
    void invalidPacket();

    QQmlDebugServerConnection *m_connection;
    QHash<QString, QQmlDebugService *> m_plugins;
    QStringList m_clientPlugins;
    bool m_gotHello;
    bool m_blockingMode;
    bool m_clientSupportsMultiPackets;

    QHash<QJSEngine *, EngineCondition> m_engineConditions;

    mutable QMutex m_helloMutex;
    QWaitCondition m_helloCondition;
    QQmlDebugServerThread m_thread;
    QPacketProtocol *m_protocol;
    QAtomicInt m_changeServiceStateCalls;
};

void QQmlDebugServerImpl::cleanup()
{
    QQmlDebugServerImpl *server = static_cast<QQmlDebugServerImpl *>(
                QQmlDebugConnector::instance());
    if (!server)
        return;

    {
        QObject signalSource;
        for (QHash<QString, QQmlDebugService *>::ConstIterator i = server->m_plugins.constBegin();
             i != server->m_plugins.constEnd(); ++i) {
            server->m_changeServiceStateCalls.ref();
            QString key = i.key();
            // Process this in the server's thread.
            connect(&signalSource, &QObject::destroyed, server, [key, server](){
                server->changeServiceState(key, QQmlDebugService::NotConnected);
            }, Qt::QueuedConnection);
        }
    }

    // Wait for changeServiceState calls to finish
    // (while running an event loop because some services
    // might again defer execution of stuff in the GUI thread)
    QEventLoop loop;
    while (!server->m_changeServiceStateCalls.testAndSetOrdered(0, 0))
        loop.processEvents();

    // Stop the thread while the application is still there.
    server->m_thread.exit();
    server->m_thread.wait();
}

void QQmlDebugServerThread::run()
{
    Q_ASSERT_X(m_server != 0, Q_FUNC_INFO, "There should always be a debug server available here.");
    QQmlDebugServerConnection *connection = loadQQmlDebugServerConnection(m_pluginName);
    if (connection) {
        {
            QMutexLocker connectionLocker(&m_server->m_helloMutex);
            m_server->m_connection = connection;
            connection->setServer(m_server);
            m_server->m_helloCondition.wakeAll();
        }

        if (m_fileName.isEmpty()) {
            if (!connection->setPortRange(m_portFrom, m_portTo, m_server->blockingMode(),
                                          m_hostAddress))
                return;
        } else if (!connection->setFileName(m_fileName, m_server->blockingMode())) {
            return;
        }

        if (m_server->blockingMode())
            connection->waitForConnection();
    } else {
        qWarning() << "QML Debugger: Couldn't load plugin" << m_pluginName;
        return;
    }

    exec();

    // make sure events still waiting are processed
    QEventLoop eventLoop;
    eventLoop.processEvents(QEventLoop::AllEvents);
}

bool QQmlDebugServerImpl::blockingMode() const
{
    return m_blockingMode;
}

static void cleanupOnShutdown()
{
    // We cannot do this in the destructor as the connection plugin will get unloaded before the
    // server plugin and we need the connection to send any remaining data. This function is
    // triggered before any plugins are unloaded.
    QQmlDebugServerImpl::cleanup();
}

QQmlDebugServerImpl::QQmlDebugServerImpl() :
    m_connection(0),
    m_gotHello(false),
    m_blockingMode(false),
    m_clientSupportsMultiPackets(false)
{
    static bool postRoutineAdded = false;
    if (!postRoutineAdded) {
        qAddPostRoutine(cleanupOnShutdown);
        postRoutineAdded = true;
    }

    // used in sendMessages
    qRegisterMetaType<QList<QByteArray> >("QList<QByteArray>");
    // used in changeServiceState
    qRegisterMetaType<QQmlDebugService::State>("QQmlDebugService::State");

    m_thread.setServer(this);
    moveToThread(&m_thread);

    // Remove the thread immmediately when it finishes, so that we don't have to wait for the
    // event loop to signal that.
    QObject::connect(&m_thread, &QThread::finished, this, &QQmlDebugServerImpl::removeThread,
                     Qt::DirectConnection);
    m_thread.setObjectName(QStringLiteral("QQmlDebugServerThread"));
    parseArguments();
}

bool QQmlDebugServerImpl::open(const QVariantHash &configuration = QVariantHash())
{
    if (m_thread.isRunning())
        return false;
    if (!configuration.isEmpty()) {
        m_blockingMode = configuration[QLatin1String("block")].toBool();
        if (configuration.contains(QLatin1String("portFrom"))) {
            int portFrom = configuration[QLatin1String("portFrom")].toInt();
            int portTo = configuration[QLatin1String("portTo")].toInt();
            m_thread.setPortRange(portFrom, portTo == -1 ? portFrom : portTo,
                                   configuration[QLatin1String("hostAddress")].toString());
        } else if (configuration.contains(QLatin1String("fileName"))) {
            m_thread.setFileName(configuration[QLatin1String("fileName")].toString());
        } else {
            return false;
        }
    }

    if (m_thread.pluginName().isEmpty())
        return false;

    QMutexLocker locker(&m_helloMutex);
    m_thread.start();
    m_helloCondition.wait(&m_helloMutex); // wait for connection
    if (m_blockingMode && !m_gotHello)
        m_helloCondition.wait(&m_helloMutex); // wait for hello
    return true;
}

void QQmlDebugServerImpl::parseArguments()
{
    // format: qmljsdebugger=port:<port_from>[,port_to],host:<ip address>][,block]
    const QString args = commandLineArguments();
    if (args.isEmpty())
        return; // Manual initialization, through QQmlDebugServer::open()

    // ### remove port definition when protocol is changed
    int portFrom = 0;
    int portTo = 0;
    bool block = false;
    bool ok = false;
    QString hostAddress;
    QString fileName;
    QStringList services;

    const auto lstjsDebugArguments = args.splitRef(QLatin1Char(','));
    for (auto argsIt = lstjsDebugArguments.begin(), argsItEnd = lstjsDebugArguments.end(); argsIt != argsItEnd; ++argsIt) {
        const QStringRef &strArgument = *argsIt;
        if (strArgument.startsWith(QLatin1String("port:"))) {
            portFrom = strArgument.mid(5).toInt(&ok);
            portTo = portFrom;
            const auto argsNext = argsIt + 1;
            if (argsNext == argsItEnd)
                break;
            if (ok) {
                const QString nextArgument = argsNext->toString();

                // Don't use QStringLiteral here. QRegExp has a global cache and will save an implicitly
                // shared copy of the passed string. That copy isn't properly detached when the library
                // is unloaded if the original string lives in the library's .rodata
                if (nextArgument.contains(QRegExp(QLatin1String("^\\s*\\d+\\s*$")))) {
                    portTo = nextArgument.toInt(&ok);
                    ++argsIt;
                }
            }
        } else if (strArgument.startsWith(QLatin1String("host:"))) {
            hostAddress = strArgument.mid(5).toString();
        } else if (strArgument == QLatin1String("block")) {
            block = true;
        } else if (strArgument.startsWith(QLatin1String("file:"))) {
            fileName = strArgument.mid(5).toString();
            ok = !fileName.isEmpty();
        } else if (strArgument.startsWith(QLatin1String("services:"))) {
            services.append(strArgument.mid(9).toString());
        } else if (!services.isEmpty()) {
            services.append(strArgument.toString());
        } else {
            const QString message = tr("QML Debugger: Invalid argument \"%1\" detected."
                                       " Ignoring the same.").arg(strArgument.toString());
            qWarning("%s", qPrintable(message));
        }
    }

    if (ok) {
        setServices(services);
        m_blockingMode = block;
        if (!fileName.isEmpty())
            m_thread.setFileName(fileName);
        else
            m_thread.setPortRange(portFrom, portTo, hostAddress);
    } else {
        QString usage;
        QTextStream str(&usage);
        str << tr("QML Debugger: Ignoring \"-qmljsdebugger=%1\".").arg(args) << '\n'
            << tr("The format is \"-qmljsdebugger=[file:<file>|port:<port_from>][,<port_to>]"
                  "[,host:<ip address>][,block][,services:<service>][,<service>]*\"") << '\n'
            << tr("\"file:\" can be used to specify the name of a file the debugger will try "
                  "to connect to using a QLocalSocket. If \"file:\" is given any \"host:\" and"
                  "\"port:\" arguments will be ignored.") << '\n'
            << tr("\"host:\" and \"port:\" can be used to specify an address and a single "
                  "port or a range of ports the debugger will try to bind to with a "
                  "QTcpServer.") << '\n'
            << tr("\"block\" makes the debugger and some services wait for clients to be "
                  "connected and ready before the first QML engine starts.") << '\n'
            << tr("\"services:\" can be used to specify which debug services the debugger "
                  "should load. Some debug services interact badly with others. The V4 "
                  "debugger should not be loaded when using the QML profiler as it will force "
                  "any V4 engines to use the JavaScript interpreter rather than the JIT. The "
                  "following debug services are available by default:") << '\n'
            << QQmlEngineDebugService::s_key   << "\t- " << tr("The QML debugger") << '\n'
            << QV4DebugService::s_key          << "\t- " << tr("The V4 debugger") << '\n'
            << QQmlInspectorService::s_key     << "\t- " << tr("The QML inspector") << '\n'
            << QQmlProfilerService::s_key      << "\t- " << tr("The QML profiler") << '\n'
            << QQmlEngineControlService::s_key << "\t- "
            //: Please preserve the line breaks and formatting
            << tr("Allows the client to delay the starting and stopping of\n"
                  "\t\t  QML engines until other services are ready. QtCreator\n"
                  "\t\t  uses this service with the QML profiler in order to\n"
                  "\t\t  profile multiple QML engines at the same time.")
            << '\n' << QDebugMessageService::s_key << "\t- "
            //: Please preserve the line breaks and formatting
            << tr("Sends qDebug() and similar messages over the QML debug\n"
               "\t\t  connection. QtCreator uses this for showing debug\n"
               "\t\t  messages in the debugger console.") << '\n'
           << tr("Other services offered by qmltooling plugins that implement "
                 "QQmlDebugServiceFactory and which can be found in the standard plugin "
                 "paths will also be available and can be specified. If no \"services\" "
                 "argument is given, all services found this way, including the default "
                 "ones, are loaded.");
        qWarning("%s", qPrintable(usage));
    }
}

void QQmlDebugServerImpl::receiveMessage()
{
    typedef QHash<QString, QQmlDebugService*>::const_iterator DebugServiceConstIt;

    // to be executed in debugger thread
    Q_ASSERT(QThread::currentThread() == thread());

    if (!m_protocol)
        return;

    QQmlDebugPacket in(m_protocol->read());

    QString name;

    in >> name;
    if (name == QLatin1String("QDeclarativeDebugServer")) {
        int op = -1;
        in >> op;
        if (op == 0) {
            int version;
            in >> version >> m_clientPlugins;

            //Get the supported QDataStream version
            if (!in.atEnd()) {
                in >> s_dataStreamVersion;
                if (s_dataStreamVersion > QDataStream::Qt_DefaultCompiledVersion)
                    s_dataStreamVersion = QDataStream::Qt_DefaultCompiledVersion;
            }

            if (!in.atEnd())
                in >> m_clientSupportsMultiPackets;
            else
                m_clientSupportsMultiPackets = false;

            // Send the hello answer immediately, since it needs to arrive before
            // the plugins below start sending messages.

            QQmlDebugPacket out;
            QStringList pluginNames;
            QList<float> pluginVersions;
            const int count = m_plugins.count();
            pluginNames.reserve(count);
            pluginVersions.reserve(count);
            for (QHash<QString, QQmlDebugService *>::ConstIterator i = m_plugins.constBegin();
                 i != m_plugins.constEnd(); ++i) {
                pluginNames << i.key();
                pluginVersions << i.value()->version();
            }

            out << QString(QStringLiteral("QDeclarativeDebugClient")) << 0 << protocolVersion
                << pluginNames << pluginVersions << dataStreamVersion();

            m_protocol->send(out.data());
            m_connection->flush();

            QMutexLocker helloLock(&m_helloMutex);
            m_gotHello = true;

            for (DebugServiceConstIt iter = m_plugins.constBegin(), cend = m_plugins.constEnd(); iter != cend; ++iter) {
                QQmlDebugService::State newState = QQmlDebugService::Unavailable;
                if (m_clientPlugins.contains(iter.key()))
                    newState = QQmlDebugService::Enabled;
                m_changeServiceStateCalls.ref();
                changeServiceState(iter.key(), newState);
            }

            m_helloCondition.wakeAll();

        } else if (op == 1) {
            // Service Discovery
            QStringList oldClientPlugins = m_clientPlugins;
            in >> m_clientPlugins;

            for (DebugServiceConstIt iter = m_plugins.constBegin(), cend = m_plugins.constEnd(); iter != cend; ++iter) {
                const QString pluginName = iter.key();
                QQmlDebugService::State newState = QQmlDebugService::Unavailable;
                if (m_clientPlugins.contains(pluginName))
                    newState = QQmlDebugService::Enabled;

                if (oldClientPlugins.contains(pluginName)
                        != m_clientPlugins.contains(pluginName)) {
                    m_changeServiceStateCalls.ref();
                    changeServiceState(iter.key(), newState);
                }
            }

        } else {
            qWarning("QML Debugger: Invalid control message %d.", op);
            invalidPacket();
            return;
        }

    } else {
        if (m_gotHello) {
            QHash<QString, QQmlDebugService *>::Iterator iter = m_plugins.find(name);
            if (iter == m_plugins.end()) {
                qWarning() << "QML Debugger: Message received for missing plugin" << name << '.';
            } else {
                QQmlDebugService *service = *iter;
                QByteArray message;
                while (!in.atEnd()) {
                    in >> message;
                    service->messageReceived(message);
                }
            }
        } else {
            qWarning("QML Debugger: Invalid hello message.");
        }

    }
}

void QQmlDebugServerImpl::changeServiceState(const QString &serviceName,
                                             QQmlDebugService::State newState)
{
    // to be executed in debugger thread
    Q_ASSERT(QThread::currentThread() == thread());

    QQmlDebugService *service = m_plugins.value(serviceName);
    if (service && service->state() != newState) {
        service->stateAboutToBeChanged(newState);
        service->setState(newState);
        service->stateChanged(newState);
    }

    m_changeServiceStateCalls.deref();
}

void QQmlDebugServerImpl::removeThread()
{
    Q_ASSERT(m_thread.isFinished());
    Q_ASSERT(QThread::currentThread() == thread());

    QThread *parentThread = m_thread.thread();

    delete m_connection;
    m_connection = 0;

    // Move it back to the parent thread so that we can potentially restart it on a new thread.
    moveToThread(parentThread);
}

QQmlDebugService *QQmlDebugServerImpl::service(const QString &name) const
{
    return m_plugins.value(name);
}

void QQmlDebugServerImpl::addEngine(QJSEngine *engine)
{
    // to be executed outside of debugger thread
    Q_ASSERT(QThread::currentThread() != &m_thread);

    QMutexLocker locker(&m_helloMutex);
    Q_ASSERT(!m_engineConditions.contains(engine));

    foreach (QQmlDebugService *service, m_plugins)
        service->engineAboutToBeAdded(engine);

    m_engineConditions[engine].waitForServices(&m_helloMutex, m_plugins.count());

    foreach (QQmlDebugService *service, m_plugins)
        service->engineAdded(engine);
}

void QQmlDebugServerImpl::removeEngine(QJSEngine *engine)
{
    // to be executed outside of debugger thread
    Q_ASSERT(QThread::currentThread() != &m_thread);

    QMutexLocker locker(&m_helloMutex);
    Q_ASSERT(m_engineConditions.contains(engine));

    foreach (QQmlDebugService *service, m_plugins)
        service->engineAboutToBeRemoved(engine);

    m_engineConditions[engine].waitForServices(&m_helloMutex, m_plugins.count());

    foreach (QQmlDebugService *service, m_plugins)
        service->engineRemoved(engine);

    m_engineConditions.remove(engine);
}

bool QQmlDebugServerImpl::hasEngine(QJSEngine *engine) const
{
    QMutexLocker locker(&m_helloMutex);
    QHash<QJSEngine *, EngineCondition>::ConstIterator i = m_engineConditions.constFind(engine);
    // if we're still waiting the engine isn't fully "there", yet, nor fully removed.
    return i != m_engineConditions.constEnd() && !i.value().isWaiting();
}

bool QQmlDebugServerImpl::addService(const QString &name, QQmlDebugService *service)
{
    // to be executed before thread starts
    Q_ASSERT(!m_thread.isRunning());

    if (!service || m_plugins.contains(name))
        return false;

    connect(service, &QQmlDebugService::messageToClient,
            this, &QQmlDebugServerImpl::sendMessage);
    connect(service, &QQmlDebugService::messagesToClient,
            this, &QQmlDebugServerImpl::sendMessages);

    connect(service, &QQmlDebugService::attachedToEngine,
            this, &QQmlDebugServerImpl::wakeEngine, Qt::QueuedConnection);
    connect(service, &QQmlDebugService::detachedFromEngine,
            this, &QQmlDebugServerImpl::wakeEngine, Qt::QueuedConnection);

    service->setState(QQmlDebugService::Unavailable);
    m_plugins.insert(name, service);

    return true;
}

bool QQmlDebugServerImpl::removeService(const QString &name)
{
    // to be executed after thread ends
    Q_ASSERT(!m_thread.isRunning());

    QQmlDebugService *service = m_plugins.value(name);
    if (!service)
        return false;

    m_plugins.remove(name);
    service->setState(QQmlDebugService::NotConnected);

    disconnect(service, &QQmlDebugService::detachedFromEngine,
               this, &QQmlDebugServerImpl::wakeEngine);
    disconnect(service, &QQmlDebugService::attachedToEngine,
               this, &QQmlDebugServerImpl::wakeEngine);

    disconnect(service, &QQmlDebugService::messagesToClient,
               this, &QQmlDebugServerImpl::sendMessages);
    disconnect(service, &QQmlDebugService::messageToClient,
               this, &QQmlDebugServerImpl::sendMessage);

    return true;
}

bool QQmlDebugServerImpl::canSendMessage(const QString &name)
{
    // to be executed in debugger thread
    Q_ASSERT(QThread::currentThread() == thread());
    return m_connection && m_connection->isConnected() && m_protocol &&
            m_clientPlugins.contains(name);
}

void QQmlDebugServerImpl::doSendMessage(const QString &name, const QByteArray &message)
{
    QQmlDebugPacket out;
    out << name << message;
    m_protocol->send(out.data());
}

void QQmlDebugServerImpl::sendMessage(const QString &name, const QByteArray &message)
{
    if (canSendMessage(name)) {
        doSendMessage(name, message);
        m_connection->flush();
    }
}

void QQmlDebugServerImpl::sendMessages(const QString &name, const QList<QByteArray> &messages)
{
    if (canSendMessage(name)) {
        if (m_clientSupportsMultiPackets) {
            QQmlDebugPacket out;
            out << name;
            foreach (const QByteArray &message, messages)
                out << message;
            m_protocol->send(out.data());
        } else {
            foreach (const QByteArray &message, messages)
                doSendMessage(name, message);
        }
        m_connection->flush();
    }
}

void QQmlDebugServerImpl::wakeEngine(QJSEngine *engine)
{
    // to be executed in debugger thread
    Q_ASSERT(QThread::currentThread() == thread());

    QMutexLocker locker(&m_helloMutex);
    m_engineConditions[engine].wake();
}

bool QQmlDebugServerImpl::EngineCondition::waitForServices(QMutex *locked, int num)
{
    Q_ASSERT_X(numServices == 0, Q_FUNC_INFO, "Request to wait again before previous wait finished");
    numServices = num;
    return numServices > 0 ? condition->wait(locked) : true;
}

void QQmlDebugServerImpl::EngineCondition::wake()
{
    if (--numServices == 0)
        condition->wakeAll();
    Q_ASSERT_X(numServices >=0, Q_FUNC_INFO, "Woken more often than #services.");
}

void QQmlDebugServerImpl::setDevice(QIODevice *socket)
{
    m_protocol = new QPacketProtocol(socket, this);
    QObject::connect(m_protocol, &QPacketProtocol::readyRead,
                     this, &QQmlDebugServerImpl::receiveMessage);
    QObject::connect(m_protocol, &QPacketProtocol::invalidPacket,
                     this, &QQmlDebugServerImpl::invalidPacket);

    if (blockingMode())
        m_protocol->waitForReadyRead(-1);
}

void QQmlDebugServerImpl::invalidPacket()
{
    qWarning("QML Debugger: Received a corrupted packet! Giving up ...");
    m_connection->disconnect();
    // protocol might still be processing packages at this point
    m_protocol->deleteLater();
    m_protocol = 0;
}

QQmlDebugConnector *QQmlDebugServerFactory::create(const QString &key)
{
    // Cannot parent it to this because it gets moved to another thread
    return (key == QLatin1String("QQmlDebugServer") ? new QQmlDebugServerImpl : 0);
}

QT_END_NAMESPACE

#include "qqmldebugserver.moc"
