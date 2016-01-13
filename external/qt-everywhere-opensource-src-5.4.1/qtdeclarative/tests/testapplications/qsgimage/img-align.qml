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

Rectangle {
    id: main
    width: 800; height: 600
    focus: true
    color: "#eeeeee"

    property variant hAlign: Image.AlignHCenter
    property variant vAlign: Image.AlignVCenter
    property bool mirror: false
    property string source: "qt-logo.png"

    Flow {
        anchors.fill: parent
        anchors { topMargin: 20; leftMargin: 20; rightMargin: 20 }
        spacing: 30

        ImageNG {
            horizontalAlignment: main.hAlign; verticalAlignment: main.vAlign
            mirror: main.mirror; source: main.source
            explicitSize: false
            label: "implicit size"
        }
        ImageNG {
            horizontalAlignment: main.hAlign; verticalAlignment: main.vAlign
            mirror: main.mirror; source: main.source
            label: "explicit size"
        }
        ImageNG {
            horizontalAlignment: main.hAlign; verticalAlignment: main.vAlign
            fillMode: Image.Pad
            mirror: main.mirror; source: main.source
            label: "padding"
        }
        ImageNG {
            horizontalAlignment: main.hAlign; verticalAlignment: main.vAlign
            fillMode: Image.Tile
            mirror: main.mirror; source: main.source
            label: "tile"
        }
        ImageNG {
            horizontalAlignment: main.hAlign; verticalAlignment: main.vAlign
            fillMode: Image.TileHorizontally
            mirror: main.mirror; source: main.source
            label: "tile horizontally"
        }
        ImageNG {
            horizontalAlignment: main.hAlign; verticalAlignment: main.vAlign
            fillMode: Image.TileVertically
            mirror: main.mirror; source: main.source
            label: "tile vertically"
        }
        ImageNG {
            horizontalAlignment: main.hAlign; verticalAlignment: main.vAlign
            fillMode: Image.PreserveAspectFit
            mirror: main.mirror; source: main.source
            label: "preserve aspect fit"
        }
        ImageNG {
            horizontalAlignment: main.hAlign; verticalAlignment: main.vAlign
            fillMode: Image.PreserveAspectFit
            width: 150; height: 200
            mirror: main.mirror; source: main.source
            label: "preserve aspect fit"
        }
        ImageNG {
            horizontalAlignment: main.hAlign; verticalAlignment: main.vAlign
            fillMode: Image.PreserveAspectCrop
            mirror: main.mirror; source: main.source
            label: "preserve aspect crop"
        }
        ImageNG {
            horizontalAlignment: main.hAlign; verticalAlignment: main.vAlign
            fillMode: Image.PreserveAspectCrop
            width: 150; height: 200
            mirror: main.mirror; source: main.source
            label: "preserve aspect crop"
        }
    }

    Keys.onUpPressed: vAlign = Image.AlignTop
    Keys.onDownPressed: vAlign = Image.AlignBottom
    Keys.onLeftPressed: hAlign = Image.AlignLeft
    Keys.onRightPressed: hAlign = Image.AlignRight
    Keys.onPressed: {
        if (event.key == Qt.Key_H)
            hAlign = Image.AlignHCenter
        else if (event.key == Qt.Key_V)
            vAlign = Image.AlignVCenter
    }
}
