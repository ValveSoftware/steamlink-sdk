/*
 * Copyright © 2010, Intel Corporation.
 * Copyright © 2012, Jolla.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#include "networkservice.h"
#include "commondbustypes.h"
#include "connman_manager_interface.h"
#include "connman_service_interface.h"

/*
 * JS returns arrays as QVariantList or a(v) in terms of D-Bus,
 * but ConnMan requires some properties to be lists of strings
 * or a(s) thus this function.
 */
QVariantMap adaptToConnmanProperties(const QVariantMap &map)
{
    QVariantMap buffer;
    Q_FOREACH (const QString &key, map.keys()) {
        if (map.value(key).type() == QVariant::List) {
            QStringList strList;
            Q_FOREACH (const QVariant &value, map.value(key).toList()) {
                strList.append(value.toString());
            }
            buffer.insert(key, strList);
        } else {
            buffer.insert(key, map.value(key));
        }
    }
    return buffer;
}

const QString NetworkService::Name("Name");
const QString NetworkService::State("State");
const QString NetworkService::Type("Type");
const QString NetworkService::Security("Security");
const QString NetworkService::Strength("Strength");
const QString NetworkService::Error("Error");
const QString NetworkService::Favorite("Favorite");
const QString NetworkService::AutoConnect("AutoConnect");
const QString NetworkService::IPv4("IPv4");
const QString NetworkService::IPv4Config("IPv4.Configuration");
const QString NetworkService::IPv6("IPv6");
const QString NetworkService::IPv6Config("IPv6.Configuration");
const QString NetworkService::Nameservers("Nameservers");
const QString NetworkService::NameserversConfig("Nameservers.Configuration");
const QString NetworkService::Domains("Domains");
const QString NetworkService::DomainsConfig("Domains.Configuration");
const QString NetworkService::Proxy("Proxy");
const QString NetworkService::ProxyConfig("Proxy.Configuration");
const QString NetworkService::Ethernet("Ethernet");
const QString NetworkService::Roaming("Roaming");
//const QString NetworkService::Immutable("Immutable");
//const QString NetworkService::Privacy("Privacy");
const QString NetworkService::Timeservers("Timeservers");
const QString NetworkService::TimeserversConfig("Timeservers.Configuration");
//const QString NetworkService::Provider("Provider");

const QString NetworkService::BSSID("BSSID");
const QString NetworkService::MaxRate("MaxRate");
const QString NetworkService::Frequency("Frequency");
const QString NetworkService::EncryptionMode("EncryptionMode");
const QString NetworkService::Hidden("Hidden");

NetworkService::NetworkService(const QString &path, const QVariantMap &properties, QObject* parent)
  : QObject(parent),
    m_service(NULL),
    m_path(path),
    m_propertiesCache(properties),
    isConnected(false)
{
    qRegisterMetaType<NetworkService *>();

    Q_ASSERT(!path.isEmpty());
    reconnectServiceInterface();
}

NetworkService::NetworkService(QObject* parent)
    : QObject(parent),
      m_service(NULL),
      m_path(QString()),
      isConnected(false)
{
    qRegisterMetaType<NetworkService *>();
}

NetworkService::~NetworkService() {}

const QString NetworkService::name() const
{
    if (m_propertiesCache.contains(Name))
        return m_propertiesCache.value(Name).toString();
    return QString();
}

const QString NetworkService::state() const
{
    if (m_propertiesCache.contains(State))
        return m_propertiesCache.value(State).toString();
    return QString();
}

const QString NetworkService::error() const
{
    if (m_propertiesCache.contains(Error))
        return m_propertiesCache.value(Error).toString();
    return QString();
}

const QString NetworkService::type() const
{
    if (m_propertiesCache.contains(Type))
        return m_propertiesCache.value(Type).toString();
    return QString();
}

const QStringList NetworkService::security() const
{
    if (m_propertiesCache.contains(Security))
        return m_propertiesCache.value(Security).toStringList();
    return QStringList();
}

uint NetworkService::strength() const
{
    if (m_propertiesCache.contains(Strength))
        return m_propertiesCache.value(Strength).toUInt();
    return 0;
}

bool NetworkService::favorite() const
{
    if (m_propertiesCache.contains(Favorite))
        return m_propertiesCache.value(Favorite).toBool();
    return false;
}

bool NetworkService::autoConnect() const
{
    if (m_propertiesCache.contains(AutoConnect))
        return m_propertiesCache.value(AutoConnect).toBool();
    return false;
}

const QString NetworkService::path() const
{
    return m_path;
}

const QVariantMap NetworkService::ipv4() const
{
    if (m_propertiesCache.contains(IPv4))
        return qdbus_cast<QVariantMap>(m_propertiesCache.value(IPv4));
    return QVariantMap();
}

const QVariantMap NetworkService::ipv4Config() const
{
    if (m_propertiesCache.contains(IPv4Config))
        return qdbus_cast<QVariantMap>(m_propertiesCache.value(IPv4Config));
    return QVariantMap();
}

const QVariantMap NetworkService::ipv6() const
{
    if (m_propertiesCache.contains(IPv6))
        return qdbus_cast<QVariantMap>(m_propertiesCache.value(IPv6));
    return QVariantMap();
}

const QVariantMap NetworkService::ipv6Config() const
{
    if (m_propertiesCache.contains(IPv6Config))
        return qdbus_cast<QVariantMap>(m_propertiesCache.value(IPv6Config));
    return QVariantMap();
}

const QStringList NetworkService::nameservers() const
{
    if (m_propertiesCache.contains(Nameservers))
        return m_propertiesCache.value(Nameservers).toStringList();
    return QStringList();
}

const QStringList NetworkService::nameserversConfig() const
{
    if (m_propertiesCache.contains(NameserversConfig))
        return m_propertiesCache.value(NameserversConfig).toStringList();
    return QStringList();
}

const QStringList NetworkService::domains() const
{
    if (m_propertiesCache.contains(Domains))
        return m_propertiesCache.value(Domains).toStringList();
    return QStringList();
}

const QStringList NetworkService::domainsConfig() const
{
    if (m_propertiesCache.contains(DomainsConfig))
        return m_propertiesCache.value(DomainsConfig).toStringList();
    return QStringList();
}

const QVariantMap NetworkService::proxy() const
{
    if (m_propertiesCache.contains(Proxy))
        return qdbus_cast<QVariantMap>(m_propertiesCache.value(Proxy));
    return QVariantMap();
}

const QVariantMap NetworkService::proxyConfig() const
{
    if (m_propertiesCache.contains(ProxyConfig))
        return qdbus_cast<QVariantMap>(m_propertiesCache.value(ProxyConfig));
    return QVariantMap();
}

const QVariantMap NetworkService::ethernet() const
{
    if (m_propertiesCache.contains(Ethernet))
        return qdbus_cast<QVariantMap>(m_propertiesCache.value(Ethernet));
    return QVariantMap();
}

bool NetworkService::roaming() const
{
    if (m_propertiesCache.contains(Roaming))
        return m_propertiesCache.value(Roaming).toBool();
    return false;
}

bool NetworkService::hidden() const
{
    if (m_propertiesCache.contains(Hidden))
        return m_propertiesCache.value(Hidden).toBool();
    return false;
}

void NetworkService::requestConnect()
{
    if (!m_service) {
        return;
    }
    if (connected()) {
        Q_EMIT connectRequestFailed("Already connected");
        return;
    }
    Q_EMIT serviceConnectionStarted();

    // If the service is in the failure state clear the Error property so that we get notified of
    // errors on subsequent connection attempts.
    if (state() == QLatin1String("failure"))
        m_service->ClearProperty(QLatin1String("Error"));

    // increase reply timeout when connecting
    int timeout = CONNECT_TIMEOUT_FAVORITE;
    int old_timeout = m_service->timeout();
    if (!favorite()) {
        timeout = CONNECT_TIMEOUT;
    }
    m_service->setTimeout(timeout);
    QDBusPendingReply<> conn_reply = m_service->Connect();
    m_service->setTimeout(old_timeout);

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(conn_reply, this);
    connect(watcher,
            SIGNAL(finished(QDBusPendingCallWatcher*)),
            this,
            SLOT(handleConnectReply(QDBusPendingCallWatcher*)));
}

void NetworkService::requestDisconnect()
{
    if (m_service) {
        Q_EMIT serviceDisconnectionStarted();
        m_service->Disconnect();
    }
}

void NetworkService::remove()
{
    if (!m_service)
        return;

    QDBusPendingReply<> reply = m_service->Remove();
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            this, SLOT(handleRemoveReply(QDBusPendingCallWatcher*)));
}

void NetworkService::setAutoConnect(bool autoConnected)
{
    if (m_service) {
         QDBusPendingReply<void> reply = m_service->SetProperty(AutoConnect, QDBusVariant(QVariant(autoConnected)));

         QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
         connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
                 this, SLOT(handleAutoConnectReply(QDBusPendingCallWatcher*)));
    }
}

void NetworkService::handleAutoConnectReply(QDBusPendingCallWatcher *watcher)
{
    QDBusPendingReply<> reply = *watcher;
    watcher->deleteLater();

    if (reply.isError())
        qDebug() << reply.error().message();
    // propertyChange should handle emit
}

void NetworkService::setIpv4Config(const QVariantMap &ipv4)
{
    // QDBusPendingReply<void> reply =
    if (m_service)
        m_service->SetProperty(IPv4Config, QDBusVariant(QVariant(ipv4)));
}

void NetworkService::setIpv6Config(const QVariantMap &ipv6)
{
    // QDBusPendingReply<void> reply =
    if (m_service)
        m_service->SetProperty(IPv6Config, QDBusVariant(QVariant(ipv6)));
}

void NetworkService::setNameserversConfig(const QStringList &nameservers)
{
    // QDBusPendingReply<void> reply =
    if (m_service)
        m_service->SetProperty(NameserversConfig, QDBusVariant(QVariant(nameservers)));
}

void NetworkService::setDomainsConfig(const QStringList &domains)
{
    // QDBusPendingReply<void> reply =
    if (m_service)
        m_service->SetProperty(DomainsConfig, QDBusVariant(QVariant(domains)));
}

void NetworkService::setProxyConfig(const QVariantMap &proxy)
{
    // QDBusPendingReply<void> reply =
    if (m_service)
        m_service->SetProperty(ProxyConfig, QDBusVariant(QVariant(adaptToConnmanProperties(proxy))));
}

void NetworkService::resetCounters()
{
    if (m_service)
        m_service->ResetCounters();
}

void NetworkService::handleConnectReply(QDBusPendingCallWatcher *call)
{
    Q_ASSERT(call);
    QDBusPendingReply<> reply = *call;

    if (!reply.isFinished()) {
       qDebug() << "connect() not finished yet";
    }
    if (reply.isError()) {
        qDebug() << "Reply from service.connect(): " << reply.error().message();
        Q_EMIT connectRequestFailed(reply.error().message());
    }

    call->deleteLater();
}

void NetworkService::handleRemoveReply(QDBusPendingCallWatcher *watcher)
{
    QDBusPendingReply<> reply = *watcher;
    watcher->deleteLater();

    if (reply.isError() && reply.error().type() == QDBusError::UnknownObject) {
        // Service is probably out of range trying RemoveSavedService.
        NetConnmanManagerInterface manager(QLatin1String("net.connman"), QLatin1String("/"),
                                           QDBusConnection::systemBus());

        // Remove /net/connman/service/ from front of string.
        manager.RemoveSavedService(m_path.mid(21));
    }
}

void NetworkService::resetProperties()
{
    QMutableMapIterator<QString, QVariant> i(m_propertiesCache);
    while (i.hasNext()) {
        i.next();

        const QString key = i.key();
        i.remove();

        if (key == Name) {
            Q_EMIT nameChanged(name());
        } else if (key == Error) {
            Q_EMIT errorChanged(error());
        } else if (key == State) {
            Q_EMIT stateChanged(state());
            if (isConnected != connected()) {
                isConnected = connected();
                Q_EMIT connectedChanged(isConnected);
            }
        } else if (key == Security) {
            Q_EMIT securityChanged(security());
        } else if (key == Strength) {
            Q_EMIT strengthChanged(strength());
        } else if (key == Favorite) {
            Q_EMIT favoriteChanged(favorite());
        } else if (key == AutoConnect) {
            Q_EMIT autoConnectChanged(autoConnect());
        } else if (key == IPv4) {
            Q_EMIT ipv4Changed(ipv4());
        } else if (key == IPv4Config) {
            Q_EMIT ipv4ConfigChanged(ipv4Config());
        } else if (key == IPv6) {
            Q_EMIT ipv6Changed(ipv6());
        } else if (key == IPv6Config) {
            Q_EMIT ipv6ConfigChanged(ipv6Config());
        } else if (key == Nameservers) {
            Q_EMIT nameserversChanged(nameservers());
        } else if (key == NameserversConfig) {
            Q_EMIT nameserversConfigChanged(nameserversConfig());
        } else if (key == Domains) {
            Q_EMIT domainsChanged(domains());
        } else if (key == DomainsConfig) {
            Q_EMIT domainsConfigChanged(domainsConfig());
        } else if (key == Proxy) {
            Q_EMIT proxyChanged(proxy());
        } else if (key == ProxyConfig) {
            Q_EMIT proxyConfigChanged(proxyConfig());
        } else if (key == Ethernet) {
            Q_EMIT ethernetChanged(ethernet());
        } else if (key == QLatin1String("Type")) {
            Q_EMIT typeChanged(type());
        } else if (key == Roaming) {
            Q_EMIT roamingChanged(roaming());
        } else if (key == Timeservers) {
            Q_EMIT timeserversChanged(timeservers());
        } else if (key == TimeserversConfig) {
            Q_EMIT timeserversConfigChanged(timeserversConfig());
        } else if (key == BSSID) {
            Q_EMIT bssidChanged(bssid());
        } else if (key == MaxRate) {
            Q_EMIT maxRateChanged(maxRate());
        } else if (key == Frequency) {
            Q_EMIT frequencyChanged(frequency());
        } else if (key == EncryptionMode) {
            Q_EMIT encryptionModeChanged(encryptionMode());
        } else if (key == Hidden) {
            Q_EMIT hiddenChanged(hidden());
        }
    }
}

void NetworkService::reconnectServiceInterface()
{
    if (m_service) {
        delete m_service;
        m_service = 0;
    }

    if (m_path.isEmpty())
        return;

    m_service = new NetConnmanServiceInterface("net.connman", m_path,
                                               QDBusConnection::systemBus(), this);

    connect(m_service, SIGNAL(PropertyChanged(QString,QDBusVariant)),
            this, SLOT(updateProperty(QString,QDBusVariant)));

    QTimer::singleShot(500,this,SIGNAL(propertiesReady()));
}

void NetworkService::emitPropertyChange(const QString &name, const QVariant &value)
{
    if (m_propertiesCache.value(name) == value)
        return;

        m_propertiesCache[name] = value;

    if (name == Name) {
        Q_EMIT nameChanged(value.toString());
    } else if (name == Error) {
        Q_EMIT errorChanged(value.toString());
    } else if (name == State) {
        Q_EMIT stateChanged(value.toString());
        if (isConnected != connected()) {
            isConnected = connected();
            Q_EMIT connectedChanged(isConnected);
        }
    } else if (name == Security) {
        Q_EMIT securityChanged(value.toStringList());
    } else if (name == Strength) {
        Q_EMIT strengthChanged(value.toUInt());
    } else if (name == Favorite) {
        Q_EMIT favoriteChanged(value.toBool());
    } else if (name == AutoConnect) {
        Q_EMIT autoConnectChanged(value.toBool());
    } else if (name == IPv4) {
        Q_EMIT ipv4Changed(qdbus_cast<QVariantMap>(m_propertiesCache.value(IPv4)));
    } else if (name == IPv4Config) {
        Q_EMIT ipv4ConfigChanged(qdbus_cast<QVariantMap>(m_propertiesCache.value(IPv4Config)));
    } else if (name == IPv6) {
        Q_EMIT ipv6Changed(qdbus_cast<QVariantMap>(m_propertiesCache.value(IPv6)));
    } else if (name == IPv6Config) {
        Q_EMIT ipv6ConfigChanged(qdbus_cast<QVariantMap>(m_propertiesCache.value(IPv6Config)));
    } else if (name == Nameservers) {
        Q_EMIT nameserversChanged(value.toStringList());
    } else if (name == NameserversConfig) {
        Q_EMIT nameserversConfigChanged(value.toStringList());
    } else if (name == Domains) {
        Q_EMIT domainsChanged(value.toStringList());
    } else if (name == DomainsConfig) {
        Q_EMIT domainsConfigChanged(value.toStringList());
    } else if (name == Proxy) {
        Q_EMIT proxyChanged(qdbus_cast<QVariantMap>(m_propertiesCache.value(Proxy)));
    } else if (name == ProxyConfig) {
        Q_EMIT proxyConfigChanged(qdbus_cast<QVariantMap>(m_propertiesCache.value(ProxyConfig)));
    } else if (name == Ethernet) {
        Q_EMIT ethernetChanged(qdbus_cast<QVariantMap>(m_propertiesCache.value(Ethernet)));
    } else if (name == QLatin1String("Type")) {
        Q_EMIT typeChanged(value.toString());
    } else if (name == Roaming) {
        Q_EMIT roamingChanged(value.toBool());
    } else if (name == Timeservers) {
        Q_EMIT timeserversChanged(value.toStringList());
    } else if (name == TimeserversConfig) {
        Q_EMIT timeserversConfigChanged(value.toStringList());
    } else if (name == BSSID) {
        Q_EMIT bssidChanged(value.toString());
    } else if (name == MaxRate) {
        Q_EMIT maxRateChanged(value.toUInt());
    } else if (name == Frequency) {
        Q_EMIT frequencyChanged(value.toUInt());
    } else if (name == EncryptionMode) {
        Q_EMIT encryptionModeChanged(value.toString());
    } else if (name == Hidden) {
        Q_EMIT hiddenChanged(value.toBool());
    }
}

void NetworkService::getPropertiesFinished(QDBusPendingCallWatcher *call)
{
    QDBusPendingReply<QVariantMap> reply = *call;
    call->deleteLater();

    if (!reply.isError())
        updateProperties(reply.value());
    else
        qDebug() << reply.error().message();
    Q_EMIT propertiesReady();
}

void NetworkService::updateProperty(const QString &name, const QDBusVariant &value)
{
    QVariant tmp = value.variant();

    Q_ASSERT(m_service);
    emitPropertyChange(name,tmp);
}

void NetworkService::updateProperties(const QVariantMap &properties)
{
    QVariantMap::const_iterator it = properties.constBegin(), end = properties.constEnd();
    for ( ; it != end; ++it) {
        emitPropertyChange(it.key(), it.value());
    }
    Q_EMIT propertiesReady();
}

void NetworkService::setPath(const QString &path)
{
    if (path == m_path)
        return;

    m_path = path;
    emit pathChanged(m_path);

    resetProperties();

    reconnectServiceInterface();

    if (!m_service || !m_service->isValid())
        return;

    QDBusPendingReply<QVariantMap> reply = m_service->GetProperties();
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);

    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            this, SLOT(getPropertiesFinished(QDBusPendingCallWatcher*)));
}

bool NetworkService::connected()
{
    if (m_propertiesCache.contains(State)) {
        QString state = m_propertiesCache.value(State).toString();
        if (state == "online" || state == "ready")
            return true;
    }
    return false;
}

QStringList NetworkService::timeservers() const
{
    if (m_propertiesCache.contains(Timeservers))
        return m_propertiesCache.value(Timeservers).toStringList();
    return QStringList();
}

QStringList NetworkService::timeserversConfig() const
{
    if (m_propertiesCache.contains(TimeserversConfig))
        return m_propertiesCache.value(TimeserversConfig).toStringList();
    return QStringList();
}

void NetworkService::setTimeserversConfig(const QStringList &servers)
{
    if (m_service)
        m_service->SetProperty(TimeserversConfig, QDBusVariant(QVariant(servers)));
}

const QString NetworkService::bssid()
{
    if (m_propertiesCache.contains(BSSID))
        return m_propertiesCache.value(BSSID).toString();
    return QString();
}

quint32 NetworkService::maxRate()
{
    if (m_propertiesCache.contains(MaxRate))
        return m_propertiesCache.value(MaxRate).toUInt();
    return 0;
}

quint16 NetworkService::frequency()
{
    if (m_propertiesCache.contains(Frequency))
        return m_propertiesCache.value(Frequency).toUInt();
    return 0;
}

const QString NetworkService::encryptionMode()
{
    if (m_propertiesCache.contains(EncryptionMode))
        return m_propertiesCache.value(EncryptionMode).toString();
    return QString();
}
