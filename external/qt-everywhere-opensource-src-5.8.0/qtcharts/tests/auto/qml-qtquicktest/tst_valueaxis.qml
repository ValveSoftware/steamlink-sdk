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
        name: "tst_qml-qtquicktest ValueAxis"
        when: windowShown

        // test functions are run in alphabetical order, the name has 'a' so that it
        // will be the first function to execute.
        function test_a_properties() {
            // Default properties
            verify(axisX.min < 0, "AxisX min");
            verify(axisX.max > 0, "AxisX max");
            verify(axisY.min < 0, "AxisY min");
            verify(axisY.max > 0, "AxisY max");
            verify(axisX.tickCount == 5, "AxisX tick count");
            verify(axisY.tickCount == 5, "AxisY tick count");
            verify(axisX.labelFormat == "", "label format");
            verify(axisX.reverse == false, "AxisX reverse");
            verify(axisY.reverse == false, "AxisY reverse");

            // Modify properties
            axisX.tickCount = 3;
            verify(axisX.tickCount == 3, "set tick count");
            axisX.reverse = true;
            verify(axisX.reverse == true, "AxisX reverse");
            axisX.reverse = false;
            verify(axisX.reverse == false, "AxisX reverse");
        }

        function test_functions() {
            // Set the axis ranges to not "nice" ones...
            var min = 0.032456456;
            var max = 10.67845634;
            axisX.min = min;
            axisX.max = max;
            axisY.min = min;
            axisY.max = max;

            // ...And then apply nice numbers and verify the range was changed
            axisX.applyNiceNumbers();
            axisY.applyNiceNumbers();
            verify(axisX.min != min);
            verify(axisX.max != max);
            verify(axisY.min != min);
            verify(axisY.max != max);
        }

        function test_signals() {
            minChangedSpy.clear();
            maxChangedSpy.clear();
            reverseChangedSpy.clear();

            axisX.min = 2;
            compare(minChangedSpy.count, 1, "onMinChanged");
            compare(maxChangedSpy.count, 0, "onMaxChanged");

            axisX.max = 8;
            compare(minChangedSpy.count, 1, "onMinChanged");
            compare(maxChangedSpy.count, 1, "onMaxChanged");

            axisX.reverse = true;
            compare(reverseChangedSpy.count, 1, "onReverseChanged");

            // restore original values
            axisX.min = 0;
            axisX.max = 10;
            axisX.reverse = false;
            compare(minChangedSpy.count, 2, "onMinChanged");
            compare(maxChangedSpy.count, 2, "onMaxChanged");
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
            }
            axisY: ValueAxis {
                id: axisY
            }
            XYPoint { x: -1; y: -1 }
            XYPoint { x: 0; y: 0 }
            XYPoint { x: 5; y: 5 }
        }

        SignalSpy {
            id: minChangedSpy
            target: axisX
            signalName: "minChanged"
        }
        SignalSpy {
            id: maxChangedSpy
            target: axisX
            signalName: "maxChanged"
        }
        SignalSpy {
            id: reverseChangedSpy
            target: axisX
            signalName: "reverseChanged"
        }
    }
}
