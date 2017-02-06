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

#include "buttonaxisinput_p.h"
#include <Qt3DInput/qbuttonaxisinput.h>
#include <Qt3DInput/qabstractphysicaldevice.h>
#include <Qt3DInput/private/qabstractphysicaldevicebackendnode_p.h>
#include <Qt3DInput/private/qbuttonaxisinput_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DInput/private/utils_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DInput {

namespace Input {

ButtonAxisInput::ButtonAxisInput()
    : AbstractAxisInput()
    , m_scale(0.0f)
    , m_acceleration(-1.0f)
    , m_deceleration(-1.0f)
    , m_speedRatio(0.0f)
    , m_lastUpdateTime(0)
{
}

void ButtonAxisInput::initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change)
{
    const auto typedChange = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<QButtonAxisInputData>>(change);
    const auto &data = typedChange->data;
    m_buttons = data.buttons;
    m_scale = data.scale;
    m_acceleration = data.acceleration;
    m_deceleration = data.deceleration;
    AbstractAxisInput::initializeFromPeer(change);
}

void ButtonAxisInput::cleanup()
{
    m_scale = 0.0f;
    m_buttons.clear();
    m_acceleration = -1.0f;
    m_deceleration = -1.0f;
    AbstractAxisInput::cleanup();
}

void ButtonAxisInput::updateSpeedRatio(qint64 currentTime, ButtonAxisInput::UpdateType type)
{
    const float accel = (type == Accelerate) ? acceleration() : -deceleration();

    // Was in nanoseconds, while acceleration will be in units per square seconds
    const float delta = m_lastUpdateTime ? (currentTime - m_lastUpdateTime) / 1000000000.0f : 0.0f;
    const float speedRatio = m_speedRatio + delta * accel;

    // Clamp it
    m_speedRatio = qMax(0.0f, qMin(speedRatio, 1.0f));

    // If we stopped, time to start over
    if (type == Decelerate && m_speedRatio == 0.0f)
        m_lastUpdateTime = 0;
    else
        m_lastUpdateTime = currentTime;
}

void ButtonAxisInput::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    if (e->type() == Qt3DCore::PropertyUpdated) {
        Qt3DCore::QPropertyUpdatedChangePtr propertyChange = qSharedPointerCast<Qt3DCore::QPropertyUpdatedChange>(e);
        if (propertyChange->propertyName() == QByteArrayLiteral("scale")) {
            m_scale = propertyChange->value().toFloat();
        } else if (propertyChange->propertyName() == QByteArrayLiteral("buttons")) {
            m_buttons = propertyChange->value().value<QVector<int>>();
        } else if (propertyChange->propertyName() == QByteArrayLiteral("acceleration")) {
            m_acceleration = propertyChange->value().toFloat();
        } else if (propertyChange->propertyName() == QByteArrayLiteral("deceleration")) {
            m_deceleration = propertyChange->value().toFloat();
        }
    }
    AbstractAxisInput::sceneChangeEvent(e);
}

namespace {

bool anyOfRequiredButtonsPressed(const QVector<int> &buttons, QAbstractPhysicalDeviceBackendNode *physicalDeviceBackend)
{
    bool validButtonWasPressed = false;
    for (int button : buttons) {
        if (physicalDeviceBackend->isButtonPressed(button)) {
            validButtonWasPressed = true;
            break;
        }
    }
    return validButtonWasPressed;
}

} // anonymous

float ButtonAxisInput::process(InputHandler *inputHandler, qint64 currentTime)
{
    if (!isEnabled())
        return 0.0f;

    if (m_buttons.isEmpty())
        return 0.0f;

    QAbstractPhysicalDeviceBackendNode *physicalDeviceBackend = Utils::physicalDeviceForInput(this, inputHandler);
    if (!physicalDeviceBackend)
        return 0.0f;

    // TO DO: Linear Curver for the progression of the scale value
    if (anyOfRequiredButtonsPressed(m_buttons, physicalDeviceBackend))
        updateSpeedRatio(currentTime, ButtonAxisInput::Accelerate);
    else if (m_speedRatio != 0.0f)
        updateSpeedRatio(currentTime, ButtonAxisInput::Decelerate);

    return m_speedRatio * m_scale;
}

} // Input

} // Qt3DInput

QT_END_NAMESPACE
