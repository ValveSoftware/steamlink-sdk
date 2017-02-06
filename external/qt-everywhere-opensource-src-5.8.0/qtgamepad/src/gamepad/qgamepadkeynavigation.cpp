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

#include "qgamepadkeynavigation.h"
#include <QtGui/QKeyEvent>
#include <QtGui/QGuiApplication>
#include <QtGui/QWindow>
#include <QtGamepad/QGamepad>

QT_BEGIN_NAMESPACE

QGamepadKeyNavigation::QGamepadKeyNavigation(QObject *parent)
    : QObject(parent)
    , m_active(true)
    , m_gamepad(Q_NULLPTR)
    , m_buttonL2Pressed(false)
    , m_buttonR2Pressed(false)
{
    m_gamepadManger = QGamepadManager::instance();

    //Default keymap
    m_keyMapping.insert(QGamepadManager::ButtonUp, Qt::Key_Up);
    m_keyMapping.insert(QGamepadManager::ButtonDown, Qt::Key_Down);
    m_keyMapping.insert(QGamepadManager::ButtonLeft, Qt::Key_Left);
    m_keyMapping.insert(QGamepadManager::ButtonRight, Qt::Key_Right);
    m_keyMapping.insert(QGamepadManager::ButtonA, Qt::Key_Return);
    m_keyMapping.insert(QGamepadManager::ButtonB, Qt::Key_Back);
    m_keyMapping.insert(QGamepadManager::ButtonX, Qt::Key_Back);
    m_keyMapping.insert(QGamepadManager::ButtonY, Qt::Key_Back);
    m_keyMapping.insert(QGamepadManager::ButtonSelect, Qt::Key_Back);
    m_keyMapping.insert(QGamepadManager::ButtonStart, Qt::Key_Return);
    m_keyMapping.insert(QGamepadManager::ButtonGuide, Qt::Key_Back);
    m_keyMapping.insert(QGamepadManager::ButtonL1, Qt::Key_Back);
    m_keyMapping.insert(QGamepadManager::ButtonR1, Qt::Key_Forward);
    m_keyMapping.insert(QGamepadManager::ButtonL2, Qt::Key_Back);
    m_keyMapping.insert(QGamepadManager::ButtonR2, Qt::Key_Forward);
    m_keyMapping.insert(QGamepadManager::ButtonL3, Qt::Key_Back);
    m_keyMapping.insert(QGamepadManager::ButtonR3, Qt::Key_Forward);

    connect(m_gamepadManger, SIGNAL(gamepadButtonPressEvent(int,QGamepadManager::GamepadButton,double)),
            this, SLOT(processGamepadButtonPressEvent(int,QGamepadManager::GamepadButton,double)));
    connect(m_gamepadManger, SIGNAL(gamepadButtonReleaseEvent(int,QGamepadManager::GamepadButton)),
            this, SLOT(procressGamepadButtonReleaseEvent(int,QGamepadManager::GamepadButton)));
}

bool QGamepadKeyNavigation::active() const
{
    return m_active;
}

QGamepad *QGamepadKeyNavigation::gamepad() const
{
    return m_gamepad;
}

Qt::Key QGamepadKeyNavigation::upKey() const
{
    return m_keyMapping[QGamepadManager::ButtonUp];
}

Qt::Key QGamepadKeyNavigation::downKey() const
{
    return m_keyMapping[QGamepadManager::ButtonDown];
}

Qt::Key QGamepadKeyNavigation::leftKey() const
{
    return m_keyMapping[QGamepadManager::ButtonLeft];
}

Qt::Key QGamepadKeyNavigation::rightKey() const
{
    return m_keyMapping[QGamepadManager::ButtonRight];
}

Qt::Key QGamepadKeyNavigation::buttonAKey() const
{
    return m_keyMapping[QGamepadManager::ButtonA];
}

Qt::Key QGamepadKeyNavigation::buttonBKey() const
{
    return m_keyMapping[QGamepadManager::ButtonB];
}

Qt::Key QGamepadKeyNavigation::buttonXKey() const
{
    return m_keyMapping[QGamepadManager::ButtonX];
}

Qt::Key QGamepadKeyNavigation::buttonYKey() const
{
    return m_keyMapping[QGamepadManager::ButtonY];
}

Qt::Key QGamepadKeyNavigation::buttonSelectKey() const
{
    return m_keyMapping[QGamepadManager::ButtonSelect];
}

Qt::Key QGamepadKeyNavigation::buttonStartKey() const
{
    return m_keyMapping[QGamepadManager::ButtonStart];
}

Qt::Key QGamepadKeyNavigation::buttonGuideKey() const
{
    return m_keyMapping[QGamepadManager::ButtonGuide];
}

Qt::Key QGamepadKeyNavigation::buttonL1Key() const
{
    return m_keyMapping[QGamepadManager::ButtonL1];
}

Qt::Key QGamepadKeyNavigation::buttonR1Key() const
{
    return m_keyMapping[QGamepadManager::ButtonL2];
}

Qt::Key QGamepadKeyNavigation::buttonL2Key() const
{
    return m_keyMapping[QGamepadManager::ButtonL2];
}

Qt::Key QGamepadKeyNavigation::buttonR2Key() const
{
    return m_keyMapping[QGamepadManager::ButtonL2];
}

Qt::Key QGamepadKeyNavigation::buttonL3Key() const
{
    return m_keyMapping[QGamepadManager::ButtonL3];
}

Qt::Key QGamepadKeyNavigation::buttonR3Key() const
{
    return m_keyMapping[QGamepadManager::ButtonL3];
}

void QGamepadKeyNavigation::setActive(bool isActive)
{
    if (m_active != isActive) {
        m_active = isActive;
        emit activeChanged(isActive);
    }
}

void QGamepadKeyNavigation::setGamepad(QGamepad *gamepad)
{
    if (m_gamepad != gamepad) {
        m_gamepad = gamepad;
        emit gamepadChanged(gamepad);
    }
}

void QGamepadKeyNavigation::setUpKey(Qt::Key key)
{
    if (m_keyMapping[QGamepadManager::ButtonUp] != key) {
        m_keyMapping[QGamepadManager::ButtonUp] = key;
        emit upKeyChanged(key);
    }
}

void QGamepadKeyNavigation::setDownKey(Qt::Key key)
{
    if (m_keyMapping[QGamepadManager::ButtonDown] != key) {
        m_keyMapping[QGamepadManager::ButtonDown] = key;
        emit downKeyChanged(key);
    }
}

void QGamepadKeyNavigation::setLeftKey(Qt::Key key)
{
    if (m_keyMapping[QGamepadManager::ButtonLeft] != key) {
        m_keyMapping[QGamepadManager::ButtonLeft] = key;
        emit leftKeyChanged(key);
    }
}

void QGamepadKeyNavigation::setRightKey(Qt::Key key)
{
    if (m_keyMapping[QGamepadManager::ButtonRight] != key) {
        m_keyMapping[QGamepadManager::ButtonRight] = key;
        emit rightKeyChanged(key);
    }
}

void QGamepadKeyNavigation::setButtonAKey(Qt::Key key)
{
    if (m_keyMapping[QGamepadManager::ButtonA] != key) {
        m_keyMapping[QGamepadManager::ButtonA] = key;
        emit buttonAKeyChanged(key);
    }
}

void QGamepadKeyNavigation::setButtonBKey(Qt::Key key)
{
    if (m_keyMapping[QGamepadManager::ButtonB] != key) {
        m_keyMapping[QGamepadManager::ButtonB] = key;
        emit buttonBKeyChanged(key);
    }
}

void QGamepadKeyNavigation::setButtonXKey(Qt::Key key)
{
    if (m_keyMapping[QGamepadManager::ButtonX] != key) {
        m_keyMapping[QGamepadManager::ButtonX] = key;
        emit buttonXKeyChanged(key);
    }
}

void QGamepadKeyNavigation::setButtonYKey(Qt::Key key)
{
    if (m_keyMapping[QGamepadManager::ButtonY] != key) {
        m_keyMapping[QGamepadManager::ButtonY] = key;
        emit buttonYKeyChanged(key);
    }
}

void QGamepadKeyNavigation::setButtonSelectKey(Qt::Key key)
{
    if (m_keyMapping[QGamepadManager::ButtonSelect] != key) {
        m_keyMapping[QGamepadManager::ButtonSelect] = key;
        emit buttonSelectKeyChanged(key);
    }
}

void QGamepadKeyNavigation::setButtonStartKey(Qt::Key key)
{
    if (m_keyMapping[QGamepadManager::ButtonStart] != key) {
        m_keyMapping[QGamepadManager::ButtonStart] = key;
        emit buttonStartKeyChanged(key);
    }
}

void QGamepadKeyNavigation::setButtonGuideKey(Qt::Key key)
{
    if (m_keyMapping[QGamepadManager::ButtonGuide] != key) {
        m_keyMapping[QGamepadManager::ButtonGuide] = key;
        emit buttonGuideKeyChanged(key);
    }
}

void QGamepadKeyNavigation::setButtonL1Key(Qt::Key key)
{
    if (m_keyMapping[QGamepadManager::ButtonL1] != key) {
        m_keyMapping[QGamepadManager::ButtonL1] = key;
        emit buttonL1KeyChanged(key);
    }
}

void QGamepadKeyNavigation::setButtonR1Key(Qt::Key key)
{
    if (m_keyMapping[QGamepadManager::ButtonR1] != key) {
        m_keyMapping[QGamepadManager::ButtonR1] = key;
        emit buttonR1KeyChanged(key);
    }
}

void QGamepadKeyNavigation::setButtonL2Key(Qt::Key key)
{
    if (m_keyMapping[QGamepadManager::ButtonL2] != key) {
        m_keyMapping[QGamepadManager::ButtonL2] = key;
        emit buttonL1KeyChanged(key);
    }
}

void QGamepadKeyNavigation::setButtonR2Key(Qt::Key key)
{
    if (m_keyMapping[QGamepadManager::ButtonR2] != key) {
        m_keyMapping[QGamepadManager::ButtonR2] = key;
        emit buttonR1KeyChanged(key);
    }
}

void QGamepadKeyNavigation::setButtonL3Key(Qt::Key key)
{
    if (m_keyMapping[QGamepadManager::ButtonL3] != key) {
        m_keyMapping[QGamepadManager::ButtonL3] = key;
        emit buttonL1KeyChanged(key);
    }
}

void QGamepadKeyNavigation::setButtonR3Key(Qt::Key key)
{
    if (m_keyMapping[QGamepadManager::ButtonR3] != key) {
        m_keyMapping[QGamepadManager::ButtonR3] = key;
        emit buttonR1KeyChanged(key);
    }
}

void QGamepadKeyNavigation::processGamepadButtonPressEvent(int index, QGamepadManager::GamepadButton button, double value)
{
    Q_UNUSED(value)

    //If a gamepad has been set then, only use the events of that gamepad
    if (m_gamepad && m_gamepad->deviceId() != index)
        return;

    //Trigger buttons are a special case as they get multiple press events as the value changes
    if (button == QGamepadManager::ButtonL2 && m_buttonL2Pressed)
        return;
    else
        m_buttonL2Pressed = true;
    if (button == QGamepadManager::ButtonR2 && m_buttonR2Pressed)
        return;
    else
        m_buttonR2Pressed = true;

    QKeyEvent *event = new QKeyEvent(QEvent::KeyPress, m_keyMapping[button], Qt::NoModifier);
    sendGeneratedKeyEvent(event);
}

void QGamepadKeyNavigation::procressGamepadButtonReleaseEvent(int index, QGamepadManager::GamepadButton button)
{
    //If a gamepad has been set then, only use the events of that gamepad
    if (m_gamepad && m_gamepad->deviceId() != index)
        return;

    //Free the trigger buttons if necessary
    if (button == QGamepadManager::ButtonL2)
        m_buttonL2Pressed = false;
    if (button == QGamepadManager::ButtonR2)
        m_buttonR2Pressed = false;

    QKeyEvent *event = new QKeyEvent(QEvent::KeyRelease, m_keyMapping[button], Qt::NoModifier);
    sendGeneratedKeyEvent(event);
}

void QGamepadKeyNavigation::sendGeneratedKeyEvent(QKeyEvent *event)
{
    if (!m_active) {
        delete event;
        return;
    }
    const QGuiApplication *app = qApp;
    QWindow *focusWindow = app ? app->focusWindow() : 0;
    if (focusWindow)
        QGuiApplication::sendEvent(focusWindow, event);
}

QT_END_NAMESPACE
