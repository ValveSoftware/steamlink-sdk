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

#include <QtCharts/QHXYModelMapper>

QT_CHARTS_BEGIN_NAMESPACE

/*!
    \class QHXYModelMapper
    \inmodule Qt Charts
    \brief The QHXYModelMapper class is a horizontal model mapper for line,
    spline, and scatter series.

    Model mappers enable using a data model derived from the QAbstractItemModel
    class as a data source for a chart. A horizontal model mapper is used to
    create a connection between a line, spline, or scatter series and the data
    model that holds the consecutive data point coordinates on rows.

    Both model and series properties can be used to manipulate the data. The
    model mapper keeps the series and the data model in sync.

    \sa QVXYModelMapper, QXYSeries
*/
/*!
    \qmltype HXYModelMapper
    \instantiates QHXYModelMapper
    \inqmlmodule QtCharts

    \brief A horizontal model mapper for XYSeries.

    Model mappers enable using a data model derived from the QAbstractItemModel
    class as a data source for a chart. A horizontal model mapper is used to
    create a connection between a line, spline, or scatter series and the data
    model that holds the consecutive data point coordinates on rows.

    Both model and series properties can be used to manipulate the data. The
    model mapper keeps the series and the data model in sync.

    \sa VXYModelMapper, XYSeries
*/

/*!
    \property QHXYModelMapper::series
    \brief The series that is used by the mapper.

    All the data in the series is discarded when it is set to the mapper.
    When a new series is specified, the old series is disconnected (but it
    preserves its data).
*/
/*!
    \qmlproperty XYSeries HXYModelMapper::series
    The series that is used by the mapper. All the data in the series is
    discarded when it is set to the mapper. When a new series is specified, the
    old series is disconnected (but it preserves its data).
*/

/*!
    \property QHXYModelMapper::model
    \brief The model that is used by the mapper.
*/
/*!
    \qmlproperty SomeModel HXYModelMapper::model
    The data model that is used by the mapper. You need to implement the model
    and expose it to QML.

    \note The model has to support adding and removing rows or columns and
    modifying the data in the cells.
*/

/*!
    \property QHXYModelMapper::xRow
    \brief The row of the model that contains the x-coordinates of the data
    points.

    The default value is -1 (invalid mapping).
*/
/*!
    \qmlproperty int HXYModelMapper::xRow
    The row of the model that contains the x-coordinates of the data points.
    The default value is -1 (invalid mapping).
*/

/*!
    \property QHXYModelMapper::yRow
    \brief The row of the model that contains the y-coordinates of the data
    points.

    The default value is -1 (invalid mapping).
*/
/*!
    \qmlproperty int HXYModelMapper::yRow
    The row of the model that contains the y-coordinates of the data points.

    The default value is -1 (invalid mapping).
*/

/*!
    \property QHXYModelMapper::firstColumn
    \brief The column of the model that contains the data for the first point of the series.

    The minimum and default value is 0.
*/
/*!
    \qmlproperty int HXYModelMapper::firstColumn
    The column of the model that contains the data for the first point of the series.
    The default value is 0.
*/

/*!
    \property QHXYModelMapper::columnCount
    \brief The number of columns of the model that are mapped as the data for series.

    The minimum and default value is -1 (the number is limited by the number of
    columns in the model).
*/
/*!
    \qmlproperty int HXYModelMapper::columnCount
    The number of columns of the model that are mapped as the data for series.
    The default value is -1 (the number is limited by the number of columns in
    the model).
*/

/*!
    \fn void QHXYModelMapper::seriesReplaced()

    This signal is emitted when the series that the mapper is connected to
    changes.
*/

/*!
    \fn void QHXYModelMapper::modelReplaced()

    This signal is emitted when the model that the mapper is connected to
    changes.
*/

/*!
    \fn void QHXYModelMapper::xRowChanged()

    This signal is emitted when the row that contains the x-coordinates of data
    points changes.
*/

/*!
    \fn void QHXYModelMapper::yRowChanged()

    This signal is emitted when the row that contains the y-coordinates of data
    points changes.
*/

/*!
    \fn void QHXYModelMapper::firstColumnChanged()
    This signal is emitted when the first column changes.
*/

/*!
    \fn void QHXYModelMapper::columnCountChanged()
    This signal is emitted when the number of columns changes.
*/

/*!
    Constructs a mapper object that is a child of \a parent.
*/
QHXYModelMapper::QHXYModelMapper(QObject *parent) :
    QXYModelMapper(parent)
{
    QXYModelMapper::setOrientation(Qt::Horizontal);
}

QAbstractItemModel *QHXYModelMapper::model() const
{
    return QXYModelMapper::model();
}

void QHXYModelMapper::setModel(QAbstractItemModel *model)
{
    if (model != QXYModelMapper::model()) {
        QXYModelMapper::setModel(model);
        emit modelReplaced();
    }
}

QXYSeries *QHXYModelMapper::series() const
{
    return QXYModelMapper::series();
}

void QHXYModelMapper::setSeries(QXYSeries *series)
{
    if (series != QXYModelMapper::series()) {
        QXYModelMapper::setSeries(series);
        emit seriesReplaced();
    }
}

int QHXYModelMapper::xRow() const
{
    return QXYModelMapper::xSection();
}

void QHXYModelMapper::setXRow(int xRow)
{
    if (xRow != xSection()) {
        QXYModelMapper::setXSection(xRow);
        emit xRowChanged();
    }
}

int QHXYModelMapper::yRow() const
{
    return QXYModelMapper::ySection();
}

void QHXYModelMapper::setYRow(int yRow)
{
    if (yRow != ySection()) {
        QXYModelMapper::setYSection(yRow);
        emit yRowChanged();
    }
}

int QHXYModelMapper::firstColumn() const
{
    return first();
}

void QHXYModelMapper::setFirstColumn(int firstColumn)
{
    if (firstColumn != first()) {
        setFirst(firstColumn);
        emit firstColumnChanged();
    }
}

int QHXYModelMapper::columnCount() const
{
    return count();
}

void QHXYModelMapper::setColumnCount(int columnCount)
{
    if (columnCount != count()) {
        setCount(columnCount);
        emit columnCountChanged();
    }
}

#include "moc_qhxymodelmapper.cpp"

QT_CHARTS_END_NAMESPACE
