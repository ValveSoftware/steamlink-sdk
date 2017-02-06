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
    id: animatedimageelementtest
    anchors.fill: parent
    property string testtext: ""

    Item {
        id: animatedimageelementcontainer
        height: 100; width: 100
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 15
        AnimatedImage { id: animatedimageelement; anchors.fill: parent; source: "pics/cat.gif" }
        Behavior on height { NumberAnimation { duration: 1000 } }
        Behavior on width { NumberAnimation { duration: 1000 } }
    }

    SystemTestHelp { id: helpbubble; visible: statenum != 0
        anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 50 }
    }
    BugPanel { id: bugpanel }

    states: [
        State { name: "start"; when: statenum == 1
            PropertyChanges { target: animatedimageelementtest
                testtext: "This is an AnimatedImage element. It should be small and showing an animated cat.\n"+
                "Next, it should animatedly increase to twice its size" }
        },
        State { name: "large"; when: statenum == 2
            PropertyChanges { target: animatedimageelementcontainer; height: 200; width: 150 }
            PropertyChanges { target: animatedimageelementtest
                testtext: "It should be large and still showing the cat, but slightly stretched.\n"+
                "Next, let's change it to preserve its aspect ratio" }
        },
        State { name: "largefit"; when: statenum == 3
            PropertyChanges { target: animatedimageelementcontainer; height: 200; width: 150 }
            PropertyChanges { target: animatedimageelement; fillMode: Image.PreserveAspectFit }
            PropertyChanges { target: animatedimageelementtest
                testtext: "It should be large and now showing the cat normally (square).\n"+
                "Next, it will change its aspect ratio to fit, but cropping the sides" }
        },
        State { name: "largecrop"; when: statenum == 4
            PropertyChanges { target: animatedimageelementcontainer; height: 200; width: 150 }
            PropertyChanges { target: animatedimageelement; fillMode: Image.PreserveAspectCrop }
            PropertyChanges { target: animatedimageelementtest
                testtext: "It should be large and now showing the cat with the sides removed.\n"+
                "Next, let's change the image to tile the square" }
        },
        State { name: "largetile"; when: statenum == 5
            PropertyChanges { target: animatedimageelementcontainer; height: 200; width: 150 }
            PropertyChanges { target: animatedimageelement; fillMode: Image.Tile;  }
            PropertyChanges { target: animatedimageelementtest
                testtext: "The image should be repeated both horizontally and vertically.\n"+
                "Next, let's change the image to tile the square vertically" }
        },
        State { name: "largetilevertical"; when: statenum == 6
            PropertyChanges { target: animatedimageelementcontainer; height: 200; width: 150 }
            PropertyChanges { target: animatedimageelement; fillMode: Image.TileVertically;  }
            PropertyChanges { target: animatedimageelementtest
                testtext: "The image should be repeated only vertically.\n"+
                "Next, let's change the image to tile the square horizontally" }
        },
        State { name: "largetilehorizontal"; when: statenum == 7
            PropertyChanges { target: animatedimageelementcontainer; height: 200; width: 150 }
            PropertyChanges { target: animatedimageelement; fillMode: Image.TileHorizontally;  }
            PropertyChanges { target: animatedimageelementtest
                testtext: "The image should be repeated only horizontally.\n"+
                "The next step will return the image to a small, stretched state" }
        }
    ]
}
