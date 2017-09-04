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

Item { id: rectangleelementtest
    anchors.fill: parent
    property string testtext: ""

    Rectangle {
        id: rectangleelement
        height: 100; width: 100; color: "blue"; border.width: 2; border.color: "red"; smooth: true
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 15
        Behavior on height { NumberAnimation { duration: 1000 } }
        Behavior on width { NumberAnimation { duration: 1000 } }
        Behavior on radius { NumberAnimation { duration: 1000 } }
        Behavior on color { ColorAnimation { duration: 1000 } }
        Behavior on border.color { ColorAnimation { duration: 1000 } }
        Behavior on border.width { NumberAnimation { duration: 1000 } }
    }

    SystemTestHelp { id: helpbubble; visible: statenum != 0
        anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 50 }
    }
    BugPanel { id: bugpanel }

    states: [
        State { name: "start"; when: statenum == 1
            PropertyChanges { target: rectangleelementtest
                testtext: "This is a Rectangle element. It should be small and blue, with a thin, red border.\n"+
                "Next, it will animatedly increase to twice its size" }
        },
        State { name: "large"; when: statenum == 2
            PropertyChanges { target: rectangleelement; height: 200; width: 200 }
            PropertyChanges { target: rectangleelementtest; testtext: "It should now be large and blue, with a thin, red border.\n"+
                "Next, a radius will be added to round the corners" }
        },
        State { name: "largerounded"; when: statenum == 3
            PropertyChanges { target: rectangleelement; height: 200; width: 200; radius: 20 }
            PropertyChanges { target: rectangleelementtest; testtext: "The borders should now be rounded.\n"+
                "Next, it will change the color to green" }
        },
        State { name: "largeroundedgreen"; when: statenum == 4
            PropertyChanges { target: rectangleelement; height: 200; width: 200; radius: 20; color: "green" }
            PropertyChanges { target: rectangleelementtest; testtext: "The rectangle should now be green.\n"+
                "Next, the border width will be increased" }
        },
        State { name: "largeroundedgreenthick"; when: statenum == 5
            PropertyChanges { target: rectangleelement; height: 200; width: 200; radius: 20; color: "green"; border.width: 10 }
            PropertyChanges { target: rectangleelementtest; testtext: "The border width should have increased significantly.\n"+
                "Advance to restart the test - everything should animate at once" }
        }
    ]

}
