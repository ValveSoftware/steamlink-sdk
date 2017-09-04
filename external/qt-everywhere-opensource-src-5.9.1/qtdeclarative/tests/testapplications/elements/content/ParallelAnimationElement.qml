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
    id: parallelanimationelementtest
    anchors.fill: parent
    property string testtext: ""
    property int firstduration: 1000
    property int secondduration: 3000
    property int firstY
    firstY: parent.height * .6
    property int secondY
    secondY: parent.height * .8

    Timer { id: startanimationtimer; interval: 1000; onTriggered: parallelanimationelement.start() }

    ParallelAnimation {
        id: parallelanimationelement
        running: false
        NumberAnimation { id: movement; target: animatedrect; properties: "y"; to: secondY; duration: firstduration }
        ColorAnimation { id: recolor; target: animatedrect; properties: "color"; to: "green"; duration: secondduration }
    }

    Rectangle {
        id: animatedrect
        width: 50; height: 50; color: "blue"; y: firstY
        anchors.horizontalCenter: parent.horizontalCenter
    }

    SystemTestHelp { id: helpbubble; visible: statenum != 0
        anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 50 }
    }
    BugPanel { id: bugpanel }

    states: [
        State { name: "start"; when: statenum == 1
            PropertyChanges { target: animatedrect; color: "blue"; y: firstY }
            PropertyChanges { target: parallelanimationelementtest
                testtext: "This square will have two properties animated simultaneously.\n"+
                "The next step will see it move quickly down the display, and slowly change its color to green, at the same time";
            }
        },
        State { name: "firstchange"; when: statenum == 2
            StateChangeScript { script: { firstduration = 1000; secondduration = 3000; startanimationtimer.start() } }
            PropertyChanges { target: parallelanimationelementtest
                testtext: "The square should have moved quickly, and recolored slowly\n"+
                "Next, it will recolor quickly and move slowly back to it's original position"
            }
        },
        State { name: "secondchange"; when: statenum == 3
            StateChangeScript { script: { firstduration = 3000; secondduration = 1000; startanimationtimer.start() } }
            PropertyChanges { target: movement; to: firstY }
            PropertyChanges { target: recolor; to: "blue" }
            PropertyChanges { target: parallelanimationelementtest
                testtext: "The square should have simultaneously moved slowly and recolored quickly.\n"+
                "Advance to restart the test"
            }
        }
    ]
}
