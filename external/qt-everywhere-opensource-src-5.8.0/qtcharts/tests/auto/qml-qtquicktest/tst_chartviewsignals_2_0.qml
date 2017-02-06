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
        name: "tst_qml-qtquicktest ChartView Signals 2.0"
        when: windowShown

        // Verify onSeriesAdded and onSeriesRemoved signals
        function test_chartView() {
            var series = chartView.createSeries(ChartView.SeriesTypeLine, "line");
            seriesAddedSpy.wait();
            compare(seriesAddedSpy.count, 1, "ChartView.onSeriesAdded");

            // Modifying layout triggers more than one plotAreaChanged signal
            chartView.titleFont.pixelSize = 50;
            verify(plotAreaChangedSpy.count > 0, "ChartView.onPlotAreaChanged");

            chartView.removeSeries(series);
            seriesRemovedSpy.wait();
            compare(seriesRemovedSpy.count, 1, "ChartView.onSeriesAdded");
        }
    }

    ChartView {
        id: chartView
        anchors.fill: parent
        title: "Chart"

        SignalSpy {
            id: plotAreaChangedSpy
            target: chartView
            signalName: "plotAreaChanged"
        }

        SignalSpy {
            id: seriesAddedSpy
            target: chartView
            signalName: "seriesAdded"
        }

        SignalSpy {
            id: seriesRemovedSpy
            target: chartView
            signalName: "seriesRemoved"
        }
    }
}
