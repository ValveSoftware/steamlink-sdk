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

#include "qabstractphysicaldevicebackendnode_p.h"
#include "qabstractphysicaldevicebackendnode_p_p.h"
#include "qabstractphysicaldevice.h"
#include "qaxissetting.h"
#include "inputhandler_p.h"
#include "inputmanagers_p.h"

#include <Qt3DInput/qinputaspect.h>
#include <Qt3DInput/qphysicaldevicecreatedchange.h>
#include <Qt3DInput/private/qinputaspect_p.h>

#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>
#include <Qt3DCore/private/qabstractaspect_p.h>

#include <cmath>

QT_BEGIN_NAMESPACE

namespace {

Q_DECL_CONSTEXPR int signum(float v)
{
    return (v > 0.0f) - (v < 0.0f);
}

}

namespace Qt3DInput {

QAbstractPhysicalDeviceBackendNodePrivate::QAbstractPhysicalDeviceBackendNodePrivate(Qt3DCore::QBackendNode::Mode mode)
    : Qt3DCore::QBackendNodePrivate(mode)
    , m_axisSettings()
    , m_inputAspect(nullptr)
{
}

void QAbstractPhysicalDeviceBackendNodePrivate::addAxisSetting(int axisIdentifier, Qt3DCore::QNodeId axisSettingsId)
{
    Input::AxisIdSetting axisIdSetting;
    axisIdSetting.m_axisIdentifier = axisIdentifier;
    axisIdSetting.m_axisSettingsId = axisSettingsId;

    // Replace if already present, otherwise append
    bool replaced = false;
    QVector<Input::AxisIdSetting>::iterator it;
    QVector<Input::AxisIdSetting>::iterator end = m_axisSettings.end();
    for (it = m_axisSettings.begin(); it != end; ++it) {
        if (it->m_axisIdentifier == axisIdentifier) {
            *it = axisIdSetting;
            replaced = true;
            break;
        }
    }

    if (!replaced)
        m_axisSettings.push_back(axisIdSetting);
}

void QAbstractPhysicalDeviceBackendNodePrivate::removeAxisSetting(Qt3DCore::QNodeId axisSettingsId)
{
    QVector<Input::AxisIdSetting>::iterator it;
    for (it = m_axisSettings.begin(); it != m_axisSettings.end(); ++it) {
        if (it->m_axisSettingsId == axisSettingsId) {
            m_axisSettings.erase(it);
            break;
        }
    }
}

Input::MovingAverage &QAbstractPhysicalDeviceBackendNodePrivate::getOrCreateFilter(int axisIdentifier)
{
    QVector<Input::AxisIdFilter>::iterator it;
    QVector<Input::AxisIdFilter>::iterator end = m_axisFilters.end();
    for (it = m_axisFilters.begin(); it != end; ++it) {
        if (it->m_axisIdentifier == axisIdentifier)
            return it->m_filter;
    }

    Input::AxisIdFilter axisIdFilter;
    axisIdFilter.m_axisIdentifier = axisIdentifier;
    m_axisFilters.push_back(axisIdFilter);
    return m_axisFilters.last().m_filter;
}

Input::AxisSetting *QAbstractPhysicalDeviceBackendNodePrivate::getAxisSetting(Qt3DCore::QNodeId axisSettingId) const
{
    Q_Q(const QAbstractPhysicalDeviceBackendNode);
    QInputAspectPrivate *aspectPrivate = static_cast<QInputAspectPrivate *>(Qt3DCore::QAbstractAspectPrivate::get(q->inputAspect()));
    Input::InputHandler *handler = aspectPrivate->m_inputHandler.data();
    Input::AxisSetting *axisSetting = handler->axisSettingManager()->getOrCreateResource(axisSettingId);
    return axisSetting;
}

QVector<Input::AxisIdSetting> QAbstractPhysicalDeviceBackendNodePrivate::convertToAxisIdSettingVector(Qt3DCore::QNodeId axisSettingId) const
{
    const auto axisSetting = getAxisSetting(axisSettingId);
    const auto axisIds = axisSetting->axes();

    auto result = QVector<Input::AxisIdSetting>();
    result.reserve(axisIds.size());
    std::transform(axisIds.constBegin(), axisIds.constEnd(),
                   std::back_inserter(result),
                   [axisSettingId] (int axisId) {
                       return Input::AxisIdSetting{ axisId, axisSettingId };
                   });
    return result;
}

void QAbstractPhysicalDeviceBackendNodePrivate::updatePendingAxisSettings()
{
    if (m_pendingAxisSettingIds.isEmpty())
        return;

    m_axisSettings = std::accumulate(
                m_pendingAxisSettingIds.constBegin(), m_pendingAxisSettingIds.constEnd(),
                QVector<Input::AxisIdSetting>(),
                [this] (const QVector<Input::AxisIdSetting> &current, Qt3DCore::QNodeId axisSettingId) {
                    return current + convertToAxisIdSettingVector(axisSettingId);
                });
    m_pendingAxisSettingIds.clear();
}

QAbstractPhysicalDeviceBackendNode::QAbstractPhysicalDeviceBackendNode(QBackendNode::Mode mode)
    : Qt3DCore::QBackendNode(*new QAbstractPhysicalDeviceBackendNodePrivate(mode))
{
}

QAbstractPhysicalDeviceBackendNode::QAbstractPhysicalDeviceBackendNode(QAbstractPhysicalDeviceBackendNodePrivate &dd)
    : Qt3DCore::QBackendNode(dd)
{
}

void QAbstractPhysicalDeviceBackendNode::initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change)
{
    const auto deviceChange = qSharedPointerCast<QPhysicalDeviceCreatedChangeBase>(change);
    Q_D(QAbstractPhysicalDeviceBackendNode);
    // Store the axis setting Ids. We will update the settings themselves when needed
    d->m_pendingAxisSettingIds = deviceChange->axisSettingIds();
}

void QAbstractPhysicalDeviceBackendNode::cleanup()
{
    Q_D(QAbstractPhysicalDeviceBackendNode);
    QBackendNode::setEnabled(false);
    d->m_axisSettings.clear();
    d->m_axisFilters.clear();
    d->m_inputAspect = nullptr;
}

void QAbstractPhysicalDeviceBackendNode::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    Q_D(QAbstractPhysicalDeviceBackendNode);
    switch (e->type()) {
    case Qt3DCore::PropertyValueAdded: {
        const auto change = qSharedPointerCast<Qt3DCore::QPropertyNodeAddedChange>(e);
        if (change->propertyName() == QByteArrayLiteral("axisSettings")) {
            const auto axisSettingId = change->addedNodeId();
            Input::AxisSetting *axisSetting = d->getAxisSetting(axisSettingId);
            const auto axisIds = axisSetting->axes();
            for (int axisId : axisIds)
                d->addAxisSetting(axisId, axisSettingId);
        }
        break;
    }

    case Qt3DCore::PropertyValueRemoved: {
        const auto change = qSharedPointerCast<Qt3DCore::QPropertyNodeRemovedChange>(e);
        if (change->propertyName() == QByteArrayLiteral("axisSettings"))
            d->removeAxisSetting(change->removedNodeId());
        break;
    }

    default:
        break;
    }
    QBackendNode::sceneChangeEvent(e);
}

void QAbstractPhysicalDeviceBackendNode::setInputAspect(QInputAspect *aspect)
{
    Q_D(QAbstractPhysicalDeviceBackendNode);
    d->m_inputAspect = aspect;
}

QInputAspect *QAbstractPhysicalDeviceBackendNode::inputAspect() const
{
    Q_D(const QAbstractPhysicalDeviceBackendNode);
    return d->m_inputAspect;
}

float QAbstractPhysicalDeviceBackendNode::processedAxisValue(int axisIdentifier)
{
    Q_D(QAbstractPhysicalDeviceBackendNode);
    d->updatePendingAxisSettings();

    // Find axis settings for this axis (if any)
    Qt3DCore::QNodeId axisSettingId;
    QVector<Input::AxisIdSetting>::const_iterator it;
    QVector<Input::AxisIdSetting>::const_iterator end = d->m_axisSettings.cend();
    for (it = d->m_axisSettings.cbegin(); it != end; ++it) {
        if (it->m_axisIdentifier == axisIdentifier) {
            axisSettingId = it->m_axisSettingsId;
            break;
        }
    }

    const float rawAxisValue = axisValue(axisIdentifier);
    if (axisSettingId.isNull()) {
        // No special processing. Just return the raw value
        return rawAxisValue;
    } else {
        // Process the raw value in accordance with the settings
        Input::AxisSetting *axisSetting = d->getAxisSetting(axisSettingId);
        Q_ASSERT(axisSetting);
        float val = rawAxisValue;

        // Low pass smoothing
        if (axisSetting->isSmoothEnabled()) {
            // Get the filter corresponding to this axis
            Input::MovingAverage &filter = d->getOrCreateFilter(axisIdentifier);
            filter.addSample(val);
            val = filter.average();
        }

        // Deadzone handling
        const float d = axisSetting->deadZoneRadius();
        if (!qFuzzyIsNull(d)) {
            if (std::abs(val) <= d) {
                val = 0.0f;
            } else {
                // Calculate value that goes from 0 to 1 linearly from the boundary of
                // the dead zone up to 1. That is we with a dead zone value of d, we do not
                // want a step change from 0 to d when the axis leaves the deadzone. Instead
                // we want to increase the gradient of the line so that it goes from 0 to 1
                // over the range d to 1. So instead of having y = x, the equation of the
                // line becomes
                //
                // y = x / (1-d) - d / (1-d) = (x - d) / (1 - d)
                //
                // for positive values, and
                //
                // y = x / (1-d) + d / (1-d) = (x + d) / (1 - d)
                //
                // for negative values.
                val = (val - signum(val) * d) / (1.0f - d);
            }
        }

        return val;
    }

}

} // Qt3DInput

QT_END_NAMESPACE
