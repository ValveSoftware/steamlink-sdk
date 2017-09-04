/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
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

import QtTest 1.0
import QtQuick 2.2
import QtQuick.Controls.Styles 1.4
import "TestUtils.js" as TestUtils

TestCase {
    id: testcase
    name: "Tests_Gauge"
    visible: true
    when: windowShown
    width: 200
    height: 200

    property var gauge

    Rectangle {
        anchors.fill: parent
        color: "black"
    }

    property Component zeroTickmarkWidthGaugeStyle: GaugeStyle {
        tickmark: Item {
            implicitWidth: 0
            implicitHeight: 1
        }
    }

    property Component largeGaugeStyle: GaugeStyle {
        tickmark: Item {
            implicitWidth: 8
            implicitHeight: 4
        }
    }

    function init() {
        gauge = Qt.createQmlObject("import QtQuick.Extras 1.4; Gauge { }", testcase, "");
        verify(gauge, "Gauge: failed to create an instance");
        verify(gauge.__style);
    }

    function cleanup() {
        if (gauge)
            gauge.destroy();
    }

    function test_assignStyle() {
        gauge.style = zeroTickmarkWidthGaugeStyle;
        compare(gauge.__panel.tickmarkLabelBoundsWidth, gauge.__hiddenText.width);
        compare(gauge.__panel.tickmarkLength, 0);
        compare(gauge.__panel.tickmarkWidth, 1);

        // Ensure that our internals are correct after changing to smaller tickmarks.
        gauge.style = largeGaugeStyle;
        compare(gauge.__panel.tickmarkLabelBoundsWidth, gauge.__hiddenText.width + 8);
        compare(gauge.__panel.tickmarkLength, 8);
        compare(gauge.__panel.tickmarkWidth, 4);

        gauge.style = zeroTickmarkWidthGaugeStyle;
        compare(gauge.__panel.tickmarkLabelBoundsWidth, gauge.__hiddenText.width);
        compare(gauge.__panel.tickmarkLength, 0);
        compare(gauge.__panel.tickmarkWidth, 1);
    }

    property Component centeredTickmarkStyle: GaugeStyle {
        tickmark: Item {
            implicitWidth: 10
            implicitHeight: 4

            Rectangle {
                // We must use this as the actual tickmark, because the parent is just spacing.
                objectName: "tickmark" + styleData.index
                x: control.tickmarkAlignment === Qt.AlignLeft
                    || control.tickmarkAlignment === Qt.AlignTop ? parent.implicitWidth : -28
                width: 28
                height: parent.height
            }
        }

        valueBar: Rectangle {
            objectName: "valueBar"
            implicitWidth: 28

            Text {
                text: parent.width
            }
        }
    }

    function test_valueBarPosition_data() {
        return [
            { tag: "AlignLeft", orientation: Qt.Vertical, tickmarkAlignment: Qt.AlignLeft },
            { tag: "AlignRight", orientation: Qt.Vertical, tickmarkAlignment: Qt.AlignRight },
            // TODO: these combinations work, but the tests need to account for them (perhaps due to rotation)
//            { tag: "AlignTop", orientation: Qt.Horizontal, tickmarkAlignment: Qt.AlignTop },
//            { tag: "AlignBottom", orientation: Qt.Horizontal, tickmarkAlignment: Qt.AlignBottom },
        ];
    }

    function test_valueBarPosition(data) {
        gauge.orientation = data.orientation;
        gauge.tickmarkAlignment = data.tickmarkAlignment;
        gauge.value = 1.8;
        gauge.maximumValue = 4;
        gauge.tickmarkStepSize = 1;
        gauge.minorTickmarkCount = 1;
        gauge.width = 50;

        gauge.style = centeredTickmarkStyle;

        var valueBar = TestUtils.findChild(gauge, "valueBar");
        var firstTickmark = TestUtils.findChild(gauge, "tickmark0");
        compare(testcase.mapFromItem(valueBar, 0, 0).x, testcase.mapFromItem(firstTickmark, 0, 0).x);
    }

    property Component tickmarkValueStyle: GaugeStyle {
        tickmark: Rectangle {
            implicitWidth: 10
            implicitHeight: 4
            objectName: "tickmark" + styleData.index

            readonly property real value: styleData.value
            readonly property real valuePosition: styleData.valuePosition
        }

        minorTickmark: Rectangle {
            implicitWidth: 5
            implicitHeight: 2
            objectName: "minorTickmark" + styleData.index

            readonly property real value: styleData.value
            readonly property real valuePosition: styleData.valuePosition
        }
    }

    function test_gaugeTickmarkValues_data() {
        var data = [
            {
                tickmarkStepSize: 1, minorTickmarkCount: 4, minimumValue: 0, maximumValue: 5,
                expectedTickmarkValues: [0, 1, 2, 3, 4, 5],
                expectedMinorTickmarkValues: [0.2, 0.4, 0.6, 0.8, 1.2, 1.4, 1.6, 1.8, 2.2, 2.4,
                    2.6, 2.8, 3.2, 3.4, 3.6, 3.8, 4.2, 4.4, 4.6, 4.8],
                expectedTickmarkValuePositions: [0, 19, 38, 58, 77, 96],
                expectedMinorTickmarkValuePositions: [6, 9, 12, 15, 25, 28, 31, 34, 44, 47, 51, 54, 64, 67, 70, 73, 83, 86, 89, 92]
            },
            {
                tickmarkStepSize: 1, minorTickmarkCount: 1, minimumValue: 0, maximumValue: 5,
                expectedTickmarkValues: [0, 1, 2, 3, 4, 5],
                expectedMinorTickmarkValues: [0.5, 1.5, 2.5, 3.5, 4.5],
                expectedTickmarkValuePositions: [0, 19, 38, 58, 77, 96],
                expectedMinorTickmarkValuePositions: [11, 30, 49, 68, 87]
            },
            {
                tickmarkStepSize: 1, minorTickmarkCount: 1, minimumValue: -1, maximumValue: 1,
                expectedTickmarkValues: [-1, 0, 1],
                expectedMinorTickmarkValues: [-0.5, 0.5],
                expectedTickmarkValuePositions: [0, 48, 96],
                expectedMinorTickmarkValuePositions: [25, 73]
            },
            {
                tickmarkStepSize: 1, minorTickmarkCount: 1, minimumValue: -2, maximumValue: -1,
                expectedTickmarkValues: [-2, -1],
                expectedMinorTickmarkValues: [-1.5],
                expectedTickmarkValuePositions: [0, 96],
                expectedMinorTickmarkValuePositions: [49]
            },
            {
                tickmarkStepSize: 0.5, minorTickmarkCount: 1, minimumValue: 1, maximumValue: 2,
                expectedTickmarkValues: [1, 1.5, 2],
                expectedMinorTickmarkValues: [1.25, 1.75],
                expectedTickmarkValuePositions: [0, 48, 96],
                expectedMinorTickmarkValuePositions: [25, 73]
            },
            {
                tickmarkStepSize: 0.5, minorTickmarkCount: 1, minimumValue: -0.5, maximumValue: 0.5,
                expectedTickmarkValues: [-0.5, 0, 0.5],
                expectedMinorTickmarkValues: [-0.25, 0.25],
                expectedTickmarkValuePositions: [0, 48, 96],
                expectedMinorTickmarkValuePositions: [25, 73]
            }
        ];

        for (var i = 0; i < data.length; ++i) {
            data[i].tag = "tickmarkStepSize=" + data[i].tickmarkStepSize
                + " minorTickmarkCount=" + data[i].minorTickmarkCount
                + " minimumValue=" + data[i].minimumValue
                + " maximumValue=" + data[i].maximumValue;
        }

        return data;
    }

    function gaugeHeightFor100PixelHighValueBar(gauge) {
        return 100 + gauge.__panel.tickmarkOffset * 2 - 2;
    }

    function test_gaugeTickmarkValues(data) {
        gauge.minimumValue = data.minimumValue;
        gauge.maximumValue = data.maximumValue;
        gauge.minorTickmarkCount = data.minorTickmarkCount;
        gauge.tickmarkStepSize = data.tickmarkStepSize;
        gauge.style = tickmarkValueStyle;

        // To make it easier on us, we expect the max length of the value bar to be 100.
        gauge.height = gaugeHeightFor100PixelHighValueBar(gauge);
        compare(gauge.__panel.barLength, 100);

        for (var tickmarkIndex = 0; tickmarkIndex < data.expectedTickmarkValues.length; ++tickmarkIndex) {
            var tickmark = TestUtils.findChild(gauge, "tickmark" + tickmarkIndex);
            // QTBUG-58859: give stuff time to re-layout after the new control height, etc.,
            // otherwise we'll be comparing against incorrect pixel positions.
            tryCompare(tickmark, "value", data.expectedTickmarkValues[tickmarkIndex], undefined,
                "Value of tickmark at index " + tickmarkIndex + " should be " + data.expectedTickmarkValues[tickmarkIndex]);

            var expectedValuePos = data.expectedTickmarkValuePositions[tickmarkIndex];
            tryCompare(tickmark, "valuePosition", expectedValuePos, undefined,
                "Value position of tickmark at index " + tickmarkIndex + " should be " + expectedValuePos);
        }

        for (var minorTickmarkIndex = 0; minorTickmarkIndex < data.expectedMinorTickmarkValues.length; ++minorTickmarkIndex) {
            var minorTickmark = TestUtils.findChild(gauge, "minorTickmark" + minorTickmarkIndex);
            compare(minorTickmark.value, data.expectedMinorTickmarkValues[minorTickmarkIndex],
                "Value of minor tickmark at index " + minorTickmarkIndex + " should be " + data.expectedMinorTickmarkValues[minorTickmarkIndex]);

            expectedValuePos = data.expectedMinorTickmarkValuePositions[minorTickmarkIndex];
            compare(minorTickmark.valuePosition, expectedValuePos,
                "Value position of minor tickmark at index " + minorTickmarkIndex + " should be " + expectedValuePos);
        }
    }

    function test_formatValue() {
        var formatValueCalled = false;
        gauge.formatValue = function(value) { formatValueCalled = true; return value; }
        verify(formatValueCalled);
    }

    function test_valuePosition() {
        gauge.minimumValue = 0;
        gauge.maximumValue = 100;
        gauge.tickmarkStepSize = 10;

        // To make it easier on us, we expect the max length of the value bar to be 100.
        gauge.height = gaugeHeightFor100PixelHighValueBar(gauge);
        compare(gauge.__panel.barLength, 100);

        for (var i = gauge.minimumValue; i < gauge.maximumValue; ++i) {
            compare(gauge.__style.valuePosition, 0);
        }
    }
}
