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
import QtCharts 2.0

Rectangle {
    width: 400
    height: 300

    TestCase {
        id: tc1
        name: "tst_qml-qtquicktest ChartView Functions 2.0"
        when: windowShown

        function test_chartViewSeriesAndAxes() {
            // Create XY series
            var line = chartView.createSeries(ChartView.SeriesTypeLine, "line");
            verify(line != null && line != undefined);
            var spline = chartView.createSeries(ChartView.SeriesTypeSpline, "spline");
            verify(spline != null && spline != undefined);
            var scatter = chartView.createSeries(ChartView.SeriesTypeScatter, "scatter");
            verify(scatter != null && scatter != undefined);

            // Create a series with specific axes
            var line2 = chartView.createSeries(ChartView.SeriesTypeLine, "line2", chartView.axisX(line), chartView.axisY(line));

            // Check that all the XY series use the same axes
            verify(chartView.axisX(line) != null);
            verify(chartView.axisY(line) != null);
            compare(chartView.axisX(line), chartView.axisX(line2));
            compare(chartView.axisY(line), chartView.axisY(line2));
            compare(chartView.axisX(line), chartView.axisX(spline));
            compare(chartView.axisY(line), chartView.axisY(spline));
            compare(chartView.axisX(line), chartView.axisX(scatter));
            compare(chartView.axisY(line), chartView.axisY(scatter));

            var bar = chartView.createSeries(ChartView.SeriesTypeBar, "bar");
            verify(bar != null && bar != undefined);
            var stackedbar = chartView.createSeries(ChartView.SeriesTypeStackedBar, "stackedbar");
            verify(stackedbar != null && stackedbar != undefined);
            var percentbar = chartView.createSeries(ChartView.SeriesTypePercentBar, "percentbar");
            verify(percentbar != null && percentbar != undefined);
            var horizontalbar = chartView.createSeries(ChartView.SeriesTypeHorizontalBar, "horizontalbar");
            verify(horizontalbar != null && horizontalbar != undefined);
            var horizontalstackedbar = chartView.createSeries(ChartView.SeriesTypeHorizontalStackedBar, "horizontalstackedbar");
            verify(horizontalstackedbar != null && horizontalstackedbar != undefined);
            var horizontalpercentbar = chartView.createSeries(ChartView.SeriesTypeHorizontalPercentBar, "horizontalpercentbar");
            verify(horizontalpercentbar != null && horizontalpercentbar != undefined);
            var area = chartView.createSeries(ChartView.SeriesTypeArea, "area");
            verify(area != null && area != undefined);

            // Remove all series
            chartView.removeAllSeries();
            compare(chartView.count, 0);
        }

        function test_chartViewRange() {
            // Set initial values
            chartView.createSeries(ChartView.SeriesTypeLine, "line");
            verify(chartView.axisX() != null);
            verify(chartView.axisY() != null);
            chartView.axisX().min = 1.0;
            chartView.axisX().max = 2.0;
            chartView.axisY().min = 1.0;
            chartView.axisY().max = 2.0;

            var xMax = chartView.axisX().max;
            var xMin = chartView.axisX().min;
            var yMax = chartView.axisY().max;
            var yMin = chartView.axisY().min;

            // zoom x 2.5
            chartView.zoom(1.5);
            verify(chartView.axisX().max < xMax);
            verify(chartView.axisX().min > xMin);
            verify(chartView.axisY().max < yMax);
            verify(chartView.axisY().min > yMin);
            xMax = chartView.axisX().max;
            xMin = chartView.axisX().min;
            yMax = chartView.axisY().max;
            yMin = chartView.axisY().min;

            // zoom x 0.5
            chartView.zoom(0.5);
            verify(chartView.axisX().max > xMax);
            verify(chartView.axisX().min < xMin);
            verify(chartView.axisY().max > yMax);
            verify(chartView.axisY().min < yMin);
            xMax = chartView.axisX().max;
            xMin = chartView.axisX().min;
            yMax = chartView.axisY().max;
            yMin = chartView.axisY().min;

            // Scroll up
            chartView.scrollUp(10);
            compare(chartView.axisX().max, xMax);
            compare(chartView.axisX().min, xMin);
            verify(chartView.axisY().max > yMax);
            verify(chartView.axisY().min > yMin);
            xMax = chartView.axisX().max;
            xMin = chartView.axisX().min;
            yMax = chartView.axisY().max;
            yMin = chartView.axisY().min;

            // Scroll down
            chartView.scrollDown(10);
            compare(chartView.axisX().max, xMax);
            compare(chartView.axisX().min, xMin);
            verify(chartView.axisY().max < yMax);
            verify(chartView.axisY().min < yMin);
            xMax = chartView.axisX().max;
            xMin = chartView.axisX().min;
            yMax = chartView.axisY().max;
            yMin = chartView.axisY().min;

            // Scroll left
            chartView.scrollLeft(10);
            verify(chartView.axisX().max < xMax);
            verify(chartView.axisX().min < xMin);
            compare(chartView.axisY().max, yMax);
            compare(chartView.axisY().min, yMin);
            xMax = chartView.axisX().max;
            xMin = chartView.axisX().min;
            yMax = chartView.axisY().max;
            yMin = chartView.axisY().min;

            // Scroll right
            chartView.scrollRight(10);
            verify(chartView.axisX().max > xMax);
            verify(chartView.axisX().min > xMin);
            compare(chartView.axisY().max, yMax);
            compare(chartView.axisY().min, yMin);
        }
    }

    ChartView {
        id: chartView
        anchors.fill: parent
        title: "Chart"
    }
}
