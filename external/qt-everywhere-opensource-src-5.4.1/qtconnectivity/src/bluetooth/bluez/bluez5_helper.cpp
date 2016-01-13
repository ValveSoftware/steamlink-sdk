/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
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

#include <QtCore/QGlobalStatic>
#include <QtCore/QLoggingCategory>
#include <QtCore/QMap>
#include "bluez5_helper_p.h"
#include "objectmanager_p.h"
#include "properties_p.h"
#include "adapter1_bluez5_p.h"

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_BT_BLUEZ)

typedef enum Bluez5TestResultType
{
    Unknown,
    Bluez4,
    Bluez5
} Bluez5TestResult;

Q_GLOBAL_STATIC_WITH_ARGS(Bluez5TestResult, bluezVersion, (Bluez5TestResult::Unknown));

bool isBluez5()
{
    if (*bluezVersion() == Bluez5TestResultType::Unknown) {
        OrgFreedesktopDBusObjectManagerInterface manager(QStringLiteral("org.bluez"),
                                                         QStringLiteral("/"),
                                                         QDBusConnection::systemBus());

        qDBusRegisterMetaType<InterfaceList>();
        qDBusRegisterMetaType<ManagedObjectList>();

        QDBusPendingReply<ManagedObjectList> reply = manager.GetManagedObjects();
        reply.waitForFinished();
        if (reply.isError()) {
            *bluezVersion() = Bluez5TestResultType::Bluez4;
            qCDebug(QT_BT_BLUEZ) << "Bluez 4 detected.";
        } else {
            *bluezVersion() = Bluez5TestResultType::Bluez5;
            qCDebug(QT_BT_BLUEZ) << "Bluez 5 detected.";
        }
    }

    return (*bluezVersion() == Bluez5TestResultType::Bluez5);
}

struct AdapterData
{
public:
    AdapterData() : reference(1), wasListeningAlready(false), propteryListener(0) {}

    int reference;
    bool wasListeningAlready;
    OrgFreedesktopDBusPropertiesInterface *propteryListener;
};

class QtBluezDiscoveryManagerPrivate
{
public:
    QMap<QString, AdapterData *> references;
    OrgFreedesktopDBusObjectManagerInterface *manager;
};

Q_GLOBAL_STATIC(QtBluezDiscoveryManager, discoveryManager)

/*!
    \internal
    \class QtBluezDiscoveryManager

    This class manages the access to "org.bluez.Adapter1::Start/StopDiscovery.

    The flag is a system flag. We want to ensure that the various Qt classes don't turn
    the flag on and off and thereby get into their way. If some other system component
    changes the flag (e.g. adapter removed) we notify all Qt classes about the change by emitting
    \l discoveryInterrupted(QString). Classes should indicate this via an appropriate
    error message to the user.

    Once the signal was emitted, all existing requests for discovery mode on the same adapter
    have to be renewed via \l registerDiscoveryInterest(QString).
*/

QtBluezDiscoveryManager::QtBluezDiscoveryManager(QObject *parent) :
    QObject(parent)
{
    qCDebug(QT_BT_BLUEZ) << "Creating QtBluezDiscoveryManager";
    d = new QtBluezDiscoveryManagerPrivate();

    d->manager = new OrgFreedesktopDBusObjectManagerInterface(
                QStringLiteral("org.bluez"), QStringLiteral("/"),
                QDBusConnection::systemBus(), this);
    connect(d->manager, SIGNAL(InterfacesRemoved(QDBusObjectPath,QStringList)),
            SLOT(InterfacesRemoved(QDBusObjectPath,QStringList)));
}

QtBluezDiscoveryManager::~QtBluezDiscoveryManager()
{
    qCDebug(QT_BT_BLUEZ) << "Destroying QtBluezDiscoveryManager";

    foreach (const QString &adapterPath, d->references.keys()) {
        AdapterData *data = d->references.take(adapterPath);
        delete data->propteryListener;

        // turn discovery off if it wasn't on already
        if (!data->wasListeningAlready) {
            OrgBluezAdapter1Interface iface(QStringLiteral("org.bluez"), adapterPath,
                                            QDBusConnection::systemBus());
            iface.StopDiscovery();
        }

        delete data;
    }

    delete d;
}

QtBluezDiscoveryManager *QtBluezDiscoveryManager::instance()
{
    if (isBluez5())
        return discoveryManager();

    Q_ASSERT(false);
    return 0;
}

bool QtBluezDiscoveryManager::registerDiscoveryInterest(const QString &adapterPath)
{
    if (adapterPath.isEmpty())
        return false;

    // already monitored adapter? -> increase ref count -> done
    if (d->references.contains(adapterPath)) {
        d->references[adapterPath]->reference++;
        return true;
    }

    AdapterData *data = new AdapterData();

    OrgFreedesktopDBusPropertiesInterface *propIface = new OrgFreedesktopDBusPropertiesInterface(
                QStringLiteral("org.bluez"), adapterPath, QDBusConnection::systemBus());
    connect(propIface, SIGNAL(PropertiesChanged(QString,QVariantMap,QStringList)),
            SLOT(PropertiesChanged(QString,QVariantMap,QStringList)));
    data->propteryListener = propIface;

    OrgBluezAdapter1Interface iface(QStringLiteral("org.bluez"), adapterPath,
                                    QDBusConnection::systemBus());
    data->wasListeningAlready = iface.discovering();

    d->references[adapterPath] = data;

    if (!data->wasListeningAlready)
        iface.StartDiscovery();

    return true;
}

void QtBluezDiscoveryManager::unregisterDiscoveryInterest(const QString &adapterPath)
{
    if (!d->references.contains(adapterPath))
        return;

    AdapterData *data = d->references[adapterPath];
    data->reference--;

    if (data->reference > 0) // more than one client requested discovery mode
        return;

    d->references.remove(adapterPath);
    if (!data->wasListeningAlready) { // Qt turned discovery mode on, Qt has to turn it off again
        OrgBluezAdapter1Interface iface(QStringLiteral("org.bluez"), adapterPath,
                                        QDBusConnection::systemBus());
        iface.StopDiscovery();
    }

    delete data->propteryListener;
    delete data;
}

//void QtBluezDiscoveryManager::dumpState() const
//{
//    qCDebug(QT_BT_BLUEZ) << "-------------------------";
//    if (d->references.isEmpty()) {
//        qCDebug(QT_BT_BLUEZ) << "No running registration";
//    } else {
//        foreach (const QString &path, d->references.keys()) {
//            qCDebug(QT_BT_BLUEZ) << path << "->" << d->references[path]->reference;
//        }
//    }
//    qCDebug(QT_BT_BLUEZ) << "-------------------------";
//}

void QtBluezDiscoveryManager::InterfacesRemoved(const QDBusObjectPath &object_path,
                                                const QStringList &interfaces)
{
    if (!d->references.contains(object_path.path())
            || !interfaces.contains(QStringLiteral("org.bluez.Adapter1")))
        return;

    removeAdapterFromMonitoring(object_path.path());
}

void QtBluezDiscoveryManager::PropertiesChanged(const QString &interface,
                                                const QVariantMap &changed_properties,
                                                const QStringList &invalidated_properties)
{
    Q_UNUSED(invalidated_properties);

    OrgFreedesktopDBusPropertiesInterface *propIface =
            qobject_cast<OrgFreedesktopDBusPropertiesInterface *>(sender());

    if (!propIface)
        return;

    if (interface == QStringLiteral("org.bluez.Adapter1")
            && d->references.contains(propIface->path())
            && changed_properties.contains(QStringLiteral("Discovering"))) {
        bool isDiscovering = changed_properties.value(QStringLiteral("Discovering")).toBool();
        if (!isDiscovering)
            removeAdapterFromMonitoring(propIface->path());
    }
}

void QtBluezDiscoveryManager::removeAdapterFromMonitoring(const QString &dbusPath)
{
    // remove adapter from monitoring
    AdapterData *data = d->references.take(dbusPath);
    delete data->propteryListener;
    delete data;

    emit discoveryInterrupted(dbusPath);
}

/*!
    Finds the path for the local adapter with \a wantedAddress or an empty string
    if no local adapter with the given address can be found.
    If \a wantedAddress is \c null it returns the first/default adapter or an empty
    string if none is available.

    If \a ok is false the lookup was aborted due to a dbus error and this function
    returns an empty string.
 */
QString findAdapterForAddress(const QBluetoothAddress &wantedAddress, bool *ok = 0)
{
    OrgFreedesktopDBusObjectManagerInterface manager(QStringLiteral("org.bluez"),
                                                     QStringLiteral("/"),
                                                     QDBusConnection::systemBus());

    QDBusPendingReply<ManagedObjectList> reply = manager.GetManagedObjects();
    reply.waitForFinished();
    if (reply.isError()) {
        if (ok)
            *ok = false;

        return QString();
    }

    typedef QPair<QString, QBluetoothAddress> AddressForPathType;
    QList<AddressForPathType> localAdapters;

    foreach (const QDBusObjectPath &path, reply.value().keys()) {
        const InterfaceList ifaceList = reply.value().value(path);
        foreach (const QString &iface, ifaceList.keys()) {
            if (iface == QStringLiteral("org.bluez.Adapter1")) {
                AddressForPathType pair;
                pair.first = path.path();
                pair.second = QBluetoothAddress(ifaceList.value(iface).value(
                                          QStringLiteral("Address")).toString());
                if (!pair.second.isNull())
                    localAdapters.append(pair);
                break;
            }
        }
    }

    if (ok)
        *ok = true;

    if (localAdapters.isEmpty())
        return QString(); // -> no local adapter found

    if (wantedAddress.isNull())
        return localAdapters.front().first; // -> return first found adapter

    foreach (const AddressForPathType &pair, localAdapters) {
        if (pair.second == wantedAddress)
            return pair.first; // -> found local adapter with wanted address
    }

    return QString(); // nothing matching found
}

QT_END_NAMESPACE
