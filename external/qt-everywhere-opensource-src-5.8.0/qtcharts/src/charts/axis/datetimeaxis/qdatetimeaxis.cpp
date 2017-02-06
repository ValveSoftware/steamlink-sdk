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

#include <QtCharts/QDateTimeAxis>
#include <private/qdatetimeaxis_p.h>
#include <private/chartdatetimeaxisx_p.h>
#include <private/chartdatetimeaxisy_p.h>
#include <private/polarchartdatetimeaxisangular_p.h>
#include <private/polarchartdatetimeaxisradial_p.h>
#include <private/abstractdomain_p.h>
#include <QtCharts/QChart>
#include <float.h>
#include <cmath>

QT_CHARTS_BEGIN_NAMESPACE
/*!
    \class QDateTimeAxis
    \inmodule Qt Charts
    \brief The QDateTimeAxis class adds dates and times to a chart's axis.

    QDateTimeAxis can be set up to show an axis line with tick marks, grid lines, and shades.
    The labels can be configured by setting an appropriate DateTime format.
    QDateTimeAxis works correctly with dates from 4714 BCE to 287396 CE.
    For other limitiations related to QDateTime, see QDateTime documentation.

    \note QDateTimeAxis is disabled on platforms that define qreal as float.

    \image api_datatime_axis.png

    QDateTimeAxis can be used with any QXYSeries.
    To add a data point to the series, QDateTime::toMSecsSinceEpoch() is used:
    \code
    QLineSeries *series = new QLineSeries;

    QDateTime xValue;
    xValue.setDate(QDate(2012, 1 , 18));
    xValue.setTime(QTime(9, 34));
    qreal yValue = 12;
    series->append(xValue.toMSecsSinceEpoch(), yValue);

    xValue.setDate(QDate(2013, 5 , 11));
    xValue.setTime(QTime(11, 14));
    qreal yValue = 22;
    series->append(xValue.toMSecsSinceEpoch(), yValue);
    \endcode

    The following code snippet illustrates adding the series to the chart and setting up
    QDateTimeAxis:
    \code
    QChartView *chartView = new QChartView;
    chartView->chart()->addSeries(series);

    // ...
    QDateTimeAxis *axisX = new QDateTimeAxis;
    axisX->setFormat("dd-MM-yyyy h:mm");
    chartView->chart()->setAxisX(axisX, series);
    \endcode
*/

/*!
    \qmltype DateTimeAxis
    \instantiates QDateTimeAxis
    \inqmlmodule QtCharts

    \brief Adds dates and times to a chart's axis.
    \inherits AbstractAxis

    The DateTimeAxis type can be set up to show an axis line with tick marks, grid lines,
    and shades. The axis labels display dates and times and can be configured by setting
    an appropriate DateTime format.

    \note Any date before 4714 BCE or after about 1.4 million CE may not be accurately stored.
*/

/*!
  \property QDateTimeAxis::min
  \brief The minimum value on the axis.

  When setting this property, the maximum value is adjusted if necessary, to ensure that the
  range remains valid.
*/
/*!
  \qmlproperty datetime DateTimeAxis::min
  The minimum value on the axis.

  When setting this property, the maximum value is adjusted if necessary, to ensure that the
  range remains valid.
*/

/*!
  \property QDateTimeAxis::max
  \brief The maximum value on the axis.

  When setting this property, the minimum value is adjusted if necessary, to ensure that the
  range remains valid.
*/
/*!
  \qmlproperty datetime DateTimeAxis::max
  The maximum value on the axis.

  When setting this property, the minimum value is adjusted if necessary, to ensure that the
  range remains valid.
*/

/*!
  \fn void QDateTimeAxis::minChanged(QDateTime min)
  This signal is emitted when the minimum value of the axis, specified by \a min, changes.
*/

/*!
  \fn void QDateTimeAxis::maxChanged(QDateTime max)
  This signal is emitted when the maximum value of the axis, specified by \a max, changes.
*/

/*!
  \fn void QDateTimeAxis::rangeChanged(QDateTime min, QDateTime max)
  This signal is emitted when the minimum or maximum value of the axis, specified by \a min
  and \a max, changes.
*/

/*!
  \property QDateTimeAxis::tickCount
  \brief The number of tick marks on the axis.
*/

/*!
  \qmlproperty int DateTimeAxis::tickCount
  The number of tick marks on the axis.
*/

/*!
  \property QDateTimeAxis::format
  \brief The format string that is used when creating the label for the axis out of a
  QDateTime object.

  See QDateTime documentation for information on how the string should be defined.

  \sa QChart::locale
*/
/*!
  \qmlproperty string DateTimeAxis::format
  The format string that is used when creating the label for the axis out of a QDateTime object.
  See QDateTime documentation for information on how the string should be defined.
*/

/*!
  \fn void QDateTimeAxis::tickCountChanged(int tickCount)
  This signal is emitted when the number of tick marks on the axis, specified by \a tickCount,
  changes.
*/

/*!
  \fn void QDateTimeAxis::formatChanged(QString format)
  This signal is emitted when the \a format of the axis changes.
*/

/*!
    Constructs an axis object that is a child of \a parent.
*/
QDateTimeAxis::QDateTimeAxis(QObject *parent) :
    QAbstractAxis(*new QDateTimeAxisPrivate(this), parent)
{

}

/*!
    \internal
*/
QDateTimeAxis::QDateTimeAxis(QDateTimeAxisPrivate &d, QObject *parent) : QAbstractAxis(d, parent)
{

}

/*!
    Destroys the object.
*/
QDateTimeAxis::~QDateTimeAxis()
{
    Q_D(QDateTimeAxis);
    if (d->m_chart)
        d->m_chart->removeAxis(this);
}

void QDateTimeAxis::setMin(QDateTime min)
{
    Q_D(QDateTimeAxis);
    if (min.isValid())
        d->setRange(min.toMSecsSinceEpoch(), qMax(d->m_max, qreal(min.toMSecsSinceEpoch())));
}

QDateTime QDateTimeAxis::min() const
{
    Q_D(const QDateTimeAxis);
    return QDateTime::fromMSecsSinceEpoch(d->m_min);
}

void QDateTimeAxis::setMax(QDateTime max)
{
    Q_D(QDateTimeAxis);
    if (max.isValid())
        d->setRange(qMin(d->m_min, qreal(max.toMSecsSinceEpoch())), max.toMSecsSinceEpoch());
}

QDateTime QDateTimeAxis::max() const
{
    Q_D(const QDateTimeAxis);
    return QDateTime::fromMSecsSinceEpoch(d->m_max);
}

/*!
  Sets the range on the axis from \a min to \a max.
  If \a min is greater than \a max, this function returns without making any changes.
*/
void QDateTimeAxis::setRange(QDateTime min, QDateTime max)
{
    Q_D(QDateTimeAxis);
    if (!min.isValid() || !max.isValid() || min > max)
        return;

    d->setRange(min.toMSecsSinceEpoch(),max.toMSecsSinceEpoch());
}

void QDateTimeAxis::setFormat(QString format)
{
    Q_D(QDateTimeAxis);
    if (d->m_format != format) {
        d->m_format = format;
        emit formatChanged(format);
    }
}

QString QDateTimeAxis::format() const
{
    Q_D(const QDateTimeAxis);
    return d->m_format;
}

/*!
  Sets the number of tick marks on the axis to \a count.
*/
void QDateTimeAxis::setTickCount(int count)
{
    Q_D(QDateTimeAxis);
    if (d->m_tickCount != count && count >= 2) {
        d->m_tickCount = count;
       emit tickCountChanged(count);
    }
}

/*!
  \fn int QDateTimeAxis::tickCount() const
  Returns the number of tick marks on the axis.
*/
int QDateTimeAxis::tickCount() const
{
    Q_D(const QDateTimeAxis);
    return d->m_tickCount;
}

/*!
  Returns the type of the axis.
*/
QAbstractAxis::AxisType QDateTimeAxis::type() const
{
    return AxisTypeDateTime;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

QDateTimeAxisPrivate::QDateTimeAxisPrivate(QDateTimeAxis *q)
    : QAbstractAxisPrivate(q),
      m_min(0),
      m_max(0),
      m_tickCount(5)
{
    m_format = QStringLiteral("dd-MM-yyyy\nh:mm");
}

QDateTimeAxisPrivate::~QDateTimeAxisPrivate()
{

}

void QDateTimeAxisPrivate::setRange(qreal min,qreal max)
{
    Q_Q(QDateTimeAxis);

    bool changed = false;

    if (m_min != min) {
        m_min = min;
        changed = true;
        emit q->minChanged(QDateTime::fromMSecsSinceEpoch(min));
    }

    if (m_max != max) {
        m_max = max;
        changed = true;
        emit q->maxChanged(QDateTime::fromMSecsSinceEpoch(max));
    }

    if (changed) {
        emit q->rangeChanged(QDateTime::fromMSecsSinceEpoch(min), QDateTime::fromMSecsSinceEpoch(max));
        emit rangeChanged(m_min,m_max);
    }
}


void QDateTimeAxisPrivate::setMin(const QVariant &min)
{
    Q_Q(QDateTimeAxis);
    if (min.canConvert(QVariant::DateTime))
        q->setMin(min.toDateTime());
}

void QDateTimeAxisPrivate::setMax(const QVariant &max)
{

    Q_Q(QDateTimeAxis);
    if (max.canConvert(QVariant::DateTime))
        q->setMax(max.toDateTime());
}

void QDateTimeAxisPrivate::setRange(const QVariant &min, const QVariant &max)
{
    Q_Q(QDateTimeAxis);
    if (min.canConvert(QVariant::DateTime) && max.canConvert(QVariant::DateTime))
        q->setRange(min.toDateTime(), max.toDateTime());
}

void QDateTimeAxisPrivate::initializeGraphics(QGraphicsItem* parent)
{
    Q_Q(QDateTimeAxis);
    ChartAxisElement *axis(0);
    if (m_chart->chartType() == QChart::ChartTypeCartesian) {
        if (orientation() == Qt::Vertical)
            axis = new ChartDateTimeAxisY(q,parent);
        if (orientation() == Qt::Horizontal)
            axis = new ChartDateTimeAxisX(q,parent);
    }

    if (m_chart->chartType() == QChart::ChartTypePolar) {
        if (orientation() == Qt::Vertical)
            axis = new PolarChartDateTimeAxisRadial(q, parent);
        if (orientation() == Qt::Horizontal)
            axis = new PolarChartDateTimeAxisAngular(q, parent);
    }

    m_item.reset(axis);
    QAbstractAxisPrivate::initializeGraphics(parent);
}

void QDateTimeAxisPrivate::initializeDomain(AbstractDomain *domain)
{
    if (m_max == m_min) {
        if (orientation() == Qt::Vertical)
            setRange(domain->minY(), domain->maxY());
        else
            setRange(domain->minX(), domain->maxX());
    } else {
        if (orientation() == Qt::Vertical)
            domain->setRangeY(m_min, m_max);
        else
            domain->setRangeX(m_min, m_max);
    }
}

#include "moc_qdatetimeaxis.cpp"
#include "moc_qdatetimeaxis_p.cpp"

QT_CHARTS_END_NAMESPACE
