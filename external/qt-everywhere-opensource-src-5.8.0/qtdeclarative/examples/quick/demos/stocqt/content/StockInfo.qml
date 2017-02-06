/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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

import QtQuick 2.0
import "."

Rectangle {
    id: root
    width: 440
    height: 160
    color: "transparent"

    property var stock: null

    Column {
        id: stockColumn
        anchors.fill: parent
        spacing: 4

        Flow {
            anchors { left: parent.left; right: parent.right }
            spacing: 12

            Text {
                id: stockIdText
                color: "#000000"
                font.family: Settings.fontFamily
                font.pointSize: 28
                font.weight: Font.DemiBold
                text: root.stock.stockId
            }

            Text {
                id: price
                color: "#6d6d6d"
                font.family: Settings.fontFamily
                font.pointSize: 28
                font.weight: Font.DemiBold
                text: parseFloat(root.stock.stockPrice).toFixed(2);
            }
        }

        Text {
            id: stockNameText
            color: "#0c0c0c"
            font.family: Settings.fontFamily
            font.pointSize: 16
            width: stockColumn.width
            elide: Text.ElideRight
            maximumLineCount: 3
            wrapMode: Text.WordWrap
            text: root.stock.stockName
        }

        Flow {
            anchors { left: parent.left; right: parent.right }
            spacing: 12

            Text {
                id: priceChange
                horizontalAlignment: Text.AlignRight
                color: root.stock.stockPriceChanged < 0 ? "#d40000" : "#328930"
                font.family: Settings.fontFamily
                font.pointSize: 18
                text: parseFloat(root.stock.stockPriceChanged).toFixed(2);
            }

            Text {
                id: priceChangePercentage
                horizontalAlignment: Text.AlignRight
                color: root.stock.stockPriceChanged < 0 ? "#d40000" : "#328930"
                font.family: Settings.fontFamily
                font.pointSize: 18
                font.weight: Font.DemiBold
                text: "(" +
                      parseFloat(root.stock.stockPriceChanged /
                          (root.stock.stockPrice - root.stock.stockPriceChanged) * 100.0).toFixed(2) +
                      "%)"
            }
        }
    }
}
