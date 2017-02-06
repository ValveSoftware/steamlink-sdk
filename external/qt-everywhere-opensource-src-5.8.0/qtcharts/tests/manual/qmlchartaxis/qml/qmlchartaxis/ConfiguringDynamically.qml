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
import QtCharts 2.0

ChartView {
    title: "configuring dynamically"
    id: chartView
    property int index: 0

    Timer {
        interval: 1000
        repeat: true
        running: true
        onTriggered: {
            switch (index) {
            case 0:
                lineSeries.axisX.max = 6;
                lineSeries.axisY.max = 6;
                break;
            case 1:
                scatterSeries.axisX.max = 10;
                scatterSeries.axisY.max = 10;
                break;
            default:
                chartView.axisX().max = 4;
                chartView.axisY().max = 4;
            }
            index = (index + 1) % 3;
        }
    }

    LineSeries {
        id: lineSeries
        name: "line series"
        XYPoint { x: 0; y: 0 }
        XYPoint { x: 1; y: 1 }
        XYPoint { x: 2; y: 2 }
        XYPoint { x: 3; y: 3 }
        XYPoint { x: 4; y: 4 }
    }

    ScatterSeries {
        id: scatterSeries
        name: "scatter series"
        XYPoint { x: 0; y: 0 }
        XYPoint { x: 0.5; y: 1 }
        XYPoint { x: 1; y: 2 }
        XYPoint { x: 1.5; y: 3 }
        XYPoint { x: 2; y: 4 }
        XYPoint { x: 1; y: 1 }
        XYPoint { x: 2; y: 2 }
        XYPoint { x: 3; y: 3 }
        XYPoint { x: 4; y: 4 }
    }
}
