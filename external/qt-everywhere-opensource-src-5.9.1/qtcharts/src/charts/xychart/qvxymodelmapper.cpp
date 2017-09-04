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

#include <QtCharts/QVXYModelMapper>

QT_CHARTS_BEGIN_NAMESPACE

/*!
    \class QVXYModelMapper
    \inmodule Qt Charts
    \brief The QVXYModelMapper class is a vertical model mapper for line,
    spline, and scatter series.

    Model mappers enable using a data model derived from the QAbstractItemModel
    class as a data source for a chart. A vertical model mapper is used to
    create a connection between a line, spline, or scatter series and the data
    model that holds the consecutive data point coordinates in columns.

    Both model and series properties can be used to manipulate the data. The
    model mapper keeps the series and the data model in sync.

    \sa QHXYModelMapper, QXYSeries
*/
/*!
    \qmltype VXYModelMapper
    \instantiates QVXYModelMapper
    \inqmlmodule QtCharts

    \brief A vertical model mapper for XYSeries.

    Model mappers enable using a data model derived from the QAbstractItemModel
    class as a data source for a chart. A vertical model mapper is used to
    create a connection between a line, spline, or scatter series and the data
    model that holds the consecutive data point coordinates in columns.

    Both model and series properties can be used to manipulate the data. The
    model mapper keeps the series and the data model in sync.

   \sa HXYModelMapper, XYSeries
*/

/*!
    \property QVXYModelMapper::series
    \brief The series that is used by the mapper.

    All the data in the series is discarded when it is set to the mapper.
    When a new series is specified, the old series is disconnected (but it
    preserves its data).
*/
/*!
    \qmlproperty XYSeries VXYModelMapper::series
    The series that is used by the mapper. All the data in the series is
    discarded when it is set to the mapper. When a new series is specified, the
    old series is disconnected (but it preserves its data).
*/

/*!
    \property QVXYModelMapper::model
    \brief The model that is used by the mapper.
*/
/*!
    \qmlproperty SomeModel VXYModelMapper::model
    The data model that is used by the mapper. You need to implement the model
    and expose it to QML.

    \note The model has to support adding and removing rows or columns and
    modifying the data in the cells.
*/

/*!
    \property QVXYModelMapper::xColumn
    \brief The column of the model that contains the x-coordinates of data
    points.

    The default value is -1 (invalid mapping).
*/
/*!
    \qmlproperty int VXYModelMapper::xColumn
    The column of the model that contains the x-coordinates of data points.
    The default value is -1 (invalid mapping).
*/

/*!
    \property QVXYModelMapper::yColumn
    \brief The column of the model that contains the y-coordinates of data
    points.

    The default value is -1 (invalid mapping).
*/
/*!
    \qmlproperty int VXYModelMapper::yColumn
    The column of the model that contains the y-coordinates of data points.
    The default value is -1 (invalid mapping).
*/

/*!
    \property QVXYModelMapper::firstRow
    \brief The row of the model that contains the data for the first point
    of the series.

    The minimum and default value is 0.
*/
/*!
    \qmlproperty int VXYModelMapper::firstRow
    The row of the model that contains the data for the first point of the series.
    The default value is 0.
*/

/*!
    \property QVXYModelMapper::rowCount
    \brief The number of rows of the model that are mapped as the data for series.

    The minimum and default value is -1 (the number is limited by the number of
    rows in the model).
*/
/*!
    \qmlproperty int VXYModelMapper::rowCount
    The number of rows of the model that are mapped as the data for series. The default value is
    -1 (the number is limited by the number of rows in the model).
*/

/*!
    \fn void QVXYModelMapper::seriesReplaced()

    This signal is emitted when the series that the mapper is connected to changes.
*/

/*!
    \fn void QVXYModelMapper::modelReplaced()

    This signal is emitted when the model that the mapper is connected to changes.
*/

/*!
    \fn void QVXYModelMapper::xColumnChanged()

    This signal is emitted when the column that contains the x-coordinates of
    data points changes.
*/

/*!
    \fn void QVXYModelMapper::yColumnChanged()

    This signal is emitted when the column that contains the y-coordinates of
    data points changes.
*/

/*!
    \fn void QVXYModelMapper::firstRowChanged()
    This signal is emitted when the first row changes.
*/

/*!
    \fn void QVXYModelMapper::rowCountChanged()
    This signal is emitted when the number of rows changes.
*/

/*!
    Constructs a mapper object that is a child of \a parent.
*/
QVXYModelMapper::QVXYModelMapper(QObject *parent) :
    QXYModelMapper(parent)
{
    QXYModelMapper::setOrientation(Qt::Vertical);
}

QAbstractItemModel *QVXYModelMapper::model() const
{
    return QXYModelMapper::model();
}

void QVXYModelMapper::setModel(QAbstractItemModel *model)
{
    if (model != QXYModelMapper::model()) {
        QXYModelMapper::setModel(model);
        emit modelReplaced();
    }
}

QXYSeries *QVXYModelMapper::series() const
{
    return QXYModelMapper::series();
}

void QVXYModelMapper::setSeries(QXYSeries *series)
{
    if (series != QXYModelMapper::series()) {
        QXYModelMapper::setSeries(series);
        emit seriesReplaced();
    }
}

int QVXYModelMapper::xColumn() const
{
    return QXYModelMapper::xSection();
}

void QVXYModelMapper::setXColumn(int xColumn)
{
    if (xColumn != xSection()) {
        QXYModelMapper::setXSection(xColumn);
        emit xColumnChanged();
    }
}

int QVXYModelMapper::yColumn() const
{
    return QXYModelMapper::ySection();
}

void QVXYModelMapper::setYColumn(int yColumn)
{
    if (yColumn != ySection()) {
        QXYModelMapper::setYSection(yColumn);
        emit yColumnChanged();
    }
}

int QVXYModelMapper::firstRow() const
{
    return first();
}

void QVXYModelMapper::setFirstRow(int firstRow)
{
    if (firstRow != first()) {
        setFirst(firstRow);
        emit firstRowChanged();
    }
}

int QVXYModelMapper::rowCount() const
{
    return count();
}

void QVXYModelMapper::setRowCount(int rowCount)
{
    if (rowCount != count()) {
        setCount(rowCount);
        emit rowCountChanged();
    }
}

#include "moc_qvxymodelmapper.cpp"

QT_CHARTS_END_NAMESPACE
