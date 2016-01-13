/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
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
