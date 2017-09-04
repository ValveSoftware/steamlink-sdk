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

ChartView {
    id: chartView
    title: "Chart Title"
    anchors.fill: parent
    property variant chart: chartView

    LineSeries {
        name: "line"
        XYPoint { x: 0; y: 0 }
        XYPoint { x: 1.1; y: 2.1 }
        XYPoint { x: 1.9; y: 3.3 }
        XYPoint { x: 2.1; y: 2.1 }
        XYPoint { x: 2.9; y: 4.9 }
        XYPoint { x: 3.4; y: 3.0 }
        XYPoint { x: 4.1; y: 3.3 }
        axisX: axisX
        axisY: axisY
    }

    onVisibleChanged:                  console.log("chart.onVisibleChanged: " + visible);
    onTitleColorChanged:               console.log("chart.onTitleColorChanged: " + color);
    onBackgroundColorChanged:          console.log("chart.onBackgroundColorChanged: " + chart.backgroundColor);
    onDropShadowEnabledChanged:        console.log("chart.onDropShadowEnabledChanged: " + enabled);
    onBackgroundRoundnessChanged:      console.log("chart.onBackgroundRoundnessChanged: " + diameter);
    onSeriesAdded:                     console.log("chart.onSeriesAdded: " + series.name);
    onSeriesRemoved:                   console.log("chart.onSeriesRemoved: " + series.name);
    onPlotAreaColorChanged:            console.log("chart.onPlotAreaColorChanged: " + chart.plotAreaColor);
    onAnimationDurationChanged:        console.log("chart.onAnimationDurationChanged: "
                                                   + chart.animationDuration);
    onAnimationEasingCurveChanged:     console.log("chart.onAnimationEasingCurveChanged: "
                                                   + chart.animationEasingCurve.type);

    legend.onVisibleChanged:           console.log("legend.onVisibleChanged: " + chart.legend.visible);
    legend.onBackgroundVisibleChanged: console.log("legend.onBackgroundVisibleChanged: " + visible);
    legend.onColorChanged:             console.log("legend.onColorChanged: " + color);
    legend.onBorderColorChanged:       console.log("legend.onBorderColorChanged: " + color);
    legend.onLabelColorChanged:        console.log("legend.onLabelColorChanged: " + color);
    legend.onReverseMarkersChanged:    console.log("legend.onReverseMarkersChanged: "
                                                   + chart.legend.reverseMarkers)
    margins.onTopChanged:       console.log("chart.margins.onTopChanged: " + top );
    margins.onBottomChanged:    console.log("chart.margins.onBottomChanged: " + bottom);
    margins.onLeftChanged:      console.log("chart.margins.onLeftChanged: " + left);
    margins.onRightChanged:     console.log("chart.margins.onRightChanged: " + right);
    onPlotAreaChanged: {
        console.log("chart.onPlotAreaChanged, width: " + chartView.plotArea.width
                                                       + " height: " + chartView.plotArea.height
                                                       + " y: " + chartView.plotArea.y
                                                       + " x: " + chartView.plotArea.x);
        marginVisualizer.opacity = 1.0;
    }

    ValueAxis{
        id: axisX
        minorGridVisible: false
        minorTickCount: 2
        onColorChanged:               console.log("axisX.onColorChanged: " + color);
        onLabelsVisibleChanged:       console.log("axisX.onLabelsVisibleChanged: " + visible);
        onLabelsColorChanged:         console.log("axisX.onLabelsColorChanged: " + color);
        onVisibleChanged:             console.log("axisX.onVisibleChanged: " + visible);
        onGridVisibleChanged:         console.log("axisX.onGridVisibleChanged: " + visible);
        onMinorGridVisibleChanged:    console.log("axisX.onMinorGridVisibleChanged: " + visible);
        onGridLineColorChanged:       console.log("axisX.onGridLineColorChanged: " + color);
        onMinorGridLineColorChanged:  console.log("axisX.onMinorGridLineColorChanged: " + color);
        onShadesVisibleChanged:       console.log("axisX.onShadesVisibleChanged: " + visible);
        onShadesColorChanged:         console.log("axisX.onShadesColorChanged: " + color);
        onShadesBorderColorChanged:   console.log("axisX.onShadesBorderColorChanged: " + color);
        onMinChanged:                 console.log("axisX.onMinChanged: " + min);
        onMaxChanged:                 console.log("axisX.onMaxChanged: " + max);
        onReverseChanged:             console.log("axisX.onReverseChanged: " + reverse);
    }

    ValueAxis{
        id: axisY
        minorGridVisible: false
        minorTickCount: 2
        onColorChanged:               console.log("axisY.onColorChanged: " + color);
        onLabelsVisibleChanged:       console.log("axisY.onLabelsVisibleChanged: " + visible);
        onLabelsColorChanged:         console.log("axisY.onLabelsColorChanged: " + color);
        onVisibleChanged:             console.log("axisY.onVisibleChanged: " + visible);
        onGridVisibleChanged:         console.log("axisY.onGridVisibleChanged: " + visible);
        onMinorGridVisibleChanged:    console.log("axisX.onMinorGridVisibleChanged: " + visible);
        onGridLineColorChanged:       console.log("axisX.onGridLineColorChanged: " + color);
        onMinorGridLineColorChanged:  console.log("axisX.onMinorGridLineColorChanged: " + color);
        onShadesVisibleChanged:       console.log("axisY.onShadesVisibleChanged: " + visible);
        onShadesColorChanged:         console.log("axisY.onShadesColorChanged: " + color);
        onShadesBorderColorChanged:   console.log("axisY.onShadesBorderColorChanged: " + color);
        onMinChanged:                 console.log("axisY.onMinChanged: " + min);
        onMaxChanged:                 console.log("axisY.onMaxChanged: " + max);
        onReverseChanged:             console.log("axisY.onReverseChanged: " + reverse);
    }

    Rectangle {
        id: marginVisualizer
        color: "transparent"
        border.color: "red"
        anchors.fill: parent
        anchors.topMargin: chartView.margins.top
        anchors.bottomMargin: chartView.margins.bottom
        anchors.leftMargin: chartView.margins.left
        anchors.rightMargin: chartView.margins.right
        opacity: 0.0
        onOpacityChanged: if (opacity > 0.9) opacity = 0.0;
        Behavior on opacity {
            NumberAnimation { duration: 800 }
        }
    }

    MouseArea {
        id: zoomArea
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        property point mousePoint;
        property point valuePoint;

        onPressed: {
            mousePoint.x = mouse.x
            mousePoint.y = mouse.y
            valuePoint = chart.mapToValue(mousePoint, series("line"));
            // Mouse point and position should be the same!
            console.log("mouse point: " + mouse.x + ", " + mouse.y);
            console.log("value point: " + valuePoint);
            console.log("position: " + chart.mapToPosition(valuePoint, series("line")));
        }
    }
}
