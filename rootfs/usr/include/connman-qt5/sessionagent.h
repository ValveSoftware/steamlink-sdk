/*
 * Copyright © 2012, Jolla.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#ifndef SESSIONAGENT_H
#define SESSIONAGENT_H

#include "networkmanager.h"

class NetConnmanSessionInterface;

class SessionAgent : public QObject
{
    Q_OBJECT

public:
    explicit SessionAgent(const QString &path,QObject* parent = 0);
    virtual ~SessionAgent();

    void setAllowedBearers(const QStringList &bearers);
    void setConnectionType(const QString &type);
    void requestConnect();
    void requestDisconnect();
    void requestDestroy();

public Q_SLOTS:
    void release();
    void update(const QVariantMap &settings);
    void createSession();

Q_SIGNALS:
    void settingsUpdated(const QVariantMap &settings);
    void released();

private Q_SLOTS:
    void onConnectFinished(QDBusPendingCallWatcher *watcher);

private:
    QString agentPath;
    QVariantMap sessionSettings;
    NetworkManager* m_manager;
    NetConnmanSessionInterface *m_session;

    friend class SessionNotificationAdaptor;
};

class SessionNotificationAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "net.connman.Notification")

public:
    explicit SessionNotificationAdaptor(SessionAgent* parent);
    virtual ~SessionNotificationAdaptor();

public Q_SLOTS:
    void Release();
    void Update(const QVariantMap &settings);
private:
    SessionAgent* m_sessionAgent;
};

#endif // USERAGENT_H
