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
    id: borderimageelementtest
    anchors.fill: parent
    property int bordervalue: 30
    property string testtext: ""

    BorderImage {
        id: borderimageelement
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 15
        width: 100; height: 100
        source: "pics/qml-borderimage.png"
        border { left: bordervalue; top: bordervalue; right: bordervalue; bottom: bordervalue }
        Rectangle {
            height: parent.height-70; width: parent.width-70; anchors.centerIn: parent
            color: "gray"; radius: 5; border.color: "black"; opacity: .5
        }
        Behavior on height { NumberAnimation { duration: 1000 } }
        Behavior on width { NumberAnimation { duration: 1000 } }
    }

    SystemTestHelp { id: helpbubble; visible: statenum != 0
        anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 50 }
    }
    BugPanel { id: bugpanel }

    states: [
        State { name: "start"; when: statenum == 1
            PropertyChanges { target: borderimageelementtest
                testtext: "This is a BorderImage element. It should be small and be showing a frame.\n"+
                "Next, it should animatedly increase to twice its size" }
        },
        State { name: "large"; when: statenum == 2
            PropertyChanges { target: borderimageelement; height: 200; width: 200 }
            PropertyChanges { target: borderimageelementtest
                testtext: "It should be large and still showing the border frame.\n"+
                "Next, it will change the sides repeat mode to tile" }
        },
        State { name: "largetile"; when: statenum == 3
            PropertyChanges { target: borderimageelement; height: 200; width: 200;
                verticalTileMode: BorderImage.Repeat; horizontalTileMode: BorderImage.Repeat }
            PropertyChanges { target: borderimageelementtest
                testtext: "The sides of the border should now be repeated.\n"+
                "Next, it will change the sides repeat mode to repeated, but not cropped at the side" }
        },
        State { name: "largecrop"; when: statenum == 4
            PropertyChanges { target: borderimageelement; height: 200; width: 200;
                verticalTileMode: BorderImage.Round; horizontalTileMode: BorderImage.Round }
            PropertyChanges { target: borderimageelementtest
                testtext: "It should now show the borders repeated but scaled to fit uniformly.\n"+
                "The next step will show the BorderImage return to the defaults" }
        }
    ]

}
