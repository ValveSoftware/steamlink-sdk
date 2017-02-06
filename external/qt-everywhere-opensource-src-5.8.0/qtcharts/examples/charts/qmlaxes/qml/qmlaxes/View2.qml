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

Item {
    anchors.fill: parent

    //![1]
    ChartView {
        title: "Accurate Historical Data"
        anchors.fill: parent
        legend.visible: false
        antialiasing: true

        LineSeries {
            axisX: DateTimeAxis {
                format: "yyyy MMM"
                tickCount: 5
            }
            axisY: ValueAxis {
                min: 0
                max: 150
            }

            // Please note that month in JavaScript months are zero based, so 2 means March
            XYPoint { x: toMsecsSinceEpoch(new Date(1950, 2, 15)); y: 5 }
            XYPoint { x: toMsecsSinceEpoch(new Date(1970, 0, 1)); y: 50 }
            XYPoint { x: toMsecsSinceEpoch(new Date(1987, 12, 31)); y: 102 }
            XYPoint { x: toMsecsSinceEpoch(new Date(1998, 7, 1)); y: 100 }
            XYPoint { x: toMsecsSinceEpoch(new Date(2012, 8, 2)); y: 110 }
        }
    }

    // DateTimeAxis is based on QDateTimes so we must convert our JavaScript dates to
    // milliseconds since epoch to make them match the DateTimeAxis values
    function toMsecsSinceEpoch(date) {
        var msecs = date.getTime();
        return msecs;
    }
    //![1]
}
