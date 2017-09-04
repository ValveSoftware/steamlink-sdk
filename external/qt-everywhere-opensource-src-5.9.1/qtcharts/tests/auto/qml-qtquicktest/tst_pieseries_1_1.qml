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
import QtCharts 1.1

Rectangle {
    width: 400
    height: 300

    TestCase {
        id: tc1
        name: "tst_qml-qtquicktest PieSeries 1.1"
        when: windowShown

        function test_properties() {
            compare(pieSeries.endAngle, 360);
            compare(pieSeries.holeSize, 0);
            compare(pieSeries.horizontalPosition, 0.5);
            compare(pieSeries.size, 0.7);
            compare(pieSeries.startAngle, 0);
            compare(pieSeries.sum, 0);
            compare(pieSeries.verticalPosition, 0.5);
        }

        function test_sliceproperties() {
            var slice = pieSeries.append("slice", 10);
            compare(slice.angleSpan, 360.0);
            verify(slice.borderColor != undefined);
            compare(slice.borderWidth, 1);
            verify(slice.color != undefined);
            compare(slice.explodeDistanceFactor, 0.15);
            compare(slice.exploded, false);
            compare(slice.label, "slice");
            compare(slice.labelArmLengthFactor, 0.15);
            verify(slice.labelColor != undefined);
            compare(slice.labelFont.bold, false);
            compare(slice.labelPosition, PieSlice.LabelOutside);
            compare(slice.labelVisible, false);
            compare(slice.percentage, 1.0);
            compare(slice.startAngle, 0.0);
            compare(slice.value, 10.0);
        }

        function test_append() {
            addedSpy.clear();
            countChangedSpy.clear();
            sumChangedSpy.clear();
            var count = 50;
            for (var i = 0; i < count; i++)
                pieSeries.append("slice" + i, Math.random() + 0.01); // Add 0.01 to avoid zero
            compare(addedSpy.count, count);
            compare(countChangedSpy.count, count);
            compare(sumChangedSpy.count, count);
            pieSeries.clear();
        }

        function test_remove() {
            removedSpy.clear();
            countChangedSpy.clear();
            sumChangedSpy.clear();
            var count = 50;
            for (var i = 0; i < count; i++)
                pieSeries.append("slice" + i, Math.random() + 0.01); // Add 0.01 to avoid zero
            for (var j = 0; j < count; j++)
                pieSeries.remove(pieSeries.at(0));
            compare(removedSpy.count, count);
            compare(countChangedSpy.count, 2 * count);
            compare(sumChangedSpy.count, 2 * count);
            compare(pieSeries.count, 0);
        }

        function test_find() {
            var count = 50;
            for (var i = 0; i < count; i++)
                pieSeries.append("slice" + i, Math.random() + 0.01); // Add 0.01 to avoid zero
            for (var j = 0; j < count; j++)
                compare(pieSeries.find("slice" + j).label, "slice" + j);
            pieSeries.clear();
        }
    }

    ChartView {
        id: chartView
        anchors.fill: parent

        PieSeries {
            id: pieSeries
            name: "pie"

            SignalSpy {
                id: addedSpy
                target: pieSeries
                signalName: "added"
            }
            SignalSpy {
                id: removedSpy
                target: pieSeries
                signalName: "removed"
            }
            SignalSpy {
                id: sumChangedSpy
                target: pieSeries
                signalName: "sumChanged"
            }
            SignalSpy {
                id: countChangedSpy
                target: pieSeries
                signalName: "countChanged"
            }
        }
    }
}
