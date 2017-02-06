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

Rectangle {
    id: hangman
    color: "transparent"

    property int errorCount: applicationData.errorCount
    property var game: Item

    Rectangle {
        id: base
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: parent.height / 20

        opacity: errorCount > 0 ? 1.0 : 0.0
        Behavior on opacity {
            NumberAnimation {
                duration: 300
            }
        }
    }

    Rectangle {
        id: pole
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        width: parent.width / 20

        opacity: errorCount > 1 ? 1.0 : 0.0
        visible: opacity > 0.0
        Behavior on opacity {
            NumberAnimation {
                duration: 300
            }
        }

        color: "white"
    }

    Rectangle {
        id: horizontalPole
        anchors.top: pole.top
        anchors.left: pole.right
        anchors.right: parent.horizontalCenter
        anchors.rightMargin:  -parent.width / 40
        height: parent.height / 20
        color: "white"
        opacity: errorCount > 2 ? 1.0 : 0.0
        visible: opacity > 0.0
        Behavior on opacity {
            NumberAnimation {
                duration: 300
            }
        }
    }

    Rectangle {
        id: rope
        anchors.top: horizontalPole.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width / 20
        height: parent.height / 5
        opacity: errorCount > 2 ? 1.0 : 0.0
        visible: opacity > 0.0
        Behavior on opacity {
            NumberAnimation {
                duration: 300
            }
        }
        color: "white"
    }

    Item {
        id: dude
        anchors.top: rope.bottom
        anchors.horizontalCenter: rope.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: parent.height / 10

        Rectangle {
            id: head
            anchors.top: parent.top
            anchors.topMargin: -head.height * 0.25
            anchors.horizontalCenter: parent.horizontalCenter
            height: parent.height * 0.15
            width: height

            radius: width * 0.5
            color: "white"
            opacity: errorCount > 3 ? 1.0 : 0.0
            visible: opacity > 0.0
            Behavior on opacity {
                NumberAnimation {
                    duration: 300
                }
            }
        }

        Rectangle {
            id: body
            anchors.top: head.bottom
            anchors.topMargin: hangman.height / 40
            anchors.horizontalCenter: head.horizontalCenter
            height: width * 1.8
            width: head.width * 1.4
            opacity: errorCount > 4 ? 1.0 : 0.0
            visible: opacity > 0.0
            radius: 5
            Behavior on opacity {
                NumberAnimation {
                    duration: 300
                }
            }
        }
        Rectangle {
            id: armTop1
            anchors.top: body.top
            anchors.right: body.horizontalCenter
            width: (body.width * 1.85) * 0.5
            height: head.height * 0.7
            radius: height
            opacity: errorCount > 5 ? 1.0 : 0.0
            visible: opacity > 0.0
            Behavior on opacity {
                NumberAnimation {
                    duration: 300
                }
            }
        }
        Rectangle {
            id: armTop2
            anchors.top: body.top
            anchors.left: body.horizontalCenter
            width: (body.width * 1.85) * 0.5
            height: head.height * 0.7
            radius: height
            opacity: errorCount > 6 ? 1.0 : 0.0
            visible: opacity > 0.0
            Behavior on opacity {
                NumberAnimation {
                    duration: 300
                }
            }
        }

        Rectangle {
            id: arm1
            anchors.top: armTop1.bottom
            anchors.topMargin: -armTop1.height * 0.5 - radius
            anchors.left: armTop1.left
            anchors.bottom: body.bottom
            radius: width * 0.5
            width: head.width * 0.35
            opacity: errorCount > 5 ? 1.0 : 0.0
            visible: opacity > 0.0
            Behavior on opacity {
                NumberAnimation {
                    duration: 300
                }
            }
        }

        Rectangle {
            id: arm2
            anchors.top: armTop2.bottom
            anchors.topMargin: -armTop2.height * 0.5 - radius
            anchors.right: armTop2.right
            anchors.bottom: body.bottom
            radius: width * 0.5
            width: head.width * 0.35
            opacity: errorCount > 6 ? 1.0 : 0.0
            visible: opacity > 0.0
            Behavior on opacity {
                NumberAnimation {
                    duration: 300
                }
            }
        }


        Rectangle {
            id: leg1
            anchors.top: body.bottom
            anchors.topMargin: -radius
            anchors.left: body.left
            height: body.height + radius
            width: body.width * 0.42857
            radius: width

            opacity: errorCount > 7 ? 1.0 : 0.0
            visible: opacity > 0.0
            Behavior on opacity {
                NumberAnimation {
                    duration: 300
                }
            }
        }

        Rectangle {
            id: leg2
            anchors.top: body.bottom
            anchors.topMargin: -radius
            anchors.right: body.right
            height: body.height + radius
            width: body.width * 0.42857
            radius: width

            opacity: errorCount > 8 ? 1.0 : 0.0
            visible: opacity > 0.0
            Behavior on opacity {
                NumberAnimation {
                    duration: 300
                }
            }
        }
    }
}
