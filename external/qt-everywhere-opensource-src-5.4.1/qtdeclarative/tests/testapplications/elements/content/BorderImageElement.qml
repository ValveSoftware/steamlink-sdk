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
