/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
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

#include <QtCore/QLoggingCategory>

#include "remotedevicemanager_p.h"
#include "bluez5_helper_p.h"
#include "device1_bluez5_p.h"
#include "objectmanager_p.h"

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_BT_BLUEZ)

/*!
 * Convenience wrapper around org.bluez.Device1 management
 *
 * Very simple and not thread safe.
 */

RemoteDeviceManager::RemoteDeviceManager(
        const QBluetoothAddress &address, QObject *parent)
    : QObject(parent), localAddress(address)
{
    if (!isBluez5())
        return;

    bool ok = false;
    adapterPath = findAdapterForAddress(address, &ok);
    if (!ok || adapterPath.isEmpty()) {
        qCWarning(QT_BT_BLUEZ) << "Cannot initialize RemoteDeviceManager";
    }
}

bool RemoteDeviceManager::scheduleJob(
        JobType job, const QVector<QBluetoothAddress> &remoteDevices)
{
    if (adapterPath.isEmpty())
        return false;

    for (const auto& remote : remoteDevices)
        jobQueue.push_back(std::make_pair(job, remote));

    QTimer::singleShot(0, this, [this](){ runQueue(); });
    return true;
}

void RemoteDeviceManager::runQueue()
{
    if (jobInProgress || adapterPath.isEmpty())
        return;

    if (jobQueue.empty())
        return;

    jobInProgress = true;
    switch (jobQueue.front().first) {
    case JobType::JobDisconnectDevice:
        disconnectDevice(jobQueue.front().second);
        break;
    default:
        break;
    }
}

void RemoteDeviceManager::prepareNextJob()
{
    Q_ASSERT(!jobQueue.empty());

    jobQueue.pop_front();
    jobInProgress = false;

    if (jobQueue.empty())
        emit finished();
    else
        runQueue();
}

void RemoteDeviceManager::disconnectDevice(const QBluetoothAddress &remote)
{
    // collect initial set of information
    OrgFreedesktopDBusObjectManagerInterface managerBluez5(
                                               QStringLiteral("org.bluez"),
                                               QStringLiteral("/"),
                                               QDBusConnection::systemBus(), this);
    QDBusPendingReply<ManagedObjectList> reply = managerBluez5.GetManagedObjects();
    reply.waitForFinished();
    if (reply.isError()) {
        QTimer::singleShot(0, this, [this](){ prepareNextJob(); });
        return;
    }

    bool jobStarted = false;
    ManagedObjectList managedObjectList = reply.value();
    for (auto it = managedObjectList.constBegin(); it != managedObjectList.constEnd(); ++it) {
        const QDBusObjectPath &path = it.key();
        const InterfaceList &ifaceList = it.value();

        for (auto jt = ifaceList.constBegin(); jt != ifaceList.constEnd(); ++jt) {
            const QString &iface = jt.key();

            if (path.path().indexOf(adapterPath) != 0)
                continue; //devices whose path doesn't start with same path we skip

            if (iface != QStringLiteral("org.bluez.Device1"))
                continue;

            const QBluetoothAddress foundAddress(ifaceList.value(iface).value(QStringLiteral("Address")).toString());
            if (foundAddress != remote)
                continue;

            // found the correct Device1 path
            OrgBluezDevice1Interface* device1 = new OrgBluezDevice1Interface(QStringLiteral("org.bluez"),
                                                                             path.path(),
                                                                             QDBusConnection::systemBus(),
                                                                             this);
            QDBusPendingReply<> asyncReply = device1->Disconnect();
            QDBusPendingCallWatcher* watcher = new QDBusPendingCallWatcher(asyncReply, this);
            const auto watcherFinished = [this, device1](QDBusPendingCallWatcher* call) {
                call->deleteLater();
                device1->deleteLater();
                prepareNextJob();
            };
            connect(watcher, &QDBusPendingCallWatcher::finished, this, watcherFinished);
            jobStarted = true;
            break;
        }
    }

    if (!jobStarted)
        QTimer::singleShot(0, this, [this](){ prepareNextJob(); });
}

QT_END_NAMESPACE
