/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Extras module of the Qt Toolkit.
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

import QtQuick 2.2
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4 as Base
import QtQuick.Controls.Styles.Flat 1.0
import QtQuick.Controls.Private 1.0
import QtQuick.Extras.Private 1.0
import QtQuick.Extras.Private.CppUtils 1.1

Base.DelayButtonStyle {
    id: delayButtonStyle

    StateGroup {
        id: privateScope

        property color buttonColor
        property color borderColor
        property color textColor
        property color progressBarColor
        readonly property real progressBarWidth: Math.round(4 * FlatStyle.scaleFactor)
        readonly property real progressBarSpacing: Math.round(16 * FlatStyle.scaleFactor)
        readonly property bool hovered: control.hovered && (!Settings.hasTouchScreen && !Settings.isMobile)

        states: [
            State {
                name: "normal"
                when: control.enabled && !control.activeFocus && !control.pressed && !privateScope.hovered && !control.checked

                PropertyChanges {
                    target: privateScope
                    buttonColor: "white"
                    borderColor: FlatStyle.styleColor
                    textColor: FlatStyle.styleColor
                    progressBarColor: borderColor
                }
            },
            State {
                name: "disabled"
                when: !control.enabled

                PropertyChanges {
                    target: privateScope
                    buttonColor: FlatStyle.disabledFillColor
                    borderColor: "transparent"
                    textColor: "white"
                    progressBarColor: buttonColor
                }
            },
            State {
                name: "hovered"
                when: !control.activeFocus && !control.pressed && privateScope.hovered

                PropertyChanges {
                    target: privateScope
                    buttonColor: FlatStyle.hoveredColor
                    borderColor: "transparent"
                    textColor: "white"
                    progressBarColor: buttonColor
                }
            },
            State {
                name: "focused"
                when: control.activeFocus && !control.pressed && !control.checked

                PropertyChanges {
                    target: privateScope
                    buttonColor: "white"
                    borderColor: FlatStyle.highlightColor
                    textColor: borderColor
                    progressBarColor: borderColor
                }
            },
            State {
                name: "pressed"
                when: !control.activeFocus && control.pressed && !control.checked

                PropertyChanges {
                    target: privateScope
                    buttonColor: FlatStyle.pressedColorAlt
                    borderColor: "transparent"
                    textColor: "white"
                    progressBarColor: buttonColor
                }
            },
            State {
                name: "focusedAndPressed"
                when: control.activeFocus && control.pressed && !control.checked

                PropertyChanges {
                    target: privateScope
                    buttonColor: FlatStyle.focusedAndPressedColor
                    borderColor: "transparent"
                    textColor: "white"
                    progressBarColor: buttonColor
                }
            },
            State {
                name: "checked"
                when: !control.activeFocus && !control.pressed && !privateScope.hovered && control.checked

                PropertyChanges {
                    target: privateScope
                    buttonColor: FlatStyle.pressedColorAlt
                    borderColor: "transparent"
                    textColor: "white"
                    progressBarColor: buttonColor
                }
            },
            State {
                name: "checkedAndPressed"
                when: !control.activeFocus && control.pressed && control.checked

                PropertyChanges {
                    target: privateScope
                    buttonColor: FlatStyle.checkedAndPressedColorAlt
                    borderColor: "transparent"
                    textColor: "white"
                    progressBarColor: buttonColor
                }
            },
            State {
                name: "checkedAndFocused"
                when: control.activeFocus && !control.pressed && control.checked

                PropertyChanges {
                    target: privateScope
                    buttonColor: FlatStyle.focusedAndPressedColor
                    borderColor: "transparent"
                    textColor: "white"
                    progressBarColor: buttonColor
                }
            },
            State {
                name: "checkedPressedAndFocused"
                when: control.activeFocus && control.pressed && control.checked

                PropertyChanges {
                    target: privateScope
                    buttonColor: FlatStyle.checkedFocusedAndPressedColor
                    borderColor: "transparent"
                    textColor: "white"
                    progressBarColor: buttonColor
                }
            }
        ]
    }

    label: Text {
        text: control.text
        anchors.fill: parent
        anchors.margins: privateScope.progressBarWidth + privateScope.progressBarSpacing
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.Wrap
        fontSizeMode: Text.Fit
        font.family: FlatStyle.fontFamily
        font.weight: Font.Light
        renderType: FlatStyle.__renderType
        color: privateScope.textColor
        font.pixelSize: TextSingleton.font.pixelSize * 1.5
    }

    background: Item {
        implicitWidth: Math.round(160 * FlatStyle.scaleFactor)
        implicitHeight: Math.round(160 * FlatStyle.scaleFactor)

        Item {
            id: container
            width: Math.min(parent.width, parent.height)
            height: width
            anchors.centerIn: parent

            Rectangle {
                id: body
                anchors.fill: parent
                anchors.margins: privateScope.progressBarWidth + privateScope.progressBarSpacing
                radius: width / 2
                color: privateScope.buttonColor
                border.color: privateScope.borderColor
                border.width: FlatStyle.onePixel
            }

            CircularProgressBar {
                id: progressBar
                anchors.fill: parent
                antialiasing: true
                barWidth: privateScope.progressBarWidth
                progress: control.progress
                minimumValueAngle: 0
                maximumValueAngle: 360

                // TODO: Add gradient property if/when we drop support for building with 5.1.
                Component.onCompleted: {
                    clearStops()
                    addStop(0, privateScope.progressBarColor)
                    redraw()
                }

                Connections {
                    target: privateScope
                    onProgressBarColorChanged: {
                        progressBar.clearStops()
                        progressBar.addStop(0, privateScope.progressBarColor)
                        progressBar.redraw()
                    }
                }

                states: [
                    State {
                        name: "normal"
                        when: control.progress < 1

                        PropertyChanges {
                            target: progressBar
                            opacity: 1
                        }
                    },
                    State {
                        name: "flashing"
                        when: control.progress === 1
                    }
                ]

                transitions: [
                    Transition {
                        from: "normal"
                        to: "flashing"

                        SequentialAnimation {
                            loops: Animation.Infinite

                            NumberAnimation {
                                target: progressBar
                                property: "opacity"
                                from: 1
                                to: 0
                                duration: 500
                                easing.type: Easing.InOutSine
                            }
                            NumberAnimation {
                                target: progressBar
                                property: "opacity"
                                from: 0
                                to: 1
                                duration: 500
                                easing.type: Easing.InOutSine
                            }
                        }
                    }
                ]
            }
        }
    }

    foreground: null
}
