/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
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

import QtQuick 2.0
import QtQuick.Scene3D 2.0

Item {
    id: root
    property string currentFont
    property bool bold : false
    property bool italic : false

    Scene3D {
        id: scene3d
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: settingsPane.left
        focus: true
        aspects: ["input", "logic", "render"]
        cameraAspectRatioMode: Scene3D.AutomaticAspectRatio

        TextScene {
            id: textScene
            fontFamily: root.currentFont
            fontBold: bold
            fontItalic: italic
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            textScene.clicked()
        }
    }

    Item {
        id: settingsPane

        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 400

        Rectangle {
            width: parent.width/2
            height:50
            color: root.bold ? "#000066" : "#222222"
            Text {
                anchors.centerIn: parent
                text: "Bold"
                font.pointSize: 20
                color: "#ffffff"
            }
            MouseArea {
                anchors.fill: parent
                onClicked: root.bold = !root.bold
            }
        }

        Rectangle {
            x: parent.width/2
            width: parent.width/2
            height:50
            color: root.italic ? "#000066" : "#222222"
            Text {
                anchors.centerIn: parent
                text: "Italic"
                font.pointSize: 20
                color: "#ffffff"
            }
            MouseArea {
                anchors.fill: parent
                onClicked: root.italic = !root.italic
            }
        }

        ListView {
            anchors.fill: parent
            anchors.topMargin: 50
            model: Qt.fontFamilies()

            delegate: Rectangle {
                height: fontFamilyText.height + 10
                width: parent.width
                color: (modelData == root.currentFont) ? "#000066" : "#222222"

                Text {
                    id: fontFamilyText
                    anchors.centerIn: parent
                    font.pointSize: 14
                    font.family: modelData
                    text: modelData
                    color: "#ffffff"
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        root.currentFont = modelData;

                    }
                }
            }
        }
    }
}
