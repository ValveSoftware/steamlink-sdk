/*
 * Copyright © 2012, Jolla Ltd
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#include "networksession.h"
#include "sessionagent.h"

NetworkSession::NetworkSession(QObject *parent) :
    QObject(parent),
    m_sessionAgent(0),
    m_path("/ConnmanQmlSessionAgent")
{
    createSession();
}

NetworkSession::~NetworkSession()
{
}

void NetworkSession::createSession()
{
    if (m_path.isEmpty())
        return;
    if (m_sessionAgent) {
        delete m_sessionAgent;
        m_sessionAgent = 0;
    }
    m_sessionAgent = new SessionAgent(m_path ,this);
    connect(m_sessionAgent,SIGNAL(settingsUpdated(QVariantMap)),
            this,SLOT(sessionSettingsUpdated(QVariantMap)));
}

QString NetworkSession::state() const
{
    return settingsMap.value("State").toString();
}

QString NetworkSession::name() const
{
    return settingsMap.value("Name").toString();
}

QString NetworkSession::bearer() const
{
    return settingsMap.value("Bearer").toString();
}

QString NetworkSession::sessionInterface() const
{
    return settingsMap.value("Interface").toString();
}

QVariantMap NetworkSession::ipv4() const
{
    return qdbus_cast<QVariantMap>(settingsMap.value("IPv4"));
}

QVariantMap NetworkSession::ipv6() const
{
    return qdbus_cast<QVariantMap>(settingsMap.value("IPv6"));
}

QStringList NetworkSession::allowedBearers() const
{
    return settingsMap.value("AllowedBearers").toStringList();
}

QString NetworkSession::connectionType() const
{
    return settingsMap.value("ConnectionType").toString();
}

void NetworkSession::setAllowedBearers(const QStringList &bearers)
{
    settingsMap.insert("AllowedBearers", qVariantFromValue(bearers));
    m_sessionAgent->setAllowedBearers(bearers);
}

void NetworkSession::setConnectionType(const QString &type)
{
    settingsMap.insert("ConnectionType", qVariantFromValue(type));
    m_sessionAgent->setConnectionType(type);
}

void NetworkSession::requestDestroy()
{
    m_sessionAgent->requestDestroy();
}

void NetworkSession::requestConnect()
{
    m_sessionAgent->requestConnect();
}

void NetworkSession::requestDisconnect()
{
    m_sessionAgent->requestDisconnect();
}

void NetworkSession::sessionSettingsUpdated(const QVariantMap &settings)
{
    Q_FOREACH(const QString &name, settings.keys()) {
        settingsMap.insert(name,settings[name]);

        if (name == QLatin1String("State")) {
            Q_EMIT stateChanged(settings[name].toString());
        } else if (name == QLatin1String("Name")) {
            Q_EMIT nameChanged(settings[name].toString());
        } else if (name == QLatin1String("Bearer")) {
            Q_EMIT bearerChanged(settings[name].toString());
        } else if (name == QLatin1String("Interface")) {
            Q_EMIT sessionInterfaceChanged(settings[name].toString());
        } else if (name == QLatin1String("IPv4")) {
            Q_EMIT ipv4Changed(ipv4());
        } else if (name == QLatin1String("IPv6")) {
            Q_EMIT ipv6Changed(ipv6());
        } else if (name == QLatin1String("AllowedBearers")) {
            Q_EMIT allowedBearersChanged(allowedBearers());
        } else if (name == QLatin1String("ConnectionType")) {
            Q_EMIT connectionTypeChanged(settings[name].toString());
        }
    }
    Q_EMIT settingsChanged(settings);
}

QString NetworkSession::path() const
{
    return m_path;
}

void NetworkSession::setPath(const QString &path)
{
    if (path != m_path) {
        m_path = path;
        createSession();
    }
}
