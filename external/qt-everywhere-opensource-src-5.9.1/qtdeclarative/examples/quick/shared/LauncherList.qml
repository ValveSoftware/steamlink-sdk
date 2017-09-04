/****************************************************************************
**
** Copyright (C) 2017 Crimson AS <info@crimson.no>
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
import QtQuick 2.0

Rectangle {
    property int activePageCount: 0

    //model is a list of {"name":"somename", "url":"file:///some/url/mainfile.qml"}
    //function used to add to model A) to enforce scheme B) to allow Qt.resolveUrl in url assignments

    color: "#eee"
    function addExample(name, desc, url)
    {
        myModel.append({"name":name, "description":desc, "url":url})
    }

    // The container rectangle here is used to give a nice "feel" when
    // transitioning into an example.
    Rectangle {
        anchors.fill: parent
        color: "black"

        ListView {
            id: launcherList
            clip: true
            delegate: SimpleLauncherDelegate{
                onClicked: {
                    var page = pageComponent.createObject(pageContainer, { exampleUrl: url })
                    page.show()
                }
            }
            model: ListModel {id:myModel}
            anchors.fill: parent
            enabled: opacity == 1.0
        }
    }

    Item {
        id: pageContainer
        anchors.fill: parent
    }

    Component {
        id: pageComponent
        Rectangle {
            id: page
            clip: true
            property url exampleUrl
            width: parent.width
            height: parent.height - bar.height
            color: "white"
            MouseArea{
                //Eats mouse events
                anchors.fill: parent
            }
            Loader{
                focus: true
                source: parent.exampleUrl
                anchors.fill: parent
            }

            x: -width

            function show() {
                showAnim.start()
            }

            function exit() {
                exitAnim.start()
            }

            ParallelAnimation {
                id: showAnim
                ScriptAction {
                    script: activePageCount++
                }
                NumberAnimation {
                    target: launcherList
                    property: "opacity"
                    from: 1.0
                    to: 0.0
                    duration: 500
                }
                NumberAnimation {
                    target: launcherList
                    property: "scale"
                    from: 1.0
                    to: 0.0
                    duration: 500
                }
                NumberAnimation {
                    target: page
                    property: "x"
                    from: -page.width
                    to: 0
                    duration: 300
                }
            }
            SequentialAnimation {
                id: exitAnim

                ScriptAction {
                    script: activePageCount--
                }

                ParallelAnimation {
                    NumberAnimation {
                        target: launcherList
                        property: "opacity"
                        from: 0.0
                        to: 1.0
                        duration: 300
                    }
                    NumberAnimation {
                        target: launcherList
                        property: "scale"
                        from: 0.0
                        to: 1.0
                        duration: 300
                    }
                    NumberAnimation {
                        target: page
                        property: "x"
                        from: 0
                        to: -page.width
                        duration: 300
                    }
                }

                ScriptAction {
                    script: page.destroy()
                }
            }
        }
    }
    Rectangle {
        id: bar
        visible: height > 0
        anchors.bottom: parent.bottom
        width: parent.width
        height: activePageCount > 0 ? 40 : 0

        Behavior on height {
            NumberAnimation {
                duration: 300
            }
        }

        Rectangle {
            height: 1
            color: "#ccc"
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
        }

        Rectangle {
            height: 1
            color: "#fff"
            anchors.top: parent.top
            anchors.topMargin: 1
            anchors.left: parent.left
            anchors.right: parent.right
        }

        gradient: Gradient {
            GradientStop { position: 0 ; color: "#eee" }
            GradientStop { position: 1 ; color: "#ccc" }
        }

        Image {
            id: back
            source: "images/back.png"
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 2
            anchors.left: parent.left
            anchors.leftMargin: 16

            MouseArea {
                id: mouse
                hoverEnabled: true
                anchors.centerIn: parent
                width: 38
                height: 31
                anchors.verticalCenterOffset: -1
                enabled: activePageCount > 0
                onClicked: {
                    pageContainer.children[pageContainer.children.length - 1].exit()
                }
                Rectangle {
                    anchors.fill: parent
                    opacity: mouse.pressed ? 1 : 0
                    Behavior on opacity { NumberAnimation{ duration: 100 }}
                    gradient: Gradient {
                        GradientStop { position: 0 ; color: "#22000000" }
                        GradientStop { position: 0.2 ; color: "#11000000" }
                    }
                    border.color: "darkgray"
                    antialiasing: true
                    radius: 4
                }
            }
        }
    }
}
