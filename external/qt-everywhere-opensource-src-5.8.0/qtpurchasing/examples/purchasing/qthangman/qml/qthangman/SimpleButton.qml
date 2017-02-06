/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Purchasing module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3-COMM$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.2

Item {
    id: button
    property string text: ""
    property color buttonColor: "white"
    property color textColor: "black"
    property bool available: true
    property alias fontPointSize: buttonText.font.pointSize

    signal clicked()

    state: "NORMAL"

    Rectangle {
        id: buttonRect
        anchors.fill: parent
        radius: 10
        color: buttonColor
        visible: button.available
        Text {
            id: buttonText
            anchors.fill: parent
            anchors.rightMargin: parent.width * 0.05
            anchors.leftMargin: parent.width * 0.05
            anchors.bottomMargin: parent.height * 0.20
            anchors.topMargin: parent.height * 0.20
            text: button.text
            color: textColor
            fontSizeMode: Text.Fit
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            minimumPointSize: 8
            font.pointSize: 64
            font.family: "Helvetica Neue"
            font.weight: Font.Light
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            button.clicked();
        }
        onPressed: {
            button.state = "PRESSED";
        }
        onReleased: {
            button.state = "NORMAL";
        }
    }

    states: [
        State {
            name: "NORMAL"
            PropertyChanges {
                target: buttonRect
                color: button.buttonColor
                border.color: "transparent"
            }
            PropertyChanges {
                target: buttonText
                color: button.textColor
            }
        },
        State {
            name: "PRESSED"
            PropertyChanges {
                target: buttonRect
                color: "transparent"
                border.color: button.buttonColor
            }
            PropertyChanges {
                target: buttonText
                color: button.buttonColor
            }
        }
    ]
}
