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

#include <QtCore/qmath.h>
#include <private/abstractdomain_p.h>
#include <private/chartlogvalueaxisx_p.h>
#include <private/chartlogvalueaxisy_p.h>
#include <private/polarchartlogvalueaxisangular_p.h>
#include <private/polarchartlogvalueaxisradial_p.h>
#include <private/qlogvalueaxis_p.h>

QT_CHARTS_BEGIN_NAMESPACE

/*!
    \class QLogValueAxis
    \inmodule Qt Charts
    \brief The QLogValueAxis class adds a logarithmic scale to a chart's axis.

    A logarithmic scale is a nonlinear scale that is based on orders of magnitude,
    so that each tick mark on the axis is the previous tick mark multiplied by a value.

    \note If QLogValueAxis is attached to a series with one or more points with
    negative or zero values on the associated dimension, the series will not be
    plotted at all. This is particularly relevant when XYModelMappers are used,
    since empty cells in models typically contain zero values.
*/

/*!
    \qmltype LogValueAxis
    \instantiates QLogValueAxis
    \inqmlmodule QtCharts

    \brief Adds a logarithmic scale to a chart's axis.
    \inherits AbstractAxis

    A logarithmic scale is a nonlinear scale that is based on orders of magnitude,
    so that each tick mark on the axis is the previous tick mark multiplied by a value.

    \note If a LogValueAxis type is attached to a series with one or more points with
    negative or zero values on the associated dimension, the series will not be
    plotted at all. This is particularly relevant when XYModelMappers are used,
    since empty cells in models typically contain zero values.
*/

/*!
  \property QLogValueAxis::min
  \brief The minimum value on the axis.

  When setting this property, the maximum value is adjusted if necessary, to ensure that
  the range remains valid.
  The value has to be greater than 0.
*/
/*!
  \qmlproperty real LogValueAxis::min
  The minimum value on the axis.

  When setting this property, the maximum value is adjusted if necessary, to ensure that
  the range remains valid.
  The value has to be greater than 0.
*/

/*!
  \property QLogValueAxis::max
  \brief The maximum value on the axis.

  When setting this property, the minimum value is adjusted if necessary, to ensure that
  the range remains valid.
  The value has to be greater than 0.
*/
/*!
  \qmlproperty real LogValueAxis::max
  The maximum value on the axis.

  When setting this property, the minimum value is adjusted if necessary, to ensure that
  the range remains valid.
  The value has to be greater than 0.
*/

/*!
  \property QLogValueAxis::base
  \brief The base of the logarithm.

  The value has to be greater than 0 and cannot equal 1.
*/
/*!
  \qmlproperty real LogValueAxis::base
  The base of the logarithm.

  The value has to be greater than 0 and cannot equal 1.
*/

/*!
  \property QLogValueAxis::tickCount
  \brief The number of tick marks on the axis. This indicates how many grid lines are drawn on the
  chart. This value is read-only.
*/
/*!
  \qmlproperty int LogValueAxis::tickCount
  The number of tick marks on the axis. This indicates how many grid lines are drawn on the
  chart. This value is read-only.
*/

/*!
  \property QLogValueAxis::minorTickCount
  \brief The number of minor tick marks on the axis. This indicates how many grid lines are drawn
  between major ticks on the chart. Labels are not drawn for minor ticks. The default value is 0.
  Set the value to -1 and the number of grid lines between major ticks will be calculated
  automatically.
*/
/*!
  \qmlproperty int LogValueAxis::minorTickCount
  The number of minor tick marks on the axis. This indicates how many grid lines are drawn between
  major ticks on the chart. Labels are not drawn for minor ticks. The default value is 0. Set the
  value to -1 and the number of grid lines between major ticks will be calculated automatically.
*/

/*!
  \property QLogValueAxis::labelFormat
  \brief The label format of the axis.

  The format string supports the following conversion specifiers, length modifiers, and flags
  provided by \c printf() in the standard C++ library: d, i, o, x, X, f, F, e, E, g, G, c.

  If QChart::localizeNumbers is \c true, the supported specifiers are limited to:
  d, e, E, f, g, G, and i. Also, only the precision modifier is supported. The rest of the
  formatting comes from the default QLocale of the application.

  \sa QString::asprintf()
*/
/*!
  \qmlproperty real LogValueAxis::labelFormat
  The label format of the axis.

  The format string supports the following conversion specifiers, length modifiers, and flags
  provided by \c printf() in the standard C++ library: d, i, o, x, X, f, F, e, E, g, G, c.

  If \l{ChartView::localizeNumbers}{ChartView.localizeNumbers} is \c true, the supported
  specifiers are limited to: d, e, E, f, g, G, and i. Also, only the precision modifier is
  supported. The rest of the formatting comes from the default QLocale of the application.

  \sa QString::asprintf()
*/

/*!
  \fn void QLogValueAxis::minChanged(qreal min)
  This signal is emitted when the minimum value of the axis, specified by \a min, changes.
*/

/*!
  \fn void QLogValueAxis::maxChanged(qreal max)
  This signal is emitted when the maximum value of the axis, specified by \a max, changes.
*/

/*!
  \fn void QLogValueAxis::rangeChanged(qreal min, qreal max)
  This signal is emitted when the minimum or maximum value of the axis, specified by \a min
  and \a max, changes.
*/

/*!
  \fn void QLogValueAxis::tickCountChanged(int tickCount)
  This signal is emitted when the number of tick marks on the axis, specified by \a tickCount,
  changes.
*/
/*!
  \qmlsignal LogValueAxis::tickCountChanged(int tickCount)
  This signal is emitted when the number of tick marks on the axis, specified by \a tickCount,
  changes.
*/

/*!
  \fn void QLogValueAxis::minorTickCountChanged(int minorTickCount)
  This signal is emitted when the number of minor tick marks on the axis, specified by
  \a minorTickCount, changes.
*/
/*!
  \qmlsignal LogValueAxis::minorTickCountChanged(int minorTickCount)
  This signal is emitted when the number of minor tick marks on the axis, specified by
  \a minorTickCount, changes.
*/

/*!
  \fn void QLogValueAxis::labelFormatChanged(const QString &format)
  This signal is emitted when the \a format of axis labels changes.
*/

/*!
  \fn void QLogValueAxis::baseChanged(qreal base)
  This signal is emitted when the \a base of the logarithm of the axis changes.
*/

/*!
    Constructs an axis object that is a child of \a parent.
*/
QLogValueAxis::QLogValueAxis(QObject *parent) :
    QAbstractAxis(*new QLogValueAxisPrivate(this), parent)
{

}

/*!
    \internal
*/
QLogValueAxis::QLogValueAxis(QLogValueAxisPrivate &d, QObject *parent) : QAbstractAxis(d, parent)
{

}

/*!
    Destroys the object.
*/
QLogValueAxis::~QLogValueAxis()
{
    Q_D(QLogValueAxis);
    if (d->m_chart)
        d->m_chart->removeAxis(this);
}

void QLogValueAxis::setMin(qreal min)
{
    Q_D(QLogValueAxis);
    setRange(min, qMax(d->m_max, min));
}

qreal QLogValueAxis::min() const
{
    Q_D(const QLogValueAxis);
    return d->m_min;
}

void QLogValueAxis::setMax(qreal max)
{
    Q_D(QLogValueAxis);
    setRange(qMin(d->m_min, max), max);
}

qreal QLogValueAxis::max() const
{
    Q_D(const QLogValueAxis);
    return d->m_max;
}

/*!
  Sets the range from \a min to \a max on the axis.
  If \a min is greater than \a max, this function returns without making any changes.
*/
void QLogValueAxis::setRange(qreal min, qreal max)
{
    Q_D(QLogValueAxis);

    if (min > max)
        return;

    if (min > 0) {
        bool changed = false;

        if (!qFuzzyCompare(d->m_min, min)) {
            d->m_min = min;
            changed = true;
            emit minChanged(min);
        }

        if (!qFuzzyCompare(d->m_max, max)) {
            d->m_max = max;
            changed = true;
            emit maxChanged(max);
        }

        if (changed) {
            d->updateTickCount();
            emit rangeChanged(min, max);
            emit d->rangeChanged(min,max);
        }
    }
}

void QLogValueAxis::setLabelFormat(const QString &format)
{
    Q_D(QLogValueAxis);

    if (d->m_labelFormat == format)
        return;

    d->m_labelFormat = format;
    emit labelFormatChanged(d->m_labelFormat);
}

QString QLogValueAxis::labelFormat() const
{
    Q_D(const QLogValueAxis);
    return d->m_labelFormat;
}

void QLogValueAxis::setBase(qreal base)
{
    Q_D(QLogValueAxis);

    if (base < 0.0 || qFuzzyIsNull(base) || qFuzzyCompare(base, 1.0) // check if base is correct
        || qFuzzyCompare(d->m_base, base)) {
        return;
    }

    d->m_base = base;
    d->updateTickCount();
    emit baseChanged(d->m_base);
}

qreal QLogValueAxis::base() const
{
    Q_D(const QLogValueAxis);
    return d->m_base;
}

int QLogValueAxis::tickCount() const
{
    Q_D(const QLogValueAxis);
    return d->m_tickCount;
}

void QLogValueAxis::setMinorTickCount(int minorTickCount)
{
    Q_D(QLogValueAxis);

    if (minorTickCount < 0)
        minorTickCount = -1;

    if (d->m_minorTickCount == minorTickCount)
        return;

    d->m_minorTickCount = minorTickCount;
    emit minorTickCountChanged(minorTickCount);
}

int QLogValueAxis::minorTickCount() const
{
    Q_D(const QLogValueAxis);
    return d->m_minorTickCount;
}

/*!
  Returns the type of the axis.
*/
QAbstractAxis::AxisType QLogValueAxis::type() const
{
    return AxisTypeLogValue;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

QLogValueAxisPrivate::QLogValueAxisPrivate(QLogValueAxis *q)
    : QAbstractAxisPrivate(q),
      m_min(1),
      m_max(1),
      m_base(10),
      m_tickCount(0),
      m_minorTickCount(0),
      m_labelFormat()
{
}

QLogValueAxisPrivate::~QLogValueAxisPrivate()
{
}

void QLogValueAxisPrivate::setMin(const QVariant &min)
{
    Q_Q(QLogValueAxis);
    bool ok;
    qreal value = min.toReal(&ok);
    if (ok)
        q->setMin(value);
}

void QLogValueAxisPrivate::setMax(const QVariant &max)
{

    Q_Q(QLogValueAxis);
    bool ok;
    qreal value = max.toReal(&ok);
    if (ok)
        q->setMax(value);
}

void QLogValueAxisPrivate::setRange(const QVariant &min, const QVariant &max)
{
    Q_Q(QLogValueAxis);
    bool ok1;
    bool ok2;
    qreal value1 = min.toReal(&ok1);
    qreal value2 = max.toReal(&ok2);
    if (ok1 && ok2)
        q->setRange(value1, value2);
}

void QLogValueAxisPrivate::setRange(qreal min, qreal max)
{
    Q_Q(QLogValueAxis);

    if (min > max)
        return;

    if (min > 0) {
        bool changed = false;

        if (!qFuzzyCompare(m_min, min)) {
            m_min = min;
            changed = true;
            emit q->minChanged(min);
        }

        if (!qFuzzyCompare(m_max, max)) {
            m_max = max;
            changed = true;
            emit q->maxChanged(max);
        }

        if (changed) {
            updateTickCount();
            emit rangeChanged(min,max);
            emit q->rangeChanged(min, max);
        }
    }
}

void QLogValueAxisPrivate::updateTickCount()
{
    Q_Q(QLogValueAxis);

    const qreal logMax = qLn(m_max) / qLn(m_base);
    const qreal logMin = qLn(m_min) / qLn(m_base);
    int tickCount = qAbs(qCeil(logMax) - qCeil(logMin));

    // If the high edge sits exactly on the tick value, add a tick
    qreal highValue = logMin < logMax ? logMax : logMin;
    if (qFuzzyCompare(highValue, qreal(qCeil(highValue))))
        ++tickCount;

    if (m_tickCount == tickCount)
        return;

    m_tickCount = tickCount;
    emit q->tickCountChanged(m_tickCount);
}

void QLogValueAxisPrivate::initializeGraphics(QGraphicsItem *parent)
{
    Q_Q(QLogValueAxis);
    ChartAxisElement *axis(0);

    if (m_chart->chartType() == QChart::ChartTypeCartesian) {
        if (orientation() == Qt::Vertical)
            axis = new ChartLogValueAxisY(q,parent);
        if (orientation() == Qt::Horizontal)
            axis = new ChartLogValueAxisX(q,parent);
    }

    if (m_chart->chartType() == QChart::ChartTypePolar) {
        if (orientation() == Qt::Vertical)
            axis = new PolarChartLogValueAxisRadial(q, parent);
        if (orientation() == Qt::Horizontal)
            axis = new PolarChartLogValueAxisAngular(q, parent);
    }

    m_item.reset(axis);
    QAbstractAxisPrivate::initializeGraphics(parent);
}


void QLogValueAxisPrivate::initializeDomain(AbstractDomain *domain)
{
    if (orientation() == Qt::Vertical) {
        if (!qFuzzyCompare(m_max, m_min)) {
            domain->setRangeY(m_min, m_max);
        } else if ( domain->minY() > 0) {
            setRange(domain->minY(), domain->maxY());
        } else if (domain->maxY() > 0) {
            domain->setRangeY(m_min, domain->maxY());
        } else {
            domain->setRangeY(1, 10);
        }
    }
    if (orientation() == Qt::Horizontal) {
        if (!qFuzzyCompare(m_max, m_min)) {
            domain->setRangeX(m_min, m_max);
        } else if (domain->minX() > 0){
            setRange(domain->minX(), domain->maxX());
        } else if (domain->maxX() > 0) {
            domain->setRangeX(m_min, domain->maxX());
        } else {
            domain->setRangeX(1, 10);
        }
    }
}

#include "moc_qlogvalueaxis.cpp"
#include "moc_qlogvalueaxis_p.cpp"

QT_CHARTS_END_NAMESPACE
