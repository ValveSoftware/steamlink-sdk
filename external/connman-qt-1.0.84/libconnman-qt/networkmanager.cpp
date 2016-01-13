/*
 * Copyright © 2010, Intel Corporation.
 * Copyright © 2012, Jolla.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#include "networkmanager.h"

#include "commondbustypes.h"
#include "connman_manager_interface.h"
#include "connman_manager_interface.cpp" // not bug
#include "moc_connman_manager_interface.cpp" // not bug
#include <QRegExp>

static NetworkManager* staticInstance = NULL;

NetworkManager* NetworkManagerFactory::createInstance()
{
    if (!staticInstance)
        staticInstance = new NetworkManager;

    return staticInstance;
}

NetworkManager* NetworkManagerFactory::instance()
{
    return createInstance();
}

// NetworkManager implementation

const QString NetworkManager::State("State");
const QString NetworkManager::OfflineMode("OfflineMode");
const QString NetworkManager::SessionMode("SessionMode");

NetworkManager::NetworkManager(QObject* parent)
  : QObject(parent),
    m_manager(NULL),
    m_defaultRoute(NULL),
    m_invalidDefaultRoute(new NetworkService("/", QVariantMap(), this)),
    watcher(NULL),
    m_available(false),
    m_servicesEnabled(true),
    m_technologiesEnabled(true)
{
    registerCommonDataTypes();
    watcher = new QDBusServiceWatcher("net.connman",QDBusConnection::systemBus(),
            QDBusServiceWatcher::WatchForRegistration |
            QDBusServiceWatcher::WatchForUnregistration, this);
    connect(watcher, SIGNAL(serviceRegistered(QString)),
            this, SLOT(connectToConnman(QString)));
    connect(watcher, SIGNAL(serviceUnregistered(QString)),
            this, SLOT(connmanUnregistered(QString)));


    m_available = QDBusConnection::systemBus().interface()->isServiceRegistered("net.connman");

    if (m_available)
        connectToConnman();
    else
        qDebug() << "connman not AVAILABLE";
}

NetworkManager::~NetworkManager()
{
}

void NetworkManager::connectToConnman(QString)
{
    disconnectFromConnman();
    m_manager = new NetConnmanManagerInterface("net.connman", "/",
            QDBusConnection::systemBus(), this);

    if (!m_manager->isValid()) {

        delete m_manager;
        m_manager = NULL;

        // shouldn't happen but in this case service isn't available
        if (m_available)
            Q_EMIT availabilityChanged(m_available = false);
    } else {
        connect(m_manager, SIGNAL(PropertyChanged(QString,QDBusVariant)),
                this, SLOT(propertyChanged(QString,QDBusVariant)));

        QDBusPendingReply<QVariantMap> props_reply = m_manager->GetProperties();
        QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(props_reply, this);
        connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
                this, SLOT(getPropertiesFinished(QDBusPendingCallWatcher*)));

        if (!m_available)
            Q_EMIT availabilityChanged(m_available = true);
    }
}

void NetworkManager::disconnectFromConnman(QString)
{
    delete m_manager;
    m_manager = NULL;

    disconnectTechnologies();
    disconnectServices();
}

void NetworkManager::disconnectTechnologies()
{
    if (m_manager) {
        disconnect(m_manager, SIGNAL(TechnologyAdded(QDBusObjectPath,QVariantMap)),
                   this, SLOT(technologyAdded(QDBusObjectPath,QVariantMap)));
        disconnect(m_manager, SIGNAL(TechnologyRemoved(QDBusObjectPath)),
                   this, SLOT(technologyRemoved(QDBusObjectPath)));
    }

    Q_FOREACH (NetworkTechnology *tech, m_technologiesCache)
        tech->deleteLater();

    if (!m_technologiesCache.isEmpty()) {
        m_technologiesCache.clear();
        Q_EMIT technologiesChanged();
    }
}

void NetworkManager::disconnectServices()
{
    if (m_manager) {
        disconnect(m_manager, SIGNAL(ServicesChanged(ConnmanObjectList,QList<QDBusObjectPath>)),
                   this, SLOT(updateServices(ConnmanObjectList,QList<QDBusObjectPath>)));
        disconnect(m_manager, SIGNAL(SavedServicesChanged(ConnmanObjectList)),
                   this, SLOT(updateSavedServices(ConnmanObjectList)));
    }

    Q_FOREACH (NetworkService *service, m_servicesCache)
        service->deleteLater();

    m_servicesCache.clear();

    if (m_defaultRoute != m_invalidDefaultRoute) {
        m_defaultRoute = m_invalidDefaultRoute;
        Q_EMIT defaultRouteChanged(m_defaultRoute);
    }

    if (!m_servicesOrder.isEmpty()) {
        m_servicesOrder.clear();
        Q_EMIT servicesChanged();
    }

    if (!m_savedServicesOrder.isEmpty()) {
        m_savedServicesOrder.clear();
        Q_EMIT savedServicesChanged();
    }
}

void NetworkManager::connmanUnregistered(QString)
{
    disconnectFromConnman();

    if (m_available)
        Q_EMIT availabilityChanged(m_available = false);
}

void NetworkManager::setupTechnologies()
{
    if (!m_available)
        return;
    connect(m_manager, SIGNAL(TechnologyAdded(QDBusObjectPath,QVariantMap)),
            this, SLOT(technologyAdded(QDBusObjectPath,QVariantMap)));
    connect(m_manager, SIGNAL(TechnologyRemoved(QDBusObjectPath)),
            this, SLOT(technologyRemoved(QDBusObjectPath)));

    QDBusPendingReply<ConnmanObjectList> reply = m_manager->GetTechnologies();
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            this, SLOT(getTechnologiesFinished(QDBusPendingCallWatcher*)));
}

void NetworkManager::setupServices()
{
    if (!m_available)
        return;

    connect(m_manager, SIGNAL(ServicesChanged(ConnmanObjectList,QList<QDBusObjectPath>)),
            this, SLOT(updateServices(ConnmanObjectList,QList<QDBusObjectPath>)));
    connect(m_manager, SIGNAL(SavedServicesChanged(ConnmanObjectList)),
            this, SLOT(updateSavedServices(ConnmanObjectList)));

    QDBusPendingReply<ConnmanObjectList> reply = m_manager->GetServices();
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            this, SLOT(getServicesFinished(QDBusPendingCallWatcher*)));

    reply = m_manager->GetSavedServices();
    watcher = new QDBusPendingCallWatcher(reply, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            this, SLOT(getSavedServicesFinished(QDBusPendingCallWatcher*)));
}

void NetworkManager::updateServices(const ConnmanObjectList &changed, const QList<QDBusObjectPath> &removed)
{
    ConnmanObject connmanobj;
    int order = -1;
    NetworkService *service = NULL;

    // make sure we don't leak memory
    m_servicesOrder.clear();

    QStringList hiddenKnownBssids;
    QStringList serviceList;
    Q_FOREACH (connmanobj, changed) {
        order++;
        bool addedService = false;

        const QString svcPath(connmanobj.objpath.path());

        if (!m_servicesCache.contains(svcPath)) {
            service = new NetworkService(svcPath,
                                         connmanobj.properties, this);
            connect(service,SIGNAL(connectedChanged(bool)),this,SLOT(updateDefaultRoute()));
            m_servicesCache.insert(svcPath, service);
            addedService = true;
        } else {
            service = m_servicesCache.value(svcPath);
            service->updateProperties(connmanobj.properties);
            if (connmanobj.properties.count() > 20) { //new services have full set of properties
                addedService = true;
            }
        }

        //a connected but non broadcast ssid will have hidden property and name
        // a non connected hidden will have no name and hidden property will be false.
        if (service->hidden() && !service->name().isEmpty())//this is the real working service
            hiddenKnownBssids.append(service->bssid());

        if (service->name().isEmpty()
                && hiddenKnownBssids.contains(service->bssid())) {
            // hide this one as it is the hidden service
            continue;
        }

        m_servicesOrder.push_back(service);
        serviceList.push_back(service->path());

        // If this is no longer a favorite network, remove it from the saved list
        if (!service->favorite()) {
            int savedIndex;
            if ((savedIndex = m_savedServicesOrder.indexOf(service)) != -1) {
                m_savedServicesOrder.remove(savedIndex);
            }
        }
        if (order == 0)
            updateDefaultRoute();

        if (addedService) { //Q_EMIT this after m_servicesOrder is updated
            Q_EMIT serviceAdded(svcPath);
        }
    }

    Q_FOREACH (QDBusObjectPath obj, removed) {
        const QString svcPath(obj.path());
        if (m_servicesCache.contains(svcPath)) {
            if (NetworkService *service = m_servicesCache.value(svcPath)) {
                if (m_savedServicesOrder.contains(service)) {
                    // Don't remove this service from the cache, since the saved model needs it
                    // Update the strength value to zero, so we know it isn't visible
                    QVariantMap properties;
                    properties.insert(QString::fromLatin1("Strength"), QVariant(static_cast<quint32>(0)));
                    properties.insert(QLatin1String("State"), QLatin1String("idle"));
                    service->updateProperties(properties);
                } else {
                    service->deleteLater();
                    m_servicesCache.remove(svcPath);
                }
                Q_EMIT serviceRemoved(svcPath);
            }
        } else {
            // connman maintains a virtual "hidden" wifi network and removes it upon init
            qDebug() << "attempted to remove non-existing service";
        }
    }

    if (order == -1)
        updateDefaultRoute();
    Q_EMIT servicesChanged();
    Q_EMIT servicesListChanged(serviceList);

    Q_EMIT savedServicesChanged();
}

void NetworkManager::updateSavedServices(const ConnmanObjectList &changed)
{
    ConnmanObject connmanobj;
    int order = -1;
    NetworkService *service = NULL;

    // make sure we don't leak memory
    m_savedServicesOrder.clear();

    Q_FOREACH (connmanobj, changed) {
        order++;

        const QString svcPath(connmanobj.objpath.path());

        QHash<QString, NetworkService *>::iterator it = m_servicesCache.find(svcPath);
        if (it == m_servicesCache.end()) {
            service = new NetworkService(svcPath,
                                         connmanobj.properties, this);
            m_servicesCache.insert(svcPath, service);
        } else {
            service = *it;
            service->updateProperties(connmanobj.properties);
        }

        m_savedServicesOrder.push_back(service);
    }

    Q_EMIT savedServicesChanged();
}

void NetworkManager::propertyChanged(const QString &name, const QDBusVariant &value)
{
    propertyChanged(name, value.variant());
}

void NetworkManager::updateDefaultRoute()
{
    QString defaultNetDev;
    QFile routeFile("/proc/net/route");
    if (routeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&routeFile);
        QString line = in.readLine();
        while (!line.isNull()) {
            QStringList lineList = line.split('\t');
            if (lineList.size() >= 11) {
                if (lineList.at(1) == "00000000" && lineList.at(3) == "0003") {
                    defaultNetDev = lineList.at(0);
                    break;
                }
            }
            line = in.readLine();
        }
        routeFile.close();
    }
    if (defaultNetDev.isNull()) {
         QFile ipv6routeFile("/proc/net/ipv6_route");
         if (ipv6routeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
             QTextStream ipv6in(&ipv6routeFile);
             QString ipv6line = ipv6in.readLine();
             while (!ipv6line.isNull()) {
                 QStringList ipv6lineList = ipv6line.split(QRegExp("\\s+"));
                 if (ipv6lineList.size() >= 10) {
                     if (ipv6lineList.at(0) == "00000000000000000000000000000000" && ipv6lineList.at(8).endsWith("3")) {
                         defaultNetDev = ipv6lineList.at(9).trimmed();
                         break;
                     }
                     ipv6line = ipv6in.readLine();
                 }
             }
             ipv6routeFile.close();
         }
    }

    Q_FOREACH (NetworkService *service, m_servicesCache) {
        if (service->state() == "online" || service->state() == "ready") {
            if (defaultNetDev == service->ethernet().value("Interface")) {
                if (m_defaultRoute != service) {
                    m_defaultRoute = service;
                    Q_EMIT defaultRouteChanged(m_defaultRoute);
                }
                return;
            }
        }
    }
    m_defaultRoute = m_invalidDefaultRoute;
    Q_EMIT defaultRouteChanged(m_defaultRoute);
}

void NetworkManager::technologyAdded(const QDBusObjectPath &technology,
                                     const QVariantMap &properties)
{
    NetworkTechnology *tech = new NetworkTechnology(technology.path(),
                                                    properties, this);

    m_technologiesCache.insert(tech->type(), tech);
    Q_EMIT technologiesChanged();
}

void NetworkManager::technologyRemoved(const QDBusObjectPath &technology)
{
    NetworkTechnology *net;
    // if we weren't storing by type() this loop would be unecessary
    // but since this function will be triggered rarely that's fine
    Q_FOREACH (net, m_technologiesCache) {
        if (net->objPath() == technology.path()) {
            m_technologiesCache.remove(net->type());
            net->deleteLater();
            break;
        }
    }

    Q_EMIT technologiesChanged();
}

void NetworkManager::getPropertiesFinished(QDBusPendingCallWatcher *watcher)
{
    QDBusPendingReply<QVariantMap> reply = *watcher;
    watcher->deleteLater();

    if (reply.isError()) {
        qDebug() << reply.error().message();
        return;
    }
    QVariantMap props = reply.value();

    for (QVariantMap::ConstIterator i = props.constBegin(); i != props.constEnd(); ++i)
        propertyChanged(i.key(), i.value());

    if (m_technologiesEnabled)
        setupTechnologies();
    if (m_servicesEnabled)
        setupServices();
}

void NetworkManager::getTechnologiesFinished(QDBusPendingCallWatcher *watcher)
{
    QDBusPendingReply<ConnmanObjectList> reply = *watcher;
    watcher->deleteLater();
    if (reply.isError())
        return;
    Q_FOREACH (const ConnmanObject &object, reply.value()) {
        NetworkTechnology *tech = new NetworkTechnology(object.objpath.path(),
                                                        object.properties, this);
        m_technologiesCache.insert(tech->type(), tech);
    }

    Q_EMIT technologiesChanged();
}

void NetworkManager::getServicesFinished(QDBusPendingCallWatcher *watcher)
{
    QDBusPendingReply<ConnmanObjectList> reply = *watcher;
    watcher->deleteLater();
    if (reply.isError())
        return;
    m_servicesOrder.clear();

    Q_FOREACH (const ConnmanObject &object, reply.value()) {
        const QString servicePath = object.objpath.path();

        NetworkService *service;

        QHash<QString, NetworkService *>::ConstIterator it = m_servicesCache.find(servicePath);
        if (it != m_servicesCache.constEnd()) {
            service = *it;
            service->updateProperties(object.properties);
        } else {
            service = new NetworkService(servicePath, object.properties, this);
            connect(service,SIGNAL(connectedChanged(bool)),this,SLOT(updateDefaultRoute()));
            m_servicesCache.insert(servicePath, service);
        }

        m_servicesOrder.append(service);
    }

    updateDefaultRoute();
    Q_EMIT servicesChanged();
    Q_EMIT servicesListChanged(m_servicesCache.keys());
}

void NetworkManager::getSavedServicesFinished(QDBusPendingCallWatcher *watcher)
{
    QDBusPendingReply<ConnmanObjectList> reply = *watcher;
    if (!reply.isError()) {
        m_savedServicesOrder.clear();

        Q_FOREACH (const ConnmanObject &object, reply.value()) {
            const QString servicePath = object.objpath.path();

            NetworkService *service;

            QHash<QString, NetworkService *>::ConstIterator it = m_servicesCache.find(servicePath);
            if (it != m_servicesCache.constEnd()) {
                service = *it;
                service->updateProperties(object.properties);
            } else {
                service = new NetworkService(servicePath, object.properties, this);
                connect(service,SIGNAL(connectedChanged(bool)),this,SLOT(updateDefaultRoute()));
                m_servicesCache.insert(servicePath, service);
            }

            m_savedServicesOrder.append(service);
        }

        Q_EMIT savedServicesChanged();
    }

    watcher->deleteLater();
}


// Public API /////////////

// Getters


bool NetworkManager::isAvailable() const
{
    return m_available;
}


const QString NetworkManager::state() const
{
    return m_propertiesCache[State].toString();
}

bool NetworkManager::offlineMode() const
{
    return m_propertiesCache[OfflineMode].toBool();
}

NetworkService* NetworkManager::defaultRoute() const
{
    return m_defaultRoute;
}

NetworkTechnology* NetworkManager::getTechnology(const QString &type) const
{
    if (m_technologiesCache.contains(type))
        return m_technologiesCache.value(type);
    else {
        qDebug() << "Technology " << type << " doesn't exist";
        return NULL;
    }
}

const QVector<NetworkTechnology *> NetworkManager::getTechnologies() const
{
    QVector<NetworkTechnology *> techs;

    Q_FOREACH (NetworkTechnology *tech, m_technologiesCache) {
        techs.push_back(tech);
    }

    return techs;
}

const QVector<NetworkService*> NetworkManager::getServices(const QString &tech) const
{
    QVector<NetworkService *> services;

    // this Q_FOREACH is based on the m_servicesOrder to keep connman's sort
    // of services.
    Q_FOREACH (NetworkService *service, m_servicesOrder) {
        if (tech.isEmpty() || service->type() == tech)
            services.push_back(service);
    }

    return services;
}

const QVector<NetworkService*> NetworkManager::getSavedServices(const QString &tech) const
{
    QVector<NetworkService *> services;

    // this Q_FOREACH is based on the m_servicesOrder to keep connman's sort
    // of services.
    Q_FOREACH (NetworkService *service, m_savedServicesOrder) {
        // A previously-saved network which is then removed, remains saved with favorite == false
        if ((tech.isEmpty() || service->type() == tech) && service->favorite())
            services.push_back(service);
    }

    return services;
}

// Setters

void NetworkManager::setOfflineMode(const bool &offlineMode)
{
    if (!m_manager) return;

    QDBusPendingReply<void> reply =
        m_manager->SetProperty(OfflineMode,
                               QDBusVariant(QVariant(offlineMode)));
}

  // these shouldn't crash even if connman isn't available
void NetworkManager::registerAgent(const QString &path)
{
    if (m_manager)
        m_manager->RegisterAgent(QDBusObjectPath(path));
}

void NetworkManager::unregisterAgent(const QString &path)
{
    if (m_manager)
        m_manager->UnregisterAgent(QDBusObjectPath(path));
}

void NetworkManager::registerCounter(const QString &path, quint32 accuracy,quint32 period)
{
    if (m_manager)
        m_manager->RegisterCounter(QDBusObjectPath(path),accuracy, period);
}

void NetworkManager::unregisterCounter(const QString &path)
{
    if (m_manager)
        m_manager->UnregisterCounter(QDBusObjectPath(path));
}

QDBusObjectPath NetworkManager::createSession(const QVariantMap &settings, const QString &sessionNotifierPath)
{
    if (!m_manager)
        return QDBusObjectPath();

    QDBusPendingReply<QDBusObjectPath> reply =
        m_manager->CreateSession(settings,QDBusObjectPath(sessionNotifierPath));
    reply.waitForFinished();
    return reply.value();
}

void NetworkManager::destroySession(const QString &sessionAgentPath)
{
    if (m_manager)
        m_manager->DestroySession(QDBusObjectPath(sessionAgentPath));
}

void NetworkManager::setSessionMode(const bool &sessionMode)
{
    if (m_manager)
        m_manager->SetProperty(SessionMode, QDBusVariant(sessionMode));
}

void NetworkManager::propertyChanged(const QString &name, const QVariant &value)
{
    if (m_propertiesCache.value(name) == value)
        return;

    m_propertiesCache[name] = value;

    if (name == State) {
        Q_EMIT stateChanged(value.toString());
        updateDefaultRoute();
    } else if (name == OfflineMode) {
        Q_EMIT offlineModeChanged(value.toBool());
    } else if (name == SessionMode) {
        Q_EMIT sessionModeChanged(value.toBool());
    }
}

bool NetworkManager::sessionMode() const
{
    return m_propertiesCache[SessionMode].toBool();
}

bool NetworkManager::servicesEnabled() const
{
    return m_servicesEnabled;
}

void NetworkManager::setServicesEnabled(bool enabled)
{
    if (m_servicesEnabled == enabled)
        return;

    m_servicesEnabled = enabled;

    if (m_servicesEnabled)
        setupServices();
    else
        disconnectServices();

    Q_EMIT servicesEnabledChanged();
}

bool NetworkManager::technologiesEnabled() const
{
    return m_technologiesEnabled;
}

void NetworkManager::setTechnologiesEnabled(bool enabled)
{
    if (m_technologiesEnabled == enabled)
        return;

    m_technologiesEnabled = enabled;

    if (m_technologiesEnabled)
        setupTechnologies();
    else
        disconnectTechnologies();

    Q_EMIT technologiesEnabledChanged();
}

void NetworkManager::resetCountersForType(const QString &type)
{
    m_manager->ResetCounters(type);
}

QStringList NetworkManager::servicesList(const QString &tech)
{
    QStringList services;
    Q_FOREACH (NetworkService *service, m_servicesOrder) {
        if (tech.isEmpty() || service->type() == tech)
            services.push_back(service->path());
    }
    return services;
}

QStringList NetworkManager::savedServicesList(const QString &tech)
{
    QStringList services;

    Q_FOREACH (NetworkService *service, m_savedServicesOrder) {
        if ((tech.isEmpty() || service->type() == tech) && service->favorite())
            services.push_back(service->path());
    }

    return services;
}

QString NetworkManager::technologyPathForService(const QString &servicePath)
{
    Q_FOREACH (NetworkService *service, m_servicesOrder) {
        if (service->path() == servicePath)
            return service->path();
    }
    return QString();
}
QString NetworkManager::technologyPathForType(const QString &techType)
{
    Q_FOREACH (NetworkTechnology *tech, m_technologiesCache) {
        if (tech->type() == techType)
            return tech->path();
    }
    return QString();
}

QStringList NetworkManager::technologiesList()
{
    QStringList techList;
    Q_FOREACH (NetworkTechnology *tech, m_technologiesCache) {
        techList << tech->type();
    }
    return techList;
}
