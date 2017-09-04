/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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
    id: root
    width:  800
    height: 100

    Rectangle {
        gradient: Gradient {
            GradientStop {
                position: 0
                color: "#aaa7a7"
            }

            GradientStop {
                position: 0.340
                color: "#a4a4a4"
            }

            GradientStop {
                position: 1
                color: "#6b6b6b"
            }
        }
        anchors.fill: parent
    }

    Button {
        id: button

        x: 19
        y: 20
        width: 133
        height: 61
        onClicked: {
            if (root.state ==="")
                root.state = "moved";
            else
                root.state = "";
        }
    }

    Rectangle {
        id: groove
        x: 163
        y: 20
        width: 622
        height: 61
        color: "#919191"
        radius: 4
        border.color: "#adadad"

        Rectangle {
            id: rectangle
            x: 9
            y: 9
            width: 46
            height: 46
            color: "#3045b7"
            radius: 4
            border.width: 2
            smooth: true
            border.color: "#9ea0bb"
            anchors.bottomMargin: 6
            anchors.topMargin: 9
            anchors.top: parent.top
            anchors.bottom: parent.bottom
        }
    }
    states: [
        State {
            name: "moved"

            PropertyChanges {
                target: rectangle
                x: 567
                y: 9
                anchors.bottomMargin: 6
                anchors.topMargin: 9
            }
        }
    ]

    transitions: [
        Transition {
            from: ""
            to: "moved"
            SequentialAnimation {
                PropertyAnimation {
                    easing: editor.easingCurve
                    property: "x"
                    duration: spinBox.value
                }
            }
        },
        Transition {
            from: "moved"
            to: ""
            PropertyAnimation {
                easing: editor.easingCurve
                property: "x"
                duration: spinBox.value
            }

        }
    ]
}
