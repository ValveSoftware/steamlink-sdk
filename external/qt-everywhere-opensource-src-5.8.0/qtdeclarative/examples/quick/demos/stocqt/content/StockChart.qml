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
    id: chart
    width: 320
    height: 200

    property var stockModel: null
    property var startDate: new Date()
    property var endDate: new Date()
    property string activeChart: "year"
    property var settings
    property int gridSize: 4
    property real gridStep: gridSize ? (width - canvas.tickMargin) / gridSize : canvas.xGridStep

    function update() {
        endDate = new Date();
        if (chart.activeChart === "year") {
            chart.startDate = new Date(chart.endDate.getFullYear() - 1,
                                       chart.endDate.getMonth(),
                                       chart.endDate.getDate());
            chart.gridSize = 12;
        }
        else if (chart.activeChart === "month") {
            chart.startDate = new Date(chart.endDate.getFullYear(),
                                       chart.endDate.getMonth() - 1,
                                       chart.endDate.getDate());
            gridSize = 0;
        }
        else if (chart.activeChart === "week") {
            chart.startDate = new Date(chart.endDate.getFullYear(),
                                       chart.endDate.getMonth(),
                                       chart.endDate.getDate() - 7);
            gridSize = 0;
        }
        else {
            chart.startDate = new Date(2005, 3, 25);
            gridSize = 4;
        }

        canvas.requestPaint();
    }

    Row {
        id: activeChartRow
        anchors.left: chart.left
        anchors.right: chart.right
        anchors.top: chart.top
        anchors.topMargin: 4
        spacing: 52
        onWidthChanged: {
            var buttonsLen = maxButton.width + yearButton.width + monthButton.width + weekButton.width;
            var space = (width - buttonsLen) / 3;
            spacing = Math.max(space, 10);
        }

        Button {
            id: maxButton
            text: "Max"
            buttonEnabled: chart.activeChart === "max"
            onClicked: {
                chart.activeChart = "max";
                chart.update();
            }
        }
        Button {
            id: yearButton
            text: "Year"
            buttonEnabled: chart.activeChart === "year"
            onClicked: {
                chart.activeChart = "year";
                chart.update();
            }
        }
        Button {
            id: monthButton
            text: "Month"
            buttonEnabled: chart.activeChart === "month"
            onClicked: {
                chart.activeChart = "month";
                chart.update();
            }
        }
        Button {
            id: weekButton
            text: "Week"
            buttonEnabled: chart.activeChart === "week"
            onClicked: {
                chart.activeChart = "week";
                chart.update();
            }
        }
    }

    Text {
        id: fromDate
        color: "#000000"
        font.family: Settings.fontFamily
        font.pointSize: 8
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        text: "| " + startDate.toDateString()
    }

    Text {
        id: toDate
        color: "#000000"
        font.family: Settings.fontFamily
        font.pointSize: 8
        anchors.right: parent.right
        anchors.rightMargin: canvas.tickMargin
        anchors.bottom: parent.bottom
        text: endDate.toDateString() + " |"
    }

    Canvas {
        id: canvas

        // Uncomment below lines to use OpenGL hardware accelerated rendering.
        // See Canvas documentation for available options.
        // renderTarget: Canvas.FramebufferObject
        // renderStrategy: Canvas.Threaded

        anchors.top: activeChartRow.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: fromDate.top

        property int pixelSkip: 1
        property int numPoints: 1
        property int tickMargin: 34

        property real xGridStep: (width - tickMargin) / numPoints
        property real yGridOffset: height / 26
        property real yGridStep: height / 12

        function drawBackground(ctx) {
            ctx.save();
            ctx.fillStyle = "#ffffff";
            ctx.fillRect(0, 0, canvas.width, canvas.height);
            ctx.strokeStyle = "#d7d7d7";
            ctx.beginPath();
            // Horizontal grid lines
            for (var i = 0; i < 12; i++) {
                ctx.moveTo(0, canvas.yGridOffset + i * canvas.yGridStep);
                ctx.lineTo(canvas.width, canvas.yGridOffset + i * canvas.yGridStep);
            }

            // Vertical grid lines
            var height = 35 * canvas.height / 36;
            var yOffset = canvas.height - height;
            var xOffset = 0;
            for (i = 0; i < chart.gridSize; i++) {
                ctx.moveTo(xOffset + i * chart.gridStep, yOffset);
                ctx.lineTo(xOffset + i * chart.gridStep, height);
            }
            ctx.stroke();

            // Right ticks
            ctx.strokeStyle = "#666666";
            ctx.beginPath();
            var xStart = canvas.width - tickMargin;
            ctx.moveTo(xStart, 0);
            ctx.lineTo(xStart, canvas.height);
            for (i = 0; i < 12; i++) {
                ctx.moveTo(xStart, canvas.yGridOffset + i * canvas.yGridStep);
                ctx.lineTo(canvas.width, canvas.yGridOffset + i * canvas.yGridStep);
            }
            ctx.moveTo(0, canvas.yGridOffset + 9 * canvas.yGridStep);
            ctx.lineTo(canvas.width, canvas.yGridOffset + 9 * canvas.yGridStep);
            ctx.closePath();
            ctx.stroke();

            ctx.restore();
        }

        // Returns a shortened, readable version of the potentially
        // large volume number.
        function volumeToString(value) {
            if (value < 1000)
                return value;
            var exponent = parseInt(Math.log(value) / Math.log(1000));
            var shortVal = parseFloat(parseFloat(value) / Math.pow(1000, exponent)).toFixed(1);

            // Drop the decimal point on 3-digit values to make it fit
            if (shortVal >= 100.0) {
                shortVal = parseFloat(shortVal).toFixed(0);
            }
            return shortVal + "KMBTG".charAt(exponent - 1);
        }

        function drawScales(ctx, high, low, vol)
        {
            ctx.save();
            ctx.strokeStyle = "#888888";
            ctx.font = "10px Open Sans"
            ctx.beginPath();

            // prices on y-axis
            var x = canvas.width - tickMargin + 3;
            var priceStep = (high - low) / 9.0;
            for (var i = 0; i < 10; i += 2) {
                var price = parseFloat(high - i * priceStep).toFixed(1);
                ctx.text(price, x, canvas.yGridOffset + i * yGridStep - 2);
            }

            // volume scale
            for (i = 0; i < 3; i++) {
                var volume = volumeToString(vol - (i * (vol/3)));
                ctx.text(volume, x, canvas.yGridOffset + (i + 9) * yGridStep + 10);
            }

            ctx.closePath();
            ctx.stroke();
            ctx.restore();
        }

        function drawPrice(ctx, from, to, color, price, points, highest, lowest)
        {
            ctx.save();
            ctx.globalAlpha = 0.7;
            ctx.strokeStyle = color;
            ctx.lineWidth = 3;
            ctx.beginPath();

            var end = points.length;

            var range = highest - lowest;
            if (range == 0) {
                range = 1;
            }

            for (var i = 0; i < end; i += pixelSkip) {
                var x = points[i].x;
                var y = points[i][price];
                var h = 9 * yGridStep;

                y = h * (lowest - y)/range + h + yGridOffset;

                if (i == 0) {
                    ctx.moveTo(x, y);
                } else {
                    ctx.lineTo(x, y);
                }
            }
            ctx.stroke();
            ctx.restore();
        }

        function drawVolume(ctx, from, to, color, price, points, highest)
        {
            ctx.save();
            ctx.fillStyle = color;
            ctx.globalAlpha = 0.8;
            ctx.lineWidth = 0;
            ctx.beginPath();

            var end = points.length;
            var margin = 0;

            if (chart.activeChart === "month" || chart.activeChart === "week") {
                margin = 8;
                ctx.shadowOffsetX = 4;
                ctx.shadowBlur = 3.5;
                ctx.shadowColor = Qt.darker(color);
            }

            // To match the volume graph with price grid, skip drawing the initial
            // volume of the first day on chart.
            for (var i = 1; i < end; i += pixelSkip) {
                var x = points[i - 1].x;
                var y = points[i][price];
                y = canvas.height * (y / highest);
                y = 3 * y / 12;
                ctx.fillRect(x, canvas.height - y + yGridOffset,
                             canvas.xGridStep - margin, y);
            }

            ctx.stroke();
            ctx.restore();
        }

        function drawError(ctx, msg)
        {
            ctx.save();
            ctx.strokeStyle = "#888888";
            ctx.font = "24px Open Sans"
            ctx.textAlign = "center"
            ctx.shadowOffsetX = 4;
            ctx.shadowOffsetY = 4;
            ctx.shadowBlur = 1.5;
            ctx.shadowColor = "#aaaaaa";
            ctx.beginPath();

            ctx.fillText(msg, (canvas.width - tickMargin) / 2,
                              (canvas.height - yGridOffset - yGridStep) / 2);

            ctx.closePath();
            ctx.stroke();
            ctx.restore();
        }

        onPaint: {
            numPoints = stockModel.indexOf(chart.startDate);

            if (chart.gridSize == 0)
                chart.gridSize = numPoints

            var ctx = canvas.getContext("2d");
            ctx.globalCompositeOperation = "source-over";
            ctx.lineWidth = 1;

            drawBackground(ctx);

            if (!stockModel.ready) {
                drawError(ctx, "No data available.");
                return;
            }

            var highestPrice = 0;
            var highestVolume = 0;
            var lowestPrice = -1;
            var points = [];
            for (var i = numPoints, j = 0; i >= 0 ; i -= pixelSkip, j += pixelSkip) {
                var price = stockModel.get(i);
                if (parseFloat(highestPrice) < parseFloat(price.high))
                    highestPrice = price.high;
                if (parseInt(highestVolume, 10) < parseInt(price.volume, 10))
                    highestVolume = price.volume;
                if (lowestPrice < 0 || parseFloat(lowestPrice) > parseFloat(price.low))
                    lowestPrice = price.low;
                points.push({
                                x: j * xGridStep,
                                open: price.open,
                                close: price.close,
                                high: price.high,
                                low: price.low,
                                volume: price.volume
                            });
            }

            if (settings.drawHighPrice)
                drawPrice(ctx, 0, numPoints, settings.highColor, "high", points, highestPrice, lowestPrice);
            if (settings.drawLowPrice)
                drawPrice(ctx, 0, numPoints, settings.lowColor, "low", points, highestPrice, lowestPrice);
            if (settings.drawOpenPrice)
                drawPrice(ctx, 0, numPoints,settings.openColor, "open", points, highestPrice, lowestPrice);
            if (settings.drawClosePrice)
                drawPrice(ctx, 0, numPoints, settings.closeColor, "close", points, highestPrice, lowestPrice);

            drawVolume(ctx, 0, numPoints, settings.volumeColor, "volume", points, highestVolume);
            drawScales(ctx, highestPrice, lowestPrice, highestVolume);
        }
    }
}
