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
import QtQuick.XmlListModel 2.0

Item {
    width: 400
    height: 300
    property int currentIndex: -1

    //![1]
    ChartView {
        id: chartView
        title: "Driver Speeds, lap 1"
        anchors.fill: parent
        legend.alignment: Qt.AlignTop
        animationOptions: ChartView.SeriesAnimations
        antialiasing: true
    }
    //![1]

    //![2]
    // An example XmlListModel containing F1 legend drivers' speeds at speed traps
    SpeedsXml {
        id: speedsXml
        onStatusChanged: {
            if (status == XmlListModel.Ready) {
                timer.start();
            }
        }
    }
    //![2]

    //![3]
    // A timer to mimic refreshing the data dynamically
    Timer {
        id: timer
        interval: 700
        repeat: true
        triggeredOnStart: true
        running: false
        onTriggered: {
            currentIndex++;
            if (currentIndex < speedsXml.count) {
                // Check if there is a series for the data already (we are using driver name to identify series)
                var lineSeries = chartView.series(speedsXml.get(currentIndex).driver);
                if (!lineSeries) {
                    lineSeries = chartView.createSeries(ChartView.SeriesTypeLine, speedsXml.get(currentIndex).driver);
                    chartView.axisY().min = 0;
                    chartView.axisY().max = 250;
                    chartView.axisY().tickCount = 6;
                    chartView.axisY().titleText = "speed (kph)";
                    chartView.axisX().titleText = "speed trap";
                    chartView.axisX().labelFormat = "%.0f";
                }
                lineSeries.append(speedsXml.get(currentIndex).speedTrap, speedsXml.get(currentIndex).speed);

                if (speedsXml.get(currentIndex).speedTrap > 3) {
                    chartView.axisX().max = Number(speedsXml.get(currentIndex).speedTrap) + 1;
                    chartView.axisX().min = chartView.axisX().max - 5;
                } else {
                    chartView.axisX().max = 5;
                    chartView.axisX().min = 0;
                }
                chartView.axisX().tickCount = chartView.axisX().max - chartView.axisX().min + 1;
            } else {
                // No more data, change x-axis range to show all the data
                timer.stop();
                chartView.animationOptions = ChartView.AllAnimations;
                chartView.axisX().min = 0;
                chartView.axisX().max = speedsXml.get(currentIndex - 1).speedTrap;
            }
        }
    }
    //![3]
}
