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
