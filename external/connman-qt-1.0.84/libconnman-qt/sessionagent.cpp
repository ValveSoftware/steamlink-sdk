/*
 * Copyright © 2012, Jolla Ltd.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#include "sessionagent.h"
#include "connman_session_interface.h"

/*
This class is used to run a connman session.
Example:
    SessionAgent *sessionAgent = new SessionAgent("/ConnmanSessionAgent",this);
    connect(sessionAgent,SIGNAL(settingsUpdated(QVariantMap)),
            this,SLOT(sessionSettingsUpdated(QVariantMap)));
    sessionAgent->setAllowedBearers(QStringList() << "wifi" << "ethernet" << "cellular");
    sessionAgent->requestConnect();

    There can be multiple sessions.

  */

SessionAgent::SessionAgent(const QString &path, QObject* parent) :
    QObject(parent),
    agentPath(path),
    m_manager(NetworkManagerFactory::createInstance()),
    m_session(0)
{
    m_manager->setSessionMode(true);
    createSession();
}

SessionAgent::~SessionAgent()
{
    m_manager->setSessionMode(false);
    m_manager->destroySession(agentPath);
}

void SessionAgent::setAllowedBearers(const QStringList &bearers)
{
    if (!m_session)
        return;
    QVariantMap map;
    map.insert("AllowedBearers",  qVariantFromValue(bearers));
    QDBusPendingReply<> reply = m_session->Change("AllowedBearers",QDBusVariant(bearers));
    // hope this is not a lengthy task
    reply.waitForFinished();
    if (reply.isError()) {
        qDebug() << Q_FUNC_INFO << reply.error();
    }

}

void SessionAgent::setConnectionType(const QString &type)
{
    if (!m_session)
        return;
    QVariantMap map;
    map.insert("ConnectionType",  qVariantFromValue(type));
    m_session->Change("ConnectionType",QDBusVariant(type));
}

void SessionAgent::createSession()
{
    if (m_manager->isAvailable()) {
        QDBusObjectPath obpath = m_manager->createSession(QVariantMap(),agentPath);
        if (!obpath.path().isEmpty()) {
            m_session = new NetConnmanSessionInterface("net.connman", obpath.path(),
                QDBusConnection::systemBus(), this);
            new SessionNotificationAdaptor(this);
            QDBusConnection::systemBus().unregisterObject(agentPath);
            if (!QDBusConnection::systemBus().registerObject(agentPath, this)) {
                qDebug() << "Could not register agent object";
            }
        } else {
            qDebug() << "agentPath is not valid" << agentPath;
        }
    } else {
        qDebug() << Q_FUNC_INFO << "manager not valid";
    }
}

void SessionAgent::requestConnect()
{
    if (m_session) {
      QDBusPendingReply<> reply = m_session->Connect();
      QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
      connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
          this, SLOT(onConnectFinished(QDBusPendingCallWatcher*)));
    }
}

void SessionAgent::requestDisconnect()
{
    if (m_session)
        m_session->Disconnect();
}

void SessionAgent::requestDestroy()
{
    if (m_session)
        m_session->Destroy();
}

void SessionAgent::release()
{
    Q_EMIT released();
}

void SessionAgent::update(const QVariantMap &settings)
{
    Q_EMIT settingsUpdated(settings);
}

void SessionAgent::onConnectFinished(QDBusPendingCallWatcher *call)
{
  QDBusPendingReply<> reply = *call;
  if (reply.isError())
    qDebug() << reply.error().message();

  call->deleteLater();
}

SessionNotificationAdaptor::SessionNotificationAdaptor(SessionAgent* parent)
  : QDBusAbstractAdaptor(parent),
    m_sessionAgent(parent)
{
}

SessionNotificationAdaptor::~SessionNotificationAdaptor() {}

void SessionNotificationAdaptor::Release()
{
    m_sessionAgent->release();
}

void SessionNotificationAdaptor::Update(const QVariantMap &settings)
{
    m_sessionAgent->update(settings);
}
