/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

import QtQuick 2.2
import QtQuick.Window 2.1

Rectangle {
    id: root
    width: 500
    height: 600
    property int animationDuration: 10
    property int itemHeight: 40

    Rectangle {
        id: sightingsListPanel
        border.width: 2
        border.color: "lightgray"
        y: 200
        anchors.fill: parent
        anchors.topMargin: 200
        anchors.leftMargin: 200
        ListView {
            id: list
            objectName: "list"
            orientation: topToBottom ? ListView.Vertical : ListView.Horizontal
            property bool transitionFinished: false
            property bool scriptActionExecuted : false
            anchors { fill: parent; margins: parent.border.width; }
            model: testModel
            delegate: listDelegate
            // clip when we have no animation running
            clip: false
            add: Transition {
                id: trans
                onRunningChanged: {
                    if (!running)
                        list.transitionFinished = true;
                }
                SequentialAnimation {
                    ParallelAnimation {
                        NumberAnimation { properties: "x"; from: -100; duration: root.animationDuration }
                        NumberAnimation { properties: "y"; from: -100; duration: root.animationDuration }
                        NumberAnimation { properties: "width"; from: 1; to: list.width; duration: root.animationDuration;}
                        // Commenting out the height animation and it works
                        NumberAnimation { properties: "height"; from: 1; to: root.itemHeight; duration: root.animationDuration }
                    }
                    ScriptAction { script: list.scriptActionExecuted = true;}
                }

            }
        }
        // Delegate for defining a template for an item in the list
        Component {
            id: listDelegate
            Rectangle {
                id: background
                width: list.width
                height: root.itemHeight
                border.width: 2
                radius: 3
            }
        }
    }
}
