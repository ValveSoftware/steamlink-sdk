/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the manual tests of the Qt Toolkit.
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

import QtQuick 2.9
import QtQuick.Layouts 1.1
import "qrc:/quick/shared/" as Shared

Item {
    width: 600; height: 600
    GridLayout {
        columns: 3; rowSpacing: 6; columnSpacing: 6
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 12 }

        // ----------------------------------------------------
        Text {
            text: "try typing and input methods in the TextInput below:"
            Layout.columnSpan: 3
        }

        // ----------------------------------------------------
        Text {
            text: "TextInput"
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: textInput.implicitHeight + 8
            radius: 4; antialiasing: true
            border.color: "black"; color: "transparent"

            TextInput {
                id: textInput
                anchors.fill: parent; anchors.margins: 4

                onTextEdited: textEditedInd.blip()
                onTextChanged: textChangedInd.blip()
                onPreeditTextChanged: preeditInd.blip()
                onDisplayTextChanged: displayTextInd.blip()
            }

        }

        SignalIndicator {
            id: textEditedInd
            label: "textEdited"
        }

        // ----------------------------------------------------
        Text { text: "text" }

        Text { text: textInput.text; Layout.fillWidth: true }

        SignalIndicator {
            id: textChangedInd
            label: "textChanged"
        }

        // ----------------------------------------------------
        Text { text: "preeditText" }

        Text { text: textInput.preeditText; Layout.fillWidth: true }

        SignalIndicator {
            id: preeditInd
            label: "preeditTextChanged"
        }

        // ----------------------------------------------------
        Text { text: "displayText" }

        Text { text: textInput.displayText; Layout.fillWidth: true }

        SignalIndicator {
            id: displayTextInd
            label: "displayTextChanged"
        }

        // ----------------------------------------------------
        Shared.TextField {
            id: copyFrom
            Layout.column: 1
            Layout.row: 5
            Layout.fillWidth: true
            text: "copy this"
        }

        Shared.Button {
            Layout.column: 2
            Layout.row: 5
            text: "setText"
            onClicked: {
                Qt.inputMethod.reset()
                textInput.text = copyFrom.text
            }
        }
    }
}
