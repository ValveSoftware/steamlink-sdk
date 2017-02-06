/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Gamepad module
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qgamepadmouseitem.h"
#include <cmath>

#include <QtGamepad/QGamepad>
#include <QtGui/QWindow>
#include <QtQuick/QQuickWindow>
#include <QtGui/QGuiApplication>

QT_BEGIN_NAMESPACE

QGamepadMouseItem::QGamepadMouseItem(QQuickItem *parent)
    : QQuickItem(parent)
    , m_active(false)
    , m_gamepad(0)
    , m_joystick(QGamepadMouseItem::LeftStick)
    , m_deadZoneSize(0.1)
{
    m_updateTimer.setInterval(16);
    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateMousePostion()));
}

bool QGamepadMouseItem::active() const
{
    return m_active;
}

QGamepad *QGamepadMouseItem::gamepad() const
{
    return m_gamepad;
}

QGamepadMouseItem::GamepadJoystick QGamepadMouseItem::joystick() const
{
    return m_joystick;
}

double QGamepadMouseItem::deadZoneSize() const
{
    return m_deadZoneSize;
}

QPointF QGamepadMouseItem::mousePosition() const
{
    return m_mousePosition;
}

void QGamepadMouseItem::setActive(bool arg)
{
    if (m_active != arg) {
        m_active = arg;
        if (m_active) {
            m_deltaTimer.start();
            m_updateTimer.start();
        } else {
            m_updateTimer.stop();
            m_deltaTimer.invalidate();
        }
        emit activeChanged(arg);
    }
}

void QGamepadMouseItem::setGamepad(QGamepad *gamepad)
{
    if (m_gamepad != gamepad) {
        m_gamepad = gamepad;
        emit gamepadChanged(gamepad);
    }
}

void QGamepadMouseItem::setJoystick(QGamepadMouseItem::GamepadJoystick joystick)
{
    if (m_joystick != joystick) {
        m_joystick = joystick;
        emit joystickChanged(joystick);
    }
}

void QGamepadMouseItem::setDeadZoneSize(double size)
{
    if (m_deadZoneSize != size) {
        m_deadZoneSize = size;
        emit deadZoneSizeChanged(size);
    }
}

void QGamepadMouseItem::mouseButtonPressed(int button)
{
    processMouseButtonEvent(true, (Qt::MouseButton)button);
}

void QGamepadMouseItem::mouseButtonReleased(int button)
{
    processMouseButtonEvent(false, (Qt::MouseButton)button);
}

void QGamepadMouseItem::updateMousePostion()
{
    //Get the delta from the last call
    qint64 delta = m_deltaTimer.restart();

    //Don't bother when there is not gamepad to read from
    if (!m_gamepad || !m_gamepad->isConnected())
        return;

    double xVelocity = 0.0;
    double yVelocity = 0.0;

    if (m_joystick == QGamepadMouseItem::LeftStick) {
        xVelocity = m_gamepad->axisLeftX();
        yVelocity = m_gamepad->axisLeftY();
    } else if (m_joystick == QGamepadMouseItem::RightStick) {
        xVelocity = m_gamepad->axisRightX();
        yVelocity = m_gamepad->axisRightY();
    } else { //The greatest of both
        if (std::abs(m_gamepad->axisLeftX()) > std::abs(m_gamepad->axisRightX()))
            xVelocity = m_gamepad->axisLeftX();
        else
            xVelocity = m_gamepad->axisRightX();
        if (std::abs(m_gamepad->axisLeftY()) > std::abs(m_gamepad->axisRightY()))
            yVelocity = m_gamepad->axisLeftY();
        else
            yVelocity = m_gamepad->axisRightY();
    }

    //Check for deadzone limits]
    if (std::abs(xVelocity) < m_deadZoneSize)
        xVelocity = 0.0;
    if (std::abs(yVelocity) < m_deadZoneSize)
        yVelocity = 0.0;
    if (xVelocity == 0.0 && yVelocity == 0.0)
        return;

    double newXPosition = m_mousePosition.x() + xVelocity * delta;
    double newYPosition = m_mousePosition.y() + yVelocity * delta;
    //Check bounds
    if (newXPosition < 0)
        newXPosition = 0;
    else if (newXPosition > width())
        newXPosition = width();

    if (newYPosition < 0)
        newYPosition = 0;
    else if (newYPosition > height())
        newYPosition = height();

    m_mousePosition = QPointF(newXPosition, newYPosition);
    emit mousePositionChanged(m_mousePosition);
}

void QGamepadMouseItem::processMouseMoveEvent(QPointF position)
{
    QMouseEvent *mouseEvent = new QMouseEvent(QEvent::MouseMove, mapToScene(position), Qt::NoButton, m_mouseButtons, Qt::NoModifier);
    sendGeneratedMouseEvent(mouseEvent);
}

void QGamepadMouseItem::processMouseButtonEvent(bool isPressed, Qt::MouseButton button)
{
    QMouseEvent *event;
    if (isPressed) {
        m_mouseButtons |= button;
        event = new QMouseEvent(QEvent::MouseButtonPress, mapToScene(m_mousePosition), button, m_mouseButtons, Qt::NoModifier);
    } else {
        m_mouseButtons ^= button;
        event = new QMouseEvent(QEvent::MouseButtonRelease, mapToScene(m_mousePosition), button, m_mouseButtons, Qt::NoModifier);
    }

    sendGeneratedMouseEvent(event);
}

void QGamepadMouseItem::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChanged(newGeometry, oldGeometry);

    m_mousePosition = newGeometry.center();
}

void QGamepadMouseItem::sendGeneratedMouseEvent(QMouseEvent *event)
{
    if (!m_active || !this->window()) {
        delete event;
        return;
    }

    QWindow *window = qobject_cast<QWindow*>(this->window());
    if (window)
        QGuiApplication::sendEvent(window, event);
}

QT_END_NAMESPACE
