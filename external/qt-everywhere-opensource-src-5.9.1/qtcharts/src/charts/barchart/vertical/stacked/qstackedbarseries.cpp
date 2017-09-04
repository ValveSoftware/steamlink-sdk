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

#include <QtCharts/QStackedBarSeries>
#include <private/qstackedbarseries_p.h>
#include <private/stackedbarchartitem_p.h>
#include <private/chartdataset_p.h>
#include <private/charttheme_p.h>
#include <QtCharts/QValueAxis>

QT_CHARTS_BEGIN_NAMESPACE

/*!
    \class QStackedBarSeries
    \inmodule Qt Charts
    \brief The QStackedBarSeries class presents a series of data as vertically stacked bars,
    with one bar per category.

    Each bar set added to the series contributes a single segment to each stacked bar.

    See the \l {StackedbarChart Example} {stacked bar chart example} to learn how to create a stacked bar chart.
    \image examples_stackedbarchart.png

    \sa QBarSet, QPercentBarSeries, QAbstractBarSeries
*/

/*!
    \qmltype StackedBarSeries
    \instantiates QStackedBarSeries
    \inqmlmodule QtCharts

    \inherits AbstractBarSeries

    \brief Presents a series of data as vertically stacked bars, with one bar per category.

    Each bar set added to the series contributes a single segment to each stacked bar.

    The following QML shows how to create a simple stacked bar chart:
    \snippet qmlchart/qml/qmlchart/View7.qml 1
    \beginfloatleft
    \image examples_qmlchart7.png
    \endfloat
    \clearfloat
*/

/*!
    Constructs an empty bar series that is a QObject and a child of \a parent.
*/
QStackedBarSeries::QStackedBarSeries(QObject *parent)
    : QAbstractBarSeries(*new QStackedBarSeriesPrivate(this), parent)
{
}

/*!
    Removes the bar series from the chart.
*/
QStackedBarSeries::~QStackedBarSeries()
{
    Q_D(QStackedBarSeries);
    if (d->m_chart)
        d->m_chart->removeSeries(this);
}
/*!
    Returns the stacked bar series.
*/
QAbstractSeries::SeriesType QStackedBarSeries::type() const
{
    return QAbstractSeries::SeriesTypeStackedBar;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

QStackedBarSeriesPrivate::QStackedBarSeriesPrivate(QStackedBarSeries *q) : QAbstractBarSeriesPrivate(q)
{

}

void QStackedBarSeriesPrivate::initializeDomain()
{
    qreal minX(domain()->minX());
    qreal minY(domain()->minY());
    qreal maxX(domain()->maxX());
    qreal maxY(domain()->maxY());

    qreal x = categoryCount();
    minX = qMin(minX, - (qreal)0.5);
    minY = qMin(minY, bottom());
    maxX = qMax(maxX, x - (qreal)0.5);
    maxY = qMax(maxY, top());

    domain()->setRange(minX, maxX, minY, maxY);
}

void QStackedBarSeriesPrivate::initializeGraphics(QGraphicsItem* parent)
{
    Q_Q(QStackedBarSeries);
    StackedBarChartItem *bar = new StackedBarChartItem(q,parent);
    m_item.reset(bar);
    QAbstractSeriesPrivate::initializeGraphics(parent);
}

#include "moc_qstackedbarseries.cpp"

QT_CHARTS_END_NAMESPACE

