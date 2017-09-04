/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Graphical Effects module.
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

Label {
    id: bgColorPicker
    property color color: "#242424"
    caption: "Background color"

    Row {
        id: selection
        x: -3
        y: 3
        spacing: 4
        Rectangle { width: 16; height: 16; color: "#ffffff"
            Image {
                anchors.centerIn: parent
                source: "images/color_selection_mask.png"
            }
            MouseArea {
                anchors.fill: parent
                anchors.margins: -3
                onClicked: {
                    bgColorPicker.color = parent.color
                    highlight.x = parent.x - 3
                }
            }
        }
        Rectangle { width: 16; height: 16; color: "#ababab"
            Image {
                anchors.centerIn: parent
                source: "images/color_selection_mask.png"
            }
            MouseArea {
                anchors.fill: parent
                anchors.margins: -3
                onClicked: {
                    bgColorPicker.color = parent.color
                    highlight.x = parent.x - 3
                }
            }
        }
        Rectangle { id: initial; width: 16; height: 16; color: "#242424"
            Image {
                anchors.centerIn: parent
                source: "images/color_selection_mask.png"
            }
            MouseArea {
                anchors.fill: parent
                anchors.margins: -3
                onClicked: {
                    bgColorPicker.color = parent.color
                    highlight.x = parent.x - 3
                }
            }
        }
        Rectangle { width: 16; height: 16; color: "#000000"
            Image {
                anchors.centerIn: parent
                source: "images/color_selection_mask.png"
            }
            MouseArea {
                anchors.fill: parent
                anchors.margins: -3
                onClicked: {
                    bgColorPicker.color = parent.color
                    highlight.x = parent.x - 3
                }
            }
        }
        Image { source: "images/background.png"
            width: 16; height: 16;
            Image {
                anchors.centerIn: parent
                source: "images/color_selection_mask.png"
            }
            MouseArea {
                anchors.fill: parent
                anchors.margins: -3
                onClicked: {
                    bgColorPicker.color = "#010101"
                    highlight.x = parent.x - 3
                }
            }
        }
    }

    Image {
        id: highlight
        source: "images/color_selection_hl.png"
        x: initial.x - (highlight.width - initial.width) * 0.5
        y: selection.y - (highlight.height - selection.height) * 0.5
    }
}
