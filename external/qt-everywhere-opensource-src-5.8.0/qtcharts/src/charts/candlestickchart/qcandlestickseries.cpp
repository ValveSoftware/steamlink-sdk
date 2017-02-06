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

#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QCandlestickLegendMarker>
#include <QtCharts/QCandlestickSeries>
#include <QtCharts/QCandlestickSet>
#include <QtCharts/QValueAxis>
#include <QtCore/QDateTime>
#include <private/candlestickanimation_p.h>
#include <private/candlestickchartitem_p.h>
#include <private/chartdataset_p.h>
#include <private/charttheme_p.h>
#include <private/qcandlestickseries_p.h>
#include <private/qcandlestickset_p.h>
#include <private/qchart_p.h>

QT_CHARTS_BEGIN_NAMESPACE

/*!
    \class QCandlestickSeries
    \since 5.8
    \inmodule Qt Charts
    \brief The QCandlestickSeries class presents data as candlesticks.

    This class acts as a container for single candlestick items. Each item is drawn to its own category
    when using QBarCategoryAxis. QDateTimeAxis and QValueAxis can be used as alternatives to
    QBarCategoryAxis. In this case, each candlestick item is drawn according to its timestamp value.

    \note The timestamps must be unique within a QCandlestickSeries. When using QBarCategoryAxis,
    only the first one of the candlestick items sharing a timestamp is drawn. If the chart includes
    multiple instances of QCandlestickSeries, items from different series sharing a timestamp are
    drawn to the same category. When using QValueAxis or QDateTimeAxis, candlestick items sharing a
    timestamp will overlap each other.

    See the \l {Candlestick Chart Example} {candlestick chart example} to learn how to create
    a candlestick chart.
    \image examples_candlestickchart.png

    \sa QCandlestickSet, QBarCategoryAxis, QDateTimeAxis, QValueAxis
*/

/*!
    \qmltype CandlestickSeries
    \since QtCharts 2.2
    \instantiates QCandlestickSeries
    \inqmlmodule QtCharts
    \inherits AbstractSeries
    \brief Represents a series of data as candlesticks.

    The CandlestickSeries type acts as a container for single candlestick items.
    Each item is drawn to its own category
    when using BarCategoryAxis. DateTimeAxis and ValueAxis can be used as an alternative to
    BarCategoryAxis. In this case, each candlestick item is drawn according to its timestamp value.

    \note The timestamps must be unique within a CandlestickSeries. When using BarCategoryAxis, only
    the first one of the candlestick items sharing a timestamp is drawn. If the chart includes
    multiple instances of CandlestickSeries, items from different series sharing a timestamp are
    drawn to the same category. When using ValueAxis or DateTimeAxis, candlestick items sharing a
    timestamp will overlap each other.

    The following QML shows how to create a simple candlestick chart:
    \code
    import QtQuick 2.5
    import QtCharts 2.2

    ChartView {
        title: "Candlestick Series"
        width: 400
        height: 300

        CandlestickSeries {
            name: "Acme Ltd."
            increasingColor: "green"
            decreasingColor: "red"

            CandlestickSet { timestamp: 1435708800000; open: 690; high: 694; low: 599; close: 660 }
            CandlestickSet { timestamp: 1435795200000; open: 669; high: 669; low: 669; close: 669 }
            CandlestickSet { timestamp: 1436140800000; open: 485; high: 623; low: 485; close: 600 }
            CandlestickSet { timestamp: 1436227200000; open: 589; high: 615; low: 377; close: 569 }
            CandlestickSet { timestamp: 1436313600000; open: 464; high: 464; low: 254; close: 254 }
        }
    }
    \endcode

    \beginfloatleft
    \image examples_qmlcandlestick.png
    \endfloat
    \clearfloat

    \sa CandlestickSet, BarCategoryAxis, DateTimeAxis, ValueAxis
*/

/*!
    \property QCandlestickSeries::count
    \brief The number of candlestick items in a series.
*/

/*!
    \qmlproperty int CandlestickSeries::count
    The number of candlestick items in a series.
*/

/*!
    \property QCandlestickSeries::maximumColumnWidth
    \brief The maximum width of the candlestick items in pixels. Setting a negative value means
    there is no maximum width. All negative values are converted to -1.0.
*/

/*!
    \qmlproperty real CandlestickSeries::maximumColumnWidth
    The maximum width of the candlestick items in pixels. Setting a negative value means
    there is no maximum width. All negative values are converted to -1.0.
*/

/*!
    \property QCandlestickSeries::minimumColumnWidth
    \brief The minimum width of the candlestick items in pixels. Setting a negative value means
    there is no minimum width. All negative values are converted to -1.0.
*/

/*!
    \qmlproperty real CandlestickSeries::minimumColumnWidth
    The minimum width of the candlestick items in pixels. Setting a negative value means
    there is no minimum width. All negative values are converted to -1.0.
*/

/*!
    \property QCandlestickSeries::bodyWidth
    \brief The relative width of the candlestick item within its own slot, in the range
    from 0.0 to 1.0.

    Values outside this range are clamped to 0.0 or 1.0.
*/

/*!
    \qmlproperty real CandlestickSeries::bodyWidth
    The relative width of the candlestick item within its own slot, in the range
    from 0.0 to 1.0. Values outside this range are clamped to 0.0 or 1.0.
*/

/*!
    \property QCandlestickSeries::bodyOutlineVisible
    \brief The visibility of the candlestick body outline.
*/

/*!
    \qmlproperty bool CandlestickSeries::bodyOutlineVisible
    The visibility of the candlestick body outlines.
*/

/*!
    \property QCandlestickSeries::capsWidth
    \brief The relative width of the caps within a candlestick, in the range from 0.0
    to 1.0.

    Values outside this range are clamped to 0.0 or 1.0.
*/

/*!
    \qmlproperty real CandlestickSeries::capsWidth
    The relative width of the caps within a candlestick, in the range from 0.0
    to 1.0. Values outside this range are clamped to 0.0 or 1.0.
*/

/*!
    \property QCandlestickSeries::capsVisible
    \brief The visibility of the caps.
*/

/*!
    \qmlproperty bool CandlestickSeries::capsVisible
    The visibility of the caps.
*/

/*!
    \property QCandlestickSeries::increasingColor
    \brief The color of the increasing candlestick item body.

    A candlestick is \e increasing when its close value is higher than the open
    value. By default, this property is set to the brush color. The default
    color is used also when the property is set to an invalid color value.
*/

/*!
    \qmlproperty color CandlestickSeries::increasingColor
    The color of the increasing candlestick item body.
    A candlestick is \e increasing when its close value is higher than the open
    value. By default, this property is set to the brush color. The default
    color is used also when the property is set to an invalid color value.
*/

/*!
    \property QCandlestickSeries::decreasingColor
    \brief The color of the decreasing candlestick item body.

    A candlestick is \e decreasing when its open value is higher than the close
    value. By default, this property is set to the brush color with the alpha
    channel set to 128. The default color is used also when the property is set
    to an invalid color value.
*/

/*!
    \qmlproperty color CandlestickSeries::decreasingColor
    The color of the decreasing candlestick item body.
    A candlestick is \e decreasing when its open value is higher than the close
    value. By default, this property is set to the brush color with the alpha
    channel set to 128. The default color is used also when the property is set
    to an invalid color value.
*/

/*!
    \property QCandlestickSeries::brush
    \brief The brush used to fill the candlestick items.
*/

/*!
    \property QCandlestickSeries::pen
    \brief The pen used to draw the lines of the candlestick items.
*/

/*!
    \qmlproperty string CandlestickSeries::brushFilename
    The name of the file used as a brush image for the series.
*/

/*!
    \fn void QCandlestickSeries::clicked(QCandlestickSet *set)
    This signal is emitted when the candlestick item specified by \a set
    is clicked on the chart.
*/

/*!
    \qmlsignal CandlestickSeries::clicked(CandlestickSet set)
    This signal is emitted when the candlestick item specified by \a set
    is clicked on the chart.

    The corresponding signal handler is \c {onClicked}.
*/

/*!
    \fn void QCandlestickSeries::hovered(bool status, QCandlestickSet *set)
    This signal is emitted when a mouse is hovered over the candlestick
    item specified by \a set in a chart.

    When the mouse moves over the item, \a status turns \c true, and when the
    mouse moves away again, it turns \c false.
*/

/*!
    \qmlsignal CandlestickSeries::hovered(bool status, CandlestickSet set)
    This signal is emitted when a mouse is hovered over the candlestick
    item specified by \a set in a chart.

    When the mouse moves over the item, \a status turns \c true, and when the
    mouse moves away again, it turns \c false.

    The corresponding signal handler is \c {onHovered}.
*/

/*!
    \fn void QCandlestickSeries::pressed(QCandlestickSet *set)
    This signal is emitted when the user clicks the candlestick item
    specified by \a set and holds down the mouse button.
*/

/*!
    \qmlsignal CandlestickSeries::pressed(CandlestickSet set)
    This signal is emitted when the user clicks the candlestick item
    specified by \a set and holds down the mouse button.

    The corresponding signal handler is \c {onPressed}.
*/

/*!
    \fn void QCandlestickSeries::released(QCandlestickSet *set)
    This signal is emitted when the user releases the mouse press on the
    candlestick item specified by \a set.
*/

/*!
    \qmlsignal CandlestickSeries::released(CandlestickSet set)
    This signal is emitted when the user releases the mouse press on the
    candlestick item specified by \a set.

    The corresponding signal handler is \c {onReleased}.
*/

/*!
    \fn void QCandlestickSeries::doubleClicked(QCandlestickSet *set)
    This signal is emitted when the candlestick item specified by \a set
    is double-clicked on the chart.
*/

/*!
    \qmlsignal CandlestickSeries::doubleClicked(CandlestickSet set)
    This signal is emitted when the candlestick item specified by \a set
    is double-clicked on the chart.

    The corresponding signal handler is \c {onDoubleClicked}.
*/

/*!
    \fn void QCandlestickSeries::candlestickSetsAdded(const QList<QCandlestickSet *> &sets)
    This signal is emitted when the candlestick items specified by \a
    sets are added to the series.
*/

/*!
    \qmlsignal CandlestickSeries::candlestickSetsAdded(list<CandlestickSet> sets)
    This signal is emitted when the candlestick items specified by
    \a sets are added to the series.

    The corresponding signal handler is \c {onCandlestickSetsAdded}.
*/

/*!
    \fn void QCandlestickSeries::candlestickSetsRemoved(const QList<QCandlestickSet *> &sets)
    This signal is emitted when the candlestick items specified by
    \a sets are removed from the series.
*/

/*!
    \qmlsignal CandlestickSeries::candlestickSetsRemoved(list<CandlestickSet> sets)
    This signal is emitted when the candlestick items specified by
    \a sets are removed from the series.

    The corresponding signal handler is \c {onCandlestickSetsRemoved}.
*/

/*!
    \fn void QCandlestickSeries::countChanged()
    This signal is emitted when the number of candlestick items in the
    series changes.
    \sa count
*/

/*!
    \fn void QCandlestickSeries::maximumColumnWidthChanged()
    This signal is emitted when there is a change in the maximum column width of candlestick items.
    \sa maximumColumnWidth
*/

/*!
    \fn void QCandlestickSeries::minimumColumnWidthChanged()
    This signal is emitted when there is a change in the minimum column width of candlestick items.
    \sa minimumColumnWidth
*/

/*!
    \fn void QCandlestickSeries::bodyWidthChanged()
    This signal is emitted when the candlestick item width changes.
    \sa bodyWidth
*/

/*!
    \fn void QCandlestickSeries::bodyOutlineVisibilityChanged()
    This signal is emitted when the visibility of the candlestick item body outline changes.
    \sa bodyOutlineVisible
*/

/*!
    \fn void QCandlestickSeries::capsWidthChanged()
    This signal is emitted when the candlestick item caps width changes.
    \sa capsWidth
*/

/*!
    \fn void QCandlestickSeries::capsVisibilityChanged()
    This signal is emitted when the visibility of the candlestick item caps changes.
    \sa capsVisible
*/

/*!
    \fn void QCandlestickSeries::increasingColorChanged()
    This signal is emitted when the candlestick item increasing color changes.
    \sa increasingColor
*/

/*!
    \fn void QCandlestickSeries::decreasingColorChanged()
    This signal is emitted when the candlestick item decreasing color changes.
    \sa decreasingColor
*/

/*!
    \fn void QCandlestickSeries::brushChanged()
    This signal is emitted when the candlestick item brush changes.

    \sa brush
*/

/*!
    \fn void QCandlestickSeries::penChanged()
    This signal is emitted when the candlestick item pen changes.

    \sa pen
*/

/*!
    \qmlmethod CandlestickSeries::at(int index)
    Returns the candlestick item at the position specified by \a index. Returns
    null if the index is not valid.
*/

/*!
    Constructs an empty QCandlestickSeries. The \a parent is optional.
*/
QCandlestickSeries::QCandlestickSeries(QObject *parent)
    : QAbstractSeries(*new QCandlestickSeriesPrivate(this), parent)
{
}

/*!
    Destroys the series. Removes the series from the chart.
*/
QCandlestickSeries::~QCandlestickSeries()
{
    Q_D(QCandlestickSeries);
    if (d->m_chart)
        d->m_chart->removeSeries(this);
}

/*!
    \qmlmethod CandlestickSeries::append(CandlestickSet set)
    Adds a single candlestick item specified by \a set to the series and takes
    ownership of it. If the item is null or it is already in the series, it
    is not appended.

    Returns \c true if appending succeeded, \c false otherwise.
*/

/*!
    Adds a single candlestick item specified by \a set to the series and takes
    ownership of it. If the item is null or it is already in the series, it
    is not appended.
    Returns \c true if appending succeeded, \c false otherwise.
*/
bool QCandlestickSeries::append(QCandlestickSet *set)
{
    QList<QCandlestickSet *> sets;
    sets.append(set);

    return append(sets);
}

/*!
    \qmlmethod CandlestickSeries::remove(CandlestickSet set)
    Removes a single candlestick item, specified by \a set, from the series.

    Returns \c true if the item is successfully deleted, \c false otherwise.
*/

/*!
    Removes a single candlestick item, specified by \a set, from the series.
    Returns \c true if the item is successfully deleted, \c false otherwise.
*/
bool QCandlestickSeries::remove(QCandlestickSet *set)
{
    QList<QCandlestickSet *> sets;
    sets.append(set);

    return remove(sets);
}

/*!
    Adds a list of candlestick items specified by \a sets to the series and
    takes ownership of it. If any of the items are null, already belong to
    the series, or appear in the list more than once, nothing is appended.
    Returns \c true if all items were appended successfully, \c false otherwise.
*/
bool QCandlestickSeries::append(const QList<QCandlestickSet *> &sets)
{
    Q_D(QCandlestickSeries);

    bool success = d->append(sets);
    if (success) {
        emit candlestickSetsAdded(sets);
        emit countChanged();
    }

    return success;
}

/*!
    Removes a list of candlestick items specified by \a sets from the series. If
    any of the items are null, were already removed from the series, or appear
    in the list more than once, nothing is removed. Returns \c true if all items
    were removed successfully, \c false otherwise.
*/
bool QCandlestickSeries::remove(const QList<QCandlestickSet *> &sets)
{
    Q_D(QCandlestickSeries);

    bool success = d->remove(sets);
    if (success) {
        emit candlestickSetsRemoved(sets);
        emit countChanged();
        foreach (QCandlestickSet *set, sets)
            set->deleteLater();
    }

    return success;
}

/*!
    \qmlmethod CandlestickSeries::insert(int index, CandlestickSet set)
    Inserts the candlestick item specified by \a set to the series at the
    position specified by \a index. Takes ownership of the item. If the
    item is null or already belongs to the series, it is not inserted.

    Returns \c true if inserting succeeded, \c false otherwise.
*/

/*!
    Inserts the candlestick item specified by \a set to the series at the
    position specified by \a index. Takes ownership of the item. If the
    item is null or already belongs to the series, it is not inserted.
    Returns \c true if inserting succeeded, \c false otherwise.
*/
bool QCandlestickSeries::insert(int index, QCandlestickSet *set)
{
    Q_D(QCandlestickSeries);

    bool success = d->insert(index, set);
    if (success) {
        QList<QCandlestickSet *> sets;
        sets.append(set);
        emit candlestickSetsAdded(sets);
        emit countChanged();
    }

    return success;
}

/*!
    Takes a single candlestick item, specified by \a set, from the series. Does
    not delete the item. Returns \c true if the take operation was successful, \c false otherwise.
    \note The series remains the item's parent object. You must set the parent
    object to take full ownership.
*/
bool QCandlestickSeries::take(QCandlestickSet *set)
{
    Q_D(QCandlestickSeries);

    QList<QCandlestickSet *> sets;
    sets.append(set);

    bool success = d->remove(sets);
    if (success) {
        emit candlestickSetsRemoved(sets);
        emit countChanged();
    }

    return success;
}

/*!
    \qmlmethod CandlestickSeries::clear()
    Removes all candlestick items from the series and permanently deletes them.
*/

/*!
    Removes all candlestick items from the series and permanently deletes them.
*/
void QCandlestickSeries::clear()
{
    Q_D(QCandlestickSeries);

    QList<QCandlestickSet *> sets = this->sets();

    bool success = d->remove(sets);
    if (success) {
        emit candlestickSetsRemoved(sets);
        emit countChanged();
        foreach (QCandlestickSet *set, sets)
            set->deleteLater();
    }
}

/*!
    Returns the list of candlestick items in the series. Ownership of the
    items does not change.
 */
QList<QCandlestickSet *> QCandlestickSeries::sets() const
{
    Q_D(const QCandlestickSeries);

    return d->m_sets;
}

/*!
    Returns the number of the candlestick items in the series.
*/
int QCandlestickSeries::count() const
{
    return sets().count();
}

/*!
    Returns the type of the series (QAbstractSeries::SeriesTypeCandlestick).
*/
QAbstractSeries::SeriesType QCandlestickSeries::type() const
{
    return QAbstractSeries::SeriesTypeCandlestick;
}

void QCandlestickSeries::setMaximumColumnWidth(qreal maximumColumnWidth)
{
    Q_D(QCandlestickSeries);

    if (maximumColumnWidth < 0.0 && maximumColumnWidth != -1.0)
        maximumColumnWidth = -1.0;

    if (d->m_maximumColumnWidth == maximumColumnWidth)
        return;

    d->m_maximumColumnWidth = maximumColumnWidth;

    emit d->updatedLayout();
    emit maximumColumnWidthChanged();
}

qreal QCandlestickSeries::maximumColumnWidth() const
{
    Q_D(const QCandlestickSeries);

    return d->m_maximumColumnWidth;
}

void QCandlestickSeries::setMinimumColumnWidth(qreal minimumColumnWidth)
{
    Q_D(QCandlestickSeries);

    if (minimumColumnWidth < 0.0 && minimumColumnWidth != -1.0)
        minimumColumnWidth = -1.0;

    if (d->m_minimumColumnWidth == minimumColumnWidth)
        return;

    d->m_minimumColumnWidth = minimumColumnWidth;

    d->updatedLayout();
    emit minimumColumnWidthChanged();
}

qreal QCandlestickSeries::minimumColumnWidth() const
{
    Q_D(const QCandlestickSeries);

    return d->m_minimumColumnWidth;
}

void QCandlestickSeries::setBodyWidth(qreal bodyWidth)
{
    Q_D(QCandlestickSeries);

    if (bodyWidth < 0.0)
        bodyWidth = 0.0;
    else if (bodyWidth > 1.0)
        bodyWidth = 1.0;

    if (d->m_bodyWidth == bodyWidth)
        return;

    d->m_bodyWidth = bodyWidth;

    emit d->updatedLayout();
    emit bodyWidthChanged();
}

qreal QCandlestickSeries::bodyWidth() const
{
    Q_D(const QCandlestickSeries);

    return d->m_bodyWidth;
}

void QCandlestickSeries::setBodyOutlineVisible(bool bodyOutlineVisible)
{
    Q_D(QCandlestickSeries);

    if (d->m_bodyOutlineVisible == bodyOutlineVisible)
        return;

    d->m_bodyOutlineVisible = bodyOutlineVisible;

    emit d->updated();
    emit bodyOutlineVisibilityChanged();
}

bool QCandlestickSeries::bodyOutlineVisible() const
{
    Q_D(const QCandlestickSeries);

    return d->m_bodyOutlineVisible;
}

void QCandlestickSeries::setCapsWidth(qreal capsWidth)
{
    Q_D(QCandlestickSeries);

    if (capsWidth < 0.0)
        capsWidth = 0.0;
    else if (capsWidth > 1.0)
        capsWidth = 1.0;

    if (d->m_capsWidth == capsWidth)
        return;

    d->m_capsWidth = capsWidth;

    emit d->updatedLayout();
    emit capsWidthChanged();
}

qreal QCandlestickSeries::capsWidth() const
{
    Q_D(const QCandlestickSeries);

    return d->m_capsWidth;
}

void QCandlestickSeries::setCapsVisible(bool capsVisible)
{
    Q_D(QCandlestickSeries);

    if (d->m_capsVisible == capsVisible)
        return;

    d->m_capsVisible = capsVisible;

    emit d->updated();
    emit capsVisibilityChanged();
}

bool QCandlestickSeries::capsVisible() const
{
    Q_D(const QCandlestickSeries);

    return d->m_capsVisible;
}

void QCandlestickSeries::setIncreasingColor(const QColor &increasingColor)
{
    Q_D(QCandlestickSeries);

    QColor color;
    if (increasingColor.isValid()) {
        color = increasingColor;
        d->m_customIncreasingColor = true;
    } else {
        color = d->m_brush.color();
        color.setAlpha(128);
        d->m_customIncreasingColor = false;
    }

    if (d->m_increasingColor == color)
        return;

    d->m_increasingColor = color;

    emit d->updated();
    emit increasingColorChanged();
}

QColor QCandlestickSeries::increasingColor() const
{
    Q_D(const QCandlestickSeries);

    return d->m_increasingColor;
}

void QCandlestickSeries::setDecreasingColor(const QColor &decreasingColor)
{
    Q_D(QCandlestickSeries);

    QColor color;
    if (decreasingColor.isValid()) {
        color = decreasingColor;
        d->m_customDecreasingColor = true;
    } else {
        color = d->m_brush.color();
        d->m_customDecreasingColor = false;
    }

    if (d->m_decreasingColor == color)
        return;

    d->m_decreasingColor = color;

    emit d->updated();
    emit decreasingColorChanged();
}

QColor QCandlestickSeries::decreasingColor() const
{
    Q_D(const QCandlestickSeries);

    return d->m_decreasingColor;
}

void QCandlestickSeries::setBrush(const QBrush &brush)
{
    Q_D(QCandlestickSeries);

    if (d->m_brush == brush)
        return;

    d->m_brush = brush;
    if (!d->m_customIncreasingColor) {
        QColor color = d->m_brush.color();
        color.setAlpha(128);
        if (d->m_increasingColor != color) {
            d->m_increasingColor = color;
            emit increasingColorChanged();
        }
    }
    if (!d->m_customDecreasingColor && d->m_decreasingColor != d->m_brush.color()) {
        d->m_decreasingColor = d->m_brush.color();
        emit decreasingColorChanged();
    }

    emit d->updated();
    emit brushChanged();
}

QBrush QCandlestickSeries::brush() const
{
    Q_D(const QCandlestickSeries);

    return d->m_brush;
}

void QCandlestickSeries::setPen(const QPen &pen)
{
    Q_D(QCandlestickSeries);

    if (d->m_pen == pen)
        return;

    d->m_pen = pen;

    emit d->updated();
    emit penChanged();
}

QPen QCandlestickSeries::pen() const
{
    Q_D(const QCandlestickSeries);

    return d->m_pen;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

QCandlestickSeriesPrivate::QCandlestickSeriesPrivate(QCandlestickSeries *q)
    : QAbstractSeriesPrivate(q),
      m_maximumColumnWidth(-1.0),
      m_minimumColumnWidth(5.0),
      m_bodyWidth(0.5),
      m_bodyOutlineVisible(true),
      m_capsWidth(0.5),
      m_capsVisible(false),
      m_increasingColor(QColor(Qt::transparent)),
      m_decreasingColor(QChartPrivate::defaultBrush().color()),
      m_customIncreasingColor(false),
      m_customDecreasingColor(false),
      m_brush(QChartPrivate::defaultBrush()),
      m_pen(QChartPrivate::defaultPen()),
      m_animation(nullptr)
{
}

QCandlestickSeriesPrivate::~QCandlestickSeriesPrivate()
{
    disconnect(this, 0, 0, 0);
}

void QCandlestickSeriesPrivate::initializeDomain()
{
    qreal minX(domain()->minX());
    qreal maxX(domain()->maxX());
    qreal minY(domain()->minY());
    qreal maxY(domain()->maxY());

    if (m_sets.count()) {
        QCandlestickSet *set = m_sets.first();
        minX = set->timestamp();
        maxX = set->timestamp();
        minY = set->low();
        maxY = set->high();
        for (int i = 1; i < m_sets.count(); ++i) {
            set = m_sets.at(i);
            minX = qMin(minX, qreal(set->timestamp()));
            maxX = qMax(maxX, qreal(set->timestamp()));
            minY = qMin(minY, set->low());
            maxY = qMax(maxY, set->high());
        }
        qreal extra = (maxX - minX) / m_sets.count() / 2;
        minX = minX - extra;
        maxX = maxX + extra;
    }

    domain()->setRange(minX, maxX, minY, maxY);
}

void QCandlestickSeriesPrivate::initializeAxes()
{
    foreach (QAbstractAxis* axis, m_axes) {
        if (axis->type() == QAbstractAxis::AxisTypeBarCategory) {
            if (axis->orientation() == Qt::Horizontal)
                populateBarCategories(qobject_cast<QBarCategoryAxis *>(axis));
        }
    }
}

void QCandlestickSeriesPrivate::initializeTheme(int index, ChartTheme* theme, bool forced)
{
    Q_Q(QCandlestickSeries);

    if (forced || QChartPrivate::defaultBrush() == m_brush) {
        const QList<QGradient> gradients = theme->seriesGradients();
        const QGradient gradient = gradients.at(index % gradients.size());
        const QBrush brush(ChartThemeManager::colorAt(gradient, 0.5));
        q->setBrush(brush);
    }

    if (forced || QChartPrivate::defaultPen() == m_pen) {
        QPen pen = theme->outlinePen();
        pen.setCosmetic(true);
        q->setPen(pen);
    }
}

void QCandlestickSeriesPrivate::initializeGraphics(QGraphicsItem *parent)
{
    Q_Q(QCandlestickSeries);

    CandlestickChartItem *item = new CandlestickChartItem(q, parent);
    m_item.reset(item);
    QAbstractSeriesPrivate::initializeGraphics(parent);

    if (m_chart) {
        connect(m_chart->d_ptr->m_dataset, SIGNAL(seriesAdded(QAbstractSeries *)),
                this, SLOT(handleSeriesChange(QAbstractSeries *)));
        connect(m_chart->d_ptr->m_dataset, SIGNAL(seriesRemoved(QAbstractSeries *)),
                this, SLOT(handleSeriesRemove(QAbstractSeries *)));

        item->handleCandlestickSeriesChange();
    }
}

void QCandlestickSeriesPrivate::initializeAnimations(QChart::AnimationOptions options, int duration,
                                                     QEasingCurve &curve)
{
    CandlestickChartItem *item = static_cast<CandlestickChartItem *>(m_item.data());
    Q_ASSERT(item);

    if (item->animation())
        item->animation()->stopAndDestroyLater();

    if (options.testFlag(QChart::SeriesAnimations))
        m_animation = new CandlestickAnimation(item, duration, curve);
    else
        m_animation = nullptr;
    item->setAnimation(m_animation);

    QAbstractSeriesPrivate::initializeAnimations(options, duration, curve);
}

QList<QLegendMarker *> QCandlestickSeriesPrivate::createLegendMarkers(QLegend *legend)
{
    Q_Q(QCandlestickSeries);

    QList<QLegendMarker *> list;

    return list << new QCandlestickLegendMarker(q, legend);
}

QAbstractAxis::AxisType QCandlestickSeriesPrivate::defaultAxisType(Qt::Orientation orientation) const
{
    if (orientation == Qt::Horizontal)
        return QAbstractAxis::AxisTypeBarCategory;

    if (orientation == Qt::Vertical)
        return QAbstractAxis::AxisTypeValue;

    return QAbstractAxis::AxisTypeNoAxis;
}

QAbstractAxis* QCandlestickSeriesPrivate::createDefaultAxis(Qt::Orientation orientation) const
{
    const QAbstractAxis::AxisType axisType = defaultAxisType(orientation);

    if (axisType == QAbstractAxis::AxisTypeBarCategory)
        return new QBarCategoryAxis;

    if (axisType == QAbstractAxis::AxisTypeValue)
        return new QValueAxis;

    return 0; // axisType == QAbstractAxis::AxisTypeNoAxis
}

bool QCandlestickSeriesPrivate::append(const QList<QCandlestickSet *> &sets)
{
    foreach (QCandlestickSet *set, sets) {
        if ((set == 0) || m_sets.contains(set) || set->d_ptr->m_series)
            return false; // Fail if any of the sets is null or is already appended.
        if (sets.count(set) != 1)
            return false; // Also fail if the same set occurs more than once in the given list.
    }

    foreach (QCandlestickSet *set, sets) {
        m_sets.append(set);
        connect(set->d_func(), SIGNAL(updatedLayout()), this, SIGNAL(updatedLayout()));
        connect(set->d_func(), SIGNAL(updatedCandlestick()), this, SIGNAL(updatedCandlesticks()));
        set->d_ptr->m_series = this;
    }

    return true;
}

bool QCandlestickSeriesPrivate::remove(const QList<QCandlestickSet *> &sets)
{
    if (sets.count() == 0)
        return false;

    foreach (QCandlestickSet *set, sets) {
        if ((set == 0) || (!m_sets.contains(set)))
            return false; // Fail if any of the sets is null or is not in series.
        if (sets.count(set) != 1)
            return false; // Also fail if the same set occurs more than once in the given list.
    }

    foreach (QCandlestickSet *set, sets) {
        set->d_ptr->m_series = nullptr;
        m_sets.removeOne(set);
        disconnect(set->d_func(), SIGNAL(updatedLayout()), this, SIGNAL(updatedLayout()));
        disconnect(set->d_func(), SIGNAL(updatedCandlestick()),this, SIGNAL(updatedCandlesticks()));
    }

    return true;
}

bool QCandlestickSeriesPrivate::insert(int index, QCandlestickSet *set)
{
    if ((m_sets.contains(set)) || (set == 0) || set->d_ptr->m_series)
        return false; // Fail if set is already in list or set is null.

    m_sets.insert(index, set);
    connect(set->d_func(), SIGNAL(updatedLayout()), this, SIGNAL(updatedLayout()));
    connect(set->d_func(), SIGNAL(updatedCandlestick()), this, SIGNAL(updatedCandlesticks()));
    set->d_ptr->m_series = this;

    return true;
}

void QCandlestickSeriesPrivate::handleSeriesChange(QAbstractSeries *series)
{
    Q_UNUSED(series);

    if (m_chart) {
        CandlestickChartItem *item = static_cast<CandlestickChartItem *>(m_item.data());
        if (item)
            item->handleCandlestickSeriesChange();
    }
}

void QCandlestickSeriesPrivate::handleSeriesRemove(QAbstractSeries *series)
{
    Q_Q(const QCandlestickSeries);

    QCandlestickSeries *removedSeries = static_cast<QCandlestickSeries *>(series);

    if (q == removedSeries && m_animation) {
        m_animation->stopAll();
        disconnect(m_chart->d_ptr->m_dataset, 0, removedSeries->d_func(), 0);
    }

    if (q != removedSeries) {
        CandlestickChartItem *item = static_cast<CandlestickChartItem *>(m_item.data());
        if (item)
            item->handleCandlestickSeriesChange();
    }
}

void QCandlestickSeriesPrivate::populateBarCategories(QBarCategoryAxis *axis)
{
    if (axis->categories().isEmpty()) {
        QStringList categories;
        for (int i = 0; i < m_sets.count(); ++i) {
            const qint64 timestamp = qRound64(m_sets.at(i)->timestamp());
            const QString timestampFormat = m_chart->locale().dateTimeFormat(QLocale::ShortFormat);
            categories << QDateTime::fromMSecsSinceEpoch(timestamp).toString(timestampFormat);
        }
        axis->append(categories);
    }
}

#include "moc_qcandlestickseries.cpp"
#include "moc_qcandlestickseries_p.cpp"

QT_CHARTS_END_NAMESPACE
