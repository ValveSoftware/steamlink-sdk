/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Graphical Effects module.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
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
