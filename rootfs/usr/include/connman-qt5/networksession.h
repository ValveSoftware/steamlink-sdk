/*
 * Copyright © 2012, Jolla Ltd
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */


#ifndef SESSIONSERVICE_H
#define SESSIONSERVICE_H

#include <QObject>
#include <QtDBus>

namespace Tests {
    class UtSession;
}

class SessionAgent;

class NetworkSession : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString name READ name NOTIFY nameChanged)
    Q_PROPERTY(QString bearer READ bearer NOTIFY bearerChanged)
    Q_PROPERTY(QString sessionInterface READ sessionInterface NOTIFY sessionInterfaceChanged)
    Q_PROPERTY(QVariantMap ipv4 READ ipv4 NOTIFY ipv4Changed)
    Q_PROPERTY(QVariantMap ipv6 READ ipv6 NOTIFY ipv6Changed)

    Q_PROPERTY(QString path READ path WRITE setPath)

    Q_PROPERTY(QStringList allowedBearers READ allowedBearers WRITE setAllowedBearers NOTIFY allowedBearersChanged)
    Q_PROPERTY(QString connectionType READ connectionType WRITE setConnectionType NOTIFY connectionTypeChanged)

    friend class Tests::UtSession;

public:
    NetworkSession(QObject *parent = 0);
    virtual ~NetworkSession();

    //Settings
    QString state() const;
    QString name() const;
    QString bearer() const;
    QString sessionInterface() const;
    QVariantMap ipv4() const;
    QVariantMap ipv6() const;
    QStringList allowedBearers() const;
    QString connectionType() const;

    QString path() const;

    void setAllowedBearers(const QStringList &bearers);
    void setConnectionType(const QString &type);

Q_SIGNALS:
    void allowedBearersChanged(const QStringList &bearers);
    void connectionTypeChanged(const QString &type);
    void settingsChanged(const QVariantMap &settings);

    void stateChanged(const QString &state);
    void nameChanged(const QString &name);
    void bearerChanged(const QString &bearer);
    void sessionInterfaceChanged(const QString &sessionInterface);
    void ipv4Changed(const QVariantMap&settings);
    void ipv6Changed(const QVariantMap&settings);

public Q_SLOTS:
    void requestDestroy();
    void requestConnect();
    void requestDisconnect();
    void sessionSettingsUpdated(const QVariantMap &settings);
    void setPath(const QString &path);

private:
    SessionAgent *m_sessionAgent;
    QVariantMap settingsMap;
    QString m_path;
    void createSession();
};

#endif // SESSIONSERVICE_H
