/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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
import "content"

Rectangle {
    id: window

    width: 320; height: 480
    color: "white"

    Column {
        id: column
        spacing: 6
        anchors.fill: parent
        anchors.margins: 10
        width: parent.width

        Row {
            spacing: 6
            width: column.width

            Text {
                id: subjectLabel
                //! [text]
                Accessible.role: Accessible.StaticText
                Accessible.name: text
                //! [text]
                text: "Subject:"
                y: 3
            }

            Rectangle {
                id: subjectBorder
                Accessible.role: Accessible.EditableText
                Accessible.name: subjectEdit.text
                border.width: 1
                border.color: "black"
                height: subjectEdit.height + 6
                width: 240
                TextInput {
                    focus: true
                    y: 3
                    x: 3
                    width: parent.width - 6
                    id: subjectEdit
                    text: "Vacation plans"
                    KeyNavigation.tab: textEdit
                }
            }
        }

        Rectangle {
            id: textBorder
            Accessible.role: Accessible.EditableText
            property alias text: textEdit.text
            border.width: 1
            border.color: "black"
            width: parent.width - 2
            height: 200

            TextEdit {
                id: textEdit
                y: 3
                x: 3
                width: parent.width - 6
                height: parent.height - 6
                text: "Hi, we're going to the Dolomites this summer. Weren't you also going to northern Italy? \n\nBest wishes, your friend Luke"
                wrapMode: TextEdit.WordWrap
                KeyNavigation.tab: sendButton
                KeyNavigation.priority: KeyNavigation.BeforeItem
            }
        }

        Text {
            id : status
            width: column.width
        }

        Row {
            spacing: 6
            Button {
                id: sendButton
                width: 100; height: 20
                text: "Send"
                onClicked: { status.text = "Send" }
                KeyNavigation.tab: discardButton
            }
            Button { id: discardButton
                width: 100; height: 20
                text: "Discard"
                onClicked: { status.text = "Discard" }
                KeyNavigation.tab: checkBox
            }
        }

        Row {
            spacing: 6

            Checkbox {
                id: checkBox
                checked: false
                KeyNavigation.tab: slider
            }

            Slider {
                id: slider
                value: 10
                KeyNavigation.tab: subjectEdit
            }
        }
    }
}
