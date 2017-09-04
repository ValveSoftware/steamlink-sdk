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

Item {
    id: rgbaSlider
    property color color: Qt.rgba(rValue, gValue, bValue, aValue)

    property alias rValue: rSlider.value
    property alias gValue: gSlider.value
    property alias bValue: bSlider.value
    property alias aValue: aSlider.value
    property bool pressed: rSlider.pressed || gSlider.pressed || bSlider.pressed || aSlider.pressed

    width: parent.width
    height: childrenRect.height

    function dec2hex(i)
    {
      if (i <= 15)
         return  "0" + i.toString(16);
      else
         return i.toString(16);
    }

    Rectangle {
        id: colorRect
        width: 50; height: 50
        anchors.left: parent.left
        anchors.leftMargin: 20
        anchors.verticalCenter: sliders.verticalCenter
        color: Qt.rgba(rgbaSlider.rValue, rgbaSlider.gValue, rgbaSlider.bValue, rgbaSlider.aValue)
    }
    Column {
        id: sliders
        anchors {left: parent.left; right: parent.right}
        Slider {
            id: aSlider
            minimum: 0
            maximum: 1
            value: 1.0
            caption: 'A'
        }
        Slider {
            id: rSlider
            minimum: 0
            maximum: 1
            value: 1.0
            caption: 'R'
        }
        Slider {
            id: gSlider
            minimum: 0
            maximum: 1
            value: 1.0
            caption: 'G'
        }
        Slider {
            id: bSlider
            minimum: 0
            maximum: 1
            value: 1.0
            caption: 'B'
        }

    }
//    Text {
//        anchors.top: colorRect.bottom
//        anchors.topMargin: 5
//        anchors.horizontalCenter: colorRect.horizontalCenter
//        horizontalAlignment: Text.AlignHCenter
//        text: "#" + dec2hex(Math.round(rgbaSlider.aValue * 255)) + dec2hex(Math.round(rgbaSlider.rValue * 255)) + dec2hex(Math.round(rgbaSlider.gValue * 255)) + dec2hex(Math.round(rgbaSlider.bValue * 255))
//        font.capitalization: Font.AllUppercase
//        color: "#999999"
//        font.pixelSize: 11
//    }
}
