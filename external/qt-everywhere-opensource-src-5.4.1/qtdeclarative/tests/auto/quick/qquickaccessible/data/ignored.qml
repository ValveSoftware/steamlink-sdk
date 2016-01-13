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
import "widgets"


Rectangle {
    id: page
    width: 640
    height: 480
    function col(str) {
        return Qt.hsla((str.charCodeAt(0)-65)/9, 1.0, 0.5, 1)
    }
    color: col(Accessible.name)
    Accessible.name: "A"
    Accessible.role: Accessible.StaticText

    Rectangle {
        id: b
        width: 20
        height: parent.height/2
        anchors.verticalCenter: parent.verticalCenter
        color: col(Accessible.name)
        Accessible.name: "B"
        Accessible.role: Accessible.StaticText
    }

    Rectangle {
        x: 20
        width: 80
        height: parent.height/2
        anchors.verticalCenter: parent.verticalCenter
        color: col(Accessible.name)
        Accessible.ignored: true
        Accessible.name: "C"
        Accessible.role: Accessible.StaticText

        Rectangle {
            width: 20
            color: col(Accessible.name)
            height: parent.height/2
            anchors.verticalCenter: parent.verticalCenter
            Accessible.name: "E"
            Accessible.role: Accessible.StaticText
        }

        Rectangle {
            x: 20
            width: 20
            color: col(Accessible.name)
            height: parent.height/2
            anchors.verticalCenter: parent.verticalCenter
            Accessible.name: "F"
            Accessible.role: Accessible.StaticText
        }

        Rectangle {
            x: 40
            width: 20
            height: parent.height/2
            anchors.verticalCenter: parent.verticalCenter
            color: col(Accessible.name)
            Accessible.ignored: true
            Accessible.name: "G"
            Accessible.role: Accessible.StaticText
            Rectangle {
                width: 20
                height: parent.height/2
                anchors.verticalCenter: parent.verticalCenter
                color: col(Accessible.name)
                Accessible.name: "I"
                Accessible.role: Accessible.StaticText
            }
        }
        Rectangle {
            x: 60
            width: 20
            height: parent.height/2
            anchors.verticalCenter: parent.verticalCenter
            color: col(Accessible.name)
            Accessible.name: "H"
            Accessible.role: Accessible.StaticText
        }
    }

    Rectangle {
        x: 100
        width: 20
        height: parent.height/2
        anchors.verticalCenter: parent.verticalCenter
        color: col(Accessible.name)
        Accessible.name: "D"
        Accessible.role: Accessible.StaticText
    }
}
