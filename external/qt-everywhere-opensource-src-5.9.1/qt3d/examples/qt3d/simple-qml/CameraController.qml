/****************************************************************************
**
** Copyright (C) 2015 Paul Lemire <paul.lemire350@gmail.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

import Qt3D.Core 2.0
import Qt3D.Render 2.0
import Qt3D.Input 2.0
import Qt3D.Logic 2.0
import QtQml 2.2

Entity {
    id: root
    property Camera camera
    property real linearSpeed: 10.0
    property real orbitSpeed: -180.0
    property real lookSpeed: 180.0
    property bool firstPersonMode: true

    QtObject {
        id: d
        readonly property vector3d firstPersonUp: Qt.vector3d(0, 1, 0)
        property bool leftMouseButtonPressed: false
        property bool rightMouseButtonPressed: false
        property real vx: 0;
        property real vy: 0;
        property real vz: 0;
        property real dx: 0
        property real dy: 0
        property bool fineMotion: false
    }

    KeyboardDevice {
        id: keyboardSourceDevice
    }

    MouseDevice {
        id: mouseSourceDevice
        sensitivity: d.fineMotion ? 0.01 : 0.1
    }

    LogicalDevice {
        id: cameraControlDevice

        actions: [
            Action {
                name: "LMB"
                inputs: [
                    ActionInput {
                        sourceDevice: mouseSourceDevice
                        buttons: [MouseEvent.LeftButton]
                    }
                ]
            },
            Action {
                name: "RMB"
                inputs: [
                    ActionInput {
                        sourceDevice: mouseSourceDevice
                        buttons: [MouseEvent.RightButton]
                    }
                ]
            },
            Action {
                name: "fineMotion"
                inputs: [
                    ActionInput {
                        sourceDevice: keyboardSourceDevice
                        buttons: [Qt.Key_Shift]
                    }
                ]
            }

        ] // actions

        axes: [
            // Rotation
            Axis {
                name: "RX"
                inputs: [
                    AnalogAxisInput {
                        sourceDevice: mouseSourceDevice
                        axis: MouseDevice.X
                    }
                ]
            },
            Axis {
                name: "RY"
                inputs: [
                    AnalogAxisInput {
                        sourceDevice: mouseSourceDevice
                        axis: MouseDevice.Y
                    }
                ]
            },
            // Translation
            Axis {
                name: "TX"
                inputs: [
                    ButtonAxisInput {
                        sourceDevice: keyboardSourceDevice
                        buttons: [Qt.Key_Left]
                        scale: -1.0
                    },
                    ButtonAxisInput {
                        sourceDevice: keyboardSourceDevice
                        buttons: [Qt.Key_Right]
                        scale: 1.0
                    }
                ]
            },
            Axis {
                name: "TZ"
                inputs: [
                    ButtonAxisInput {
                        sourceDevice: keyboardSourceDevice
                        buttons: [Qt.Key_Up]
                        scale: 1.0
                    },
                    ButtonAxisInput {
                        sourceDevice: keyboardSourceDevice
                        buttons: [Qt.Key_Down]
                        scale: -1.0
                    }
                ]
            },
            Axis {
                name: "TY"
                inputs: [
                    ButtonAxisInput {
                        sourceDevice: keyboardSourceDevice
                        buttons: [Qt.Key_PageUp]
                        scale: 1.0
                    },
                    ButtonAxisInput {
                        sourceDevice: keyboardSourceDevice
                        buttons: [Qt.Key_PageDown]
                        scale: -1.0
                    }
                ]
            }
        ] // axes
    }

    components: [
        AxisActionHandler {
            id: handler
            logicalDevice: cameraControlDevice

            onAxisValueChanged: {

                switch (name) {

                case "TX": {
                    d.vx = axisValue * linearSpeed
                    break;
                }

                case "TY": {
                    d.vy = axisValue * linearSpeed
                    break;
                }

                case "TZ": {
                    d.vz = axisValue * linearSpeed
                    break;
                }

                case "RX": {
                    d.dx = axisValue;
                    break;
                }

                case "RY": {
                    d.dy = axisValue;
                    break;
                }

                }
            }

            onActionStarted: {

                switch (name) {

                case "LMB": {
                    d.leftMouseButtonPressed = true;
                    break;
                }

                case "RMB": {
                    d.rightMouseButtonPressed = true;
                    break;
                }

                case "fineMotion": {
                    console.log("fineMotion started")
                    d.fineMotion = true;
                    break;
                }

                }

            }

            onActionFinished: {

                switch (name) {

                case "LMB": {
                    d.leftMouseButtonPressed = false;
                    break;
                }

                case "RMB": {
                    d.rightMouseButtonPressed = false;
                    break;
                }

                case "fineMotion": {
                    console.log("fineMotion finished")
                    d.fineMotion = false;
                    break;
                }

                }
            }
        },

        FrameAction {
            onTriggered: {
                // The time difference since the last frame is passed in as the
                // argument dt. It is a floating point value in units of seconds.
                root.camera.translate(Qt.vector3d(d.vx, d.vy, d.vz).times(dt))

                if (d.leftMouseButtonPressed) {
                    if (root.firstPersonMode)
                        root.camera.pan(root.lookSpeed * d.dx * dt, d.firstPersonUp)
                    else
                        root.camera.pan(root.lookSpeed * d.dx * dt)
                    root.camera.tilt(root.lookSpeed * d.dy * dt)
                } else if (d.rightMouseButtonPressed) {
                    if (root.firstPersonMode)
                        root.camera.panAboutViewCenter(root.lookSpeed * d.dx * dt, d.firstPersonUp)
                    else
                        root.camera.panAboutViewCenter(root.lookSpeed * d.dx * dt)
                    root.camera.tiltAboutViewCenter(root.orbitSpeed * d.dy * dt)
                }
            }
        }
    ] // components
}
