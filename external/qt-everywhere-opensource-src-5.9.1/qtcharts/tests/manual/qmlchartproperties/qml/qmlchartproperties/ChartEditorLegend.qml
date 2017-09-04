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
import QtCharts 2.2

Row {
    anchors.fill: parent
    spacing: 5
    property variant chartLegend

    Flow {
        spacing: 5
        flow: Flow.TopToBottom

        Button {
            text: "legend visible"
            onClicked: chartLegend.visible = !chartLegend.visible;
        }
        Button {
            text: "legend bckgrd visible"
            onClicked: chartLegend.backgroundVisible = !chartLegend.backgroundVisible;
        }
        Button {
            text: "legend color"
            onClicked: chartLegend.color = main.nextColor();
        }
        Button {
            text: "legend border color"
            onClicked: chartLegend.borderColor = main.nextColor();
        }
        Button {
            text: "legend label color"
            onClicked: chartLegend.labelColor = main.nextColor();
        }
        Button {
            text: "legend top"
            onClicked: chartLegend.alignment = Qt.AlignTop;
        }
        Button {
            text: "legend bottom"
            onClicked: chartLegend.alignment = Qt.AlignBottom;
        }
        Button {
            text: "legend left"
            onClicked: chartLegend.alignment = Qt.AlignLeft;
        }
        Button {
            text: "legend right"
            onClicked: chartLegend.alignment = Qt.AlignRight;
        }
        Button {
            text: "legend use reverse order"
            onClicked: chartLegend.reverseMarkers = !chartLegend.reverseMarkers;
        }
        Button {
            text: "legend marker shape"
            onClicked: {
                if (chartLegend.markerShape === Legend.MarkerShapeRectangle)
                    chartLegend.markerShape = Legend.MarkerShapeCircle
                else if (chartLegend.markerShape === Legend.MarkerShapeCircle)
                    chartLegend.markerShape = Legend.MarkerShapeFromSeries
                else
                    chartLegend.markerShape = Legend.MarkerShapeRectangle
            }
        }
    }

    FontEditor {
        fontDescription: "legend"
        function editedFont() {
            return chartLegend.font;
        }
    }
}
