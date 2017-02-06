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
#include "qdarwingamepadbackend_p.h"

#include <QtCore/QDebug>

#import <GameController/GameController.h>

@interface QT_MANGLE_NAMESPACE(DarwinGamepadManager) : NSObject
{
    QDarwinGamepadBackend *backend;
    NSMutableArray *connectedControllers;
}

@property (nonatomic, strong) id connectObserver;
@property (nonatomic, strong) id disconnectObserver;

-(void)addMonitoredController:(GCController *)controller;
-(void)removeMonitoredController:(GCController *)controller;

@end

@implementation QT_MANGLE_NAMESPACE(DarwinGamepadManager)

-(id)initWithBackend:(QDarwinGamepadBackend *)gamepadBackend {
    if (self = [super init]) {
        backend = gamepadBackend;
        connectedControllers = [[NSMutableArray alloc] init];
        //Setup observers for monitoring controller connections/disconnections
        self.connectObserver = [[NSNotificationCenter defaultCenter] addObserverForName:GCControllerDidConnectNotification
                                                                                 object:nil
                                                                                  queue:[NSOperationQueue mainQueue]
                                                                             usingBlock:^(NSNotification *note) {
            GCController *controller = (GCController*)note.object;
            [self addMonitoredController:controller];

        }];
        self.disconnectObserver = [[NSNotificationCenter defaultCenter] addObserverForName:GCControllerDidDisconnectNotification
                                                                                    object:nil
                                                                                    queue:[NSOperationQueue mainQueue]
                                                                               usingBlock:^(NSNotification *note) {
            GCController *controller = (GCController*)note.object;
            [self removeMonitoredController:controller];
        }];
        //Set initial controller values
        for (int i = 0; i < 4; ++i)
            [connectedControllers addObject:[NSNull null]];

        //Add monitoring for any alrready connected controllers
        for (NSUInteger i = 0; i < [[GCController controllers] count]; ++i) {
            [self addMonitoredController:[GCController controllers][i]];
        }
    }
    return self;
}

-(void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self.connectObserver];
    [[NSNotificationCenter defaultCenter] removeObserver:self.disconnectObserver];
    [connectedControllers release];
    [super dealloc];
}

-(void)addMonitoredController:(GCController *)controller
{
    int index = -1;
    for (int i = 0; i < 4; ++i) {
        if (connectedControllers[i] == [NSNull null]) {
            [connectedControllers replaceObjectAtIndex: i withObject: controller];
            index = i;
            break;
        }
    }
#if QT_OSX_PLATFORM_SDK_EQUAL_OR_ABOVE(__MAC_10_11) || QT_IOS_PLATFORM_SDK_EQUAL_OR_ABOVE(__IPHONE_9_0) || defined(Q_OS_TVOS)
    controller.playerIndex = GCControllerPlayerIndex(index);
#else
    controller.playerIndex = index;
#endif

    QMetaObject::invokeMethod(backend, "darwinGamepadAdded", Qt::AutoConnection, Q_ARG(int, index));

    //Pause button handler
    [controller setControllerPausedHandler:^(GCController *controller) {
        Q_UNUSED(controller)
        QMetaObject::invokeMethod(backend, "handlePauseButton", Qt::AutoConnection, Q_ARG(int, index));
    }];

    if (controller.extendedGamepad) {
        //leftShoulder
        [controller.extendedGamepad.leftShoulder setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
            Q_UNUSED(button)
            if (pressed) {
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonPressed", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonL1),
                                          Q_ARG(double, value));
            } else {
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonReleased", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonL1));
            }
        }];
        //rightShoulder
        [controller.extendedGamepad.rightShoulder setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
            Q_UNUSED(button)
            if (pressed) {
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonPressed", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonR1),
                                          Q_ARG(double, value));
            } else {
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonReleased", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonR1));
            }
        }];
        //dpad
        [controller.extendedGamepad.dpad setValueChangedHandler:^(GCControllerDirectionPad *dpad, float xValue, float yValue) {
            Q_UNUSED(dpad)
            if (xValue > 0) {
                //right
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonPressed", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonRight),
                                          Q_ARG(double, 1));
            } else if (xValue < 0) {
                //left
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonPressed", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonLeft),
                                          Q_ARG(double, 1));
            } else {
                //released
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonReleased", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonRight));
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonReleased", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonLeft));
            }
            if (yValue > 0) {
                //up
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonPressed", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonUp),
                                          Q_ARG(double, 1));
            } else if (yValue < 0) {
                //down
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonPressed", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonDown),
                                          Q_ARG(double, 1));
            } else {
                //released
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonReleased", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonUp));
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonReleased", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonDown));
            }
        }];
        //buttonA
        [controller.extendedGamepad.buttonA setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
            Q_UNUSED(button)
            if (pressed) {
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonPressed", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonA),
                                          Q_ARG(double, value));
            } else {
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonReleased", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonA));
            }
        }];
        //buttonB
        [controller.extendedGamepad.buttonB setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
            Q_UNUSED(button)
            if (pressed) {
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonPressed", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonB),
                                          Q_ARG(double, value));
            } else {
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonReleased", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonB));
            }
        }];
        //buttonX
        [controller.extendedGamepad.buttonX setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
            Q_UNUSED(button)
            if (pressed) {
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonPressed", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonX),
                                          Q_ARG(double, value));
            } else {
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonReleased", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonX));
            }
        }];
        //buttonY
        [controller.extendedGamepad.buttonY setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
            //Invoke slot
            Q_UNUSED(button)
            if (pressed) {
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonPressed", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonY),
                                          Q_ARG(double, value));
            } else {
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonReleased", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonY));
            }
        }];

        //leftThumbstick
        [controller.extendedGamepad.leftThumbstick setValueChangedHandler:^(GCControllerDirectionPad *dpad, float xValue, float yValue) {
            Q_UNUSED(dpad)
            QMetaObject::invokeMethod(backend, "darwinGamepadAxisMoved", Qt::AutoConnection,
                                      Q_ARG(int, index),
                                      Q_ARG(QGamepadManager::GamepadAxis, QGamepadManager::AxisLeftX),
                                      Q_ARG(double, xValue));
            QMetaObject::invokeMethod(backend, "darwinGamepadAxisMoved", Qt::AutoConnection,
                                      Q_ARG(int, index),
                                      Q_ARG(QGamepadManager::GamepadAxis, QGamepadManager::AxisLeftY),
                                      Q_ARG(double, -yValue));
        }];
        //rightTumbstick
        [controller.extendedGamepad.rightThumbstick setValueChangedHandler:^(GCControllerDirectionPad *dpad, float xValue, float yValue) {
            Q_UNUSED(dpad)
            QMetaObject::invokeMethod(backend, "darwinGamepadAxisMoved", Qt::AutoConnection,
                                      Q_ARG(int, index),
                                      Q_ARG(QGamepadManager::GamepadAxis, QGamepadManager::AxisRightX),
                                      Q_ARG(double, xValue));
            QMetaObject::invokeMethod(backend, "darwinGamepadAxisMoved", Qt::AutoConnection,
                                      Q_ARG(int, index),
                                      Q_ARG(QGamepadManager::GamepadAxis, QGamepadManager::AxisRightY),
                                      Q_ARG(double, -yValue));
        }];
        //leftTrigger
        [controller.extendedGamepad.leftTrigger setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
            Q_UNUSED(button)
            if (pressed) {
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonPressed", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonL2),
                                          Q_ARG(double, value));
            } else {
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonReleased", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonL2));
            }
        }];
        //rightTrigger
        [controller.extendedGamepad.rightTrigger setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
            Q_UNUSED(button)
            if (pressed) {
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonPressed", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonR2),
                                          Q_ARG(double, value));
            } else {
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonReleased", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonR2));
            }

        }];
    } else if (controller.gamepad) {
        //leftShoulder
        [controller.gamepad.leftShoulder setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
            Q_UNUSED(button)
            if (pressed) {
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonPressed", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonL1),
                                          Q_ARG(double, value));
            } else {
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonReleased", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonL1));
            }
        }];
        //rightShoulder
        [controller.gamepad.rightShoulder setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
            Q_UNUSED(button)
            if (pressed) {
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonPressed", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonR1),
                                          Q_ARG(double, value));
            } else {
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonReleased", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonR1));
            }
        }];
        //dpad
        [controller.gamepad.dpad setValueChangedHandler:^(GCControllerDirectionPad *dpad, float xValue, float yValue) {
            Q_UNUSED(dpad)
            if (xValue > 0) {
                //right
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonPressed", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonRight),
                                          Q_ARG(double, 1));
            } else if (xValue < 0) {
                //left
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonPressed", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonLeft),
                                          Q_ARG(double, 1));
            } else {
                //released
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonReleased", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonRight));
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonReleased", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonLeft));
            }
            if (yValue > 0) {
                //up
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonPressed", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonUp),
                                          Q_ARG(double, 1));
            } else if (yValue < 0) {
                //down
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonPressed", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonDown),
                                          Q_ARG(double, 1));
            } else {
                //released
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonReleased", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonUp));
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonReleased", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonDown));
            }
        }];
        //buttonA
        [controller.gamepad.buttonA setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
            Q_UNUSED(button)
            if (pressed) {
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonPressed", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonA),
                                          Q_ARG(double, value));
            } else {
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonReleased", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonA));
            }
        }];
        //buttonB
        [controller.gamepad.buttonB setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
            Q_UNUSED(button)
            if (pressed) {
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonPressed", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonB),
                                          Q_ARG(double, value));
            } else {
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonReleased", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonB));
            }
        }];
        //buttonX
        [controller.gamepad.buttonX setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
            Q_UNUSED(button)
            if (pressed) {
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonPressed", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonX),
                                          Q_ARG(double, value));
            } else {
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonReleased", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonX));
            }
        }];
        //buttonY
        [controller.gamepad.buttonY setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
            Q_UNUSED(button)
            if (pressed) {
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonPressed", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonY),
                                          Q_ARG(double, value));
            } else {
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonReleased", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonY));
            }
        }];
    }
#ifdef Q_OS_TVOS
    else if (controller.microGamepad) {
        //leftThumbstick
        [controller.microGamepad.dpad setValueChangedHandler:^(GCControllerDirectionPad *dpad, float xValue, float yValue) {
            Q_UNUSED(dpad)
            QMetaObject::invokeMethod(backend, "darwinGamepadAxisMoved", Qt::AutoConnection,
                                      Q_ARG(int, index),
                                      Q_ARG(QGamepadManager::GamepadAxis, QGamepadManager::AxisLeftX),
                                      Q_ARG(double, xValue));
            QMetaObject::invokeMethod(backend, "darwinGamepadAxisMoved", Qt::AutoConnection,
                                      Q_ARG(int, index),
                                      Q_ARG(QGamepadManager::GamepadAxis, QGamepadManager::AxisLeftY),
                                      Q_ARG(double, -yValue));
        }];
        //buttonA
        [controller.microGamepad.buttonA setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
            Q_UNUSED(button)
            if (pressed) {
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonPressed", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonA),
                                          Q_ARG(double, value));
            } else {
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonReleased", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonA));
            }
        }];
        //buttonX
        [controller.microGamepad.buttonX setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
            Q_UNUSED(button)
            if (pressed) {
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonPressed", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonX),
                                          Q_ARG(double, value));
            } else {
                QMetaObject::invokeMethod(backend, "darwinGamepadButtonReleased", Qt::AutoConnection,
                                          Q_ARG(int, index),
                                          Q_ARG(QGamepadManager::GamepadButton, QGamepadManager::ButtonX));
            }
        }];
    }
#endif
}

-(void)removeMonitoredController:(GCController *)controller
{
    int index = -1;
    for (int i = 0; i < 4; ++i) {
        if (connectedControllers[i] == controller) {
            [connectedControllers replaceObjectAtIndex: i withObject: [NSNull null]];
            index = i;
            break;
        }
    }

    QMetaObject::invokeMethod(backend, "darwinGamepadRemoved", Qt::AutoConnection, Q_ARG(int, index));
}

@end

QT_BEGIN_NAMESPACE

QDarwinGamepadBackend::QDarwinGamepadBackend(QObject *parent)
    : QGamepadBackend(parent)
    , m_darwinGamepadManager(Q_NULLPTR)
    , m_isMonitoringActive(false)
{
    m_darwinGamepadManager = [[QT_MANGLE_NAMESPACE(DarwinGamepadManager) alloc] initWithBackend:this];
}

QDarwinGamepadBackend::~QDarwinGamepadBackend()
{
    [m_darwinGamepadManager release];
}

bool QDarwinGamepadBackend::start()
{
    m_isMonitoringActive = true;
    return true;
}

void QDarwinGamepadBackend::stop()
{
    m_isMonitoringActive = false;
}

void QDarwinGamepadBackend::darwinGamepadAdded(int index)
{
    if (m_isMonitoringActive) {
        emit gamepadAdded(index);
        m_pauseButtonMap.insert(index, false);
    }
}

void QDarwinGamepadBackend::darwinGamepadRemoved(int index)
{
    if (m_isMonitoringActive) {
        emit gamepadRemoved(index);
        m_pauseButtonMap.remove(index);
    }
}

void QDarwinGamepadBackend::darwinGamepadAxisMoved(int index, QGamepadManager::GamepadAxis axis, double value)
{
    if (m_isMonitoringActive)
        emit gamepadAxisMoved(index, axis, value);
}

void QDarwinGamepadBackend::darwinGamepadButtonPressed(int index, QGamepadManager::GamepadButton button, double value)
{
    if (m_isMonitoringActive)
        emit gamepadButtonPressed(index, button, value);
}

void QDarwinGamepadBackend::darwinGamepadButtonReleased(int index, QGamepadManager::GamepadButton button)
{
    if (m_isMonitoringActive)
        emit gamepadButtonReleased(index, button);
}

void QDarwinGamepadBackend::handlePauseButton(int index)
{
    //If already pressed
    if (m_pauseButtonMap.value(index)) {
        emit gamepadButtonReleased(index, QGamepadManager::ButtonStart);
        m_pauseButtonMap[index] = false;
    } else {
        //If not currently pressed
        emit gamepadButtonPressed(index, QGamepadManager::ButtonStart, 1.0);
        m_pauseButtonMap[index] = true;
    }
}

QT_END_NAMESPACE
