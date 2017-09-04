/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#include <QtCharts/QPolarChart>
#include <QtCharts/QAbstractAxis>

QT_CHARTS_BEGIN_NAMESPACE

/*!
    \enum QPolarChart::PolarOrientation

    This enum type specifies the polar orientation of an axis.

    \value PolarOrientationRadial
           A radial axis, where the values are placed along the radius of the
           chart, starting at the pole.
    \value PolarOrientationAngular
           An angular axis, where the values are placed around the chart.
*/

/*!
 \class QPolarChart
 \inmodule Qt Charts
 \brief The QPolarChart presents data in polar charts.

 Polar charts present data in a circular graph, where the placement of data
 is based on the angle and distance from the center of the graph, the \e pole.

 \image examples_polarchart.png

 A polar chart is a specialization of QChart that supports line, spline, area,
 and scatter series, and all axis types supported by them. Each axis can be used
 either as a radial or an angular axis.

 The first and last tick mark on an angular QValueAxis are co-located at a 0/360 degree angle.

 If the angular distance between two consecutive points in a series is more than 180 degrees,
 any direct line connecting the two points becomes meaningless, and will not be drawn. Instead,
 a line will be drawn to and from the center of the chart. Therefore, the axis ranges must be
 chosen accordingly when displaying line, spline, or area series.

 Polar charts draw all axes of the same orientation in the same position, so using multiple
 axes of the same orientation can be confusing, unless the extra axes are only used to customize the
 grid. For example, you can display a highlighted range with a secondary shaded QCategoryAxis
 or provide unlabeled subticks with a secondary QValueAxis thas has hidden labels.

 \sa QChart
 */

/*!
 Constructs a polar chart as a child of \a parent.
 The properties specified by \a wFlags are passed to the QChart constructor.
 */
QPolarChart::QPolarChart(QGraphicsItem *parent, Qt::WindowFlags wFlags)
    : QChart(QChart::ChartTypePolar, parent, wFlags)
{
}

/*!
 Deletes the polar chart object and its children, such as the series and axis objects added to it.
 */
QPolarChart::~QPolarChart()
{
}

/*!
 Returns the axes added for the series \a series with the polar orientation
 \a polarOrientation. If no series is provided, any axis with the
 specified polar orientation is returned.

 \sa addAxis()
 */
QList<QAbstractAxis *> QPolarChart::axes(PolarOrientations polarOrientation, QAbstractSeries *series) const
{
    Qt::Orientations orientation(0);
    if (polarOrientation.testFlag(PolarOrientationAngular))
        orientation |= Qt::Horizontal;
    if (polarOrientation.testFlag(PolarOrientationRadial))
        orientation |= Qt::Vertical;

    return QChart::axes(orientation, series);
}

/*!
  This convenience method adds the axis \a axis to the polar chart with the polar
  orientation \a polarOrientation.
  The chart takes the ownership of the axis.

  \note Axes can be added to a polar chart also with QChart::addAxis().
  The specified alignment determines the polar orientation: horizontal alignments indicate
  an angular axis and vertical alignments indicate a radial axis.

  \sa QChart::removeAxis(), QChart::createDefaultAxes(), QAbstractSeries::attachAxis(), QChart::addAxis()
*/
void QPolarChart::addAxis(QAbstractAxis *axis, PolarOrientation polarOrientation)
{
    if (!axis || axis->type() == QAbstractAxis::AxisTypeBarCategory) {
        qWarning("QAbstractAxis::AxisTypeBarCategory is not a supported axis type for polar charts.");
    } else {
        Qt::Alignment alignment = Qt::AlignLeft;
        if (polarOrientation == PolarOrientationAngular)
            alignment = Qt::AlignBottom;
        QChart::addAxis(axis, alignment);
    }
}

/*!
  The angular axes of a polar chart report horizontal orientation and the radial axes report
  vertical orientation.
  This function is a convenience function for converting the orientation of the axis \a axis to
  the corresponding polar orientation. If the \a axis is null or not added to a polar chart,
  the return value is meaningless.
*/
QPolarChart::PolarOrientation QPolarChart::axisPolarOrientation(QAbstractAxis *axis)
{
    if (axis && axis->orientation() == Qt::Horizontal)
        return PolarOrientationAngular;
    else
        return PolarOrientationRadial;
}

#include "moc_qpolarchart.cpp"

QT_CHARTS_END_NAMESPACE
