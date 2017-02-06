/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
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

import QtQuick 2.1

FocusScope {
    onActiveFocusChanged: {
        if (activeFocus)
            mainView.state = "showTabViews"
    }

    Rectangle {
        anchors.fill: parent
        clip: true
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#193441" }
            GradientStop { position: 1.0; color: Qt.darker("#193441") }
        }

        Row {
            id: tabView
            anchors.fill: parent; anchors.leftMargin: 20; anchors.rightMargin: 20
            Repeater {
                activeFocusOnTab: false
                model: 5
                Item {
                    id: container
                    width: 152; height: 152
                    activeFocusOnTab: true
                    focus: true

                    KeyNavigation.up: listMenu
                    KeyNavigation.down: gridMenu

                    Rectangle {
                        id: content
                        color: "transparent"
                        antialiasing: true
                        anchors.fill: parent; anchors.margins: 20; radius: 10

                        Rectangle { color: "#91AA9D"; anchors.fill: parent; anchors.margins: 3; radius: 8; antialiasing: true }
                        Image { source: "images/qt-logo.png"; anchors.centerIn: parent }
                    }

                    MouseArea {
                        id: mouseArea
                        anchors.fill: parent
                        hoverEnabled: true

                        onClicked: {
                            container.forceActiveFocus()
                        }
                    }

                    states: State {
                        name: "active"; when: container.activeFocus
                        PropertyChanges { target: content; color: "#FCFFF5"; scale: 1.1 }
                    }

                    transitions: Transition {
                        NumberAnimation { properties: "scale"; duration: 100 }
                    }
                }
            }
        }
    }
}
