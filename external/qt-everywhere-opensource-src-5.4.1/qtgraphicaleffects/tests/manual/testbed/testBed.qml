/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Graphical Effects module.
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

import QtQuick 2.0

Rectangle {
    id: main

    width: 950
    height: 620
    color: '#171717'

    ListView {
        id: testCaseList
        width: 160
        anchors.top: titlebar.bottom
        anchors.left: parent.left
        anchors.bottom: parent.bottom

        model: TestBedModel {}
        delegate: delegateItem

        section.property: "group"
        section.criteria: ViewSection.FullString
        section.delegate: sectionHeading

    }

    Image {
        id: titlebar
        source: "images/title.png"
        anchors.top: parent.top
        anchors.left: parent.left
        width: 160

        Text {
            id: effectsListTitle
            anchors.fill: parent
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: "Effects"
            color: "white"
            font.family: "Arial"
            font.bold: true
            font.pixelSize: 12
        }
    }

    Loader {
        id: testLoader
        anchors {left: testCaseList.right; right: parent.right; top: parent.top; bottom: parent.bottom}
    }

    Image {
        anchors {right: testLoader.right; rightMargin: 300; top: parent.top; bottom: parent.bottom}
        source: "images/workarea_right.png"
    }

    Image {
        anchors {left: testLoader.left; top: parent.top; bottom: parent.bottom}
        source: "images/workarea_left.png"
    }


    Component {
        id: sectionHeading
        Item {
            width: parent.width
            height: 23
            Image {
                source: "images/group_top.png"
                width: parent.width
            }
            Image {
                id: icon
                source: "images/icon_" + section.replace(/ /g, "_").toLowerCase() + ".png"
                anchors {top: parent.top; topMargin: 6; left: parent.left; leftMargin: 6}
            }
            Text {
                id: sectionText
                text: section
                anchors {fill: parent; leftMargin: 25; topMargin: 3}
                color: "white"
                verticalAlignment: Text.AlignVCenter
                font.family: "Arial"
                font.bold: true
                font.pixelSize: 12
            }
        }
    }

    Component {
        id: delegateItem
        Item {
            width: ListView.view.width
            height: last ? 27 : 20

            Image {
                source: "images/group_bottom.png"
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                smooth: true
                visible: last && index != 20 ? 1 : 0
            }

            Rectangle {
                width: parent.width
                color: "#323232"
                height: 20
                visible: (testLoader.source.toString().search(name) != -1)
            }
            Text {
                id: delegateText;
                anchors {fill: parent; leftMargin: 25; topMargin: 3}
                text: name.slice(4, name.indexOf("."))
                font.family: "Arial"
                font.pixelSize: 12
                color: delegateMouseArea.pressed  ? "white" : "#CCCCCC"
            }
            MouseArea {
                id: delegateMouseArea
                anchors.fill: parent;
                onClicked: {
                    testLoader.source = name;
                    testLoader.item.currentTest = delegateText.text;
                }
            }
        }
    }
}
