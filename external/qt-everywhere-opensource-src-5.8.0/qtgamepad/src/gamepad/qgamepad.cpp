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

#include "qgamepad.h"

QT_BEGIN_NAMESPACE

/*!
   \class QGamepad
   \inmodule QtGamepad
   \brief A gamepad device connected to a system

   QGamepad is used to access the current state of gamepad hardware connected
   to a system.
 */

/*!
 * \brief Constructs a QGamepad for \a deviceId.
 * \param deviceId is the id of the gamepad you wish to see the state of
 * \param parent
 */
QGamepad::QGamepad(int deviceId, QObject *parent)
    : QObject(parent)
    , m_deviceId(deviceId)
    , m_connected(false)
    , m_axisLeftX(0.0)
    , m_axisLeftY(0.0)
    , m_axisRightX(0.0)
    , m_axisRightY(0.0)
    , m_buttonA(false)
    , m_buttonB(false)
    , m_buttonX(false)
    , m_buttonY(false)
    , m_buttonL1(false)
    , m_buttonR1(false)
    , m_buttonL2(0.0)
    , m_buttonR2(0.0)
    , m_buttonSelect(false)
    , m_buttonStart(false)
    , m_buttonL3(false)
    , m_buttonR3(false)
    , m_buttonUp(false)
    , m_buttonDown(false)
    , m_buttonLeft(false)
    , m_buttonRight(false)
    , m_buttonCenter(false)
    , m_buttonGuide(false)
{
    m_gamepadManager = QGamepadManager::instance();
    connect(m_gamepadManager, SIGNAL(gamepadConnected(int)), this, SLOT(handleGamepadConnected(int)));
    connect(m_gamepadManager, SIGNAL(gamepadDisconnected(int)), this, SLOT(handleGamepadDisconnected(int)));
    connect(m_gamepadManager, SIGNAL(gamepadAxisEvent(int,QGamepadManager::GamepadAxis,double)), this, SLOT(handleGamepadAxisEvent(int,QGamepadManager::GamepadAxis,double)));
    connect(m_gamepadManager, SIGNAL(gamepadButtonPressEvent(int,QGamepadManager::GamepadButton,double)), this, SLOT(handleGamepadButtonPressEvent(int,QGamepadManager::GamepadButton,double)));
    connect(m_gamepadManager, SIGNAL(gamepadButtonReleaseEvent(int,QGamepadManager::GamepadButton)), this, SLOT(handleGamepadButtonReleaseEvent(int,QGamepadManager::GamepadButton)));

    setConnected(m_gamepadManager->isGamepadConnected(m_deviceId));
}

/*!
 * \brief Destroys the QGamepad.
 */
QGamepad::~QGamepad()
{
}

/*!
 * \property QGamepad::deviceId
 *
 * This property holds the deviceId of the gamepad device.  It is possible for there to be
 * multiple gamepad devices connected at any given time, so setting this property defines
 * which gamepad to use.
 *
 */
int QGamepad::deviceId() const
{
    return m_deviceId;
}

/*!
 * \property QGamepad::connected
 * This property holds the connectivity state of the gamepad device. If a gamepad is connected
 * this property will be true, otherwise false.
 */
bool QGamepad::isConnected() const
{
    return m_connected;
}

/*!
 * \property QGamepad::name
 *
 * This property holds the reported name of the gamepad if one is available.
 */
QString QGamepad::name() const
{
    return m_name;
}

/*!
 * \property QGamepad::axisLeftX
 *
 * This property holds the value of the left thumbstick's X axis.
 * The range of axis values are from -1.0 to 1.0
 */
double QGamepad::axisLeftX() const
{
    return m_axisLeftX;
}

/*!
 * \property QGamepad::axisLeftY
 *
 * This property holds the value of the left thumbstick's Y axis.
 * The range of axis values are from -1.0 to 1.0
 */
double QGamepad::axisLeftY() const
{
    return m_axisLeftY;
}

/*!
 * \property QGamepad::axisRightX
 *
 * This property holds the value of the right thumbstick's X axis.
 * The range of axis values are from -1.0 to 1.0
 */
double QGamepad::axisRightX() const
{
    return m_axisRightX;
}

/*!
 * \property QGamepad::axisRightY
 *
 * This property holds the value of the right thumbstick's Y axis.
 * The range of axis values are from -1.0 to 1.0
 */
double QGamepad::axisRightY() const
{
    return m_axisRightY;
}

/*!
 * \property QGamepad::buttonA
 *
 * This property holds the state of the A button.  True when pressed, false when not
 * pressed.
 */
bool QGamepad::buttonA() const
{
    return m_buttonA;
}

/*!
 * \property QGamepad::buttonB
 *
 * This property holds the state of the B button.  True when pressed, false when not
 * pressed.
 */
bool QGamepad::buttonB() const
{
    return m_buttonB;
}

/*!
 * \property QGamepad::buttonX
 *
 * This property holds the state of the X button.  True when pressed, false when not
 * pressed.
 */
bool QGamepad::buttonX() const
{
    return m_buttonX;
}

/*!
 * \property QGamepad::buttonY
 *
 * This property holds the state of the Y button.  True when pressed, false when not
 * pressed.
 */
bool QGamepad::buttonY() const
{
    return m_buttonY;
}

/*!
 * \property QGamepad::buttonL1
 *
 * This property holds the state of the left shoulder button.
 * True when pressed, false when not pressed.
 */
bool QGamepad::buttonL1() const
{
    return m_buttonL1;
}

/*!
 * \property QGamepad::buttonR1
 *
 * This property holds the state of the right shoulder button.
 * True when pressed, false when not pressed.
 */
bool QGamepad::buttonR1() const
{
    return m_buttonR1;
}

/*!
 * \property QGamepad::buttonL2
 *
 * This property holds the value of the left trigger button.
 * This trigger value ranges from 0.0 when not pressed to 1.0
 * when pressed completely.
 */
double QGamepad::buttonL2() const
{
    return m_buttonL2;
}

/*!
 * \property QGamepad::buttonR2
 *
 * This property holds the value of the right trigger button.
 * This trigger value ranges from 0.0 when not pressed to 1.0
 * when pressed completely.
 */
double QGamepad::buttonR2() const
{
    return m_buttonR2;
}

/*!
 * \property QGamepad::buttonSelect
 *
 * This property holds the state of the Select button.  True when pressed, false when not
 * pressed. This button can sometimes be labled as the Back button on some gamepads.
 */
bool QGamepad::buttonSelect() const
{
    return m_buttonSelect;
}

/*!
 * \property QGamepad::buttonStart
 *
 * This property holds the state of the Start button.  True when pressed, false when not
 * pressed. This button can sometimes be labled as the Forward button on some gamepads.
 */
bool QGamepad::buttonStart() const
{
    return m_buttonStart;
}

/*!
 * \property QGamepad::buttonL3
 *
 * This property holds the state of the left stick button.  True when pressed, false when not
 * pressed.  This button is usually triggered by pressing the left joystick itself.
 */
bool QGamepad::buttonL3() const
{
    return m_buttonL3;
}

/*!
 * \property QGamepad::buttonR3
 *
 * This property holds the state of the right stick button.  True when pressed, false when not
 * pressed.  This button is usually triggered by pressing the right joystick itself.
 */
bool QGamepad::buttonR3() const
{
    return m_buttonR3;
}

/*!
 * \property QGamepad::buttonUp
 *
 * This property holds the state of the direction pad up button.
 * True when pressed, false when not pressed.
 */
bool QGamepad::buttonUp() const
{
    return m_buttonUp;
}

/*!
 * \property QGamepad::buttonDown
 *
 * This property holds the state of the direction pad down button.
 * True when pressed, false when not pressed.
 */
bool QGamepad::buttonDown() const
{
    return m_buttonDown;
}

/*!
 * \property QGamepad::buttonLeft
 *
 * This property holds the state of the direction pad left button.
 * True when pressed, false when not pressed.
 */
bool QGamepad::buttonLeft() const
{
    return m_buttonLeft;
}

/*!
 * \property QGamepad::buttonRight
 *
 * This property holds the state of the direction pad right button.
 * True when pressed, false when not pressed.
 */
bool QGamepad::buttonRight() const
{
    return m_buttonRight;
}

bool QGamepad::buttonCenter() const
{
    return m_buttonCenter;
}

/*!
 * \property QGamepad::buttonGuide
 *
 * This property holds the state of the guide button.
 * True when pressed, false when not pressed.
 * This button is typically the one in the center of the gamepad with a logo.
 * Some gamepads will not have a guide button.
 */
bool QGamepad::buttonGuide() const
{
    return m_buttonGuide;
}

void QGamepad::setDeviceId(int number)
{
    if (m_deviceId != number) {
        m_deviceId = number;
        emit deviceIdChanged(number);
        setConnected(m_gamepadManager->isGamepadConnected(m_deviceId));
    }
}

void QGamepad::setConnected(bool isConnected)
{
    if (m_connected != isConnected) {
        m_connected = isConnected;
        emit connectedChanged(m_connected);
    }
}

/*!
 * \internal
 */\
void QGamepad::handleGamepadConnected(int deviceId)
{
    if (deviceId == m_deviceId) {
        setConnected(true);
    }
}

/*!
 * \internal
 */\
void QGamepad::handleGamepadDisconnected(int deviceId)
{
    if (deviceId == m_deviceId) {
        setConnected(false);
    }
}

/*!
 * \internal
 */\
void QGamepad::handleGamepadAxisEvent(int deviceId, QGamepadManager::GamepadAxis axis, double value)
{
    if (deviceId != m_deviceId)
        return;

    switch (axis) {
    case QGamepadManager::AxisLeftX:
        m_axisLeftX = value;
        emit axisLeftXChanged(value);
        break;
    case QGamepadManager::AxisLeftY:
        m_axisLeftY = value;
        emit axisLeftYChanged(value);
        break;
    case QGamepadManager::AxisRightX:
        m_axisRightX = value;
        emit axisRightXChanged(value);
        break;
    case QGamepadManager::AxisRightY:
        m_axisRightY = value;
        emit axisRightYChanged(value);
        break;
    default:
        break;
    }
}

/*!
 * \internal
 */\
void QGamepad::handleGamepadButtonPressEvent(int deviceId, QGamepadManager::GamepadButton button, double value)
{
    if (deviceId != m_deviceId)
        return;

    switch (button) {
    case QGamepadManager::ButtonA:
        m_buttonA = true;
        emit buttonAChanged(true);
        break;
    case QGamepadManager::ButtonB:
        m_buttonB = true;
        emit buttonBChanged(true);
        break;
    case QGamepadManager::ButtonX:
        m_buttonX = true;
        emit buttonXChanged(true);
        break;
    case QGamepadManager::ButtonY:
        m_buttonY = true;
        emit buttonYChanged(true);
        break;
    case QGamepadManager::ButtonL1:
        m_buttonL1 = true;
        emit buttonL1Changed(true);
        break;
    case QGamepadManager::ButtonR1:
        m_buttonR1 = true;
        emit buttonR1Changed(true);
        break;
    case QGamepadManager::ButtonL2:
        m_buttonL2 = value;
        emit buttonL2Changed(value);
        break;
    case QGamepadManager::ButtonR2:
        m_buttonR2 = value;
        emit buttonR2Changed(value);
        break;
    case QGamepadManager::ButtonL3:
        m_buttonL3 = true;
        emit buttonL3Changed(true);
        break;
    case QGamepadManager::ButtonR3:
        m_buttonR3 = true;
        emit buttonR3Changed(true);
        break;
    case QGamepadManager::ButtonSelect:
        m_buttonSelect = true;
        emit buttonSelectChanged(true);
        break;
    case QGamepadManager::ButtonStart:
        m_buttonStart = true;
        emit buttonStartChanged(true);
        break;
    case QGamepadManager::ButtonUp:
        m_buttonUp = true;
        emit buttonUpChanged(true);
        break;
    case QGamepadManager::ButtonDown:
        m_buttonDown = true;
        emit buttonDownChanged(true);
        break;
    case QGamepadManager::ButtonLeft:
        m_buttonLeft = true;
        emit buttonLeftChanged(true);
        break;
    case QGamepadManager::ButtonRight:
        m_buttonRight = true;
        emit buttonRightChanged(true);
        break;
    case QGamepadManager::ButtonCenter:
        m_buttonCenter = true;
        emit buttonCenterChanged(true);
        break;
    case QGamepadManager::ButtonGuide:
        m_buttonGuide = true;
        emit buttonGuideChanged(true);
        break;
    default:
        break;
    }

}

/*!
 * \internal
 */\
void QGamepad::handleGamepadButtonReleaseEvent(int deviceId, QGamepadManager::GamepadButton button)
{
    if (deviceId != m_deviceId)
        return;

    switch (button) {
    case QGamepadManager::ButtonA:
        m_buttonA = false;
        emit buttonAChanged(false);
        break;
    case QGamepadManager::ButtonB:
        m_buttonB = false;
        emit buttonBChanged(false);
        break;
    case QGamepadManager::ButtonX:
        m_buttonX = false;
        emit buttonXChanged(false);
        break;
    case QGamepadManager::ButtonY:
        m_buttonY = false;
        emit buttonYChanged(false);
        break;
    case QGamepadManager::ButtonL1:
        m_buttonL1 = false;
        emit buttonL1Changed(false);
        break;
    case QGamepadManager::ButtonR1:
        m_buttonR1 = false;
        emit buttonR1Changed(false);
        break;
    case QGamepadManager::ButtonL2:
        m_buttonL2 = 0.0;
        emit buttonL2Changed(0.0);
        break;
    case QGamepadManager::ButtonR2:
        m_buttonR2 = 0.0;
        emit buttonR2Changed(0.0);
        break;
    case QGamepadManager::ButtonL3:
        m_buttonL3 = false;
        emit buttonL3Changed(false);
        break;
    case QGamepadManager::ButtonR3:
        m_buttonR3 = false;
        emit buttonR3Changed(false);
        break;
    case QGamepadManager::ButtonSelect:
        m_buttonSelect = false;
        emit buttonSelectChanged(false);
        break;
    case QGamepadManager::ButtonStart:
        m_buttonStart = false;
        emit buttonStartChanged(false);
        break;
    case QGamepadManager::ButtonUp:
        m_buttonUp = false;
        emit buttonUpChanged(false);
        break;
    case QGamepadManager::ButtonDown:
        m_buttonDown = false;
        emit buttonDownChanged(false);
        break;
    case QGamepadManager::ButtonLeft:
        m_buttonLeft = false;
        emit buttonLeftChanged(false);
        break;
    case QGamepadManager::ButtonRight:
        m_buttonRight = false;
        emit buttonRightChanged(false);
        break;
    case QGamepadManager::ButtonCenter:
        m_buttonCenter = false;
        emit buttonCenterChanged(false);
        break;
    case QGamepadManager::ButtonGuide:
        m_buttonGuide = false;
        emit buttonGuideChanged(false);
        break;
    default:
        break;
    }
}

QT_END_NAMESPACE
