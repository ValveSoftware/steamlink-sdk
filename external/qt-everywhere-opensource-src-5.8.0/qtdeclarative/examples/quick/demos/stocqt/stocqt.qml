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
import QtQml.Models 2.1
import "./content"

Rectangle {
    id: mainRect
    width: 1000
    height: 700

    property alias currentIndex: root.currentIndex

    Rectangle {
        id: banner
        height: 80
        anchors.top: parent.top
        width: parent.width
        color: "#000000"

        Image {
            id: arrow
            source: "./content/images/icon-left-arrow.png"
            anchors.left: banner.left
            anchors.leftMargin: 20
            anchors.verticalCenter: banner.verticalCenter
            visible: root.currentIndex == 1 ? true : false

            MouseArea {
                anchors.fill: parent
                onClicked: root.currentIndex = 0;
            }
        }

        Item {
            id: textItem
            width: stocText.width + qtText.width
            height: stocText.height + qtText.height
            anchors.horizontalCenter: banner.horizontalCenter
            anchors.verticalCenter: banner.verticalCenter

            Text {
                id: stocText
                anchors.verticalCenter: textItem.verticalCenter
                color: "#ffffff"
                font.family: "Abel"
                font.pointSize: 40
                text: "Stoc"
            }
            Text {
                id: qtText
                anchors.verticalCenter: textItem.verticalCenter
                anchors.left: stocText.right
                color: "#5caa15"
                font.family: "Abel"
                font.pointSize: 40
                text: "Qt"
            }
        }
    }

    ListView {
        id: root
        width: parent.width
        anchors.top: banner.bottom
        anchors.bottom: parent.bottom
        snapMode: ListView.SnapOneItem
        highlightRangeMode: ListView.StrictlyEnforceRange
        highlightMoveDuration: 250
        focus: false
        orientation: ListView.Horizontal
        boundsBehavior: Flickable.StopAtBounds

        StockModel {
            id: stock
            stockId: listView.currentStockId
            stockName: listView.currentStockName
            onStockIdChanged: stock.updateStock();
            onDataReady: {
                root.currentIndex = 1
                stockView.update()
            }
        }

        model: ObjectModel {
            StockListView {
                id: listView
                width: root.width
                height: root.height
            }

            StockView {
                id: stockView
                width: root.width
                height: root.height
                stocklist: listView
                stock: stock
            }
        }
    }
}
