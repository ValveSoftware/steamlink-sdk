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

#include <private/qobject_p.h>

QT_BEGIN_NAMESPACE

class QGamepadPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QGamepad)

public:
    QGamepadPrivate(int deviceId)
        : deviceId(deviceId)
        , connected(false)
        , axisLeftX(0.0)
        , axisLeftY(0.0)
        , axisRightX(0.0)
        , axisRightY(0.0)
        , buttonA(false)
        , buttonB(false)
        , buttonX(false)
        , buttonY(false)
        , buttonL1(false)
        , buttonR1(false)
        , buttonL2(0.0)
        , buttonR2(0.0)
        , buttonSelect(false)
        , buttonStart(false)
        , buttonL3(false)
        , buttonR3(false)
        , buttonUp(false)
        , buttonDown(false)
        , buttonLeft(false)
        , buttonRight(false)
        , buttonCenter(false)
        , buttonGuide(false)
    {
    }

    QGamepadManager *gamepadManager;

    int deviceId;
    bool connected;
    QString name;
    double axisLeftX;
    double axisLeftY;
    double axisRightX;
    double axisRightY;
    bool buttonA;
    bool buttonB;
    bool buttonX;
    bool buttonY;
    bool buttonL1;
    bool buttonR1;
    double buttonL2;
    double buttonR2;
    bool buttonSelect;
    bool buttonStart;
    bool buttonL3;
    bool buttonR3;
    bool buttonUp;
    bool buttonDown;
    bool buttonLeft;
    bool buttonRight;
    bool buttonCenter;
    bool buttonGuide;

    void setConnected(bool isConnected);

    void _q_handleGamepadConnected(int id);
    void _q_handleGamepadDisconnected(int id);
    void _q_handleGamepadAxisEvent(int id, QGamepadManager::GamepadAxis axis, double value);
    void _q_handleGamepadButtonPressEvent(int id, QGamepadManager::GamepadButton button, double value);
    void _q_handleGamepadButtonReleaseEvent(int id, QGamepadManager::GamepadButton button);
};

void QGamepadPrivate::setConnected(bool isConnected)
{
    Q_Q(QGamepad);
    if (connected != isConnected) {
        connected = isConnected;
        emit q->connectedChanged(connected);
    }
}

/*!
 * \internal
 */\
void QGamepadPrivate::_q_handleGamepadConnected(int id)
{
    if (id == deviceId) {
        setConnected(true);
    }
}

/*!
 * \internal
 */\
void QGamepadPrivate::_q_handleGamepadDisconnected(int id)
{
    if (id == deviceId) {
        setConnected(false);
    }
}

/*!
 * \internal
 */\
void QGamepadPrivate::_q_handleGamepadAxisEvent(int id, QGamepadManager::GamepadAxis axis, double value)
{
    Q_Q(QGamepad);
    if (id != deviceId)
        return;

    switch (axis) {
    case QGamepadManager::AxisLeftX:
        axisLeftX = value;
        emit q->axisLeftXChanged(value);
        break;
    case QGamepadManager::AxisLeftY:
        axisLeftY = value;
        emit q->axisLeftYChanged(value);
        break;
    case QGamepadManager::AxisRightX:
        axisRightX = value;
        emit q->axisRightXChanged(value);
        break;
    case QGamepadManager::AxisRightY:
        axisRightY = value;
        emit q->axisRightYChanged(value);
        break;
    default:
        break;
    }
}

/*!
 * \internal
 */\
void QGamepadPrivate::_q_handleGamepadButtonPressEvent(int id, QGamepadManager::GamepadButton button, double value)
{
    Q_Q(QGamepad);
    if (id != deviceId)
        return;

    switch (button) {
    case QGamepadManager::ButtonA:
        buttonA = true;
        emit q->buttonAChanged(true);
        break;
    case QGamepadManager::ButtonB:
        buttonB = true;
        emit q->buttonBChanged(true);
        break;
    case QGamepadManager::ButtonX:
        buttonX = true;
        emit q->buttonXChanged(true);
        break;
    case QGamepadManager::ButtonY:
        buttonY = true;
        emit q->buttonYChanged(true);
        break;
    case QGamepadManager::ButtonL1:
        buttonL1 = true;
        emit q->buttonL1Changed(true);
        break;
    case QGamepadManager::ButtonR1:
        buttonR1 = true;
        emit q->buttonR1Changed(true);
        break;
    case QGamepadManager::ButtonL2:
        buttonL2 = value;
        emit q->buttonL2Changed(value);
        break;
    case QGamepadManager::ButtonR2:
        buttonR2 = value;
        emit q->buttonR2Changed(value);
        break;
    case QGamepadManager::ButtonL3:
        buttonL3 = true;
        emit q->buttonL3Changed(true);
        break;
    case QGamepadManager::ButtonR3:
        buttonR3 = true;
        emit q->buttonR3Changed(true);
        break;
    case QGamepadManager::ButtonSelect:
        buttonSelect = true;
        emit q->buttonSelectChanged(true);
        break;
    case QGamepadManager::ButtonStart:
        buttonStart = true;
        emit q->buttonStartChanged(true);
        break;
    case QGamepadManager::ButtonUp:
        buttonUp = true;
        emit q->buttonUpChanged(true);
        break;
    case QGamepadManager::ButtonDown:
        buttonDown = true;
        emit q->buttonDownChanged(true);
        break;
    case QGamepadManager::ButtonLeft:
        buttonLeft = true;
        emit q->buttonLeftChanged(true);
        break;
    case QGamepadManager::ButtonRight:
        buttonRight = true;
        emit q->buttonRightChanged(true);
        break;
    case QGamepadManager::ButtonCenter:
        buttonCenter = true;
        emit q->buttonCenterChanged(true);
        break;
    case QGamepadManager::ButtonGuide:
        buttonGuide = true;
        emit q->buttonGuideChanged(true);
        break;
    default:
        break;
    }

}

/*!
 * \internal
 */\
void QGamepadPrivate::_q_handleGamepadButtonReleaseEvent(int id, QGamepadManager::GamepadButton button)
{
    Q_Q(QGamepad);
    if (id != deviceId)
        return;

    switch (button) {
    case QGamepadManager::ButtonA:
        buttonA = false;
        emit q->buttonAChanged(false);
        break;
    case QGamepadManager::ButtonB:
        buttonB = false;
        emit q->buttonBChanged(false);
        break;
    case QGamepadManager::ButtonX:
        buttonX = false;
        emit q->buttonXChanged(false);
        break;
    case QGamepadManager::ButtonY:
        buttonY = false;
        emit q->buttonYChanged(false);
        break;
    case QGamepadManager::ButtonL1:
        buttonL1 = false;
        emit q->buttonL1Changed(false);
        break;
    case QGamepadManager::ButtonR1:
        buttonR1 = false;
        emit q->buttonR1Changed(false);
        break;
    case QGamepadManager::ButtonL2:
        buttonL2 = 0.0;
        emit q->buttonL2Changed(0.0);
        break;
    case QGamepadManager::ButtonR2:
        buttonR2 = 0.0;
        emit q->buttonR2Changed(0.0);
        break;
    case QGamepadManager::ButtonL3:
        buttonL3 = false;
        emit q->buttonL3Changed(false);
        break;
    case QGamepadManager::ButtonR3:
        buttonR3 = false;
        emit q->buttonR3Changed(false);
        break;
    case QGamepadManager::ButtonSelect:
        buttonSelect = false;
        emit q->buttonSelectChanged(false);
        break;
    case QGamepadManager::ButtonStart:
        buttonStart = false;
        emit q->buttonStartChanged(false);
        break;
    case QGamepadManager::ButtonUp:
        buttonUp = false;
        emit q->buttonUpChanged(false);
        break;
    case QGamepadManager::ButtonDown:
        buttonDown = false;
        emit q->buttonDownChanged(false);
        break;
    case QGamepadManager::ButtonLeft:
        buttonLeft = false;
        emit q->buttonLeftChanged(false);
        break;
    case QGamepadManager::ButtonRight:
        buttonRight = false;
        emit q->buttonRightChanged(false);
        break;
    case QGamepadManager::ButtonCenter:
        buttonCenter = false;
        emit q->buttonCenterChanged(false);
        break;
    case QGamepadManager::ButtonGuide:
        buttonGuide = false;
        emit q->buttonGuideChanged(false);
        break;
    default:
        break;
    }
}

/*!
   \class QGamepad
   \inmodule QtGamepad
   \brief A gamepad device connected to a system

   QGamepad is used to access the current state of gamepad hardware connected
   to a system.
 */

/*!
 * Constructs a QGamepad with the given \a deviceId and \a parent.
 */
QGamepad::QGamepad(int deviceId, QObject *parent)
    : QObject(*new QGamepadPrivate(deviceId), parent)
{
    Q_D(QGamepad);
    d->gamepadManager = QGamepadManager::instance();
    connect(d->gamepadManager, SIGNAL(gamepadConnected(int)), this, SLOT(_q_handleGamepadConnected(int)));
    connect(d->gamepadManager, SIGNAL(gamepadDisconnected(int)), this, SLOT(_q_handleGamepadDisconnected(int)));
    connect(d->gamepadManager, SIGNAL(gamepadAxisEvent(int,QGamepadManager::GamepadAxis,double)), this, SLOT(_q_handleGamepadAxisEvent(int,QGamepadManager::GamepadAxis,double)));
    connect(d->gamepadManager, SIGNAL(gamepadButtonPressEvent(int,QGamepadManager::GamepadButton,double)), this, SLOT(_q_handleGamepadButtonPressEvent(int,QGamepadManager::GamepadButton,double)));
    connect(d->gamepadManager, SIGNAL(gamepadButtonReleaseEvent(int,QGamepadManager::GamepadButton)), this, SLOT(_q_handleGamepadButtonReleaseEvent(int,QGamepadManager::GamepadButton)));

    d->setConnected(d->gamepadManager->isGamepadConnected(deviceId));
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
 * This property holds the deviceId of the gamepad device.  Multiple gamepad devices can be
 * connected at any given time, so setting this property defines which gamepad to use.
 *
 * \sa QGamepadManager::connectedGamepads()
 */
int QGamepad::deviceId() const
{
    Q_D(const QGamepad);
    return d->deviceId;
}

/*!
 * \property QGamepad::connected
 * This read-only property holds the connectivity state of the gamepad device.
 * If a gamepad is connected, this property will be true, otherwise false.
 */
bool QGamepad::isConnected() const
{
    Q_D(const QGamepad);
    return d->connected;
}

/*!
 * \property QGamepad::name
 *
 * This read-only property holds the reported name of the gamepad if one
 * is available.
 */
QString QGamepad::name() const
{
    Q_D(const QGamepad);
    return d->name;
}

/*!
 * \property QGamepad::axisLeftX
 *
 * This read-only property holds the value of the left thumbstick's X axis.
 * The axis values range from -1.0 to 1.0.
 */
double QGamepad::axisLeftX() const
{
    Q_D(const QGamepad);
    return d->axisLeftX;
}

/*!
 * \property QGamepad::axisLeftY
 *
 * This read-only property holds the value of the left thumbstick's Y axis.
 * The axis values range from -1.0 to 1.0.
 */
double QGamepad::axisLeftY() const
{
    Q_D(const QGamepad);
    return d->axisLeftY;
}

/*!
 * \property QGamepad::axisRightX
 *
 * This read-only property holds the value of the right thumbstick's X axis.
 * The axis values range from -1.0 to 1.0.
 */
double QGamepad::axisRightX() const
{
    Q_D(const QGamepad);
    return d->axisRightX;
}

/*!
 * \property QGamepad::axisRightY
 *
 * This read-only property holds the value of the right thumbstick's Y axis.
 * The axis values range from -1.0 to 1.0.
 */
double QGamepad::axisRightY() const
{
    Q_D(const QGamepad);
    return d->axisRightY;
}

/*!
 * \property QGamepad::buttonA
 *
 * This read-only property holds the state of the A button.
 * The value is \c true when pressed, and \c false when not pressed.
 */
bool QGamepad::buttonA() const
{
    Q_D(const QGamepad);
    return d->buttonA;
}

/*!
 * \property QGamepad::buttonB
 *
 * This read-only property holds the state of the B button.
 * The value is \c true when pressed, and \c false when not pressed.
 *
 * \sa QGamepadManager::connectedGamepads()
 */
bool QGamepad::buttonB() const
{
    Q_D(const QGamepad);
    return d->buttonB;
}

/*!
 * \property QGamepad::buttonX
 *
 * This read-only property holds the state of the X button.
 * The value is \c true when pressed, and \c false when not pressed.
 */
bool QGamepad::buttonX() const
{
    Q_D(const QGamepad);
    return d->buttonX;
}

/*!
 * \property QGamepad::buttonY
 *
 * This read-only property holds the state of the Y button.
 * The value is \c true when pressed, and \c false when not pressed.
 */
bool QGamepad::buttonY() const
{
    Q_D(const QGamepad);
    return d->buttonY;
}

/*!
 * \property QGamepad::buttonL1
 *
 * This read-only property holds the state of the left shoulder button.
 * The value is \c true when pressed, and \c false when not pressed.
 */
bool QGamepad::buttonL1() const
{
    Q_D(const QGamepad);
    return d->buttonL1;
}

/*!
 * \property QGamepad::buttonR1
 *
 * This read-only property holds the state of the right shoulder button.
 * The value is \c true when pressed, and \c false when not pressed.
 */
bool QGamepad::buttonR1() const
{
    Q_D(const QGamepad);
    return d->buttonR1;
}

/*!
 * \property QGamepad::buttonL2
 *
 * This read-only property holds the value of the left trigger button.
 * This trigger value ranges from 0.0 when not pressed to 1.0
 * when pressed completely.
 */
double QGamepad::buttonL2() const
{
    Q_D(const QGamepad);
    return d->buttonL2;
}

/*!
 * \property QGamepad::buttonR2
 *
 * This read-only property holds the value of the right trigger button.
 * This trigger value ranges from 0.0 when not pressed to 1.0
 * when pressed completely.
 */
double QGamepad::buttonR2() const
{
    Q_D(const QGamepad);
    return d->buttonR2;
}

/*!
 * \property QGamepad::buttonSelect
 *
 * This read-only property holds the state of the Select button.
 * The value is \c true when pressed, and \c false when not pressed.
 * This button can sometimes be labeled as the Back button on some gamepads.
 */
bool QGamepad::buttonSelect() const
{
    Q_D(const QGamepad);
    return d->buttonSelect;
}

/*!
 * \property QGamepad::buttonStart
 *
 * This read-only property holds the state of the Start button.
 * The value is \c true when pressed, and \c false when not pressed.
 * This button can sometimes be labeled as the Forward button on some gamepads.
 */
bool QGamepad::buttonStart() const
{
    Q_D(const QGamepad);
    return d->buttonStart;
}

/*!
 * \property QGamepad::buttonL3
 *
 * This read-only property holds the state of the left stick button.
 * The value is \c true when pressed, and \c false when not pressed.
 * This button is usually triggered by pressing the left joystick itself.
 */
bool QGamepad::buttonL3() const
{
    Q_D(const QGamepad);
    return d->buttonL3;
}

/*!
 * \property QGamepad::buttonR3
 *
 * This read-only property holds the state of the right stick button.
 * The value is \c true when pressed, and \c false when not pressed.
 * This button is usually triggered by pressing the right joystick itself.
 */
bool QGamepad::buttonR3() const
{
    Q_D(const QGamepad);
    return d->buttonR3;
}

/*!
 * \property QGamepad::buttonUp
 *
 * This read-only property holds the state of the direction pad up button.
 * The value is \c true when pressed, and \c false when not pressed.
 */
bool QGamepad::buttonUp() const
{
    Q_D(const QGamepad);
    return d->buttonUp;
}

/*!
 * \property QGamepad::buttonDown
 *
 * This read-only property holds the state of the direction pad down button.
 * The value is \c true when pressed, and \c false when not pressed.
 */
bool QGamepad::buttonDown() const
{
    Q_D(const QGamepad);
    return d->buttonDown;
}

/*!
 * \property QGamepad::buttonLeft
 *
 * This read-only property holds the state of the direction pad left button.
 * The value is \c true when pressed, and \c false when not pressed.
 */
bool QGamepad::buttonLeft() const
{
    Q_D(const QGamepad);
    return d->buttonLeft;
}

/*!
 * \property QGamepad::buttonRight
 *
 * This read-only property holds the state of the direction pad right button.
 * The value is \c true when pressed, and \c false when not pressed.
 */
bool QGamepad::buttonRight() const
{
    Q_D(const QGamepad);
    return d->buttonRight;
}

/*!
 * \property QGamepad::buttonCenter
 *
 * This read-only property holds the state of the center button.
 * The value is \c true when pressed, and \c false when not pressed.
 */
bool QGamepad::buttonCenter() const
{
    Q_D(const QGamepad);
    return d->buttonCenter;
}

/*!
 * \property QGamepad::buttonGuide
 *
 * This read-only property holds the state of the guide button.
 * The value is \c true when pressed, and \c false when not pressed.
 * This button is typically the one in the center of the gamepad with a logo.
 * Some gamepads will not have a guide button.
 */
bool QGamepad::buttonGuide() const
{
    Q_D(const QGamepad);
    return d->buttonGuide;
}

void QGamepad::setDeviceId(int number)
{
    Q_D(QGamepad);
    if (d->deviceId != number) {
        d->deviceId = number;
        emit deviceIdChanged(number);
        d->setConnected(d->gamepadManager->isGamepadConnected(d->deviceId));
    }
}

QT_END_NAMESPACE

#include "moc_qgamepad.cpp"
