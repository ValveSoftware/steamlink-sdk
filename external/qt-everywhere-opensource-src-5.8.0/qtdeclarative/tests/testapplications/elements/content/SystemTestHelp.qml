/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
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
