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

#include <QtCharts/QHorizontalBarSeries>
#include <private/qhorizontalbarseries_p.h>
#include <private/horizontalbarchartitem_p.h>
#include <QtCharts/QBarCategoryAxis>

#include <private/chartdataset_p.h>
#include <private/charttheme_p.h>


QT_CHARTS_BEGIN_NAMESPACE

/*!
    \class QHorizontalBarSeries
    \inmodule Qt Charts
    \brief The QHorizontalBarSeries class presents a series of data as horizontal bars grouped by
    category.

    This class draws data as a series of horizontal bars grouped by category, with one bar per
    category from each bar set added to the series.

    See the \l {HorizontalBarChart Example} {horizontal bar chart example} to learn how to create a horizontal bar chart.
    \image examples_horizontalbarchart.png

    \sa QBarSet, QBarSeries, QPercentBarSeries, QAbstractBarSeries, QStackedBarSeries, QHorizontalStackedBarSeries, QHorizontalPercentBarSeries
*/
/*!
    \qmltype HorizontalBarSeries
    \instantiates QHorizontalBarSeries
    \inqmlmodule QtCharts

    \inherits AbstractBarSeries

    \brief Presents a series of data as horizontal bars grouped by category.

    The data is drawn as a series of horizontal bars grouped by category, with one bar per
    category from each bar set added to the series.

    The following QML code snippet shows how to create a simple horizontal bar chart:
    \snippet qmlchart/qml/qmlchart/View9.qml 1
    \beginfloatleft
    \image examples_qmlchart9.png
    \endfloat
    \clearfloat
*/

/*!
    Constructs an empty horizontal bar series that is a QObject and a child of \a parent.
*/
QHorizontalBarSeries::QHorizontalBarSeries(QObject *parent)
    : QAbstractBarSeries(*new QHorizontalBarSeriesPrivate(this), parent)
{
}

/*!
    Removes the horizontal bar series from the chart.
*/
QHorizontalBarSeries::~QHorizontalBarSeries()
{
    Q_D(QHorizontalBarSeries);
    if (d->m_chart)
        d->m_chart->removeSeries(this);
}

/*!
    Returns the horizontal bar series.
*/
QAbstractSeries::SeriesType QHorizontalBarSeries::type() const
{
    return QAbstractSeries::SeriesTypeHorizontalBar;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

QHorizontalBarSeriesPrivate::QHorizontalBarSeriesPrivate(QHorizontalBarSeries *q)
    : QAbstractBarSeriesPrivate(q)
{

}

void QHorizontalBarSeriesPrivate::initializeDomain()
{
    qreal minX(domain()->minX());
    qreal minY(domain()->minY());
    qreal maxX(domain()->maxX());
    qreal maxY(domain()->maxY());

    qreal y = categoryCount();
    minX = qMin(minX, min());
    minY = qMin(minY, - (qreal)0.5);
    maxX = qMax(maxX, max());
    maxY = qMax(maxY, y - (qreal)0.5);

    domain()->setRange(minX, maxX, minY, maxY);
}

void QHorizontalBarSeriesPrivate::initializeGraphics(QGraphicsItem* parent)
{
    Q_Q(QHorizontalBarSeries);
    HorizontalBarChartItem *bar = new HorizontalBarChartItem(q,parent);
    m_item.reset(bar);
    QAbstractSeriesPrivate::initializeGraphics(parent);
}

#include "moc_qhorizontalbarseries.cpp"

QT_CHARTS_END_NAMESPACE
