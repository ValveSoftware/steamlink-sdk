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

Rectangle {
    id: main
    width: 800
    height: 600
    property int viewCount: 9
    property variant colors: ["#637D74", "#403D3A", "#8C3B3B", "#AB6937", "#D4A960"]
    property int colorIndex: 0
    property int buttonWidth: 42

    function nextColor() {
        colorIndex++;
        return colors[colorIndex % colors.length];
    }

    Row {
        anchors.top: parent.top
        anchors.bottom: buttonRow.top
        anchors.bottomMargin: 10
        anchors.left: parent.left
        anchors.right: parent.right

        Loader {
            id: chartLoader
            width: main.width - editorLoader.width
            height: parent.height
            source: "Chart.qml"
            onStatusChanged: {
                if (status == Loader.Ready && editorLoader.status == Loader.Ready && chartLoader.item) {
                    if (source.toString().search("/Chart.qml") > 0)
                        editorLoader.item.chart = chartLoader.item.chart;
                    else
                        editorLoader.item.series = chartLoader.item.series;
                }
            }
        }

        Loader {
            id: editorLoader
            width: 280
            height: parent.height
            source: "ChartEditor.qml"
            onStatusChanged: {
                if (status == Loader.Ready && chartLoader.status == Loader.Ready && chartLoader.item) {
                    if (source.toString().search("/ChartEditor.qml") > 0)
                        editorLoader.item.chart = chartLoader.item.chart;
                    else
                        editorLoader.item.series = chartLoader.item.series;
                }
            }
        }
    }

    Row {
        id: buttonRow
        height: 40
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 10

        Button {
            text: "chart"
            width: buttonWidth
            onClicked: {
                chartLoader.source = "Chart.qml";
                editorLoader.source = "ChartEditor.qml";
            }
        }
        Button {
            text: "pie"
            width: buttonWidth
            onClicked: {
                chartLoader.source = "PieChart.qml";
                editorLoader.source = "PieEditor.qml";
            }
        }
        Button {
            text: "line"
            width: buttonWidth
            onClicked: {
                chartLoader.source = "LineChart.qml";
                editorLoader.source = "LineEditor.qml";
            }
        }
        Button {
            text: "spline"
            width: buttonWidth
            onClicked: {
                chartLoader.source = "SplineChart.qml";
                editorLoader.source = "LineEditor.qml";
            }
        }
        Button {
            text: "scatter"
            width: buttonWidth
            onClicked: {
                chartLoader.source = "ScatterChart.qml";
                editorLoader.source = "ScatterEditor.qml";
            }
        }
        Button {
            text: "area"
            width: buttonWidth
            onClicked: {
                chartLoader.source = "AreaChart.qml";
                editorLoader.source = "AreaEditor.qml";
            }
        }
        Button {
            text: "bar"
            width: buttonWidth
            onClicked: {
                chartLoader.source = "BarChart.qml";
                editorLoader.source = "BarEditor.qml";
            }
        }
        Button {
            text: "sbar"
            width: buttonWidth
            onClicked: {
                chartLoader.source = "StackedBarChart.qml";
                editorLoader.source = "BarEditor.qml";
            }
        }
        Button {
            text: "pbar"
            width: buttonWidth
            onClicked: {
                chartLoader.source = "PercentBarChart.qml";
                editorLoader.source = "BarEditor.qml";
            }
        }
        Button {
            text: "hbar"
            width: buttonWidth
            onClicked: {
                chartLoader.source = "HorizontalBarChart.qml";
                editorLoader.source = "BarEditor.qml";
            }
        }
        Button {
            text: "hsbar"
            width: buttonWidth
            onClicked: {
                chartLoader.source = "HorizontalStackedBarChart.qml";
                editorLoader.source = "BarEditor.qml";
            }
        }
        Button {
            text: "hpbar"
            width: buttonWidth
            onClicked: {
                chartLoader.source = "HorizontalPercentBarChart.qml";
                editorLoader.source = "BarEditor.qml";
            }
        }
    }
}
