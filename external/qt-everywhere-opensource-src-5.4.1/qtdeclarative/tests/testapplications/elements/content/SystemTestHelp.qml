/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
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

Rectangle {
    id: helpbubble
    visible: false
    property bool showtext: false
    property bool showadvance: true
    onShowadvanceChanged: { advancearrow.visible = showadvance; }

    width: 300; radius: 15; color: "white"; border.color: "black"; border.width: 3
    height: showadvance ? bubbletext.height + advancearrow.height + 25 : bubbletext.height + 25
    Behavior on height {
        SequentialAnimation {
            ScriptAction { script: { bubbletext.visible = false; advancearrow.visible = false } }
            NumberAnimation { duration: 200 }
            ScriptAction { script: { bubbletext.visible = true; advancearrow.visible = showadvance && true } }
        }
    }
    Behavior on width {
        SequentialAnimation {
            ScriptAction { script: { bubbletext.visible = false; advancearrow.visible = false } }
            NumberAnimation { duration: 200 }
            ScriptAction { script: { bubbletext.visible = true; advancearrow.visible = true } }
        }
    }
    Text { id: bubbletext; width: parent.width - 30; text: testtext; wrapMode: Text.WordWrap
            anchors { top: parent.top; topMargin: 15; left: parent.left; leftMargin: 20 }
    }
    Image { id: advancearrow; source: "pics/arrow.png"; height: 30; width: 30; visible: showadvance
        anchors { right: parent.right; bottom: parent.bottom; rightMargin: 5; bottomMargin: 5 }
        MouseArea { enabled: showadvance; anchors.fill: parent; onClicked: { advance(); } }
    }
}
