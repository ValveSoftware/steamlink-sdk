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
    id: imageelementtest
    anchors.fill: parent
    property string testtext: ""

    Item {
        id: imageelementcontainer
        height: 100; width: 100; clip: true
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 15
        Behavior on height { NumberAnimation { duration: 1000 } }
        Behavior on width { NumberAnimation { duration: 1000 } }
        Image {
            id: imageelement
            anchors.fill: parent
            source: "pics/nokialogo.gif"; sourceSize.width: 60; sourceSize.height: 80
        }
    }

    SystemTestHelp { id: helpbubble; visible: statenum != 0
        anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 50 }
    }
    BugPanel { id: bugpanel }

    states: [
        State { name: "start"; when: statenum == 1
            PropertyChanges { target: imageelementtest
                testtext: "This is an Image element. It should be small and be showing the Nokia logo, but slightly stretched.\n"+
                "Next, it should animatedly increase to twice its size" }
        },
        State { name: "large"; when: statenum == 2
            PropertyChanges { target: imageelementcontainer; height: 200; width: 200 }
            PropertyChanges { target: imageelementtest
                testtext: "It should be large and still showing the slightly stretched Nokia logo.\n"+
                "Next, it will change to preserve its aspect ratio" }
        },
        State { name: "largefit"; when: statenum == 3
            PropertyChanges { target: imageelementcontainer; height: 200; width: 200 }
            PropertyChanges { target: imageelement; fillMode: Image.PreserveAspectFit }
            PropertyChanges { target: imageelementtest
                testtext: "It should be large and now showing the Nokia logo normally (longer horizontally).\n"+
                "Next, it will change its aspect ratio to fit, but cropping the sides" }
        },
        State { name: "largecrop"; when: statenum == 4
            PropertyChanges { target: imageelementcontainer; height: 200; width: 200 }
            PropertyChanges { target: imageelement; fillMode: Image.PreserveAspectCrop }
            PropertyChanges { target: imageelementtest
                testtext: "It should be large and now showing the Nokia logo with the sides removed.\n"+
                "Next, let's change the image to tile the square" }
        },
        State { name: "largetile"; when: statenum == 5
            PropertyChanges { target: imageelementcontainer; height: 200; width: 200 }
            PropertyChanges { target: imageelement; fillMode: Image.Tile;  }
            PropertyChanges { target: imageelementtest
                testtext: "The image should be repeated both horizontally and vertically.\n"+
                "Next, let's change the image to tile the square vertically" }
        },
        State { name: "largetilevertical"; when: statenum == 6
            PropertyChanges { target: imageelementcontainer; height: 200; width: 200 }
            PropertyChanges { target: imageelement; fillMode: Image.TileVertically;  }
            PropertyChanges { target: imageelementtest
                testtext: "The image should be repeated only vertically.\n"+
                "Next, let's change the image to tile the square horizontally" }
        },
        State { name: "largetilehorizontal"; when: statenum == 7
            PropertyChanges { target: imageelementcontainer; height: 200; width: 200 }
            PropertyChanges { target: imageelement; fillMode: Image.TileHorizontally;  }
            PropertyChanges { target: imageelementtest
                testtext: "The image should be repeated only horizontally.\n"+
                "The next step will return the image to a small, stretched state" }
        }
    ]

}
