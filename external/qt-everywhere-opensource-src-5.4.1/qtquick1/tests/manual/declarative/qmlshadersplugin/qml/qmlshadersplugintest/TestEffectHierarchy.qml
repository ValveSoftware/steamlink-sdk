/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
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
import Qt.labs.shaders 1.0

Rectangle {
    id :root
    anchors.fill: parent;
    color: "green"

    Timer {
        running: true
        interval: 2000
        repeat: true
        onTriggered: {
            effect1.visible = !effect1.visible
            effect2.visible = !effect2.visible
            //effect3.visible = !effect3.visible
        }
    }

    Rectangle {
        id: a
        x: 90
        y: 90
        color: "red"
        width: 220
        height: 220
        Rectangle {
            id: b
            x: 10
            y: 10
            color: "blue"
            width: 100
            height: 100
            rotation: 5
            Rectangle {
                id: c
                x: 10
                y: 10
                color: "black"
                width: 80
                height: 80
            }
        }
        Rectangle {
            id: d
            x: 10
            y: 110
            color: "yellow"
            width: 100
            height: 100
        }
    }

    ShaderEffectItem {
        id: effect1
        anchors.fill: a
        property variant source: ShaderEffectSource{ sourceItem: a; hideSource: true }
    }

    ShaderEffectItem {
        id: effect2
        x: 100
        y: 100
        width: 100
        height: 100
        rotation: 5
        property variant source: ShaderEffectSource{ sourceItem: b; hideSource: true }
    }

//    ShaderEffectItem {
//        id: effect3
//        x: 110
//        y: 210
//        width: 80
//        height: 80
//        property variant source: ShaderEffectSource{ sourceItem: c; hideSource: true }
//    }


    Rectangle {
        width: parent.width
        height: 40
        color: "#cc000000"

        Text {
             id: label
             anchors.centerIn:  parent
             text: effect1.visible ? "Effects active" : "Effects NOT active"
             color: "white"
             font.bold: true
        }
    }
}
