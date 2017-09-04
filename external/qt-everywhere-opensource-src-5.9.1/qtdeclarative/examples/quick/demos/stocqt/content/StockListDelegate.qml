/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
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
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
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

import QtQuick 2.5
import QtQuick.Layouts 1.1
import "."

Rectangle {
    height: 102
    width: parent.width
    color: "transparent"
    MouseArea {
        anchors.fill: parent;
        onClicked: {
            if (view.currentIndex == index)
                mainRect.currentIndex = 1;
            else
                view.currentIndex = index;
        }
    }
    GridLayout {
        id: stockGrid
        columns: 3
        rows: 2
        anchors.fill: parent

        Text {
            id: stockIdText
            Layout.alignment: Qt.AlignLeft | Qt.AlignBottom
            Layout.leftMargin: 10
            color: "#000000"
            font.family: Settings.fontFamily
            font.pointSize: 20
            font.weight: Font.Bold
            verticalAlignment: Text.AlignVCenter
            text: stockId
        }

        Text {
            id: stockValueText
            Layout.preferredWidth: 100
            Layout.alignment: Qt.AlignRight | Qt.AlignBottom
            color: "#000000"
            font.family: Settings.fontFamily
            font.pointSize: 20
            font.bold: true
            horizontalAlignment: Text.AlignRight
            verticalAlignment: Text.AlignVCenter
            text: value
        }
        Text {
            id: stockValueChangeText
            Layout.preferredWidth: 135
            Layout.rightMargin: 10
            Layout.alignment: Qt.AlignRight | Qt.AlignBottom
            color: "#328930"
            font.family: Settings.fontFamily
            font.pointSize: 20
            font.bold: true
            horizontalAlignment: Text.AlignRight
            verticalAlignment: Text.AlignVCenter
            text: change
            onTextChanged: {
                if (parseFloat(text) >= 0.0)
                    color = "#328930";
                else
                    color = "#d40000";
            }
        }
        Text {
            id: stockNameText
            Layout.preferredWidth: 300
            Layout.leftMargin: 10
            Layout.alignment: Qt.AlignLeft | Qt.AlignTop
            color: "#000000"
            font.family: Settings.fontFamily
            font.pointSize: 16
            font.bold: false
            elide: Text.ElideRight
            maximumLineCount: 1
            verticalAlignment: Text.AlignVCenter
            text: name
        }

        Item {Layout.fillWidth: true }

        Text {
            id: stockValueChangePercentageText
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignRight | Qt.AlignTop
            Layout.rightMargin: 10
            color: "#328930"
            font.family: Settings.fontFamily
            font.pointSize: 18
            font.bold: false
            horizontalAlignment: Text.AlignRight
            verticalAlignment: Text.AlignVCenter
            text: changePercentage
            onTextChanged: {
                if (parseFloat(text) >= 0.0)
                    color = "#328930";
                else
                    color = "#d40000";
            }
        }
    }

    Rectangle {
        id: endingLine
        anchors.top: stockGrid.bottom
        height: 1
        width: parent.width
        color: "#d7d7d7"
    }
}

