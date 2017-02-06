/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include "physicaldeviceproxy_p.h"
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DInput/qabstractphysicaldevice.h>
#include <Qt3DInput/private/qabstractphysicaldeviceproxy_p_p.h>
#include <Qt3DInput/private/inputmanagers_p.h>
#include <QCoreApplication>

QT_BEGIN_NAMESPACE

namespace Qt3DInput {

namespace Input {

PhysicalDeviceProxy::PhysicalDeviceProxy()
    : QBackendNode(QBackendNode::ReadWrite)
    , m_manager(nullptr)
{
}

void PhysicalDeviceProxy::cleanup()
{
    QBackendNode::setEnabled(false);
    m_deviceName.clear();
    m_manager = nullptr;
    m_physicalDeviceId = Qt3DCore::QNodeId();
}

QString PhysicalDeviceProxy::deviceName() const
{
    return m_deviceName;
}

void PhysicalDeviceProxy::setManager(PhysicalDeviceProxyManager *manager)
{
    m_manager = manager;
}

PhysicalDeviceProxyManager *PhysicalDeviceProxy::manager() const
{
    return m_manager;
}

void PhysicalDeviceProxy::setDevice(QAbstractPhysicalDevice *device)
{
    m_physicalDeviceId = Qt3DCore::QNodeId();
    // Move the device to the main thread
    if (device != nullptr) {
        m_physicalDeviceId = device->id();
        device->moveToThread(QCoreApplication::instance()->thread());
    }

    auto e = Qt3DCore::QPropertyUpdatedChangePtr::create(peerId());
    e->setDeliveryFlags(Qt3DCore::QSceneChange::DeliverToAll);
    e->setPropertyName("device");
    e->setValue(QVariant::fromValue(device));
    notifyObservers(e);
}

Qt3DCore::QNodeId PhysicalDeviceProxy::physicalDeviceId() const
{
    return m_physicalDeviceId;
}

void PhysicalDeviceProxy::initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change)
{
    const auto typedChange = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<QAbstractPhysicalDeviceProxyData>>(change);
    const QAbstractPhysicalDeviceProxyData &data = typedChange->data;
    m_deviceName = data.deviceName;

    // Request to load the actual device
    m_manager->addPendingProxyToLoad(peerId());
}

PhysicalDeviceProxyNodeFunctor::PhysicalDeviceProxyNodeFunctor(PhysicalDeviceProxyManager *manager)
    : m_manager(manager)
{
}

Qt3DCore::QBackendNode *PhysicalDeviceProxyNodeFunctor::create(const Qt3DCore::QNodeCreatedChangeBasePtr &change) const
{
    HPhysicalDeviceProxy handle = m_manager->getOrAcquireHandle(change->subjectId());
    PhysicalDeviceProxy *backend = m_manager->data(handle);
    backend->setManager(m_manager);
    return backend;
}

Qt3DCore::QBackendNode *PhysicalDeviceProxyNodeFunctor::get(Qt3DCore::QNodeId id) const
{
    return m_manager->lookupResource(id);
}

void PhysicalDeviceProxyNodeFunctor::destroy(Qt3DCore::QNodeId id) const
{
    m_manager->releaseResource(id);
}

} // namespace Input

} // namespace Qt3DInput

QT_END_NAMESPACE

