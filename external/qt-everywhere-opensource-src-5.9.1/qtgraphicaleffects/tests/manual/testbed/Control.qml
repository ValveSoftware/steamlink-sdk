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

Item {
    id: control
    property string caption: ""
    default property alias children: controlsColumn.children
    property bool last: false

    property bool __hide: caption == "advanced"

    anchors {left: parent.left; right: parent.right}
    height: __hide ? 30 : controlsColumn.height + 40

    Behavior on height {
        id: heightBehavior
        enabled: false
        NumberAnimation { duration: 100 }
    }

    Image {
        source: "images/group_top.png"
        anchors {top: parent.top; left: parent.left; right: parent.right}
    }

    Image {
        source: "images/group_bottom.png"
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        smooth: true
        visible: !last || __hide
    }

    Text {
        id: sectionText
        anchors {left: parent.left; top: parent.top; leftMargin: 11; topMargin: 8}
        color: "white"
        font.family: "Arial"
        font.bold: true
        font.pixelSize: 12
        text: caption
    }

    Image {
        anchors {right: parent.right; rightMargin: 5; top: parent.top}
        source: __hide ? "images/expand.png" : "images/collapse.png"
    }

    MouseArea {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 30
        onClicked: {
            heightBehavior.enabled = true
            control.__hide = !control.__hide
        }
    }

    Column {
        id: controlsColumn
        anchors.top: parent.top
        anchors.topMargin: 30
        anchors.left: parent.left
        anchors.right: parent.right
        opacity: !__hide
        Behavior on opacity {
            NumberAnimation { duration: 100 }
        }
    }
}
