/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
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

#include "logicaldevice_p.h"
#include <Qt3DInput/qlogicaldevice.h>
#include <Qt3DInput/qaxis.h>
#include <Qt3DInput/qaction.h>
#include <Qt3DInput/private/inputmanagers_p.h>
#include <Qt3DInput/private/qlogicaldevice_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DInput {

namespace Input {

LogicalDevice::LogicalDevice()
    : QBackendNode()
{
}

void LogicalDevice::initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change)
{
    const auto typedChange = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<QLogicalDeviceData>>(change);
    const auto &data = typedChange->data;
    m_actions = data.actionIds;
    m_axes = data.axisIds;
}

void LogicalDevice::cleanup()
{
    QBackendNode::setEnabled(false);
    m_actions.clear();
    m_axes.clear();
}

void LogicalDevice::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    switch (e->type()) {
    case Qt3DCore::PropertyValueAdded: {
        const auto change = qSharedPointerCast<Qt3DCore::QPropertyNodeAddedChange>(e);
        if (change->propertyName() == QByteArrayLiteral("axis"))
            m_axes.push_back(change->addedNodeId());
        else if (change->propertyName() == QByteArrayLiteral("action"))
            m_actions.push_back(change->addedNodeId());
        break;
    }

    case Qt3DCore::PropertyValueRemoved: {
        const auto change = qSharedPointerCast<Qt3DCore::QPropertyNodeRemovedChange>(e);
        if (change->propertyName() == QByteArrayLiteral("axis"))
            m_axes.removeOne(change->removedNodeId());
        else if (change->propertyName() == QByteArrayLiteral("action"))
            m_actions.removeOne(change->removedNodeId());
        break;
    }

    default:
        break;
    }
    QBackendNode::sceneChangeEvent(e);
}

LogicalDeviceNodeFunctor::LogicalDeviceNodeFunctor(LogicalDeviceManager *manager)
    : m_manager(manager)
{
}

Qt3DCore::QBackendNode *LogicalDeviceNodeFunctor::create(const Qt3DCore::QNodeCreatedChangeBasePtr &change) const
{
    HLogicalDevice handle = m_manager->getOrAcquireHandle(change->subjectId());
    LogicalDevice *backend = m_manager->data(handle);
    m_manager->addActiveDevice(handle);
    return backend;
}

Qt3DCore::QBackendNode *LogicalDeviceNodeFunctor::get(Qt3DCore::QNodeId id) const
{
    return m_manager->lookupResource(id);
}

void LogicalDeviceNodeFunctor::destroy(Qt3DCore::QNodeId id) const
{
    HLogicalDevice handle = m_manager->lookupHandle(id);
    m_manager->releaseResource(id);
    m_manager->removeActiveDevice(handle);
}

} // namespace Input

} // namespace Qt3DInput

QT_END_NAMESPACE
