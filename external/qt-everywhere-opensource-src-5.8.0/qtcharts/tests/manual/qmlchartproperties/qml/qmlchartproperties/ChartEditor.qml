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

Item {
    id: chartEditor
    property variant chart
    onChartChanged: {
        if (loader.item != undefined)
            loader.item.chart = chart;
    }

    function selectButton(button) {
        chartButton.color = "#79bd8f";
        titleButton.color = "#79bd8f";
        legendButton.color = "#79bd8f";
        axisXButton.color = "#79bd8f";
        axisYButton.color = "#79bd8f";
        seriesButton.color = "#79bd8f";
        button.color = "#00a388";
    }

    Flow {
        id: selectorFlow
        anchors.top: parent.top
        height: 90
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 5
        flow: Flow.TopToBottom

        Button {
            id: chartButton
            text: "Chart properties"
            unpressedColor: "#79bd8f"
            onClicked: {
                selectButton(chartButton);
                loader.source = "ChartEditorProperties.qml";
                loader.item.chart = chart;
            }
        }
        Button {
            id: titleButton
            text: "Title properties"
            unpressedColor: "#79bd8f"
            onClicked: {
                selectButton(titleButton);
                loader.source = "ChartEditorTitle.qml";
                loader.item.chart = chart;
            }
        }
        Button {
            id: legendButton
            text: "Legend properties"
            unpressedColor: "#79bd8f"
            onClicked: {
                selectButton(legendButton);
                loader.source = "ChartEditorLegend.qml";
                loader.item.chartLegend = chart.legend;
            }
        }
        Button {
            id: axisXButton
            text: "Axis X properties"
            unpressedColor: "#79bd8f"
            onClicked: {
                selectButton(axisXButton);
                loader.source = "ChartEditorAxis.qml";
                loader.item.axis = chart.axisX();
            }
        }
        Button {
            id: axisYButton
            text: "Axis Y properties"
            unpressedColor: "#79bd8f"
            onClicked: {
                selectButton(axisYButton);
                loader.source = "ChartEditorAxis.qml";
                loader.item.axis = chart.axisY();
            }
        }
        Button {
            id: seriesButton
            text: "Series"
            unpressedColor: "#79bd8f"
            onClicked: {
                selectButton(seriesButton);
                loader.source = "ChartEditorSeries.qml";
                loader.item.chart = chart;
            }
        }
    }

    Loader {
        id: loader
        anchors.top: selectorFlow.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
    }
}
