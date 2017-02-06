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

#include <QtCharts/QBarSeries>
#include <private/qbarseries_p.h>
#include <private/barchartitem_p.h>
#include <private/chartdataset_p.h>
#include <private/charttheme_p.h>
#include <QtCharts/QValueAxis>
#include <QtCharts/QBarCategoryAxis>

QT_CHARTS_BEGIN_NAMESPACE

/*!
    \class QBarSeries
    \inmodule Qt Charts
    \brief The QBarSeries class presents a series of data as vertical bars grouped by category.

    This class draws data as a series of vertical bars grouped by category, with one bar per
    category from each bar set added to the series.

    See the \l {BarChart Example} {bar chart example} to learn how to create a grouped bar chart.
    \image examples_barchart.png

    \sa QBarSet, QPercentBarSeries, QAbstractBarSeries, QStackedBarSeries
*/
/*!
    \qmltype BarSeries
    \instantiates QBarSeries
    \inqmlmodule QtCharts

    \inherits AbstractBarSeries

    \brief Presents a series of data as vertical bars grouped by category.

    The data is drawn as a series of vertical bars grouped by category, with one bar per
    category from each bar set added to the series.

    The following QML code snippet shows how to create a simple grouped bar chart:
    \snippet qmlchart/qml/qmlchart/View6.qml 1
    \beginfloatleft
    \image examples_qmlchart6.png
    \endfloat
    \clearfloat
*/

/*!
    Constructs an empty bar series that is a QObject and a child of \a parent.
*/
QBarSeries::QBarSeries(QObject *parent)
    : QAbstractBarSeries(*new QBarSeriesPrivate(this), parent)
{

}

/*!
    Returns the bar series.
*/
QAbstractSeries::SeriesType QBarSeries::type() const
{
    return QAbstractSeries::SeriesTypeBar;
}

/*!
    Removes the bar series from the chart.
*/
QBarSeries::~QBarSeries()
{
    Q_D(QBarSeries);
    if (d->m_chart)
        d->m_chart->removeSeries(this);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

QBarSeriesPrivate::QBarSeriesPrivate(QBarSeries *q) : QAbstractBarSeriesPrivate(q)
{

}

void QBarSeriesPrivate::initializeDomain()
{
    qreal minX(domain()->minX());
    qreal minY(domain()->minY());
    qreal maxX(domain()->maxX());
    qreal maxY(domain()->maxY());

    qreal x = categoryCount();
    minX = qMin(minX, - (qreal)0.5);
    minY = qMin(minY, min());
    maxX = qMax(maxX, x - (qreal)0.5);
    maxY = qMax(maxY, max());

    domain()->setRange(minX, maxX, minY, maxY);
}


void QBarSeriesPrivate::initializeGraphics(QGraphicsItem* parent)
{
    Q_Q(QBarSeries);
    BarChartItem *bar = new BarChartItem(q,parent);
    m_item.reset(bar);
    QAbstractSeriesPrivate::initializeGraphics(parent);
}

#include "moc_qbarseries.cpp"

QT_CHARTS_END_NAMESPACE

