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
import QtQuick.Window 2.1
import QtQuick.Layouts 1.1

Rectangle {
    id: root
    width: 320
    height: 480
    color: "transparent"

    property var stock: null
    property var stocklist: null
    signal settingsClicked

    function update() {
        chart.update()
    }

    Rectangle {
        id: mainRect
        color: "transparent"
        anchors.fill: parent

        GridLayout {
            anchors.fill: parent
            rows: 2
            columns: Screen.primaryOrientation === Qt.PortraitOrientation ? 1 : 2

            StockInfo {
                id: stockInfo
                Layout.alignment: Qt.AlignTop
                Layout.preferredWidth: 400
                Layout.preferredHeight: 160
                stock: root.stock
            }

            StockChart {
                id: chart
                Layout.alignment: Qt.AlignRight
                Layout.margins: 5
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.rowSpan: 2
                stockModel: root.stock
                settings: settingsPanel
            }
            StockSettingsPanel {
                id: settingsPanel
                Layout.alignment: Qt.AlignBottom
                Layout.fillHeight: true
                Layout.preferredWidth: 400
                Layout.bottomMargin: 5
                onDrawOpenPriceChanged: root.update()
                onDrawClosePriceChanged: root.update();
                onDrawHighPriceChanged: root.update();
                onDrawLowPriceChanged: root.update();
            }
        }
    }
}
