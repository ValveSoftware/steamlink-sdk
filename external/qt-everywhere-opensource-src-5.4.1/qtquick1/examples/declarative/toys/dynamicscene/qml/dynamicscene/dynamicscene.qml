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
import Qt.labs.particles 1.0

Item {
    id: window

    property int activeSuns: 0

    //This is a desktop-sized example
    width: 800; height: 480


    MouseArea {
        anchors.fill: parent
        onClicked: window.focus = false;
    }

    //This is the message box that pops up when there's an error
    Rectangle {
        id: dialog

        opacity: 0
        anchors.centerIn: parent
        width: dialogText.width + 6; height: dialogText.height + 6
        border.color: 'black'
        color: 'lightsteelblue'
        z: 65535 //Arbitrary number chosen to be above all the items, including the scaled perspective ones.

        function show(str){
            dialogText.text = str;
            dialogAnim.start();
        }

        Text {
            id: dialogText
            x: 3; y: 3
            font.pixelSize: 14
        }

        SequentialAnimation {
            id: dialogAnim
            NumberAnimation { target: dialog; property:"opacity"; to: 1; duration: 1000 }
            PauseAnimation { duration: 5000 }
            NumberAnimation { target: dialog; property:"opacity"; to: 0; duration: 1000 }
        }
    }

    // sky
    Rectangle {
        id: sky
        anchors { left: parent.left; top: parent.top; right: toolbox.right; bottom: parent.verticalCenter }
        gradient: Gradient {
            GradientStop { id: gradientStopA; position: 0.0; color: "#0E1533" }
            GradientStop { id: gradientStopB; position: 1.0; color: "#437284" }
        }
    }

    // stars (when there's no sun)
    Particles {
        id: stars
        x: 0; y: 0; width: parent.width; height: parent.height / 2
        source: "images/star.png"
        angleDeviation: 360
        velocity: 0; velocityDeviation: 0
        count: parent.width / 10
        fadeInDuration: 2800
        opacity: 1
    }

    // ground
    Rectangle {
        id: ground
        z: 2    // just above the sun so that the sun can set behind it
        anchors { left: parent.left; top: parent.verticalCenter; right: toolbox.left; bottom: parent.bottom }
        gradient: Gradient {
            GradientStop { position: 0.0; color: "ForestGreen" }
            GradientStop { position: 1.0; color: "DarkGreen" }
        }
    }

    SystemPalette { id: activePalette }

    // right-hand panel
    Rectangle {
        id: toolbox

        width: 380
        color: activePalette.window
        anchors { right: parent.right; top: parent.top; bottom: parent.bottom }

        Column {
            anchors.centerIn: parent
            spacing: 8

            Text { text: "Drag an item into the scene." }

            Rectangle {
                width: palette.width + 10; height: palette.height + 10
                border.color: "black"

                Row {
                    id: palette
                    anchors.centerIn: parent
                    spacing: 8

                    PaletteItem {
                        anchors.verticalCenter: parent.verticalCenter
                        componentFile: "Sun.qml"
                        image: "images/sun.png"
                    }
                    PaletteItem {
                        anchors.verticalCenter: parent.verticalCenter
                        componentFile: "GenericSceneItem.qml"
                        image: "images/moon.png"
                    }
                    PaletteItem {
                        anchors.verticalCenter: parent.verticalCenter
                        componentFile: "PerspectiveItem.qml"
                        image: "images/tree_s.png"
                    }
                    PaletteItem {
                        anchors.verticalCenter: parent.verticalCenter
                        componentFile: "PerspectiveItem.qml"
                        image: "images/rabbit_brown.png"
                    }
                    PaletteItem {
                        anchors.verticalCenter: parent.verticalCenter
                        componentFile: "PerspectiveItem.qml"
                        image: "images/rabbit_bw.png"
                    }
                }
            }

            Text { text: "Active Suns: " + activeSuns }

            Rectangle { width: parent.width; height: 1; color: "black" }

            Text { text: "Arbitrary QML:" }

            Rectangle {
                width: 360; height: 240

                TextEdit {
                    id: qmlText
                    anchors.fill: parent; anchors.margins: 5
                    readOnly: false
                    font.pixelSize: 14
                    wrapMode: TextEdit.WordWrap

                    text: "import QtQuick 1.0\nImage {\n    id: smile\n    x: 360 * Math.random()\n    y: 180 * Math.random() \n    source: 'images/face-smile.png'\n    NumberAnimation on opacity { \n        to: 0; duration: 1500\n    }\n    Component.onCompleted: smile.destroy(1500);\n}"
                }
            }

            Button {
                text: "Create"
                onClicked: {
                    try {
                        Qt.createQmlObject(qmlText.text, window, 'CustomObject');
                    } catch(err) {
                        dialog.show('Error on line ' + err.qmlErrors[0].lineNumber + '\n' + err.qmlErrors[0].message);
                    }
                }
            }
        }
    }

    //Day state, for when a sun is added to the scene
    states: State {
        name: "Day"
        when: window.activeSuns > 0

        PropertyChanges { target: gradientStopA; color: "DeepSkyBlue" }
        PropertyChanges { target: gradientStopB; color: "SkyBlue" }
        PropertyChanges { target: stars; opacity: 0 }
    }

    transitions: Transition {
        PropertyAnimation { duration: 3000 }
        ColorAnimation { duration: 3000 }
    }

}
