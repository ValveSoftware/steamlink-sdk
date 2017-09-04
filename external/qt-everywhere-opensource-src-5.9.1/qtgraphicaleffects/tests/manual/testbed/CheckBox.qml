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
    id: checkBox
    property string caption: ""
    property bool selected: true
    property bool pressed: mouse.pressed
    anchors {left: parent.left; right: parent.right}
    height: 20

    Text {
        id: captionText
        width: 110
        anchors.verticalCenter: parent.verticalCenter
        horizontalAlignment: Text.AlignRight
        text: checkBox.caption + ':'
        font.family: "Arial"
        font.pixelSize: 11
        color: "#B3B3B3"
    }

    Image {
        id: button
        anchors {left: captionText.right; leftMargin: 5; verticalCenter: parent.verticalCenter}
        source: "images/checkbox_outer.png"
        smooth: true
        Image {
            id: buttonFill
            anchors.centerIn: parent
            source: "images/checkbox_inner.png"
            smooth: true
            visible: checkBox.selected
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        anchors.rightMargin: 160
        onClicked: checkBox.selected = !checkBox.selected
        preventStealing: true
    }
}
