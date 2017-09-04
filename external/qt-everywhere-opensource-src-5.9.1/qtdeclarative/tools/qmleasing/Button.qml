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
