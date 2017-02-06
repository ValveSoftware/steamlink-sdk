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
#include <QtCharts/QHorizontalStackedBarSeries>
#include <private/qhorizontalstackedbarseries_p.h>
#include <private/horizontalstackedbarchartitem_p.h>

#include <private/chartdataset_p.h>
#include <private/charttheme_p.h>

QT_CHARTS_BEGIN_NAMESPACE
/*!
    \class QHorizontalStackedBarSeries
    \inmodule Qt Charts
    \brief The QHorizontalStackedBarSeries class presents a series of data as horizontally
    stacked bars, with one bar per category.

    Each bar set added to the series contributes a single segment to each stacked bar.

    See the \l {HorizontalStackedBarChart Example} {horizontal stacked bar chart example} to learn how to create a horizontal stacked bar chart.
    \image examples_horizontalstackedbarchart.png

    \sa QBarSet, QBarSeries, QPercentBarSeries, QAbstractBarSeries, QStackedBarSeries, QHorizontalPercentBarSeries, QHorizontalBarSeries
*/
/*!
    \qmltype HorizontalStackedBarSeries
    \instantiates QHorizontalStackedBarSeries
    \inqmlmodule QtCharts

    \inherits AbstractBarSeries

    \brief Presents a series of data as stacked horizontal bars, with one bar per category.

    Each bar set added to the series contributes a single segment to each stacked bar.

    The following QML code snippet shows how to create a simple horizontal stacked bar chart:
    \snippet qmlchart/qml/qmlchart/View10.qml 1
    \beginfloatleft
    \image examples_qmlchart10.png
    \endfloat
    \clearfloat
*/

/*!
    Constructs an empty horizontal stacked bar series that is a QObject and a child of \a parent.
*/
QHorizontalStackedBarSeries::QHorizontalStackedBarSeries(QObject *parent)
    : QAbstractBarSeries(*new QHorizontalStackedBarSeriesPrivate(this), parent)
{
}

/*!
    Removes the bar series from the chart.
*/
QHorizontalStackedBarSeries::~QHorizontalStackedBarSeries()
{
    Q_D(QHorizontalStackedBarSeries);
    if (d->m_chart)
        d->m_chart->removeSeries(this);
}

/*!
    Returns the horizontal stacked bar series.
*/
QAbstractSeries::SeriesType QHorizontalStackedBarSeries::type() const
{
    return QAbstractSeries::SeriesTypeHorizontalStackedBar;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

QHorizontalStackedBarSeriesPrivate::QHorizontalStackedBarSeriesPrivate(QHorizontalStackedBarSeries *q) : QAbstractBarSeriesPrivate(q)
{

}

void QHorizontalStackedBarSeriesPrivate::initializeDomain()
{
    qreal minX(domain()->minX());
    qreal minY(domain()->minY());
    qreal maxX(domain()->maxX());
    qreal maxY(domain()->maxY());

    qreal y = categoryCount();
    minX = qMin(minX, bottom());
    minY = qMin(minY, - (qreal)0.5);
    maxX = qMax(maxX, top());
    maxY = qMax(maxY, y - (qreal)0.5);

    domain()->setRange(minX, maxX, minY, maxY);
}

void QHorizontalStackedBarSeriesPrivate::initializeGraphics(QGraphicsItem *parent)
{
    Q_Q(QHorizontalStackedBarSeries);
    HorizontalStackedBarChartItem *bar = new HorizontalStackedBarChartItem(q,parent);
    m_item.reset(bar);
    QAbstractSeriesPrivate::initializeGraphics(parent);
}

#include "moc_qhorizontalstackedbarseries.cpp"

QT_CHARTS_END_NAMESPACE
