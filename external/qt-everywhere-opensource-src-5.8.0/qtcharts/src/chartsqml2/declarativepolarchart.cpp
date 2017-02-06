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

#include "declarativepolarchart.h"
#include <QtCharts/QChart>

QT_CHARTS_BEGIN_NAMESPACE

/*!
    \qmltype PolarChartView
    \instantiates DeclarativePolarChart
    \inqmlmodule QtCharts

    \brief Polar chart element.

    PolarChartView element is the parent that is responsible for showing different chart series
    types in a polar chart.

    Polar charts support line, spline, area, and scatter series, and all axis types
    supported by those series.

    \note When setting ticks to an angular ValueAxis, keep in mind that the first and last tick
    are co-located at 0/360 degree angle.

    \note If the angular distance between two consecutive points in a series is more than 180
    degrees, any line connecting the two points becomes meaningless, so choose the axis ranges
    accordingly when displaying line, spline, or area series.

    The following QML shows how to create a polar chart with two series:
    \snippet qmlpolarchart/qml/qmlpolarchart/View1.qml 1

    \beginfloatleft
    \image examples_qmlpolarchart1.png
    \endfloat
    \clearfloat
*/

DeclarativePolarChart::DeclarativePolarChart(QQuickItem *parent)
    : DeclarativeChart(QChart::ChartTypePolar, parent)
{
}

DeclarativePolarChart::~DeclarativePolarChart()
{
}

#include "moc_declarativepolarchart.cpp"

QT_CHARTS_END_NAMESPACE
