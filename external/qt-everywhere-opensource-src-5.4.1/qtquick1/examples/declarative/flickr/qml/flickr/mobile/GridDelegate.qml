/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the demonstration applications of the Qt Toolkit.
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

import QtQuick 1.0

Item {
    id: wrapper; width: GridView.view.cellWidth; height: GridView.view.cellHeight

    function photoClicked() {
        imageDetails.photoTitle = title;
        imageDetails.photoTags = tags;
        imageDetails.photoWidth = photoWidth;
        imageDetails.photoHeight = photoHeight;
        imageDetails.photoType = photoType;
        imageDetails.photoAuthor = photoAuthor;
        imageDetails.photoDate = photoDate;
        imageDetails.photoUrl = url;
        imageDetails.rating = 0;
        scaleMe.state = "Details";
    }

    Item {
        anchors.centerIn: parent
        scale: 0.0
        Behavior on scale { NumberAnimation { easing.type: Easing.InOutQuad} }
        id: scaleMe

        Item {
            width: 77; height: 77; anchors.centerIn: parent
            Rectangle {
                id: whiteRect; width: 77; height: 77; color: "#dddddd"; smooth: true
                Image { id: thumb; source: imagePath; x: 1; y: 1; smooth: true }
                Image { source: "images/gloss.png" }
            }
        }

        Connections {
            target: toolBar
            onButton2Clicked: if (scaleMe.state == 'Details' ) scaleMe.state = 'Show'
        }

        states: [
            State {
                name: "Show"; when: thumb.status == Image.Ready
                PropertyChanges { target: scaleMe; scale: 1 }
            },
            State {
                name: "Details"
                PropertyChanges { target: scaleMe; scale: 1 }
                ParentChange { target: whiteRect; x: 10; y: 20; parent: imageDetails.frontContainer }
                PropertyChanges { target: background; state: "DetailedView" }
            }
        ]
        transitions: [
            Transition {
                from: "Show"; to: "Details"
                ParentAnimation {
                    via: foreground
                    NumberAnimation { properties: "x,y"; duration: 500; easing.type: Easing.InOutQuad }
                }
            },
            Transition {
                from: "Details"; to: "Show"
                ParentAnimation {
                    via: foreground
                    NumberAnimation { properties: "x,y"; duration: 500; easing.type: Easing.InOutQuad }
                }
            }
        ]
    }
    MouseArea { anchors.fill: wrapper; onClicked: photoClicked() }
}

