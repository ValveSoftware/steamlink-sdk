/***************************************************************************
**
** Copyright (C) 2016 BlackBerry Limited. All rights reserved.
** Copyright (C) 2016 BasysKom GmbH.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtNfc module of the Qt Toolkit.
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

#include "qnearfieldmanager_neard_p.h"
#include "qnearfieldtarget_neard_p.h"

#include "neard/adapter_p.h"
#include "neard/dbusproperties_p.h"
#include "neard/dbusobjectmanager_p.h"

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_NFC_NEARD)

// TODO We need a constructor that lets us select an adapter
QNearFieldManagerPrivateImpl::QNearFieldManagerPrivateImpl()
    : QNearFieldManagerPrivate(),
      m_neardHelper(NeardHelper::instance())
{
    QDBusPendingReply<ManagedObjectList> reply = m_neardHelper->dbusObjectManager()->GetManagedObjects();
    reply.waitForFinished();
    if (reply.isError()) {
        qCWarning(QT_NFC_NEARD) << "Error getting managed objects";
        return;
    }

    bool found = false;
    foreach (const QDBusObjectPath &path, reply.value().keys()) {
        const InterfaceList ifaceList = reply.value().value(path);
        foreach (const QString &iface, ifaceList.keys()) {
            if (iface == QStringLiteral("org.neard.Adapter")) {
                found = true;
                m_adapterPath = path.path();
                qCDebug(QT_NFC_NEARD) << "org.neard.Adapter found for path" << m_adapterPath;
                break;
            }
        }

        if (found)
            break;
    }

    if (!found) {
        qCWarning(QT_NFC_NEARD) << "no adapter found, neard daemon running?";
    } else {
        connect(m_neardHelper, SIGNAL(tagFound(QDBusObjectPath)),
                this,          SLOT(handleTagFound(QDBusObjectPath)));
        connect(m_neardHelper, SIGNAL(tagRemoved(QDBusObjectPath)),
                this,          SLOT(handleTagRemoved(QDBusObjectPath)));
    }
}

QNearFieldManagerPrivateImpl::~QNearFieldManagerPrivateImpl()
{
    stopTargetDetection();
}

bool QNearFieldManagerPrivateImpl::isAvailable() const
{
    if (!m_neardHelper->dbusObjectManager()->isValid() || m_adapterPath.isNull()) {
        qCWarning(QT_NFC_NEARD) << "dbus object manager invalid or adapter path invalid";
        return false;
    }

    QDBusPendingReply<ManagedObjectList> reply = m_neardHelper->dbusObjectManager()->GetManagedObjects();
    reply.waitForFinished();
    if (reply.isError()) {
        qCWarning(QT_NFC_NEARD) << "error getting managed objects";
        return false;
    }

    foreach (const QDBusObjectPath &path, reply.value().keys()) {
        if (m_adapterPath == path.path())
            return true;
    }

    return false;
}

bool QNearFieldManagerPrivateImpl::startTargetDetection()
{
    qCDebug(QT_NFC_NEARD) << "starting target detection";
    if (!isAvailable())
        return false;

    OrgFreedesktopDBusPropertiesInterface dbusProperties(QStringLiteral("org.neard"),
                                                         m_adapterPath,
                                                         QDBusConnection::systemBus());

    if (!dbusProperties.isValid()) {
        qCWarning(QT_NFC_NEARD) << "dbus property interface invalid";
        return false;
    }

    // check if the adapter is currently polling
    QDBusPendingReply<QDBusVariant> replyPolling = dbusProperties.Get(QStringLiteral("org.neard.Adapter"),
                                                                      QStringLiteral("Polling"));
    replyPolling.waitForFinished();
    if (!replyPolling.isError()) {
        if (replyPolling.value().variant().toBool()) {
            qCDebug(QT_NFC_NEARD) << "adapter is already polling";
            return true;
        }
    } else {
        qCWarning(QT_NFC_NEARD) << "error getting 'Polling' state from property interface";
        return false;
    }

    // check if the adapter it powered
    QDBusPendingReply<QDBusVariant> replyPowered = dbusProperties.Get(QStringLiteral("org.neard.Adapter"),
                                                                      QStringLiteral("Powered"));
    replyPowered.waitForFinished();
    if (!replyPowered.isError()) {
        if (replyPowered.value().variant().toBool()) {
            qCDebug(QT_NFC_NEARD) << "adapter is already powered";
        } else {
            QDBusPendingReply<QDBusVariant> replyTryPowering = dbusProperties.Set(QStringLiteral("org.neard.Adapter"),
                                                                                  QStringLiteral("Powered"),
                                                                                  QDBusVariant(true));
            replyTryPowering.waitForFinished();
            if (!replyTryPowering.isError()) {
                qCDebug(QT_NFC_NEARD) << "powering adapter";
            }
        }
    } else {
        qCWarning(QT_NFC_NEARD) << "error getting 'Powered' state from property interface";
        return false;
    }

    // create adapter and start poll loop
    OrgNeardAdapterInterface neardAdapter(QStringLiteral("org.neard"),
                                          m_adapterPath,
                                          QDBusConnection::systemBus());

    // possible modes: "Target", "Initiator", "Dual"
    QDBusPendingReply<> replyPollLoop = neardAdapter.StartPollLoop(QStringLiteral("Dual"));
    replyPollLoop.waitForFinished();
    if (replyPollLoop.isError()) {
        qCWarning(QT_NFC_NEARD) << "error when starting polling";
        return false;
    } else {
        qCDebug(QT_NFC_NEARD) << "successfully started polling";
    }

    return true;
}

void QNearFieldManagerPrivateImpl::stopTargetDetection()
{
    qCDebug(QT_NFC_NEARD) << "stopping target detection";
    if (!isAvailable())
        return;

    OrgFreedesktopDBusPropertiesInterface dbusProperties(QStringLiteral("org.neard"),
                                                         m_adapterPath,
                                                         QDBusConnection::systemBus());

    if (!dbusProperties.isValid()) {
        qCWarning(QT_NFC_NEARD) << "dbus property interface invalid";
        return;
    }

    // check if the adapter is currently polling
    QDBusPendingReply<QDBusVariant> replyPolling = dbusProperties.Get(QStringLiteral("org.neard.Adapter"),
                                                                      QStringLiteral("Polling"));
    replyPolling.waitForFinished();
    if (!replyPolling.isError()) {
        if (replyPolling.value().variant().toBool()) {
            // create adapter and stop poll loop
            OrgNeardAdapterInterface neardAdapter(QStringLiteral("org.neard"),
                                                  m_adapterPath,
                                                  QDBusConnection::systemBus());

            QDBusPendingReply<> replyStopPolling = neardAdapter.StopPollLoop();
            replyStopPolling.waitForFinished();
            if (replyStopPolling.isError())
                qCWarning(QT_NFC_NEARD) << "error when stopping polling";
            else
                qCDebug(QT_NFC_NEARD) << "successfully stopped polling";
        } else {
            qCDebug(QT_NFC_NEARD) << "already stopped polling";
        }
    } else {
        qCWarning(QT_NFC_NEARD) << "error getting 'Polling' state from property interface";
    }
}

int QNearFieldManagerPrivateImpl::registerNdefMessageHandler(QObject *object, const QMetaMethod &method)
{
    Q_UNUSED(object);
    Q_UNUSED(method);
    return -1;
}

int QNearFieldManagerPrivateImpl::registerNdefMessageHandler(const QNdefFilter &filter, QObject *object, const QMetaMethod &method)
{
    Q_UNUSED(filter);
    Q_UNUSED(object);
    Q_UNUSED(method);
    return -1;
}

bool QNearFieldManagerPrivateImpl::unregisterNdefMessageHandler(int handlerId)
{
    Q_UNUSED(handlerId);
    return false;
}

void QNearFieldManagerPrivateImpl::requestAccess(QNearFieldManager::TargetAccessModes accessModes)
{
    Q_UNUSED(accessModes);
}

void QNearFieldManagerPrivateImpl::releaseAccess(QNearFieldManager::TargetAccessModes accessModes)
{
    Q_UNUSED(accessModes);
}

void QNearFieldManagerPrivateImpl::handleTagFound(const QDBusObjectPath &path)
{
    NearFieldTarget<QNearFieldTarget> *nfTag = new NearFieldTarget<QNearFieldTarget>(this, path);
    m_activeTags.insert(path.path(), nfTag);
    emit targetDetected(nfTag);
}

void QNearFieldManagerPrivateImpl::handleTagRemoved(const QDBusObjectPath &path)
{
    const QString adapterPath = path.path();
    if (m_activeTags.contains(adapterPath)) {
        QNearFieldTarget *nfTag = m_activeTags.value(adapterPath);
        m_activeTags.remove(adapterPath);
        emit targetLost(nfTag);
    }
}

QT_END_NAMESPACE
