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
    title: "area series"
    anchors.fill: parent
    animationOptions: ChartView.SeriesAnimations

    property variant series: areaSeries

    AreaSeries {
        id: areaSeries
        name: "area 1"

        upperSeries: LineSeries {
            XYPoint { x: 0; y: 1 }
            XYPoint { x: 1; y: 1 }
            XYPoint { x: 2; y: 3 }
            XYPoint { x: 3; y: 3 }
            XYPoint { x: 4; y: 2 }
            XYPoint { x: 5; y: 0 }
            XYPoint { x: 6; y: 2 }
            XYPoint { x: 7; y: 1 }
            XYPoint { x: 8; y: 2 }
            XYPoint { x: 9; y: 1 }
            XYPoint { x: 10; y: 3 }
            XYPoint { x: 11; y: 3 }
        }
        lowerSeries: LineSeries {
            XYPoint { x: 0; y: 0 }
            XYPoint { x: 1; y: 0 }
            XYPoint { x: 2; y: 0 }
            XYPoint { x: 3; y: 0 }
            XYPoint { x: 4; y: 0 }
            XYPoint { x: 5; y: 0 }
            XYPoint { x: 6; y: 0 }
            XYPoint { x: 7; y: 0 }
            XYPoint { x: 8; y: 0 }
            XYPoint { x: 9; y: 0 }
            XYPoint { x: 10; y: 0 }
            XYPoint { x: 11; y: 0 }
        }

        pointLabelsFormat: "@xPoint, @yPoint";

        onNameChanged:              console.log(name + ".onNameChanged: " + name);
        onVisibleChanged:           console.log(name + ".onVisibleChanged: " + visible);
        onOpacityChanged:           console.log(name + ".onOpacityChanged: " + opacity);
        onClicked:                  console.log(name + ".onClicked: " + point.x + ", " + point.y);
        onSelected:                 console.log(name + ".onSelected");
        onColorChanged:             console.log(name + ".onColorChanged: " + color);
        onBorderColorChanged:       console.log(name + ".onBorderColorChanged: " + borderColor);
        onBorderWidthChanged:       console.log(name + ".onBorderChanged: " + borderWidth);
//        onCountChanged:             console.log(name + ".onCountChanged: " + count);
        onHovered:                  console.log("lineSeries.onHovered:" + point.x + "," + point.y + " " + state);
        onPointLabelsVisibilityChanged:  console.log(name + ".onPointLabelsVisibilityChanged: "
                                                     + visible);
        onPointLabelsFormatChanged:      console.log(name + ".onPointLabelsFormatChanged: "
                                                     + format);
        onPointLabelsFontChanged:        console.log(name + ".onPointLabelsFontChanged: "
                                                     + font.family);
        onPointLabelsColorChanged:       console.log(name + ".onPointLabelsColorChanged: "
                                                     + color);
        onPointLabelsClippingChanged:    console.log(name + ".onPointLabelsClippingChanged: "
                                                     + clipping);
        onPressed:          console.log(name + ".onPressed: " + point.x + ", " + point.y);
        onReleased:         console.log(name + ".onReleased: " + point.x + ", " + point.y);
        onDoubleClicked:    console.log(name + ".onDoubleClicked: " + point.x + ", " + point.y);
    }

    AreaSeries {
        name: "area 2"

        upperSeries: LineSeries {
            XYPoint { x: 0; y: 0.5 }
            XYPoint { x: 1; y: 1.5 }
            XYPoint { x: 2; y: 0.3 }
            XYPoint { x: 3; y: 1.5 }
            XYPoint { x: 4; y: 0.1 }
            XYPoint { x: 5; y: 0.3 }
            XYPoint { x: 6; y: 1.2 }
            XYPoint { x: 7; y: 1.3 }
            XYPoint { x: 8; y: 0.2 }
            XYPoint { x: 9; y: 0.1 }
            XYPoint { x: 10; y: 3.2 }
            XYPoint { x: 11; y: 4.6 }
        }

        onNameChanged:              console.log(name + ".onNameChanged: " + name);
        onVisibleChanged:           console.log(name + ".onVisibleChanged: " + visible);
        onClicked:                  console.log(name + ".onClicked: " + point.x + ", " + point.y);
        onSelected:                 console.log(name + ".onSelected");
        onColorChanged:             console.log(name + ".onColorChanged: " + color);
        onBorderColorChanged:       console.log(name + ".onBorderColorChanged: " + borderColor);
        onPressed:          console.log(name + ".onPressed: " + point.x + ", " + point.y);
        onReleased:         console.log(name + ".onReleased: " + point.x + ", " + point.y);
        onDoubleClicked:    console.log(name + ".onDoubleClicked: " + point.x + ", " + point.y);
    }
}
