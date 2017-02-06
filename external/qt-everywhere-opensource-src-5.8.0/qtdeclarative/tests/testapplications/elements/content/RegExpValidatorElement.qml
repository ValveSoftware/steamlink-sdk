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
    id: regexpvalidatorelementtest
    anchors.fill: parent
    property string testtext: ""
    property variant regexp: /[a-z]{3}/

    RegExpValidator { id: regexpvalidatorelement; regExp: regexp }

    Rectangle {
        id: regexpvalidatorelementbackground
        color: regexpvalidatorelementinput.acceptableInput ? "green" : "red"; height: 50; width: parent.width *.8
        border.color: "gray"; opacity: 0.7; radius: 5
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 15

        TextInput {
            id: regexpvalidatorelementinput
            font.pointSize: 12; width: parent.width; text: "0"; horizontalAlignment: Text.AlignHCenter; validator: regexpvalidatorelement
            anchors.centerIn: parent
            Behavior on font.pointSize { NumberAnimation { duration: 1000 } }
            Behavior on color { ColorAnimation { duration: 1000 } }
        }
    }

    Text{
        anchors.top: regexpvalidatorelementbackground.bottom; anchors.topMargin: 50; anchors.horizontalCenter: parent.horizontalCenter
        text: "Regular Expression: " + regexp
    }

    SystemTestHelp { id: helpbubble; visible: statenum != 0
        anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 50 }
    }
    BugPanel { id: bugpanel }

    states: [
        State { name: "start"; when: statenum == 1
            PropertyChanges { target: regexpvalidatorelementinput; text: "abc" }
            PropertyChanges { target: regexpvalidatorelementtest
                testtext: "This is a TextInput element using an RegExpValidator for input masking. At present it should be indicating abc.\n"+
                "The regExp value will only match to a value that has three alpha characters\n"+
                "Next, let's attempt to enter text that does not match the regular expression: 123" }
        },
        State { name: "notmatch"; when: statenum == 2
            PropertyChanges { target: regexpvalidatorelementinput; text: "123" }
            PropertyChanges { target: regexpvalidatorelementtest
                testtext: "The TextInput background should be showing red - input is not acceptable.\n"+
                "Next, let's enter a word with not enough characters to match: aa." }
        },
        State { name: "notenough"; when: statenum == 3
            PropertyChanges { target: regexpvalidatorelementinput; text: "aa" }
            PropertyChanges { target: regexpvalidatorelementtest
                testtext: "The TextInput background should be showing red - input is not acceptable.\n"+
                "Next, let's attempt to enter a word with too many characters to match: abcd" }
        },
        State { name: "toomany"; when: statenum == 4
            PropertyChanges { target: regexpvalidatorelementinput; text: "abcd" }
            PropertyChanges { target: regexpvalidatorelementtest
                testtext: "The TextInput background should still be showing red - input is not acceptable.\n"+
                "Next, let's change the regex to accept the new value." }
        },
        State { name: "changedregex"; when: statenum == 5
            PropertyChanges { target: regexpvalidatorelementinput; text: "abcd" }
            PropertyChanges { target: regexpvalidatorelementtest; regexp: /[a-z]{4}/
                testtext: "The regular expression should have changed to match four characters, "+
                "thus making the text valid and turning the input background green.\n"+
                "Press advance to restart the test." }
            PropertyChanges { target: bugpanel; bugnumber: "19956" }
        }
    ]

}
