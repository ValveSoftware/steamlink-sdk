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
    id: main
    width: 400
    height: 320

    Column {
        id: column
        anchors.fill: parent
        anchors.bottomMargin: 10
        spacing: 0

        ChartViewSelector {
            id: chartViewSelector
            width: parent.width
            height: parent.height - customLegend.height - anchors.bottomMargin
            onSeriesAdded: customLegend.addSeries(seriesName, seriesColor);
        }

        CustomLegend {
            id: customLegend
            width: parent.width
            height: 50
            anchors.horizontalCenter: parent.horizontalCenter
            onEntered: chartViewSelector.highlightSeries(seriesName);
            onExited: chartViewSelector.highlightSeries("");
            onSelected: chartViewSelector.selectSeries(seriesName);
        }
    }

    states: State {
        name: "highlighted"
        PropertyChanges {
            target: chartViewHighlighted
            width: column.width
            height: (column.height - column.anchors.margins * 2 - customLegend.height)
        }
        PropertyChanges {
            target: chartViewStacked
            width: 1
            height: 1
        }
    }
}
