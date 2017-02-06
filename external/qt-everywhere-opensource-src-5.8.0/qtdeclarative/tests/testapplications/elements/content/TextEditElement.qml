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

Item {
    id: texteditelementtest
    anchors.fill: parent
    property string testtext: ""
    property string wraptext: "Set this property to wrap the text to the TextEdit item's width. The text will only wrap if an explicit width has been set."

    Rectangle {
        id: texteditelementbackground
        color: "green"; height: 150; width: parent.width *.8; border.color: "gray"; opacity: 0.7; radius: 5; clip: true
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: secondarybackground.top
        anchors.bottomMargin: 10

        TextEdit {
            id: texteditelement
            font.pointSize: 12; width: parent.width; text: ""; horizontalAlignment: Text.AlignLeft
            anchors.centerIn: parent
            Behavior on font.pointSize { NumberAnimation { duration: 1000 } }
            Behavior on color { ColorAnimation { duration: 1000 } }
        }
    }

    Rectangle {
        id: secondarybackground
        color: "lightgray"; border.color: "gray"; opacity: 0.7; radius: 5; height: 50; width: parent.width *.8
        anchors { bottom: parent.bottom; bottomMargin: 15; horizontalCenter: parent.horizontalCenter }

        TextEdit {
            id: secondary
            property string ignoretext: "Nothing to see here"
            font.pointSize: 12; width: parent.width; text: ""; opacity: text == ignoretext ? .3 : 1; horizontalAlignment: Text.AlignLeft
            anchors.centerIn: parent
        }
    }
    Rectangle {
        id: shadowrect
        color: "lightgray"; height: 50; width: parent.width *.8; border.color: "gray"; opacity: 0; radius: 5
        anchors.horizontalCenter: texteditelementbackground.horizontalCenter;
        anchors.verticalCenter: texteditelementbackground.verticalCenter;

        Text {
            id: shadowtext
            font.pointSize: 12; width: parent.width; text: ""; horizontalAlignment: Text.AlignLeft
            anchors.centerIn: parent
        }
    }
    transitions: Transition {
        AnchorAnimation { targets: shadowrect; duration: 1000 }
    }

    SequentialAnimation { id: copypaste
        ScriptAction { script: { secondary.text = ""; shadowtext.text = texteditelement.selectedText; texteditelement.copy(); } }
        NumberAnimation { target: shadowrect; property: "opacity"; to: 0.5; duration: 100 }
        PauseAnimation { duration: 1000 }
        ScriptAction { script: { secondary.paste(); } }
        NumberAnimation { target: shadowrect; property: "opacity"; to: 0; duration: 100 }
        NumberAnimation { target: shadowrect; property: "x"; to: texteditelementbackground.x; duration: 0 }
    }

    SystemTestHelp { id: helpbubble; visible: statenum != 0
        anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 50 }
    }
    BugPanel { id: bugpanel }

    states: [
        State { name: "start"; when: statenum == 1
            StateChangeScript {
                script: {
                    texteditelement.text = "Hello, my name is TextEdit"
                    secondary.text = "Nothing to see here";
                }
            }
            PropertyChanges { target: texteditelementtest
                testtext: "This is a TextEdit element. At present it should be saying hello.\n"+
                "Next, the TextEdit portion of the displayed text will be selected" }
        },
        State { name: "highlight"; when: statenum == 2
            StateChangeScript { script: texteditelement.select(18, 26); }
            PropertyChanges { target: texteditelementtest
                testtext: "TextEdit should now be highlighted.\nNext, it will be copied this to the other TextEdit." }
        },
        State { name: "copypaste"; when: statenum == 3
            PropertyChanges { target: copypaste; running: true }
            AnchorChanges { target: shadowrect; anchors.verticalCenter: secondarybackground.verticalCenter }
            PropertyChanges { target: texteditelementtest
                testtext: "The second TextEdit should now be showing the selected text of the top TextEdit.\n"+
                "Next, a larger amount of text will be entered." }
        },
        State { name: "largetextnowrap"; when: statenum == 4
        PropertyChanges { target: texteditelement; text: wraptext }
            PropertyChanges { target: texteditelementtest
                testtext: "The TextEdit should now be showing a line of text, chopped off by the edge of the rectangle.\n"+
                "Next, the TextEdit will set wrapping to fix that." }
        },
        State { name: "largetextwrap"; when: statenum == 5
        PropertyChanges { target: texteditelement; text: wraptext; wrapMode: TextEdit.WordWrap }
            PropertyChanges { target: texteditelementtest
                testtext: "The TextEdit should now be showing a block of text.\n"+
                "Advance to restart the test." }
        }
    ]

}
