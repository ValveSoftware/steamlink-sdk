/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Charts module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0
import QtTest 1.0
import QtCharts 1.3

Rectangle {
    width: 400
    height: 300

    TestCase {
        id: tc1
        name: "tst_qml-qtquicktest BarSeries 1.3"
        when: windowShown

        function test_properties() {
            compare(barSeries.barWidth, 0.5);
            compare(barSeries.labelsVisible, false);
        }

        function test_axes() {
            verify(chartView.axisX() == barSeries.axisX);
            verify(chartView.axisY() == barSeries.axisY);

            compare(barSeries.axisX, stackedBarSeries.axisX);
            compare(barSeries.axisY, stackedBarSeries.axisY);

            compare(barSeries.axisX, percentBarSeries.axisX);
            compare(barSeries.axisY, percentBarSeries.axisY);
        }

        function test_append() {
            var setCount = 5;
            var valueCount = 50;
            addedSpy.clear();
            append(setCount, valueCount);

            compare(barSeries.count, setCount);
            for (var k = 0; k < setCount; k++) {
                compare(barSeries.at(k).count, valueCount);
                compare(barSeries.at(k).label, "barset" + k);
            }
            compare(addedSpy.count, setCount);

            barSeries.clear();
            compare(barSeries.count, 0);
        }

        function test_insert() {
            var setCount = 5;
            var valueCount = 50;
            addedSpy.clear();
            append(setCount, valueCount);

            for (var i = 0; i < setCount; i++) {
                var values = [];
                for (var j = 0; j < valueCount; j++)
                    values[j] = Math.random() * 10;
                var set = barSeries.insert(i, "barset" + i, values);
                compare(set.label, "barset" + i);
            }

            compare(barSeries.count, setCount * 2);
            for (var k = 0; k < setCount * 2; k++)
                compare(barSeries.at(k).count, valueCount);
            compare(addedSpy.count, 2 * setCount);

            barSeries.clear();
            compare(barSeries.count, 0);
        }

        function test_remove() {
            var setCount = 5;
            var valueCount = 50;
            removedSpy.clear();
            append(setCount, valueCount);

            for (var k = 0; k < setCount; k++)
                barSeries.remove(barSeries.at(0));

            compare(barSeries.count, 0);
            compare(removedSpy.count, setCount);
        }

        // Not a test function, used by one or more test functions
        function append(setCount, valueCount) {
            for (var i = 0; i < setCount; i++) {
                var values = [];
                for (var j = 0; j < valueCount; j++)
                    values[j] = Math.random() * 10;
                barSeries.append("barset" + i, values);
            }
        }
    }

    ChartView {
        id: chartView
        anchors.fill: parent

        BarSeries {
            id: barSeries
            name: "bar"
            axisX: BarCategoryAxis {}
            axisY: ValueAxis { min: 0; max: 10 }

            SignalSpy {
                id: addedSpy
                target: barSeries
                signalName: "barsetsAdded"
            }
            SignalSpy {
                id: removedSpy
                target: barSeries
                signalName: "barsetsRemoved"
            }
        }

        StackedBarSeries {
            id: stackedBarSeries
            name: "stackedBar"
        }

        PercentBarSeries {
            id: percentBarSeries
            name: "percentBar"
        }
    }
}
