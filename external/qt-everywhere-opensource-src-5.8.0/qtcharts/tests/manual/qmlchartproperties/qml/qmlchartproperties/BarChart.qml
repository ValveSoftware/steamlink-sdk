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
    title: "Bar series"
    anchors.fill: parent
    theme: ChartView.ChartThemeLight
    legend.alignment: Qt.AlignBottom
    animationOptions: ChartView.SeriesAnimations

    property variant series: mySeries


    BarSeries {
        id: mySeries
        name: "bar"
        labelsFormat: "@value";
        axisX: BarCategoryAxis { categories: ["2007", "2008", "2009", "2010", "2011", "2012" ] }
        BarSet { label: "Bob"; values: [2, 2, 3, 4, 5, 6]
            onClicked:                  console.log("barset.onClicked: " + index);
            onHovered:                  console.log("barset.onHovered: " + status + " " + index);
            onPenChanged:               console.log("barset.onPenChanged: " + pen);
            onBrushChanged:             console.log("barset.onBrushChanged: " + brush);
            onLabelChanged:             console.log("barset.onLabelChanged: " + label);
            onLabelBrushChanged:        console.log("barset.onLabelBrushChanged: " + labelBrush);
            onLabelFontChanged:         console.log("barset.onLabelFontChanged: " + labelFont);
            onColorChanged:             console.log("barset.onColorChanged: " + color);
            onBorderColorChanged:       console.log("barset.onBorderColorChanged: " + color);
            onLabelColorChanged:        console.log("barset.onLabelColorChanged: " + color);
            onCountChanged:             console.log("barset.onCountChanged: " + count);
            onValuesAdded:              console.log("barset.onValuesAdded: " + index + ", " + count);
            onValuesRemoved:            console.log("barset.onValuesRemoved: " + index + ", " + count);
            onValueChanged:             console.log("barset.onValuesChanged: " + index);
            onPressed:                  console.log("barset.onPressed: " + index);
            onReleased:                 console.log("barset.onReleased: " + index);
            onDoubleClicked:            console.log("barset.onDoubleClicked: " + index);

        }
        BarSet { label: "Susan"; values: [5, 1, 2, 4, 1, 7] }
        BarSet { label: "James"; values: [3, 5, 8, 13, 5, 8] }

        onNameChanged:              console.log("barSeries.onNameChanged: " + series.name);
        onVisibleChanged:           console.log("barSeries.onVisibleChanged: " + series.visible);
        onOpacityChanged:           console.log("barSeries.onOpacityChanged: " + opacity);
        onClicked:                  console.log("barSeries.onClicked: " + barset + " " + index);
        onHovered:                  console.log("barSeries.onHovered: " + barset + " " + status
                                                + " " + index);
        onLabelsVisibleChanged:     console.log("barSeries.onLabelsVisibleChanged: " + series.labelsVisible);
        onCountChanged:             console.log("barSeries.onCountChanged: " + count);
        onLabelsFormatChanged:      console.log("barSeries.onLabelsFormatChanged: " + format);
        onLabelsPositionChanged:    console.log("barSeries.onLabelsPositionChanged: " + series.labelsPosition);
        onPressed:              console.log("barSeries.onPressed: " + barset + " " + index);
        onReleased:             console.log("barSeries.onReleased: " + barset + " " + index);
        onDoubleClicked:        console.log("barSeries.onDoubleClicked: " + barset + " " + index);

        function changeLabelsPosition() {
            if (labelsPosition === BarSeries.LabelsCenter)
                labelsPosition = BarSeries.LabelsInsideEnd;
            else
                labelsPosition = BarSeries.LabelsCenter;
        }
    }
}
