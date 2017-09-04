/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the documentation of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:FDL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Free Documentation License Usage
** Alternatively, this file may be used under the terms of the GNU Free
** Documentation License version 1.3 as published by the Free Software
** Foundation and appearing in the file included in the packaging of
** this file. Please review the following information to ensure
** the GNU Free Documentation License version 1.3 requirements
** will be met: https://www.gnu.org/licenses/fdl-1.3.html.
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.7
import QtQuick.Controls 2.0

Item {
    width: 200
    height: 200

//! [1]
Rectangle {
    id: frame
    clip: true
    width: 160
    height: 160
    border.color: "black"
    anchors.centerIn: parent

    Text {
        id: content
        text: "ABC"
        font.pixelSize: 169

        MouseArea {
            id: mouseArea
            drag.target: content
            drag.minimumX: frame.width - width
            drag.minimumY: frame.height - height
            drag.maximumX: 0
            drag.maximumY: 0
            anchors.fill: content
        }
    }

    ScrollIndicator {
        id: verticalIndicator
        active: mouseArea.pressed
        orientation: Qt.Vertical
        size: frame.height / content.height
        position: -content.y / content.height
        anchors { top: parent.top; right: parent.right; bottom: parent.bottom }
    }

    ScrollIndicator {
        id: horizontalIndicator
        active: mouseArea.pressed
        orientation: Qt.Horizontal
        size: frame.width / content.width
        position: -content.x / content.width
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
    }
}
//! [1]

Component.onCompleted: {
    horizontalIndicator.active = true;
    verticalIndicator.active = true;
}
}
