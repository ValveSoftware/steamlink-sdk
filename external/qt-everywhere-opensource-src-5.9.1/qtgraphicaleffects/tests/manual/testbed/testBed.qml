/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Graphical Effects module.
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

Rectangle {
    id: main

    width: 950
    height: 620
    color: '#171717'

    ListView {
        id: testCaseList
        width: 160
        anchors.top: titlebar.bottom
        anchors.left: parent.left
        anchors.bottom: parent.bottom

        model: TestBedModel {}
        delegate: delegateItem

        section.property: "group"
        section.criteria: ViewSection.FullString
        section.delegate: sectionHeading

    }

    Image {
        id: titlebar
        source: "images/title.png"
        anchors.top: parent.top
        anchors.left: parent.left
        width: 160

        Text {
            id: effectsListTitle
            anchors.fill: parent
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: "Effects"
            color: "white"
            font.family: "Arial"
            font.bold: true
            font.pixelSize: 12
        }
    }

    Loader {
        id: testLoader
        anchors {left: testCaseList.right; right: parent.right; top: parent.top; bottom: parent.bottom}
    }

    Image {
        anchors {right: testLoader.right; rightMargin: 300; top: parent.top; bottom: parent.bottom}
        source: "images/workarea_right.png"
    }

    Image {
        anchors {left: testLoader.left; top: parent.top; bottom: parent.bottom}
        source: "images/workarea_left.png"
    }


    Component {
        id: sectionHeading
        Item {
            width: parent.width
            height: 23
            Image {
                source: "images/group_top.png"
                width: parent.width
            }
            Image {
                id: icon
                source: "images/icon_" + section.replace(/ /g, "_").toLowerCase() + ".png"
                anchors {top: parent.top; topMargin: 6; left: parent.left; leftMargin: 6}
            }
            Text {
                id: sectionText
                text: section
                anchors {fill: parent; leftMargin: 25; topMargin: 3}
                color: "white"
                verticalAlignment: Text.AlignVCenter
                font.family: "Arial"
                font.bold: true
                font.pixelSize: 12
            }
        }
    }

    Component {
        id: delegateItem
        Item {
            width: ListView.view.width
            height: last ? 27 : 20

            Image {
                source: "images/group_bottom.png"
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                smooth: true
                visible: last && index != 20 ? 1 : 0
            }

            Rectangle {
                width: parent.width
                color: "#323232"
                height: 20
                visible: (testLoader.source.toString().search(name) != -1)
            }
            Text {
                id: delegateText;
                anchors {fill: parent; leftMargin: 25; topMargin: 3}
                text: name.slice(4, name.indexOf("."))
                font.family: "Arial"
                font.pixelSize: 12
                color: delegateMouseArea.pressed  ? "white" : "#CCCCCC"
            }
            MouseArea {
                id: delegateMouseArea
                anchors.fill: parent;
                onClicked: {
                    testLoader.source = name;
                    testLoader.item.currentTest = delegateText.text;
                }
            }
        }
    }
}
