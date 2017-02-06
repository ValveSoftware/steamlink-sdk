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

#include "mousedevice_p.h"
#include "inputmanagers_p.h"
#include "inputhandler_p.h"
#include "qmousedevice.h"
#include <Qt3DInput/private/qmousedevice_p.h>

#include <Qt3DCore/qnode.h>
#include <Qt3DCore/qentity.h>
#include <Qt3DCore/qpropertyupdatedchange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DInput {
namespace Input {

MouseDevice::MouseDevice()
    : QAbstractPhysicalDeviceBackendNode(ReadOnly)
    , m_inputHandler(nullptr)
    , m_wasPressed(false)
    , m_sensitivity(0.1f)
{
}

MouseDevice::~MouseDevice()
{
}

void MouseDevice::initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change)
{
    const auto typedChange = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<QMouseDeviceData>>(change);
    const auto &data = typedChange->data;
    m_sensitivity = data.sensitivity;
    QAbstractPhysicalDeviceBackendNode::initializeFromPeer(change);
}

void MouseDevice::setInputHandler(InputHandler *handler)
{
    m_inputHandler = handler;
}

InputHandler *MouseDevice::inputHandler() const
{
    return m_inputHandler;
}

float MouseDevice::axisValue(int axisIdentifier) const
{
    switch (axisIdentifier) {
    case QMouseDevice::X:
        return m_mouseState.xAxis;
    case QMouseDevice::Y:
        return m_mouseState.yAxis;
    case QMouseDevice::WheelX:
        return m_mouseState.wXAxis;
    case QMouseDevice::WheelY:
        return m_mouseState.wYAxis;
    default:
        break;
    }
    return 0.0f;
}

bool MouseDevice::isButtonPressed(int buttonIdentifier) const
{
    switch (buttonIdentifier) {
    case QMouseEvent::LeftButton:
        return m_mouseState.leftPressed;
    case QMouseEvent::MiddleButton:
        return m_mouseState.centerPressed;
    case QMouseEvent::RightButton:
        return m_mouseState.rightPressed;
    default:
        break;
    }
    return false;
}

MouseDevice::MouseState MouseDevice::mouseState() const
{
    return m_mouseState;
}

QPointF MouseDevice::previousPos() const
{
    return m_previousPos;
}

bool MouseDevice::wasPressed() const
{
    return m_wasPressed;
}

float MouseDevice::sensitivity() const
{
    return m_sensitivity;
}

void MouseDevice::updateWheelEvents(const QList<QT_PREPEND_NAMESPACE (QWheelEvent)> &events)
{
    // Reset axis values before we accumulate new values for this frame
    m_mouseState.wXAxis = 0.0f;
    m_mouseState.wYAxis = 0.0f;
    if (!events.isEmpty()) {
        for (const QT_PREPEND_NAMESPACE(QWheelEvent) &e : events) {
            m_mouseState.wXAxis += m_sensitivity * e.angleDelta().x();
            m_mouseState.wYAxis += m_sensitivity * e.angleDelta().y();
        }
    }
}

void MouseDevice::updateMouseEvents(const QList<QT_PREPEND_NAMESPACE(QMouseEvent)> &events)
{
    // Reset axis values before we accumulate new values for this frame
    m_mouseState.xAxis = 0.0f;
    m_mouseState.yAxis = 0.0f;

    if (!events.isEmpty()) {
        for (const QT_PREPEND_NAMESPACE(QMouseEvent) &e : events) {
            m_mouseState.leftPressed = e.buttons() & (Qt::LeftButton);
            m_mouseState.centerPressed = e.buttons() & (Qt::MiddleButton);
            m_mouseState.rightPressed = e.buttons() & (Qt::RightButton);
            bool pressed = m_mouseState.leftPressed || m_mouseState.centerPressed || m_mouseState.rightPressed;
            if (m_wasPressed && pressed) {
                m_mouseState.xAxis += m_sensitivity * (e.screenPos().x() - m_previousPos.x());
                m_mouseState.yAxis += m_sensitivity * (m_previousPos.y() - e.screenPos().y());
            }
            m_wasPressed = pressed;
            m_previousPos = e.screenPos();
        }
    }
}

void MouseDevice::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    if (e->type() == Qt3DCore::PropertyUpdated) {
        Qt3DCore::QPropertyUpdatedChangePtr propertyChange = qSharedPointerCast<Qt3DCore::QPropertyUpdatedChange>(e);
        if (propertyChange->propertyName() == QByteArrayLiteral("sensitivity"))
            m_sensitivity = propertyChange->value().toFloat();
    }
}

MouseDeviceFunctor::MouseDeviceFunctor(QInputAspect *inputAspect, InputHandler *handler)
    : m_inputAspect(inputAspect)
    , m_handler(handler)
{
}

Qt3DCore::QBackendNode *MouseDeviceFunctor::create(const Qt3DCore::QNodeCreatedChangeBasePtr &change) const
{
    MouseDevice *controller = m_handler->mouseDeviceManager()->getOrCreateResource(change->subjectId());
    controller->setInputAspect(m_inputAspect);
    controller->setInputHandler(m_handler);
    m_handler->appendMouseDevice(m_handler->mouseDeviceManager()->lookupHandle(change->subjectId()));
    return controller;
}

Qt3DCore::QBackendNode *MouseDeviceFunctor::get(Qt3DCore::QNodeId id) const
{
    return m_handler->mouseDeviceManager()->lookupResource(id);
}

void MouseDeviceFunctor::destroy(Qt3DCore::QNodeId id) const
{
    m_handler->removeMouseDevice(m_handler->mouseDeviceManager()->lookupHandle(id));
    m_handler->mouseDeviceManager()->releaseResource(id);
}

} // namespace Input
} // namespace Qt3DInput

QT_END_NAMESPACE
