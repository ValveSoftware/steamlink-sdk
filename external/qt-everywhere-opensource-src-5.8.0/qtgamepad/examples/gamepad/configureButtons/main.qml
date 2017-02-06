/****************************************************************************
**
** Copyright (C) 2015 BogDan Vatra <bogdan@kde.org>
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

import QtQuick 2.5
import QtQuick.Controls 1.4

import QtQuick 2.5
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.2
import QtQuick.Window 2.0
import QtGamepad 1.0


ApplicationWindow {
    visible: true
    title: qsTr("Configure gamepad")

    Component.onCompleted: {
        if (Qt.platform.os === "android")
            visibility = Window.Maximized
    }

    property Button checkedButton: null
    function pressButton(button)
    {
        if (checkedButton !== null && button !== checkedButton)
            checkedButton.checked = false;
        checkedButton = button
    }

    Gamepad {
        id: gamepad
        deviceId: GamepadManager.connectedGamepads.length > 0 ? GamepadManager.connectedGamepads[0] : -1
        onDeviceIdChanged: GamepadManager.setCancelConfigureButton(deviceId, GamepadManager.ButtonStart);
    }

    Connections {
        target: GamepadManager
        onButtonConfigured: pressButton(null)
        onAxisConfigured: pressButton(null)
        onConfigurationCanceled: pressButton(null)
    }

    ColumnLayout {
        anchors.fill: parent

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: qsTr("Connected gamepads")
            }
            ComboBox {
                model: GamepadManager.connectedGamepads
                onCurrentIndexChanged: gamepad.deviceId[GamepadManager.connectedGamepads[currentIndex]]
            }
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Start button cancel's current configuration")
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            GroupBox {
                title: qsTr("Configure Gamepad Buttons")
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("ButtonA")
                            horizontalAlignment: Text.AlignRight
                        }

                        Text {
                            text: gamepad.buttonA ? qsTr("DOWN") : qsTr("UP")
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Button {
                            text: qsTr("Configure")
                            checkable: true
                            enabled: !checked
                            onCheckedChanged: {
                                pressButton(this);
                                if (checked)
                                    GamepadManager.configureButton(gamepad.deviceId, GamepadManager.ButtonA);
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("ButtonB")
                            horizontalAlignment: Text.AlignRight
                        }
                        Text {
                            text: gamepad.buttonB ? qsTr("DOWN") : qsTr("UP")
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                        }
                        Button {
                            text: qsTr("Configure")
                            checkable: true
                            enabled: !checked
                            onCheckedChanged: {
                                pressButton(this);
                                if (checked)
                                    GamepadManager.configureButton(gamepad.deviceId, GamepadManager.ButtonB);
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("ButtonX")
                            horizontalAlignment: Text.AlignRight
                        }
                        Text {
                            text: gamepad.buttonX ? qsTr("DOWN") : qsTr("UP")
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                        }
                        Button {
                            text: qsTr("Configure")
                            checkable: true
                            enabled: !checked
                            onCheckedChanged: {
                                pressButton(this);
                                if (checked)
                                    GamepadManager.configureButton(gamepad.deviceId, GamepadManager.ButtonX);
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("ButtonY")
                            horizontalAlignment: Text.AlignRight
                        }
                        Text {
                            text: gamepad.buttonY ? qsTr("DOWN") : qsTr("UP")
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                        }
                        Button {
                            text: qsTr("Configure")
                            checkable: true
                            enabled: !checked
                            onCheckedChanged: {
                                pressButton(this);
                                if (checked)
                                    GamepadManager.configureButton(gamepad.deviceId, GamepadManager.ButtonY);
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("ButtonStart")
                            horizontalAlignment: Text.AlignRight
                        }
                        Text {
                            text: gamepad.buttonStart ? qsTr("DOWN") : qsTr("UP")
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                        }
                        Button {
                            text: qsTr("Configure")
                            checkable: true
                            enabled: !checked
                            onCheckedChanged: {
                                pressButton(this);
                                if (checked)
                                    GamepadManager.configureButton(gamepad.deviceId, GamepadManager.ButtonStart);
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("ButtonSelect")
                            horizontalAlignment: Text.AlignRight
                        }
                        Text {
                            text: gamepad.buttonSelect ? qsTr("DOWN") : qsTr("UP")
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                        }
                        Button {
                            text: qsTr("Configure")
                            checkable: true
                            enabled: !checked
                            onCheckedChanged: {
                                pressButton(this);
                                if (checked)
                                    GamepadManager.configureButton(gamepad.deviceId, GamepadManager.ButtonSelect);
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("Button L1")
                            horizontalAlignment: Text.AlignRight
                        }
                        Text {
                            text: gamepad.buttonL1 ? qsTr("DOWN") : qsTr("UP")
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                        }
                        Button {
                            text: qsTr("Configure")
                            checkable: true
                            enabled: !checked
                            onCheckedChanged: {
                                pressButton(this);
                                if (checked)
                                    GamepadManager.configureButton(gamepad.deviceId, GamepadManager.ButtonL1);
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("Button R1")
                            horizontalAlignment: Text.AlignRight
                        }
                        Text {
                            text: gamepad.buttonR1 ? qsTr("DOWN") : qsTr("UP")
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                        }
                        Button {
                            text: qsTr("Configure")
                            checkable: true
                            enabled: !checked
                            onCheckedChanged: {
                                pressButton(this);
                                if (checked)
                                    GamepadManager.configureButton(gamepad.deviceId, GamepadManager.ButtonR1);
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("Button L2")
                            horizontalAlignment: Text.AlignRight
                        }
                        Text {
                            text: gamepad.buttonL2 ? qsTr("DOWN") : qsTr("UP")
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                        }
                        Button {
                            text: qsTr("Configure")
                            checkable: true
                            enabled: !checked
                            onCheckedChanged: {
                                pressButton(this);
                                if (checked)
                                    GamepadManager.configureButton(gamepad.deviceId, GamepadManager.ButtonL2);
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("Button R2")
                            horizontalAlignment: Text.AlignRight
                        }
                        Text {
                            text: gamepad.buttonR2 ? qsTr("DOWN") : qsTr("UP")
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                        }
                        Button {
                            text: qsTr("Configure")
                            checkable: true
                            enabled: !checked
                            onCheckedChanged: {
                                pressButton(this);
                                if (checked)
                                    GamepadManager.configureButton(gamepad.deviceId, GamepadManager.ButtonR2);
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("Button L3")
                            horizontalAlignment: Text.AlignRight
                        }
                        Text {
                            text: gamepad.buttonL3 ? qsTr("DOWN") : qsTr("UP")
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                        }
                        Button {
                            text: qsTr("Configure")
                            checkable: true
                            enabled: !checked
                            onCheckedChanged: {
                                pressButton(this);
                                if (checked)
                                    GamepadManager.configureButton(gamepad.deviceId, GamepadManager.ButtonL3);
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("Button R3")
                            horizontalAlignment: Text.AlignRight
                        }
                        Text {
                            text: gamepad.buttonR3 ? qsTr("DOWN") : qsTr("UP")
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                        }
                        Button {
                            text: qsTr("Configure")
                            checkable: true
                            enabled: !checked
                            onCheckedChanged: {
                                pressButton(this);
                                if (checked)
                                    GamepadManager.configureButton(gamepad.deviceId, GamepadManager.ButtonR3);
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("Button Up")
                            horizontalAlignment: Text.AlignRight
                        }
                        Text {
                            text: gamepad.buttonUp ? qsTr("DOWN") : qsTr("UP")
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                        }
                        Button {
                            text: qsTr("Configure")
                            checkable: true
                            enabled: !checked
                            onCheckedChanged: {
                                pressButton(this);
                                if (checked)
                                    GamepadManager.configureButton(gamepad.deviceId, GamepadManager.ButtonUp);
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("Button Down")
                            horizontalAlignment: Text.AlignRight
                        }
                        Text {
                            text: gamepad.buttonDown ? qsTr("DOWN") : qsTr("UP")
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                        }
                        Button {
                            text: qsTr("Configure")
                            checkable: true
                            enabled: !checked
                            onCheckedChanged: {
                                pressButton(this);
                                if (checked)
                                    GamepadManager.configureButton(gamepad.deviceId, GamepadManager.ButtonDown  );
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("Button Left")
                            horizontalAlignment: Text.AlignRight
                        }
                        Text {
                            Layout.fillWidth: true
                            text: gamepad.buttonLeft ? qsTr("DOWN") : qsTr("UP")
                            horizontalAlignment: Text.AlignHCenter
                        }
                        Button {
                            text: qsTr("Configure")
                            checkable: true
                            enabled: !checked
                            onCheckedChanged: {
                                pressButton(this);
                                if (checked)
                                    GamepadManager.configureButton(gamepad.deviceId, GamepadManager.ButtonLeft);
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("Button Right")
                            horizontalAlignment: Text.AlignRight
                        }
                        Text {
                            Layout.fillWidth: true
                            text: gamepad.buttonRight ? qsTr("DOWN") : qsTr("UP")
                            horizontalAlignment: Text.AlignHCenter
                        }
                        Button {
                            text: qsTr("Configure")
                            checkable: true
                            enabled: !checked
                            onCheckedChanged: {
                                pressButton(this);
                                if (checked)
                                    GamepadManager.configureButton(gamepad.deviceId, GamepadManager.ButtonRight);
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("Button Center")
                            horizontalAlignment: Text.AlignRight
                        }
                        Text {
                            Layout.fillWidth: true
                            text: gamepad.buttonCenter ? qsTr("DOWN") : qsTr("UP")
                            horizontalAlignment: Text.AlignHCenter
                        }
                        Button {
                            text: qsTr("Configure")
                            checkable: true
                            enabled: !checked
                            onCheckedChanged: {
                                pressButton(this);
                                if (checked)
                                    GamepadManager.configureButton(gamepad.deviceId, GamepadManager.ButtonCenter);
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("Button Guide")
                            horizontalAlignment: Text.AlignRight
                        }
                        Text {
                            Layout.fillWidth: true
                            text: gamepad.buttonGuide ? qsTr("DOWN") : qsTr("UP")
                            horizontalAlignment: Text.AlignHCenter
                        }
                        Button {
                            text: qsTr("Configure")
                            checkable: true
                            enabled: !checked
                            onCheckedChanged: {
                                pressButton(this);
                                if (checked)
                                    GamepadManager.configureButton(gamepad.deviceId, GamepadManager.ButtonGuide);
                            }
                        }
                    }
                }
            }
            GroupBox {
                title: qsTr("Gamepad Axies")
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("AxisLeftX")
                            horizontalAlignment: Text.AlignRight
                        }

                        Text {
                            text: gamepad.axisLeftX
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Button {
                            text: qsTr("Configure")
                            checkable: true
                            enabled: !checked
                            onCheckedChanged: {
                                pressButton(this);
                                if (checked)
                                    GamepadManager.configureAxis(gamepad.deviceId, GamepadManager.AxisLeftX);
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("AxisLeftY")
                            horizontalAlignment: Text.AlignRight
                        }

                        Text {
                            text: gamepad.axisLeftY
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Button {
                            text: qsTr("Configure")
                            checkable: true
                            enabled: !checked
                            onCheckedChanged: {
                                pressButton(this);
                                if (checked)
                                    GamepadManager.configureAxis(gamepad.deviceId, GamepadManager.AxisLeftY);
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("AxisRightX")
                            horizontalAlignment: Text.AlignRight
                        }

                        Text {
                            text: gamepad.axisRightX
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Button {
                            text: qsTr("Configure")
                            checkable: true
                            enabled: !checked
                            onCheckedChanged: {
                                pressButton(this);
                                if (checked)
                                    GamepadManager.configureAxis(gamepad.deviceId, GamepadManager.AxisRightX);
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("AxisRightY")
                            horizontalAlignment: Text.AlignRight
                        }

                        Text {
                            text: gamepad.axisRightY
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Button {
                            text: qsTr("Configure")
                            checkable: true
                            enabled: !checked
                            onCheckedChanged: {
                                pressButton(this);
                                if (checked)
                                    GamepadManager.configureAxis(gamepad.deviceId, GamepadManager.AxisRightY);
                            }
                        }
                    }

                    Item {
                        Layout.fillHeight: true
                    }
                }
            }
        }
    }
}
