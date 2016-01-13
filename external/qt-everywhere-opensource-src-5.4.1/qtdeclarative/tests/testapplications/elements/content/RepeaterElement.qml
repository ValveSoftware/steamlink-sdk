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
    id: repeaterelementtest
    anchors.fill: parent
    property string testtext: ""
    property bool showme: true

    Column {
        id: container
        height: 200; width: 250
        spacing: 5
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 15
        Repeater { id: repeaterelement; model: repeatermodel }
        Rectangle { height: 50; width: 150; color: "green" }
        move: Transition { NumberAnimation { properties: "x,y"; duration: 1000; easing.type: Easing.OutBounce } }
        add: Transition { NumberAnimation { properties: "x,y"; duration: 1000; easing.type: Easing.OutBounce } }

    }

    VisualItemModel {
        id: repeatermodel
        Rectangle { color: "blue"; height: 40; width: 150; border.color: "black"; border.width: 3; opacity: .9; radius: 5; clip: true
            Text { text: "I am Thing 1"; anchors.centerIn: parent } }
        Rectangle { color: "blue"; height: 40; width: 150; border.color: "black"; border.width: 3; opacity: .9; radius: 5; clip: true
            Text { text: "I am Thing 2"; anchors.centerIn: parent } }
        Rectangle { visible: repeaterelementtest.showme;
            color: "blue"; height: 40; width: 150; border.color: "black"; border.width: 3; opacity: .9; radius: 5; clip: true
            Text { text: "I am Thing 3"; anchors.centerIn: parent } }
        Rectangle { color: "blue"; height: 40; width: 150; border.color: "black"; border.width: 3; opacity: .9; radius: 5; clip: true
            Text { text: "I am Thing 4"; anchors.centerIn: parent } }
        Rectangle { color: "blue"; height: 40; width: 150; border.color: "black"; border.width: 3; opacity: .9; radius: 5; clip: true
            Text { text: "I am Thing 5"; anchors.centerIn: parent } }
    }

    SystemTestHelp { id: helpbubble; visible: statenum != 0
        anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 50 }
    }
    BugPanel { id: bugpanel }

    states: [
        State { name: "start"; when: statenum == 1
            PropertyChanges { target: repeaterelementtest
                testtext: "This is a Repeater element. At present it should be showing five blue rectangles in a column, with"+
                "a green rectangle at the bottom.\n"+
                "Next, let's remove one of the rectangles from the column - the column should bounce when they are removed." }
        },
        State { name: "back"; when: statenum == 2
            PropertyChanges { target: repeaterelementtest; showme: false }
            PropertyChanges { target: repeaterelementtest
                testtext: "Repeater should now be showing a total of five rectangles.\n"+
                "Advance to restart the test." }
        }
    ]

}
