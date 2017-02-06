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

#include <QtCharts/QCandlestickSet>
#include <private/qcandlestickset_p.h>

QT_CHARTS_BEGIN_NAMESPACE

/*!
    \class QCandlestickSet
    \since 5.8
    \inmodule Qt Charts
    \brief The QCandlestickSet class represents a single candlestick item in a
    candlestick chart.

    Five values are needed to create a graphical representation of a candlestick
    item: \e open, \e high, \e low, \e close, and \e timestamp. These values can
    be either passed to a QCandlestickSet constructor
    or set by using setOpen(), setHigh(), setLow(), setClose(), and setTimestamp().

    \sa QCandlestickSeries
*/

/*!
    \qmltype CandlestickSet
    \since QtCharts 2.2
    \instantiates QCandlestickSet
    \inqmlmodule QtCharts
    \brief Represents a single candlestick item in a candlestick chart.

    Five values are needed to create a graphical representation of a candlestick
    item: \l open, \l high, \l low, \l close, and \l timestamp.

    \sa CandlestickSeries
*/

/*!
    \property QCandlestickSet::timestamp
    \brief The timestamp value of the candlestick item.
*/

/*!
    \qmlproperty real CandlestickSet::timestamp
    The timestamp value of the candlestick item.
*/

/*!
    \property QCandlestickSet::open
    \brief The open value of the candlestick item.
*/

/*!
    \qmlproperty real CandlestickSet::open
    The open value of the candlestick item.
*/

/*!
    \property QCandlestickSet::high
    \brief The high value of the candlestick item.
*/

/*!
    \qmlproperty real CandlestickSet::high
    The high value of the candlestick item.
*/

/*!
    \property QCandlestickSet::low
    \brief The low value of the candlestick item.
*/

/*!
    \qmlproperty real CandlestickSet::low
    The low value of the candlestick item.
*/

/*!
    \property QCandlestickSet::close
    \brief The close value of the candlestick item.
*/

/*!
    \qmlproperty real CandlestickSet::close
    The close value of the candlestick item.
*/

/*!
    \property QCandlestickSet::brush
    \brief The brush used to fill the candlestick item.
*/

/*!
    \property QCandlestickSet::pen
    \brief The pen used to draw the lines of the candlestick item.
*/

/*!
    \qmlproperty string CandlestickSet::brushFilename
    The name of the file used as a brush for the candlestick item.
*/

/*!
    \fn void QCandlestickSet::clicked()
    This signal is emitted when the candlestick item is clicked.
*/

/*!
    \qmlsignal CandlestickSet::clicked()
    This signal is emitted when the candlestick item is clicked.

    The corresponding signal handler is \c {onClicked}.
*/

/*!
    \fn void QCandlestickSet::hovered(bool status)
    This signal is emitted when a mouse is hovered over a candlestick
    item.

    When the mouse moves over the item, \a status turns \c true, and when the
    mouse moves away again, it turns \c false.
*/

/*!
    \qmlsignal CandlestickSet::hovered(bool status)
    This signal is emitted when a mouse is hovered over a candlestick
    item.

    When the mouse moves over the item, \a status turns \c true, and when the
    mouse moves away again, it turns \c false.

    The corresponding signal handler is \c {onHovered}.
*/

/*!
    \fn void QCandlestickSet::pressed()
    This signal is emitted when the user clicks the candlestick item and
    holds down the mouse button.
*/

/*!
    \qmlsignal CandlestickSet::pressed()
    This signal is emitted when the user clicks the candlestick item and
    holds down the mouse button.

    The corresponding signal handler is \c {onPressed}.
*/

/*!
    \fn void QCandlestickSet::released()
    This signal is emitted when the user releases the mouse press on the
    candlestick item.
*/

/*!
    \qmlsignal CandlestickSet::released()
    This signal is emitted when the user releases the mouse press on the
    candlestick item.

    The corresponding signal handler is \c {onReleased}.
*/

/*!
    \fn void QCandlestickSet::doubleClicked()
    This signal is emitted when the user double-clicks a candlestick item.
*/

/*!
    \qmlsignal CandlestickSet::doubleClicked()
    This signal is emitted when the user double-clicks a candlestick item.

    The corresponding signal handler is \c {onDoubleClicked}.
*/

/*!
    \fn void QCandlestickSet::timestampChanged()
    This signal is emitted when the candlestick item timestamp changes.
    \sa timestamp
*/

/*!
    \fn void QCandlestickSet::openChanged()
    This signal is emitted when the candlestick item open value changes.
    \sa open
*/

/*!
    \fn void QCandlestickSet::highChanged()
    This signal is emitted when the candlestick item high value changes.
    \sa high
*/

/*!
    \fn void QCandlestickSet::lowChanged()
    This signal is emitted when the candlestick item low value changes.
    \sa low
*/

/*!
    \fn void QCandlestickSet::closeChanged()
    This signal is emitted when the candlestick item close value changes.
    \sa close
*/

/*!
    \fn void QCandlestickSet::brushChanged()
    This signal is emitted when the candlestick item brush changes.
    \sa brush
*/

/*!
    \fn void QCandlestickSet::penChanged()
    This signal is emitted when the candlestick item pen changes.
    \sa pen
*/

/*!
    Constructs a candlestick item with an optional \a timestamp and a \a parent.
*/
QCandlestickSet::QCandlestickSet(qreal timestamp, QObject *parent)
    : QObject(parent),
      d_ptr(new QCandlestickSetPrivate(timestamp, this))
{
}

/*!
    Constructs a candlestick item with given ordered values. The values \a open, \a high, \a low,
    and \a close are mandatory. The values \a timestamp and \a parent are optional.
*/
QCandlestickSet::QCandlestickSet(qreal open, qreal high, qreal low, qreal close, qreal timestamp,
                                 QObject *parent)
    : QObject(parent),
      d_ptr(new QCandlestickSetPrivate(timestamp, this))
{
    Q_D(QCandlestickSet);

    d->m_open = open;
    d->m_high = high;
    d->m_low = low;
    d->m_close = close;

    emit d->updatedLayout();
}

/*!
    Destroys the candlestick item.
*/
QCandlestickSet::~QCandlestickSet()
{
}

void QCandlestickSet::setTimestamp(qreal timestamp)
{
    Q_D(QCandlestickSet);

    bool changed = d->setTimestamp(timestamp);
    if (!changed)
        return;

    emit d->updatedLayout();
    emit timestampChanged();
}

qreal QCandlestickSet::timestamp() const
{
    Q_D(const QCandlestickSet);

    return d->m_timestamp;
}

void QCandlestickSet::setOpen(qreal open)
{
    Q_D(QCandlestickSet);

    if (d->m_open == open)
        return;

    d->m_open = open;

    emit d->updatedLayout();
    emit openChanged();
}

qreal QCandlestickSet::open() const
{
    Q_D(const QCandlestickSet);

    return d->m_open;
}

void QCandlestickSet::setHigh(qreal high)
{
    Q_D(QCandlestickSet);

    if (d->m_high == high)
        return;

    d->m_high = high;

    emit d->updatedLayout();
    emit highChanged();
}

qreal QCandlestickSet::high() const
{
    Q_D(const QCandlestickSet);

    return d->m_high;
}

void QCandlestickSet::setLow(qreal low)
{
    Q_D(QCandlestickSet);

    if (d->m_low == low)
        return;

    d->m_low = low;

    emit d->updatedLayout();
    emit lowChanged();
}

qreal QCandlestickSet::low() const
{
    Q_D(const QCandlestickSet);

    return d->m_low;
}

void QCandlestickSet::setClose(qreal close)
{
    Q_D(QCandlestickSet);

    if (d->m_close == close)
        return;

    d->m_close = close;

    emit d->updatedLayout();
    emit closeChanged();
}

qreal QCandlestickSet::close() const
{
    Q_D(const QCandlestickSet);

    return d->m_close;
}

void QCandlestickSet::setBrush(const QBrush &brush)
{
    Q_D(QCandlestickSet);

    if (d->m_brush == brush)
        return;

    d->m_brush = brush;

    emit d->updatedCandlestick();
    emit brushChanged();
}

QBrush QCandlestickSet::brush() const
{
    Q_D(const QCandlestickSet);

    return d->m_brush;
}

void QCandlestickSet::setPen(const QPen &pen)
{
    Q_D(QCandlestickSet);

    if (d->m_pen == pen)
        return;

    d->m_pen = pen;

    emit d->updatedCandlestick();
    emit penChanged();
}

QPen QCandlestickSet::pen() const
{
    Q_D(const QCandlestickSet);

    return d->m_pen;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

QCandlestickSetPrivate::QCandlestickSetPrivate(qreal timestamp, QCandlestickSet *parent)
    : QObject(parent),
      q_ptr(parent),
      m_timestamp(0.0),
      m_open(0.0),
      m_high(0.0),
      m_low(0.0),
      m_close(0.0),
      m_brush(QBrush(Qt::NoBrush)),
      m_pen(QPen(Qt::NoPen)),
      m_series(nullptr)
{
    setTimestamp(timestamp);
}

QCandlestickSetPrivate::~QCandlestickSetPrivate()
{
}

bool QCandlestickSetPrivate::setTimestamp(qreal timestamp)
{
    timestamp = qMax(timestamp, qreal(0.0));
    timestamp = qRound64(timestamp);

    if (m_timestamp == timestamp)
        return false;

    m_timestamp = timestamp;

    return true;
}

#include "moc_qcandlestickset.cpp"
#include "moc_qcandlestickset_p.cpp"

QT_CHARTS_END_NAMESPACE
