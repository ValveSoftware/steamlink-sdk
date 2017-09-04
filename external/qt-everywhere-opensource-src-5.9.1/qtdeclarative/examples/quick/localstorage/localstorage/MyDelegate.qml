/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the documentation of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:FDL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Free Documentation License Usage
** Alternatively, this file may be used under the terms of the GNU Free
** Documentation License version 1.3 as published by the Free Software
** Foundation and appearing in the file included in the packaging of
** this file. Please review the following information to ensure
** the GNU Free Documentation License version 1.3 requirements
** will be met: http://www.gnu.org/copyleft/fdl.html.
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0
import QtQuick.LocalStorage 2.0
import QtQuick.Layouts 1.1
import "Database.js" as JS

Item {
    width: parent.width
    height: rDate.implicitHeight

    Rectangle {
        id: baseRec
        anchors.fill: parent
        opacity: 0.8
        color: index % 2 ? "lightgrey" : "grey"

        MouseArea {
            anchors.fill: parent
            onClicked: listView.currentIndex = index
        }
        GridLayout {
            anchors.fill:parent
            columns: 3

            Text {
                id: rDate
                text: date
                font.pixelSize: 22
                Layout.preferredWidth: parent.width / 4
                color: "black"
            }
            Text {
                id: rDesc
                text: trip_desc
                Layout.fillWidth: true
                font.pixelSize: 22
                color: "black"
            }
            Text {
                id: rDistance
                text: distance
                font.pixelSize: 22
                Layout.alignment: Qt.AlignRight
                color: "black"
            }
        }
    }
}
