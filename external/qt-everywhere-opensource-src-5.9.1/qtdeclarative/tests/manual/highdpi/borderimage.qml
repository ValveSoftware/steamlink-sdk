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

Rectangle {
    width: 900
    height: 400

    readonly property real imageBorder: 32
    readonly property real animDuration: 3000
    readonly property real animMin: 2 * imageBorder
    readonly property real animMax: 280

    Text {
        anchors.bottom: row.top
        anchors.horizontalCenter: parent.horizontalCenter
        text: "Green => standard DPI; purple => @2x"
    }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 10
        Repeater {
            model: 3
            delegate: Item {
                width: animMax
                height: animMax
                BorderImage {
                    source : index === 0 ? "BorderImage.png" : "TiledBorderImage.png"
                    anchors.centerIn: parent

                    border {
                        left: imageBorder; right: imageBorder
                        top: imageBorder; bottom: imageBorder
                    }

                    horizontalTileMode: index === 0 ? BorderImage.Stretch :
                                        index === 1 ? BorderImage.Repeat : BorderImage.Round
                    verticalTileMode: index === 0 ? BorderImage.Stretch :
                                      index === 1 ? BorderImage.Repeat : BorderImage.Round

                    width: animMin
                    SequentialAnimation on width {
                        NumberAnimation { to: animMax; duration: animDuration }
                        NumberAnimation { to: animMin; duration: animDuration }
                        loops: Animation.Infinite
                    }

                    height: animMax
                    SequentialAnimation on height {
                        NumberAnimation { to: animMin; duration: animDuration }
                        NumberAnimation { to: animMax; duration: animDuration }
                        loops: Animation.Infinite
                    }
                }

                Text {
                    anchors.top: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    font.pixelSize: 18
                    text: index === 0 ? "Stretch" :
                          index === 1 ? "Repeat" : "Round"
                }
            }
        }
    }
}
