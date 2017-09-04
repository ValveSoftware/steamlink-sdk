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
import QtCharts 1.4

Rectangle {
    width: 400
    height: 300

    TestCase {
        id: tc1
        name: "tst_qml-qtquicktest BarCategoryAxis 1.4"
        when: windowShown

        function test_minMax() {
            compare(barSeries1.axisX.min, "Jan", "AxisX min");
            compare(barSeries1.axisX.max, "Jun", "AxisX max");
            compare(barSeries1.axisY.min, 0, "AxisY min");
            compare(barSeries1.axisY.max, 10, "AxisY max");
        }

        function test_categories() {
            compare(barSeries1.axisX.count, 6, "AxisX count");
            categoriesCountChangedSpy.clear();
            categoriesChangedSpy.clear();

            // Replace categories
            barSeries1.axisX.categories = ["Tam", "Hel", "Maa", "Huh"];
            compare(barSeries1.axisX.count, 4, "AxisX count");
            compare(categoriesCountChangedSpy.count, 1, "onCountChanged");
            compare(categoriesChangedSpy.count, 1, "onCategoriesChanged");

            // Reset the original categories
            barSeries1.axisX.categories = ["Jan", "Feb", "Mar", "Apr", "May", "Jun"]
            compare(barSeries1.axisX.count, 6, "AxisX count");
            compare(categoriesCountChangedSpy.count, 2, "onCountChanged");
            compare(categoriesChangedSpy.count, 2, "onCategoriesChanged");
        }

        function test_minMaxChanged() {
            axisY.min = -1;
            compare(minChangedSpy.count, 1, "onMinChanged");
            compare(maxChangedSpy.count, 0, "onMaxChanged");
            axisY.max = 12;
            compare(minChangedSpy.count, 1, "onMinChanged");
            compare(maxChangedSpy.count, 1, "onMaxChanged");
        }

        function test_seriesAxisClear() {
            verify(barSeries1.axisX.min !== "", "barSeries1.axisX.min");
            verify(barSeries1.axisX.max !== "", "barSeries1.axisX.max");
            verify(barSeries1.axisX.count !== 0, "barSeries1.axisX.count"); // category count
            barSeries1.axisX.clear();
            verify(barSeries1.axisX.min === "", "barSeries1.axisX.min");
            verify(barSeries1.axisX.max === "", "barSeries1.axisX.max");
            verify(barSeries1.axisX.count === 0);
        }

    }

    ChartView {
        id: chartView
        anchors.fill: parent

        BarSeries {
            id: barSeries1
            axisX: BarCategoryAxis {
                id: axisX
                categories: ["Jan", "Feb", "Mar", "Apr", "May", "Jun" ]
            }
            axisY: ValueAxis {
                id: axisY
                min: 0
                max: 10
            }
        }

        SignalSpy {
            id: categoriesCountChangedSpy
            target: axisX
            signalName: "countChanged"
        }
        SignalSpy {
            id: categoriesChangedSpy
            target: axisX
            signalName: "categoriesChanged"
        }
        SignalSpy {
            id: minChangedSpy
            target: axisY
            signalName: "minChanged"
        }
        SignalSpy {
            id: maxChangedSpy
            target: axisY
            signalName: "maxChanged"
        }
    }
}
