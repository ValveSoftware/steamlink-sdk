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
#include "qsdlgamepadbackend_p.h"

#include <QtCore/QDebug>

#include <SDL.h>

QT_BEGIN_NAMESPACE

QSdlGamepadBackend::QSdlGamepadBackend(QObject *parent)
    : QGamepadBackend(parent)
{
    connect(&m_eventLoopTimer, SIGNAL(timeout()), this, SLOT(pumpSdlEventLoop()));
}

QSdlGamepadBackend::~QSdlGamepadBackend()
{
}

void QSdlGamepadBackend::pumpSdlEventLoop()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_CONTROLLERAXISMOTION) {
            SDL_ControllerAxisEvent axisEvent = event.caxis;
            //qDebug() << axisEvent.timestamp << "Axis Event: " << axisEvent.which << axisEvent.axis << axisEvent.value;
            double value;
            if (axisEvent.value >= 0)
                value = axisEvent.value / 32767.0;
            else
                value = axisEvent.value / 32768.0;
            switch (axisEvent.axis) {
                case SDL_CONTROLLER_AXIS_LEFTX:
                    emit gamepadAxisMoved(m_instanceIdForIndex[axisEvent.which], QGamepadManager::AxisLeftX, value);
                    break;
                case SDL_CONTROLLER_AXIS_LEFTY:
                    emit gamepadAxisMoved(m_instanceIdForIndex[axisEvent.which], QGamepadManager::AxisLeftY, value);
                    break;
                case SDL_CONTROLLER_AXIS_RIGHTX:
                    emit gamepadAxisMoved(m_instanceIdForIndex[axisEvent.which], QGamepadManager::AxisRightX, value);
                    break;
                case SDL_CONTROLLER_AXIS_RIGHTY:
                    emit gamepadAxisMoved(m_instanceIdForIndex[axisEvent.which], QGamepadManager::AxisRightY, value);
                    break;
                case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
                    if (value == 0)
                        emit gamepadButtonReleased(m_instanceIdForIndex[axisEvent.which], QGamepadManager::ButtonL2);
                    else
                        emit gamepadButtonPressed(m_instanceIdForIndex[axisEvent.which], QGamepadManager::ButtonL2, value);
                    break;
                case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
                    if (value == 0)
                        emit gamepadButtonReleased(m_instanceIdForIndex[axisEvent.which], QGamepadManager::ButtonR2);
                    else
                        emit gamepadButtonPressed(m_instanceIdForIndex[axisEvent.which], QGamepadManager::ButtonR2, value);
                    break;
                default:
                    break;
            }

        } else if (event.type == SDL_CONTROLLERBUTTONDOWN) {
            SDL_ControllerButtonEvent buttonEvent = event.cbutton;
            //qDebug() << buttonEvent.timestamp << "Button Press: " << buttonEvent.which << buttonEvent.button << buttonEvent.state;
            emit gamepadButtonPressed(m_instanceIdForIndex[buttonEvent.which], translateButton(buttonEvent.button), 1.0);
        } else if (event.type == SDL_CONTROLLERBUTTONUP) {
            SDL_ControllerButtonEvent buttonEvent = event.cbutton;
            //qDebug() << buttonEvent.timestamp << "Button Release: " << buttonEvent.which << buttonEvent.button << buttonEvent.state;
            emit gamepadButtonReleased(m_instanceIdForIndex[buttonEvent.which], translateButton(buttonEvent.button));
        } else if (event.type == SDL_CONTROLLERDEVICEADDED) {
            SDL_ControllerDeviceEvent deviceEvent = event.cdevice;
            //qDebug() << deviceEvent.timestamp << "Controller Added: " << deviceEvent.which;
            addController(deviceEvent.which);
        } else if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
            SDL_ControllerDeviceEvent deviceEvent = event.cdevice;

            int index = m_instanceIdForIndex[deviceEvent.which];
            SDL_GameControllerClose(m_indexForController[index]);
            emit gamepadRemoved(index);
            m_indexForController.remove(index);
            m_instanceIdForIndex.remove(deviceEvent.which);

        } else if (event.type == SDL_CONTROLLERDEVICEREMAPPED) {
            //SDL_ControllerDeviceEvent deviceEvent = event.cdevice;
            //qDebug() << deviceEvent.timestamp << "Controller Remapped: " << deviceEvent.which;
        }
    }
}

bool QSdlGamepadBackend::start()
{
    //Initialize SDL with necessary subsystems for gamepad support
    if (SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK)) {
        qDebug() << SDL_GetError();
        return false;
    }

    m_eventLoopTimer.start(16);
    for (int i = 0; i < SDL_NumJoysticks() ; i++)
        addController(i);

    return true;
}

void QSdlGamepadBackend::stop()
{
    m_eventLoopTimer.stop();
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);
}

QGamepadManager::GamepadButton QSdlGamepadBackend::translateButton(int button)
{
    switch (button) {
    case SDL_CONTROLLER_BUTTON_A:
        return QGamepadManager::ButtonA;
    case SDL_CONTROLLER_BUTTON_B:
        return QGamepadManager::ButtonB;
    case SDL_CONTROLLER_BUTTON_X:
        return QGamepadManager::ButtonX;
    case SDL_CONTROLLER_BUTTON_Y:
        return QGamepadManager::ButtonY;
    case SDL_CONTROLLER_BUTTON_BACK:
        return QGamepadManager::ButtonSelect;
    case SDL_CONTROLLER_BUTTON_GUIDE:
        return QGamepadManager::ButtonGuide;
    case SDL_CONTROLLER_BUTTON_START:
        return QGamepadManager::ButtonStart;
    case SDL_CONTROLLER_BUTTON_LEFTSTICK:
        return QGamepadManager::ButtonL3;
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
        return QGamepadManager::ButtonR3;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
        return QGamepadManager::ButtonL1;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
        return QGamepadManager::ButtonR1;
    case SDL_CONTROLLER_BUTTON_DPAD_UP:
        return QGamepadManager::ButtonUp;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
        return QGamepadManager::ButtonDown;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
        return QGamepadManager::ButtonLeft;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
        return QGamepadManager::ButtonRight;
    default:
        return QGamepadManager::ButtonInvalid;
    }
}

void QSdlGamepadBackend::addController(int index)
{
    char GUID[100];
    SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(index), GUID, 100);
    if (!SDL_IsGameController(index))
        return;

    SDL_GameController *controller = SDL_GameControllerOpen(index);
    if (controller) {
        m_indexForController.insert(index, controller);
        int instanceID = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller));
        m_instanceIdForIndex.insert(instanceID, index);
        //qDebug() << "Controller " << index << " added with instanceId: " << instanceID;
        emit gamepadAdded(index);
    }
}

QT_END_NAMESPACE
