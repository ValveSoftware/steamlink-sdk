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
    id: button

    signal clicked

    Rectangle {
        id: normalBackground
        radius: 4
        anchors.fill: parent
        smooth: true
        gradient: Gradient {
            GradientStop {
                position: 0
                color: "#afafaf"
            }

            GradientStop {
                position: 0.460
                color: "#808080"
            }

            GradientStop {
                position: 1
                color: "#adadad"
            }
        }
        border.color: "#000000"
    }


    Rectangle {
        id: hoveredBackground
        x: 2
        y: -8
        radius: 4
        opacity: 0
        gradient: Gradient {
            GradientStop {
                position: 0
                color: "#cacaca"
            }

            GradientStop {
                position: 0.460
                color: "#a2a2a2"
            }

            GradientStop {
                position: 1
                color: "#c8c8c8"
            }
        }
        smooth: true
        anchors.fill: parent
        border.color: "#000000"
    }


    Rectangle {
        id: pressedBackground
        x: -8
        y: 2
        radius: 4
        opacity: 0
        gradient: Gradient {
            GradientStop {
                position: 0
                color: "#8b8b8b"
            }

            GradientStop {
                position: 0.470
                color: "#626161"
            }

            GradientStop {
                position: 1
                color: "#8f8e8e"
            }
        }
        smooth: true
        anchors.fill: parent
        border.color: "#000000"
    }
    states: [
        State {
            name: "hovered"

            PropertyChanges {
                target: normalBackground
                opacity: 0
            }

            PropertyChanges {
                target: hoveredBackground
                opacity: 1
            }
        },
        State {
            name: "pressed"

            PropertyChanges {
                target: normalBackground
                opacity: 0
            }

            PropertyChanges {
                target: pressedBackground
                opacity: 1
            }
        }
    ]

    Text {
        color: "#e8e8e8"
        text: qsTr("Play")
        anchors.fill: parent
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font.bold: true
        font.pixelSize: 20
    }

    MouseArea {
        hoverEnabled: true
        anchors.fill: parent
        onEntered: button.state = "hovered"
        onExited: button.state = ""
        onClicked: {
            button.state = "pressed"
            button.clicked();
        }
    }
}
