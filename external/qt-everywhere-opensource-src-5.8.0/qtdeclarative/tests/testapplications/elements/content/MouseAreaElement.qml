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
    id: mouseareaelementtest
    anchors.fill: parent
    property string testtext: ""
    focus: true

    Rectangle {
        id: mouseareaelementbox
        color: "lightgray"; border.color: "gray"; radius: 5; clip: true; opacity: .7; height: 200; width: 200
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 15
        MouseArea {
            id: mouseareaelement
            hoverEnabled: true
            anchors.fill: parent
        }
        Rectangle {
            id: topleft
            height: 50; width: 50; color: metopleft.containsMouse ? "green" : "blue"
            anchors { top: mouseareaelementbox.top; left: mouseareaelementbox.left; margins: 5 }
            MouseArea {
                id: metopleft
                hoverEnabled: true
                anchors.fill: parent
                onContainsMouseChanged: { if (containsMouse && statenum == 1) { advance(); } }
            }
        }
        Rectangle {
            id: topright
            height: 50; width: 50; color: metopright.containsMouse ? "green" : "blue"
            anchors { top: mouseareaelementbox.top; right: mouseareaelementbox.right; topMargin: 5; rightMargin: 5 }
            MouseArea {
                id: metopright
                hoverEnabled: true
                anchors.fill: parent
                onContainsMouseChanged: { if (containsMouse && statenum == 4) { advance(); } }
            }
        }
        Rectangle {
            id: bottomleft
            height: 50; width: 50; color: mebottomleft.containsMouse ? "green" : "blue"
            anchors { bottom: mouseareaelementbox.bottom; left: mouseareaelementbox.left; bottomMargin: 5; leftMargin: 5 }
            MouseArea {
                id: mebottomleft
                hoverEnabled: true
                anchors.fill: parent
                onContainsMouseChanged: { if (containsMouse && statenum == 3) { advance(); } }
            }
        }
        Rectangle {
            id: bottomright
            height: 50; width: 50; color: mebottomright.containsMouse ? "green" : "blue"
            anchors { bottom: mouseareaelementbox.bottom; right: mouseareaelementbox.right; bottomMargin: 5; rightMargin: 5 }
            MouseArea {
                id: mebottomright
                hoverEnabled: true
                anchors.fill: parent
                onContainsMouseChanged: { if (containsMouse && statenum == 2) { advance(); } }
            }
        }

        Rectangle {
            height: 10; width: 10; radius: 5; x: mouseareaelement.mouseX; y: mouseareaelement.mouseY; color: "red"
        }

    }

    SystemTestHelp { id: helpbubble; visible: statenum != 0
        anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 50 }
    }
    BugPanel { id: bugpanel }

    states: [
        State { name: "start"; when: statenum == 1
            PropertyChanges { target: helpbubble; showadvance: false }
            PropertyChanges { target: mouseareaelementtest
                testtext: "This test contains a number of MouseArea elements. At present there should be four rectangles\n"+
                "Next, move the pointer to the top left of the square" }
        },
        State { name: "topleft"; when: statenum == 2
            PropertyChanges { target: helpbubble; showadvance: false }
            PropertyChanges { target: topleft; color: "yellow" }
            PropertyChanges { target: mouseareaelementtest; testtext: "Good. Now move it down to the bottom right." }
        },
        State { name: "bottomright"; when: statenum == 3
            PropertyChanges { target: helpbubble; showadvance: false }
            PropertyChanges { target: bottomright; color: "yellow" }
            PropertyChanges { target: mouseareaelementtest; testtext: "To the bottom left." }
        },
        State { name: "bottomleft"; when: statenum == 4
            PropertyChanges { target: helpbubble; showadvance: false }
            PropertyChanges { target: bottomleft; color: "yellow" }
            PropertyChanges { target: mouseareaelementtest; testtext: "Then to top right" }
        },
        State { name: "topright"; when: statenum == 5
            PropertyChanges { target: helpbubble; showadvance: false }
            PropertyChanges { target: topright; color: "yellow" }
            PropertyChanges { target: mouseareaelementtest
                testtext: "Excellent.\n"+
                "Modifiers test not yet defined." }
        },
        State { name: "modifiers"; when: statenum == 6
            PropertyChanges { target: helpbubble; showadvance: true }
            PropertyChanges { target: mouseareaelementtest
                testtext: "Test has completed\n"+
                "Advance to restart the test." }
        }
    ]

}
