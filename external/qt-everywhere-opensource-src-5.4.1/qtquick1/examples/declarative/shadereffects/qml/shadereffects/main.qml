/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
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
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
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

import QtQuick 1.0
import Qt.labs.shaders 1.0

Item {
    id: main
    width: 360
    height: 640

    Rectangle {
        anchors.fill: parent
        color: "black"
    }

    Image {
        id: header
        source: "images/shader_effects.jpg"
    }

    ListModel {
        id: demoModel
        ListElement { name: "ImageMask" }
        ListElement { name: "RadialWave" }
        ListElement { name: "Water" }
        ListElement { name: "Grayscale" }
        ListElement { name: "DropShadow" }
        ListElement { name: "Curtain" }
    }

    Grid {
        id: menuGrid
        anchors.top: header.bottom
        anchors.bottom: toolbar.top
        width: parent.width
        columns: 2
        Repeater {
            model: demoModel
            Item {
                width: main.width / 2
                height: menuGrid.height / 3
                clip: true
                Image {
                    width: parent.width
                    height: width
                    source: "images/" + name + ".jpg"
                    opacity: mouseArea.pressed ? 0.6 : 1.0
                }
                MouseArea {
                    id: mouseArea
                    anchors.fill: parent
                    onClicked: {
                        demoLoader.source = name + ".qml"
                        main.state = "showDemo"
                    }
                }
            }
        }
    }

    Loader {
        anchors.fill: parent
        id: demoLoader
        visible: false
        Behavior on opacity {
            NumberAnimation { duration: 300 }
        }
    }

    Image {
        id: toolbar
        source: "images/toolbar.png"
        width: parent.width
        anchors.bottom: parent.bottom
    }

    Rectangle {
        id: translucentToolbar
        color: "black"
        opacity: 0.3
        anchors.fill: toolbar
        visible: !toolbar.visible
    }

    Item {
        height: toolbar.height
        width: height
        anchors.bottom: parent.bottom

        Image {
            source: "images/back.png"
            anchors.centerIn: parent
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                if (main.state == "") Qt.quit(); else {
                    main.state = ""
                    demoLoader.source = ""
                }
            }
        }
    }

    states: State {
        name: "showDemo"
        PropertyChanges {
            target: menuGrid
            visible: false
        }
        PropertyChanges {
            target: demoLoader
            visible: true
        }
        PropertyChanges {
            target: toolbar
            visible: false
        }
    }
}
