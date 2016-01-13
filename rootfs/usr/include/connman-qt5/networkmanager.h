/*
 * Copyright © 2010, Intel Corporation.
 * Copyright © 2012, Jolla.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include "commondbustypes.h"

#include "networktechnology.h"
#include "networkservice.h"
#include <QtDBus>

class NetConnmanManagerInterface;
class NetworkManager;

class NetworkManagerFactory : public QObject
{
    Q_OBJECT

    Q_PROPERTY(NetworkManager* instance READ instance CONSTANT)

public:
    static NetworkManager* createInstance();
    NetworkManager* instance();
};

class NetworkManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool available READ isAvailable NOTIFY availabilityChanged)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool offlineMode READ offlineMode WRITE setOfflineMode NOTIFY offlineModeChanged)
    Q_PROPERTY(NetworkService* defaultRoute READ defaultRoute NOTIFY defaultRouteChanged)

    Q_PROPERTY(bool sessionMode READ sessionMode WRITE setSessionMode NOTIFY sessionModeChanged)

    Q_PROPERTY(bool servicesEnabled READ servicesEnabled WRITE setServicesEnabled NOTIFY servicesEnabledChanged)
    Q_PROPERTY(bool technologiesEnabled READ technologiesEnabled WRITE setTechnologiesEnabled NOTIFY technologiesEnabledChanged)

public:
    NetworkManager(QObject* parent=0);
    virtual ~NetworkManager();

    bool isAvailable() const;

    Q_INVOKABLE NetworkTechnology* getTechnology(const QString &type) const;
    const QVector<NetworkTechnology *> getTechnologies() const;
    const QVector<NetworkService*> getServices(const QString &tech = "") const;
    const QVector<NetworkService*> getSavedServices(const QString &tech = "") const;
    void removeSavedService(const QString &identifier) const;

    Q_INVOKABLE QStringList servicesList(const QString &tech);
    Q_INVOKABLE QStringList savedServicesList(const QString &tech = QString());
    Q_INVOKABLE QStringList technologiesList();
    Q_INVOKABLE QString technologyPathForService(const QString &path);
    Q_INVOKABLE QString technologyPathForType(const QString &type);


    const QString state() const;
    bool offlineMode() const;
    NetworkService* defaultRoute() const;

    bool sessionMode() const;

    bool servicesEnabled() const;
    void setServicesEnabled(bool enabled);

    bool technologiesEnabled() const;
    void setTechnologiesEnabled(bool enabled);

    Q_INVOKABLE void resetCountersForType(const QString &type);

public Q_SLOTS:
    void setOfflineMode(const bool &offlineMode);
    void registerAgent(const QString &path);
    void unregisterAgent(const QString &path);
    void registerCounter(const QString &path, quint32 accuracy,quint32 period);
    void unregisterCounter(const QString &path);
    QDBusObjectPath createSession(const QVariantMap &settings, const QString &sessionNotifierPath);
    void destroySession(const QString &sessionAgentPath);

    void setSessionMode(const bool &sessionMode);

Q_SIGNALS:
    void availabilityChanged(bool available);

    void stateChanged(const QString &state);
    void offlineModeChanged(bool offlineMode);
    void technologiesChanged();
    void servicesChanged();
    void savedServicesChanged();
    void defaultRouteChanged(NetworkService* defaultRoute);
    void sessionModeChanged(bool);
    void servicesListChanged(const QStringList &list);
    void serviceAdded(const QString &servicePath);
    void serviceRemoved(const QString &servicePath);

    void servicesEnabledChanged();
    void technologiesEnabledChanged();

private:
    void propertyChanged(const QString &name, const QVariant &value);

    NetConnmanManagerInterface *m_manager;

    /* Contains all property related to this net.connman.Manager object */
    QVariantMap m_propertiesCache;

    /* Not just for cache, but actual containers of Network* type objects */
    QHash<QString, NetworkTechnology *> m_technologiesCache;
    QHash<QString, NetworkService *> m_servicesCache;

    /* This is for sorting purpose only, never delete an object from here */
    QVector<NetworkService *> m_servicesOrder;
    QVector<NetworkService *> m_savedServicesOrder;

    /* This variable is used just to send signal if changed */
    NetworkService* m_defaultRoute;

    /* Invalid default route service for use when there is no default route */
    NetworkService *m_invalidDefaultRoute;

    QDBusServiceWatcher *watcher;

    static const QString State;
    static const QString OfflineMode;
    static const QString SessionMode;

    bool m_available;

    bool m_servicesEnabled;
    bool m_technologiesEnabled;


private Q_SLOTS:
    void connectToConnman(QString = "");
    void disconnectFromConnman(QString = "");
    void connmanUnregistered(QString = "");
    void disconnectTechnologies();
    void setupTechnologies();
    void disconnectServices();
    void setupServices();
    void propertyChanged(const QString &name, const QDBusVariant &value);
    void updateServices(const ConnmanObjectList &changed, const QList<QDBusObjectPath> &removed);
    void updateSavedServices(const ConnmanObjectList &changed);
    void technologyAdded(const QDBusObjectPath &technology, const QVariantMap &properties);
    void technologyRemoved(const QDBusObjectPath &technology);
    void getPropertiesFinished(QDBusPendingCallWatcher *watcher);
    void updateDefaultRoute();
    void getTechnologiesFinished(QDBusPendingCallWatcher *watcher);
    void getServicesFinished(QDBusPendingCallWatcher *watcher);
    void getSavedServicesFinished(QDBusPendingCallWatcher *watcher);

private:
    Q_DISABLE_COPY(NetworkManager)
};

#endif //NETWORKMANAGER_H
