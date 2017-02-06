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
        title: "NHL All-Star Team Players"
        anchors.fill: parent
        antialiasing: true

        ValueAxis {
            id: valueAxis
            min: 2000
            max: 2011
            tickCount: 12
            labelFormat: "%.0f"
        }

        AreaSeries {
            name: "Russian"
            color: "#FFD52B1E"
            borderColor: "#FF0039A5"
            borderWidth: 3
            axisX: valueAxis
            upperSeries: LineSeries {
                XYPoint { x: 2000; y: 1 }
                XYPoint { x: 2001; y: 1 }
                XYPoint { x: 2002; y: 1 }
                XYPoint { x: 2003; y: 1 }
                XYPoint { x: 2004; y: 1 }
                XYPoint { x: 2005; y: 0 }
                XYPoint { x: 2006; y: 1 }
                XYPoint { x: 2007; y: 1 }
                XYPoint { x: 2008; y: 4 }
                XYPoint { x: 2009; y: 3 }
                XYPoint { x: 2010; y: 2 }
                XYPoint { x: 2011; y: 1 }
            }
        }
        //![1]

        AreaSeries {
            name: "Swedish"
            color: "#AF005292"
            borderColor: "#AFFDCA00"
            borderWidth: 3
            axisX: valueAxis
            upperSeries: LineSeries {
                XYPoint { x: 2000; y: 1 }
                XYPoint { x: 2001; y: 1 }
                XYPoint { x: 2002; y: 3 }
                XYPoint { x: 2003; y: 3 }
                XYPoint { x: 2004; y: 2 }
                XYPoint { x: 2005; y: 0 }
                XYPoint { x: 2006; y: 2 }
                XYPoint { x: 2007; y: 1 }
                XYPoint { x: 2008; y: 2 }
                XYPoint { x: 2009; y: 1 }
                XYPoint { x: 2010; y: 3 }
                XYPoint { x: 2011; y: 3 }
            }
        }

        AreaSeries {
            name: "Finnish"
            color: "#00357F"
            borderColor: "#FEFEFE"
            borderWidth: 3
            axisX: valueAxis
            upperSeries: LineSeries {
                XYPoint { x: 2000; y: 0 }
                XYPoint { x: 2001; y: 0 }
                XYPoint { x: 2002; y: 0 }
                XYPoint { x: 2003; y: 0 }
                XYPoint { x: 2004; y: 0 }
                XYPoint { x: 2005; y: 0 }
                XYPoint { x: 2006; y: 1 }
                XYPoint { x: 2007; y: 0 }
                XYPoint { x: 2008; y: 0 }
                XYPoint { x: 2009; y: 0 }
                XYPoint { x: 2010; y: 0 }
                XYPoint { x: 2011; y: 1 }
            }
        }
    }
}
