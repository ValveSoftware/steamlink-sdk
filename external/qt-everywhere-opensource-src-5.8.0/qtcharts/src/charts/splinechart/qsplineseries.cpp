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

#include <QtCharts/QSplineSeries>
#include <private/qsplineseries_p.h>
#include <private/splinechartitem_p.h>
#include <private/chartdataset_p.h>
#include <private/charttheme_p.h>
#include <private/splineanimation_p.h>
#include <private/qchart_p.h>

/*!
    \class QSplineSeries
    \inmodule Qt Charts
    \brief Series type used to store data needed to draw a spline.

    QSplineSeries stores the data points along with the segment control points needed by QPainterPath to draw spline
    Control points are automatically calculated when data changes. The algorithm computes the points so that the normal spline can be drawn.

    \image examples_splinechart.png

    Creating basic spline chart is simple:
    \code
    QSplineSeries* series = new QSplineSeries();
    series->append(0, 6);
    series->append(2, 4);
    ...
    chart->addSeries(series);
    \endcode
*/
/*!
    \qmltype SplineSeries
    \instantiates QSplineSeries
    \inqmlmodule QtCharts

    \inherits XYSeries

    \brief The SplineSeries type is used for making spline charts.

    The following QML shows how to create a simple spline chart:
    \snippet qmlchart/qml/qmlchart/View3.qml 1
    \beginfloatleft
    \image examples_qmlchart3.png
    \endfloat
    \clearfloat
*/

/*!
    \fn QSeriesType QSplineSeries::type() const
    Returns the type of the series
*/

/*!
    \qmlproperty real SplineSeries::width
    The width of the line. By default the width is 2.0.
*/

/*!
    \qmlproperty Qt::PenStyle SplineSeries::style
    Controls the style of the line. Set to one of Qt.NoPen, Qt.SolidLine, Qt.DashLine, Qt.DotLine,
    Qt.DashDotLine or Qt.DashDotDotLine. Using Qt.CustomDashLine is not supported in the QML API.
    By default the style is Qt.SolidLine.
*/

/*!
    \qmlproperty Qt::PenCapStyle SplineSeries::capStyle
    Controls the cap style of the line. Set to one of Qt.FlatCap, Qt.SquareCap or Qt.RoundCap. By
    default the cap style is Qt.SquareCap.
*/

QT_CHARTS_BEGIN_NAMESPACE

/*!
    Constructs empty series object which is a child of \a parent.
    When series object is added to a QChart instance then the ownerships is transferred.
  */

QSplineSeries::QSplineSeries(QObject *parent)
    : QLineSeries(*new QSplineSeriesPrivate(this), parent)
{
}

/*!
  Destroys the object.
*/
QSplineSeries::~QSplineSeries()
{
    Q_D(QSplineSeries);
    if (d->m_chart)
        d->m_chart->removeSeries(this);
}

QAbstractSeries::SeriesType QSplineSeries::type() const
{
    return QAbstractSeries::SeriesTypeSpline;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

QSplineSeriesPrivate::QSplineSeriesPrivate(QSplineSeries *q)
    : QLineSeriesPrivate(q)
{
}

void QSplineSeriesPrivate::initializeGraphics(QGraphicsItem* parent)
{
    Q_Q(QSplineSeries);
    SplineChartItem *spline = new SplineChartItem(q,parent);
    m_item.reset(spline);
    QAbstractSeriesPrivate::initializeGraphics(parent);
}

void QSplineSeriesPrivate::initializeTheme(int index, ChartTheme* theme, bool forced)
{
    Q_Q(QSplineSeries);
    const QList<QColor> colors = theme->seriesColors();

    if (forced || QChartPrivate::defaultPen() == m_pen) {
        QPen pen;
        pen.setColor(colors.at(index % colors.size()));
        pen.setWidthF(2);
        q->setPen(pen);
    }

    if (forced || QChartPrivate::defaultPen().color() == m_pointLabelsColor) {
        QColor color = theme->labelBrush().color();
        q->setPointLabelsColor(color);
    }
}

void QSplineSeriesPrivate::initializeAnimations(QtCharts::QChart::AnimationOptions options,
                                                int duration, QEasingCurve &curve)
{
    SplineChartItem *item = static_cast<SplineChartItem *>(m_item.data());
    Q_ASSERT(item);
    if (item->animation())
        item->animation()->stopAndDestroyLater();

    if (options.testFlag(QChart::SeriesAnimations))
        item->setAnimation(new SplineAnimation(item, duration, curve));
    else
        item->setAnimation(0);
    QAbstractSeriesPrivate::initializeAnimations(options, duration, curve);
}

#include "moc_qsplineseries.cpp"
#include "moc_qsplineseries_p.cpp"

QT_CHARTS_END_NAMESPACE
