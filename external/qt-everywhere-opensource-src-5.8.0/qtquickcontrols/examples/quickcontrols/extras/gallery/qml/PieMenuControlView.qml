/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
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

import QtQuick 2.2
import QtGraphicalEffects 1.0
import QtQuick.Controls 1.1
import QtQuick.Extras 1.4

Rectangle {
    id: view
    color: customizerItem.currentStyleDark ? "#111" : "#555"

    Behavior on color {
        ColorAnimation {}
    }

    Keys.onReleased: {
        if (event.key === Qt.Key_Back) {
            stackView.pop();
            event.accepted = true;
        }
    }

    property bool darkBackground: true

    property Component mouseArea

    property Component customizer: Item {
        property alias currentStylePath: stylePicker.currentStylePath
        property alias currentStyleDark: stylePicker.currentStyleDark

        StylePicker {
            id: stylePicker
            currentIndex: 0
            width: Math.round(Math.max(textSingleton.implicitHeight * 6 * 2, parent.width * 0.5))
            anchors.centerIn: parent

            model: ListModel {
                ListElement {
                    name: "Default"
                    path: "PieMenuDefaultStyle.qml"
                    dark: false
                }
                ListElement {
                    name: "Dark"
                    path: "PieMenuDarkStyle.qml"
                    dark: true
                }
            }
        }
    }

    property alias controlItem: pieMenu
    property alias customizerItem: customizerLoader.item

    Item {
        id: controlBoundsItem
        width: parent.width
        height: parent.height - toolbar.height
        visible: customizerLoader.opacity === 0

        Image {
            id: bgImage
            anchors.centerIn: parent
            height: 48
            Text {
                id: bgLabel
                anchors.top: parent.bottom
                anchors.topMargin: 20
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Tap to open"
                color: "#999"
                font.pointSize: 20
            }
        }

        MouseArea {
            id: touchArea
            anchors.fill: parent
            onClicked: pieMenu.popup(touchArea.mouseX, touchArea.mouseY)
        }

        PieMenu {
            id: pieMenu
            triggerMode: TriggerMode.TriggerOnClick
            width: Math.min(controlBoundsItem.width, controlBoundsItem.height) * 0.5
            height: width

            style: Qt.createComponent(customizerItem.currentStylePath)

            MenuItem {
                text: "Zoom In"
                onTriggered: {
                    bgImage.source = iconSource
                    bgLabel.text = text + " selected"
                }
                iconSource: "qrc:/images/zoom_in.png"
            }
            MenuItem {
                text: "Zoom Out"
                onTriggered: {
                    bgImage.source = iconSource
                    bgLabel.text = text + " selected"
                }
                iconSource: "qrc:/images/zoom_out.png"
            }
            MenuItem {
                text: "Info"
                onTriggered: {
                    bgImage.source = iconSource
                    bgLabel.text = text + " selected"
                }
                iconSource: "qrc:/images/info.png"
            }
        }
    }
    Loader {
        id: customizerLoader
        sourceComponent: customizer
        opacity: 0
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 30
        anchors.rightMargin: 30
        y: parent.height / 2 - height / 2 - toolbar.height
        visible: customizerLoader.opacity > 0

        property alias view: view

        Behavior on y {
            NumberAnimation {
                duration: 300
            }
        }

        Behavior on opacity {
            NumberAnimation {
                duration: 300
            }
        }
    }

    ControlViewToolbar {
        id: toolbar

        onCustomizeClicked: {
            customizerLoader.opacity = customizerLoader.opacity == 0 ? 1 : 0;
        }
    }
}
