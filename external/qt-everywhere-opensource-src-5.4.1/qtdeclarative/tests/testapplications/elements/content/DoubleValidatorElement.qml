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

Item {
    id: doublevalidatorelementtest
    anchors.fill: parent
    property string testtext: ""

    DoubleValidator {
        id: doublevalidatorelement
        top: 30.06; bottom: 30.02
        //Behavior on top { NumberAnimation { duration: 1000 } }
    }

    Rectangle {
        id: doublevalidatorelementbackground
        color: doublevalidatorelementinput.acceptableInput ? "green" : "red"; height: 50; width: parent.width *.8
        border.color: "gray"; opacity: 0.7; radius: 5
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 15

        TextInput {
            id: doublevalidatorelementinput
            font.pointSize: 12; width: parent.width; text: "0"; horizontalAlignment: Text.AlignHCenter; validator: doublevalidatorelement
            anchors.centerIn: parent
            Behavior on font.pointSize { NumberAnimation { duration: 1000 } }
            Behavior on color { ColorAnimation { duration: 1000 } }
        }
    }

    Text{
        anchors.top: doublevalidatorelementbackground.bottom; anchors.topMargin: 50; anchors.horizontalCenter: parent.horizontalCenter
        text: "Top: " + doublevalidatorelement.top + " Bottom: " + doublevalidatorelement.bottom
    }

    SystemTestHelp { id: helpbubble; visible: statenum != 0
        anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 50 }
    }
    BugPanel { id: bugpanel }



    states: [
        State { name: "start"; when: statenum == 1
            PropertyChanges { target: doublevalidatorelementinput; text: "30.03" }
            PropertyChanges { target: doublevalidatorelementtest
                testtext: "This is a TextInput element using an DoubleValidator for input masking. At present it should be indicating 30.03.\n"+
                "Next, let's attempt to enter a number smaller than the lowest allowable value: 30.01" }
        },
        State { name: "lowerthan"; when: statenum == 2
            PropertyChanges { target: doublevalidatorelementinput; text: "30.01" }
            PropertyChanges { target: doublevalidatorelementtest
                testtext: "The TextInput background should be showing red - input is not acceptable.\n"+
                "Next, let's enter a value equal to the lowest allowable value: 30.02." }
        },
        State { name: "equaltobottom"; when: statenum == 3
            PropertyChanges { target: doublevalidatorelementinput; text: "30.02" }
            PropertyChanges { target: doublevalidatorelementtest
                testtext: "The TextInput background should be showing green - input is acceptable.\n"+
                "Next, let's attempt to enter a number greater than the highest allowable value: 30.08" }
        },
        State { name: "greaterthan"; when: statenum == 4
            PropertyChanges { target: doublevalidatorelementinput; text: "30.08" }
            PropertyChanges { target: doublevalidatorelementtest
                testtext: "The TextInput background should be showing red - input is not acceptable.\n"+
                "Next, let's change the value of top to be greater than the entered number." }
        },
        State { name: "increasedtop"; when: statenum == 5
            PropertyChanges { target: doublevalidatorelementinput; text: "30.08" }
            PropertyChanges { target: doublevalidatorelement; top: 30.09 }
            PropertyChanges { target: doublevalidatorelementtest
                testtext: "The highest value should have increased to 30.09, thus making the number valid and turning the input background green.\n"+
                "Next, let's change the value of bottom to be greater than the entered number." }
            PropertyChanges { target: bugpanel; bugnumber: "19956" }
        },
        State { name: "increasedbottom"; when: statenum == 6
            PropertyChanges { target: doublevalidatorelementinput; text: "30.08" }
            PropertyChanges { target: doublevalidatorelement; top: 30.09; bottom: 40.05 }
            PropertyChanges { target: doublevalidatorelementtest
                testtext: "The lowest value should have increased to 36, thus making the validator invalid and turning the input background red.\n"+
                "Press advance to restart the test." }
        }
    ]

}
