/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
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
