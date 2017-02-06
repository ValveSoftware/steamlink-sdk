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
import QtCharts 2.1

Flow {
    anchors.fill: parent
    property variant chart
    flow: Flow.TopToBottom
    spacing: 5

    Button {
        text: "add line"
        onClicked: addXYSeries(ChartView.SeriesTypeLine, "line", false);
    }
    Button {
        text: "add GL line"
        onClicked: addXYSeries(ChartView.SeriesTypeLine, "GL line", true);
    }
    Button {
        text: "add spline"
        onClicked: addXYSeries(ChartView.SeriesTypeSpline, "spline", false);
    }
    Button {
        text: "add scatter"
        onClicked: addXYSeries(ChartView.SeriesTypeScatter, "scatter", false);
    }
    Button {
        text: "add GL scatter"
        onClicked: addXYSeries(ChartView.SeriesTypeScatter, "GL scatter", true);
    }
    Button {
        text: "remove last"
        onClicked: {
            if (chart.count > 0)
                chart.removeSeries(chart.series(chart.count - 1));
            else
                chart.removeSeries(0);
        }
    }
    Button {
        text: "remove all"
        onClicked: chart.removeAllSeries();
    }

    function addXYSeries(type, name, openGl) {
        var series = chart.createSeries(type, name + " " + chart.count);
        var multiplier = 10;
        if (openGl) {
            series.useOpenGL = true;
            multiplier = 100;
        }
        for (var i = chart.axisX().min * multiplier; i < chart.axisX().max * multiplier; i++) {
            var y = Math.random() * (chart.axisY().max - chart.axisY().min) + chart.axisY().min;
            var x = (Math.random() + i) / multiplier;
            series.append(x, y);
        }
    }
}
