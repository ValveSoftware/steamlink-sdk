/*
 * Copyright © 2010, Intel Corporation.
 * Copyright © 2012, Jolla.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#include "networktechnology.h"
#include "connman_technology_interface.h"

const QString NetworkTechnology::Name("Name");
const QString NetworkTechnology::Type("Type");
const QString NetworkTechnology::Powered("Powered");
const QString NetworkTechnology::Connected("Connected");
const QString NetworkTechnology::IdleTimeout("IdleTimeout");
const QString NetworkTechnology::Tethering("Tethering");
const QString NetworkTechnology::TetheringIdentifier("TetheringIdentifier");
const QString NetworkTechnology::TetheringPassphrase("TetheringPassphrase");

NetworkTechnology::NetworkTechnology(const QString &path, const QVariantMap &properties, QObject* parent)
  : QObject(parent),
    m_technology(NULL),
    m_path(QString())
{
    Q_ASSERT(!path.isEmpty());
    m_propertiesCache = properties;
    init(path);
}

NetworkTechnology::NetworkTechnology(QObject* parent)
    : QObject(parent),
      m_technology(NULL),
      m_path(QString())
{
}

NetworkTechnology::~NetworkTechnology()
{
}

void NetworkTechnology::init(const QString &path)
{
    if (path != m_path) {
        m_path = path;

        if (m_technology) {
            delete m_technology;
            m_technology = 0;
            m_propertiesCache.clear();
        }
        Q_EMIT pathChanged(m_path);
        if (m_path.isEmpty())
            return;
        m_technology = new NetConnmanTechnologyInterface("net.connman", path,
                                                         QDBusConnection::systemBus(), this);
        if (!m_technology->isValid()) {
            qWarning() << "Invalid technology: " << path;
            qFatal("Cannot init with invalid technology");
        }

        QDBusPendingReply<QVariantMap> reply = m_technology->GetProperties();
        QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);

        connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
                this, SLOT(getPropertiesFinished(QDBusPendingCallWatcher*)));

        connect(m_technology,
                SIGNAL(PropertyChanged(const QString&, const QDBusVariant&)),
                this,
                SLOT(propertyChanged(const QString&, const QDBusVariant&)));
    }

}

void NetworkTechnology::getPropertiesFinished(QDBusPendingCallWatcher *call)
{
    QDBusPendingReply<QVariantMap> reply = *call;
    call->deleteLater();

    if (!reply.isError()) {

        m_propertiesCache = reply.value();
        Q_FOREACH(const QString &name,m_propertiesCache.keys()) {
            emitPropertyChange(name,m_propertiesCache[name]);
        }
        Q_EMIT propertiesReady();
    }
}

// Public API

// Getters

const QString NetworkTechnology::name() const
{
    if (m_propertiesCache.contains(NetworkTechnology::Name))
        return m_propertiesCache[NetworkTechnology::Name].toString();
    else
        return QString();
}

const QString NetworkTechnology::type() const
{
    if (m_propertiesCache.contains(NetworkTechnology::Type))
        return m_propertiesCache[NetworkTechnology::Type].toString();
    else
        return QString();
}

bool NetworkTechnology::powered() const
{
    if (m_propertiesCache.contains(NetworkTechnology::Powered))
        return m_propertiesCache[NetworkTechnology::Powered].toBool();
    else
        return false;
}

bool NetworkTechnology::connected() const
{
    if (m_propertiesCache.contains(NetworkTechnology::Connected))
        return m_propertiesCache[NetworkTechnology::Connected].toBool();
    else
        return false;
}

const QString NetworkTechnology::objPath() const
{
    if (m_technology)
        return m_technology->path();
    return QString();
}

// Setters

void NetworkTechnology::setPowered(const bool &powered)
{
    if (m_technology)
        m_technology->SetProperty(Powered, QDBusVariant(QVariant(powered)));
}

void NetworkTechnology::scan()
{
    if (!m_technology)
        return;

    QDBusPendingReply<> reply = m_technology->Scan();
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            this, SLOT(scanReply(QDBusPendingCallWatcher*)));
}

// Private
void NetworkTechnology::emitPropertyChange(const QString &name, const QVariant &value)
{
    if (name == Powered) {
        Q_EMIT poweredChanged(value.toBool());
    } else if (name == Connected) {
        Q_EMIT connectedChanged(value.toBool());
    } else if (name == IdleTimeout) {
      Q_EMIT idleTimeoutChanged(value.toUInt());
    } else if (name == Tethering) {
      Q_EMIT tetheringChanged(value.toBool());
    } else if (name == TetheringIdentifier) {
      Q_EMIT tetheringIdChanged(value.toString());
    } else if (name == TetheringPassphrase) {
      Q_EMIT tetheringPassphraseChanged(value.toString());
    }
}

void NetworkTechnology::propertyChanged(const QString &name, const QDBusVariant &value)
{
    QVariant tmp = value.variant();

    Q_ASSERT(m_technology);

    m_propertiesCache[name] = tmp;
    emitPropertyChange(name,tmp);
}

void NetworkTechnology::scanReply(QDBusPendingCallWatcher *call)
{
    Q_EMIT scanFinished();

    call->deleteLater();
}

QString NetworkTechnology::path() const
{
    return m_path;
}

void NetworkTechnology::setPath(const QString &path)
{
    if (path != m_path && !path.isEmpty()) {
        init(path);
    }
}

quint32 NetworkTechnology::idleTimeout() const
{
    if (m_propertiesCache.contains(NetworkTechnology::IdleTimeout))
        return m_propertiesCache[NetworkTechnology::IdleTimeout].toUInt();
    else
        return 0;
}

void NetworkTechnology::setIdleTimeout(quint32 timeout)
{
    if (m_technology)
        m_technology->SetProperty(IdleTimeout, QDBusVariant(QVariant(timeout)));
}

bool NetworkTechnology::tethering() const
{
    if (m_propertiesCache.contains(NetworkTechnology::Tethering))
        return m_propertiesCache[NetworkTechnology::Tethering].toBool();
    else
        return false;
}

void NetworkTechnology::setTethering(bool b)
{
    if (m_technology)
        m_technology->SetProperty(Tethering, QDBusVariant(QVariant(b)));
}


QString NetworkTechnology::tetheringId() const
{
    if (m_propertiesCache.contains(NetworkTechnology::TetheringIdentifier))
        return m_propertiesCache[NetworkTechnology::TetheringIdentifier].toString();
    else
        return QString();
}

void NetworkTechnology::setTetheringId(const QString &id)
{
    if (m_technology)
        m_technology->SetProperty(TetheringIdentifier, QDBusVariant(QVariant(id)));
}

QString NetworkTechnology::tetheringPassphrase() const
{
    if (m_propertiesCache.contains(NetworkTechnology::TetheringPassphrase))
        return m_propertiesCache[NetworkTechnology::TetheringPassphrase].toString();
    else
        return QString();

}

void NetworkTechnology::setTetheringPassphrase(const QString &pass)
{
    if (m_technology)
        m_technology->SetProperty(TetheringPassphrase, QDBusVariant(QVariant(pass)));
}
