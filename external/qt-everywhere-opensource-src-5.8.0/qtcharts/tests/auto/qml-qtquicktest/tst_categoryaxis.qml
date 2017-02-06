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
import QtCharts 2.1

Rectangle {
    width: 400
    height: 300

    TestCase {
        id: tc1
        name: "tst_qml-qtquicktest CategoryAxis"
        when: windowShown

        function test_minMax() {
            compare(lineSeries1.axisX.min, 0, "AxisX min");
            compare(lineSeries1.axisX.max, 10, "AxisX max");
            compare(lineSeries1.axisY.min, 0, "AxisY min");
            compare(lineSeries1.axisY.max, 10, "AxisY max");
        }

        function test_categories() {
            compare(lineSeries1.axisY.startValue, 0, "AxisY start value");
            compare(lineSeries1.axisY.count, 3, "AxisY count");
            compare(lineSeries1.axisY.categoriesLabels[0], "label0", "AxisY categories labels");
            compare(lineSeries1.axisY.categoriesLabels[1], "label1", "AxisY categories labels");
            compare(lineSeries1.axisY.categoriesLabels[2], "label2", "AxisY categories labels");
        }

        function test_properties() {
            compare(lineSeries1.axisY.labelsPosition, CategoryAxis.AxisLabelsPositionCenter);
            verify(lineSeries1.axisX.reverse == false, "AxisX reverse");
            verify(lineSeries1.axisY.reverse == false, "AxisY reverse");

            // Modify properties
            lineSeries1.axisX.reverse = true;
            verify(lineSeries1.axisX.reverse == true, "AxisX reverse");
            lineSeries1.axisX.reverse = false;
            verify(lineSeries1.axisX.reverse == false, "AxisX reverse");
        }

        function test_signals() {
            axisLabelsPositionSpy.clear();
            reverseChangedSpy.clear();
            lineSeries1.axisY.labelsPosition = CategoryAxis.AxisLabelsPositionOnValue;
            compare(axisLabelsPositionSpy.count, 1, "onLabelsPositionChanged");

            lineSeries1.axisX.reverse = true;
            compare(reverseChangedSpy.count, 1, "onReverseChanged");

            // restore original values
            lineSeries1.axisX.reverse = false;
            compare(reverseChangedSpy.count, 2, "onReverseChanged");
        }
    }

    ChartView {
        id: chartView
        anchors.fill: parent

        LineSeries {
            id: lineSeries1
            axisX: ValueAxis {
                id: axisX
                min: 0
                max: 10
            }
            axisY: CategoryAxis {
                id: axisY
                min: 0
                max: 10
                startValue: 0
                CategoryRange {
                    label: "label0"
                    endValue: 1
                }
                CategoryRange {
                    label: "label1"
                    endValue: 3
                }
                CategoryRange {
                    label: "label2"
                    endValue: 10
                }
                SignalSpy {
                    id: axisLabelsPositionSpy
                    target: axisY
                    signalName: "labelsPositionChanged"
                }
            }
            XYPoint { x: -1; y: -1 }
            XYPoint { x: 0; y: 0 }
            XYPoint { x: 5; y: 5 }
        }
        SignalSpy {
            id: reverseChangedSpy
            target: axisX
            signalName: "reverseChanged"
        }
    }
}
