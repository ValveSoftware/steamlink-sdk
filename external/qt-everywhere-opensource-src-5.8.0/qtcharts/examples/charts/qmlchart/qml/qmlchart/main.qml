/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Charts module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

//![1]
import QtQuick 2.0
//![1]

Item {
    width: 600
    height: 400
    property bool sourceLoaded: false

    ListView {
        id: root
        focus: true
        anchors.fill: parent
        snapMode: ListView.SnapOneItem
        highlightRangeMode: ListView.StrictlyEnforceRange
        highlightMoveDuration: 250
        orientation: ListView.Horizontal
        boundsBehavior: Flickable.StopAtBounds

        onCurrentIndexChanged: {
            if (infoText.opacity > 0.0) {
                if (sourceLoaded)
                    infoText.opacity = 0.0;
                else if (currentIndex != 0)
                    currentIndex = 0;
            }
        }

        model: ListModel {
            ListElement {component: "View1.qml"}
            ListElement {component: "View2.qml"}
            ListElement {component: "View3.qml"}
            ListElement {component: "View4.qml"}
            ListElement {component: "View5.qml"}
            ListElement {component: "View6.qml"}
            ListElement {component: "View7.qml"}
            ListElement {component: "View8.qml"}
            ListElement {component: "View9.qml"}
            ListElement {component: "View10.qml"}
            ListElement {component: "View11.qml"}
            ListElement {component: "View12.qml"}
        }

        delegate: Loader {
            width: root.width
            height: root.height

            source: component
            asynchronous: true

            onLoaded: sourceLoaded = true
        }
    }

    Rectangle {
        id: infoText
        anchors.centerIn: parent
        width: parent.width
        height: 40
        color: "black"
        Text {
            color: "white"
            anchors.centerIn: parent
            text: "You can navigate between views using swipe or arrow keys"
        }

        Behavior on opacity {
            NumberAnimation { duration: 400 }
        }
    }
}
