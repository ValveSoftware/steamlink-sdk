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
    id: scaleelementtest
    anchors.fill: parent
    property string testtext: ""

    Rectangle {
        id: scaletarget
        color: "green"; height: 100; width: 100; border.color: "gray"; opacity: 0.7; radius: 5
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 15
        transform: Scale {
            id: scaleelement
            property alias originx: scaleelement.origin.x
            property alias originy: scaleelement.origin.y
            origin.x: 50; origin.y: 50
            Behavior on xScale { NumberAnimation { duration: 500 } }
            Behavior on yScale { NumberAnimation { duration: 500 } }
            // QTBUG-20827 Behavior on origin.x { NumberAnimation { duration: 500 } }
            // QTBUG-20827 Behavior on origin.y { NumberAnimation { duration: 500 } }

        }
    }

    SystemTestHelp { id: helpbubble; visible: statenum != 0
        anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 50 }
    }
    BugPanel { id: bugpanel }

    states: [
        State { name: "start"; when: statenum == 1
            PropertyChanges { target: scaleelementtest
                testtext: "This is a Rectangle that will be transformed using a Scale element.\n"+
                "Next, it will be scaled to 2x size." }
        },
        State { name: "scaleup"; when: statenum == 2
            PropertyChanges { target: scaleelement; xScale: 2; yScale: 2 }
            PropertyChanges { target: scaleelementtest
                testtext: "It should be scaled to 2x.\nNext, it will shift to the right." }
        },
        State { name: "shiftright"; when: statenum == 3
            PropertyChanges { target: scaleelement; xScale: 2; yScale: 2; origin.x: 0; origin.y: 50 }
            PropertyChanges { target: scaleelementtest
                testtext: "It should be on the right, still scaled to 2x.\nNext, it will shift to the left" }
        },
        State { name: "shiftleft"; when: statenum == 4
            PropertyChanges { target: scaleelement; xScale: 2; yScale: 2; origin.x: 100; origin.y: 50 }
            PropertyChanges { target: scaleelementtest
                testtext: "It should be on the left, still scaled to 2x.\nAdvance to restart the test." }
        }
    ]

}
