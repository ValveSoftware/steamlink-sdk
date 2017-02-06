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
    id: textinputelementtest
    anchors.fill: parent
    property string testtext: ""

    Rectangle {
        id: textinputelementbackground
        color: "green"; height: 50; width: parent.width *.8; border.color: "gray"; opacity: 0.7; radius: 5
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: secondarybackground.top
        anchors.bottomMargin: 10
        TextInput {
            id: textinputelement
            font.pointSize: 12; width: parent.width; text: ""; horizontalAlignment: Text.AlignHCenter
            anchors.centerIn: parent
            Behavior on font.pointSize { NumberAnimation { duration: 1000 } }
            Behavior on color { ColorAnimation { duration: 1000 } }
        }
    }

    Rectangle {
        id: secondarybackground
        color: "lightgray"; border.color: "gray"; opacity: 0.7; radius: 5; height: 50; width: parent.width *.8
        anchors { bottom: parent.bottom; bottomMargin: 15; horizontalCenter: parent.horizontalCenter }

        TextInput {
            id: secondary
            property string ignoretext: "Nothing to see here"
            font.pointSize: 12; text: ""; opacity: text == ignoretext ? .3 : 1; horizontalAlignment: Text.AlignHCenter
            anchors.centerIn: parent; width: parent.width
        }
    }
    Rectangle {
        id: shadowrect
        color: "lightgray"; height: 50; width: parent.width *.8; border.color: "gray"; opacity: 0; radius: 5
        anchors.horizontalCenter: textinputelementbackground.horizontalCenter;
        anchors.verticalCenter: textinputelementbackground.verticalCenter;
        Text {
            id: shadowtext
            font.pointSize: 12; width: parent.width; text: ""; horizontalAlignment: Text.AlignHCenter
            anchors.centerIn: parent
        }
    }
    transitions: Transition {
        AnchorAnimation { targets: shadowrect; duration: 1000 }
    }

    SequentialAnimation { id: copypaste
        ScriptAction { script: { secondary.text = ""; shadowtext.text = textinputelement.selectedText; textinputelement.copy(); } }
        NumberAnimation { target: shadowrect; property: "opacity"; to: 0.5; duration: 100 }
        PauseAnimation { duration: 1000 }
        ScriptAction { script: { secondary.paste(); } }
        NumberAnimation { target: shadowrect; property: "opacity"; to: 0; duration: 100 }
        NumberAnimation { target: shadowrect; property: "x"; to: textinputelementbackground.x; duration: 0 }
    }

    BugPanel { id: bugpanel }

    SystemTestHelp { id: helpbubble; visible: statenum != 0
        anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 50 }
    }

    states: [
        State { name: "start"; when: statenum == 1
            StateChangeScript {
                script: {
                    textinputelement.text = "Hello, my name is TextInput"
                    secondary.text = "Nothing to see here";
                }
            }
            PropertyChanges { target: textinputelementtest
                testtext: "This is a TextInput element. At present it should be saying hello.\n"+
                "Next, it will select the TextInput portion of the displayed text" }
        },
        State { name: "highlight"; when: statenum == 2
            StateChangeScript { script: textinputelement.select(18, 27); }
            PropertyChanges { target: textinputelementtest
                testtext: "TextInput should now be highlighted.\nNext, copy this to the other TextInput." }
        },
        State { name: "copypaste"; when: statenum == 3
            PropertyChanges { target: copypaste; running: true }
            AnchorChanges { target: shadowrect; anchors.verticalCenter: secondarybackground.verticalCenter }
            PropertyChanges { target: textinputelementtest
                testtext: "The second TextInput should now be showing the selected text of the top TextInput.\n"+
                "Next, let's change the way the entered text is displayed." }
        },
        State { name: "passwordmode"; when: statenum == 4
            StateChangeScript { script: textinputelement.deselect(); }
            PropertyChanges { target: textinputelement; echoMode: TextInput.Password }
            PropertyChanges { target: textinputelementtest
                testtext: "The TextInput should now be showing asterisks (*) for every character in the top TextInput - all "+
                textinputelement.text.length+" of them.\n"+
                "Next, let's return to the start." }
        }
    ]


}
