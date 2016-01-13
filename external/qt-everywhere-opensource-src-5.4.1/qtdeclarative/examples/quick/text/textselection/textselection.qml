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
import QtQuick 2.0

Rectangle {
    id: editor
    color: "lightGrey"
    width: 640; height: 480

    Rectangle {
        color: "white"
        anchors.fill: parent
        anchors.margins: 20

        BorderImage {
            id: startHandle
            source: "pics/startHandle.sci"
            opacity: 0.0
            width: 10
            x: edit.positionToRectangle(edit.selectionStart).x - flick.contentX-width
            y: edit.positionToRectangle(edit.selectionStart).y - flick.contentY
            height: edit.positionToRectangle(edit.selectionStart).height
        }

        BorderImage {
            id: endHandle
            source: "pics/endHandle.sci"
            opacity: 0.0
            width: 10
            x: edit.positionToRectangle(edit.selectionEnd).x - flick.contentX
            y: edit.positionToRectangle(edit.selectionEnd).y - flick.contentY
            height: edit.positionToRectangle(edit.selectionEnd).height
        }

        Flickable {
            id: flick

            anchors.fill: parent
            contentWidth: edit.paintedWidth
            contentHeight: edit.paintedHeight
            interactive: true
            clip: true

            function ensureVisible(r) {
                if (contentX >= r.x)
                    contentX = r.x;
                else if (contentX+width <= r.x+r.width)
                    contentX = r.x+r.width-width;
                if (contentY >= r.y)
                    contentY = r.y;
                else if (contentY+height <= r.y+r.height)
                    contentY = r.y+r.height-height;
            }

            TextEdit {
                id: edit
                width: flick.width
                height: flick.height
                focus: true
                wrapMode: TextEdit.Wrap
                textFormat: TextEdit.RichText

                onCursorRectangleChanged: flick.ensureVisible(cursorRectangle)

                text: "<h1>Text Selection</h1>"
                    +"<p>This example is a whacky text selection mechanisms, showing how these can be implemented in the TextEdit element, to cater for whatever style is appropriate for the target platform."
                    +"<p><b>Press-and-hold</b> to select a word, then drag the selection handles."
                    +"<p><b>Drag outside the selection</b> to scroll the text."
                    +"<p><b>Click inside the selection</b> to cut/copy/paste/cancel selection."
                    +"<p>It's too whacky to let you paste if there is no current selection."

            }
        }

        Item {
            id: menu
            opacity: 0.0
            width: 100
            height: 120
            anchors.centerIn: parent

            Rectangle {
                border.width: 1
                border.color: "darkBlue"
                radius: 15
                color: "#806080FF"
                anchors.fill: parent
            }

            Column {
                anchors.centerIn: parent
                spacing: 8

                Rectangle {
                    border.width: 1
                    border.color: "darkBlue"
                    color: "#ff7090FF"
                    width: 60
                    height: 16

                    Text { anchors.centerIn: parent; text: "Cut" }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: { edit.cut(); editor.state = "" }
                    }
                }

                Rectangle {
                    border.width: 1
                    border.color: "darkBlue"
                    color: "#ff7090FF"
                    width: 60
                    height: 16

                    Text { anchors.centerIn: parent; text: "Copy" }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: { edit.copy(); editor.state = "selection" }
                    }
                }

                Rectangle {
                    border.width: 1
                    border.color: "darkBlue"
                    color: "#ff7090FF"
                    width: 60
                    height: 16

                    Text { anchors.centerIn: parent; text: "Paste" }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: { edit.paste(); edit.cursorPosition = edit.selectionEnd; editor.state = "" }
                    }
                }

                Rectangle {
                    border.width: 1
                    border.color: "darkBlue"
                    color: "#ff7090FF"
                    width: 60
                    height: 16

                    Text { anchors.centerIn: parent; text: "Deselect" }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            edit.cursorPosition = edit.selectionEnd;
                            edit.deselect();
                            editor.state = ""
                        }
                    }
                }
            }
        }
    }

    states: [
        State {
            name: "selection"
            PropertyChanges { target: startHandle; opacity: 1.0 }
            PropertyChanges { target: endHandle; opacity: 1.0 }
        },
        State {
            name: "menu"
            PropertyChanges { target: startHandle; opacity: 0.5 }
            PropertyChanges { target: endHandle; opacity: 0.5 }
            PropertyChanges { target: menu; opacity: 1.0 }
        }
    ]
}
