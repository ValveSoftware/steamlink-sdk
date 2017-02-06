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

import QtQuick 2.0

FlipBar {
    id: flipBar
    animDuration: 250
    property string label: ""
    property string placeHolder: ""
    property alias searchText: lineInput.text
    property alias prefix: lineInput.prefix
    property bool opened: false
    signal ok
    signal hasOpened

    height: 60
    width: parent.width

    function open() {
        flipBar.flipUp()
        flipBar.opened = true
        lineInput.forceActiveFocus()
        flipBar.hasOpened()
    }

    function close() {
        if (opened) {
            flipBar.flipDown()
            flipBar.opened = false
        }
    }

    front: Rectangle {
        height: 60
        width: parent.width
        color: "#999999"

        Rectangle { color: "#c1c1c1"; width: parent.width; height: 1 }
        Rectangle { color: "#707070"; width: parent.width; height: 1; anchors.bottom: parent.bottom }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            onClicked: {
                if (!flipBar.opened)
                    open()
                else if (!lineInput.activeFocus)
                    lineInput.forceActiveFocus()
                else
                    close()
            }
        }

        Text {
            text: flipBar.label
            anchors { left: parent.left; leftMargin: 20 }
            anchors.verticalCenter: parent.verticalCenter
            font.pixelSize: 18
            color: "#ffffff"
        }
    }

    back: FocusScope {
        height: 60
        width: parent.width
        Rectangle {
            anchors.fill: parent
            color: "#999999"

            Rectangle { color: "#c1c1c1"; width: parent.width; height: 1 }
            Rectangle { color: "#707070"; width: parent.width; height: 1; anchors.bottom: parent.bottom }

            LineInput {
                id: lineInput
                hint: flipBar.placeHolder
                focus: flipBar.opened
                anchors { fill: parent; margins: 6 }
                onAccepted: {
                    if (Qt.inputMethod.visible)
                        Qt.inputMethod.hide()
                    flipBar.ok()
                }
            }
        }
    }

}
