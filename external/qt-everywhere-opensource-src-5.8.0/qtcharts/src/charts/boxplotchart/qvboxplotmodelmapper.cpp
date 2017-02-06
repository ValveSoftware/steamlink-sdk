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

#include <QtCharts/QVBoxPlotModelMapper>

QT_CHARTS_BEGIN_NAMESPACE

/*!
    \class QVBoxPlotModelMapper
    \inmodule Qt Charts
    \brief The QVBoxPlotModelMapper is a vertical model mapper for box plot
    series.

    Model mappers enable using a data model derived from the QAbstractItemModel class
    as a data source for a chart. A vertical model mapper is used to create a connection
    between a data model and a QBoxPlotSeries object, so that each column in the data model
    defines a box-and-whiskers item and each row maps to the range and three median
    values of the box-and-whiskers item.

    Both model and series properties can be used to manipulate the data. The model mapper
    keeps the series and the data model in sync.

    The model mapper ensures that all the bar box-and-whiskers items in the box plot
    series have equal sizes. Therefore, adding or removing a value from a box-and-whiskers
    item causes the same change to be made in all the box-and-whiskers items in the
    box plot series.

    \sa QHBoxPlotModelMapper
*/
/*!
    \qmltype VBoxPlotModelMapper
    \instantiates QVBoxPlotModelMapper
    \inqmlmodule QtCharts

    \brief Vertical model mapper for box plot series.

    The VBoxPlotModelMapper type enables using a data model derived from the QAbstractItemModel
    class as a data source for a chart. A vertical model mapper is used to create a connection
    between a data model and a BoxPlotSeries type, so that each column in the data model
    defines a box-and-whiskers item and each row maps to the range and three median values of
    the box-and-whiskers item.

    Both model and series properties can be used to manipulate the data. The model mapper
    keeps the series and the data model in sync.

    The model mapper ensures that all the bar box-and-whiskers items in the box plot
    series have equal sizes. Therefore, adding or removing a value from a box-and-whiskers
    item causes the same change to be made in all the box-and-whiskers items in the
    box plot series.

    The following QML code snippet creates a box plot series with three box-and-whiskers items
    (assuming the model has at least four columns). Each box-and-whiskers item contains
    data starting from row 1. The name of an item is defined by the column header.
    \code
        BoxPlotSeries {
            VBoxPlotModelMapper {
                model: myCustomModel // QAbstractItemModel derived implementation
                firstBoxSetColumn: 1
                lastBoxSetColumn: 3
                firstRow: 1
            }
        }
    \endcode

   \sa HBoxPlotModelMapper
*/

/*!
    \property QVBoxPlotModelMapper::series
    \brief The box plot series that is used by the mapper.

    All the data in the series is discarded when it is set to the mapper.
    When a new series is specified, the old series is disconnected (but it
    preserves its data).
*/
/*!
    \qmlproperty AbstractBarSeries VBoxPlotModelMapper::series
    The box plot series that is used by the mapper.

    All the data in the series is discarded when it is set to the mapper.
    When a new series is specified, the old series is disconnected (but it
    preserves its data).
*/

/*!
    \property QVBoxPlotModelMapper::model
    \brief The model that is used by the mapper.
*/
/*!
    \qmlproperty SomeModel VBoxPlotModelMapper::model
    The data model that is used by the mapper. You need to implement the model
    and expose it to QML.

    \note The model has to support adding and removing rows or columns and
    modifying the data in the cells.
*/

/*!
    \property QVBoxPlotModelMapper::firstBoxSetColumn
    \brief The column of the model that is used as the data source for the first
    box-and-whiskers item.

    The default value is -1 (invalid mapping).
*/
/*!
    \qmlproperty int VBoxPlotModelMapper::firstBoxSetColumn
    The column of the model that is used as the data source for the first
    box-and-whiskers item. The default value is -1 (invalid mapping).
*/

/*!
    \property QVBoxPlotModelMapper::lastBoxSetColumn
    \brief The column of the model that is used as the data source for the last
    box-and-whiskers item.

    The default value is -1 (invalid mapping).
*/
/*!
    \qmlproperty int VBoxPlotModelMapper::lastBoxSetColumn
    The column of the model that is used as the data source for the last
    box-and-whiskers item. The default value is -1 (invalid mapping).
*/

/*!
    \property QVBoxPlotModelMapper::firstRow
    \brief The row of the model that contains the first values of the
    box-and-whiskers items in the box plot series.

    The minimum and default value is 0.
*/
/*!
    \qmlproperty int VBoxPlotModelMapper::firstRow
    The row of the model that contains the first values of the
    box-and-whiskers items in the box plot series.

    The default value is 0.
*/

/*!
    \property QVBoxPlotModelMapper::rowCount
    \brief The number of rows of the model that are mapped as the data for the
    box plot series.

    The minimum and default value is  -1 (number limited to the number of
    columns in the model).
*/
/*!
    \qmlproperty int VBoxPlotModelMapper::rowCount
    The number of rows of the model that are mapped as the data for
    the box plot series.

    The default value is  -1 (number limited to the number of
    columns in the model).
*/

/*!
    \fn void QVBoxPlotModelMapper::seriesReplaced()

    This signal is emitted when the series that the mapper is connected to
    changes.
*/

/*!
    \fn void QVBoxPlotModelMapper::modelReplaced()

    This signal is emitted when the model that the mapper is connected to
    changes.
*/

/*!
    \fn void QVBoxPlotModelMapper::firstBoxSetColumnChanged()
    This signal is emitted when the first box-and-whiskers item column changes.
*/

/*!
    \fn void QVBoxPlotModelMapper::lastBoxSetColumnChanged()
    This signal is emitted when the last box-and-whiskers item column changes.
*/

/*!
    \fn void QVBoxPlotModelMapper::firstRowChanged()
    This signal is emitted when the first row changes.
*/

/*!
    \fn void QVBoxPlotModelMapper::rowCountChanged()
    This signal is emitted when the number of rows changes.
*/

/*!
    Constructs a mapper object that is a child of \a parent.
*/
QVBoxPlotModelMapper::QVBoxPlotModelMapper(QObject *parent) :
    QBoxPlotModelMapper(parent)
{
    QBoxPlotModelMapper::setOrientation(Qt::Vertical);
}

QAbstractItemModel *QVBoxPlotModelMapper::model() const
{
    return QBoxPlotModelMapper::model();
}

void QVBoxPlotModelMapper::setModel(QAbstractItemModel *model)
{
    if (model != QBoxPlotModelMapper::model()) {
        QBoxPlotModelMapper::setModel(model);
        emit modelReplaced();
    }
}

QBoxPlotSeries *QVBoxPlotModelMapper::series() const
{
    return QBoxPlotModelMapper::series();
}

void QVBoxPlotModelMapper::setSeries(QBoxPlotSeries *series)
{
    if (series != QBoxPlotModelMapper::series()) {
        QBoxPlotModelMapper::setSeries(series);
        emit seriesReplaced();
    }
}

int QVBoxPlotModelMapper::firstBoxSetColumn() const
{
    return QBoxPlotModelMapper::firstBoxSetSection();
}

void QVBoxPlotModelMapper::setFirstBoxSetColumn(int firstBoxSetColumn)
{
    if (firstBoxSetColumn != firstBoxSetSection()) {
        QBoxPlotModelMapper::setFirstBoxSetSection(firstBoxSetColumn);
        emit firstBoxSetColumnChanged();
    }
}

int QVBoxPlotModelMapper::lastBoxSetColumn() const
{
    return QBoxPlotModelMapper::lastBoxSetSection();
}

void QVBoxPlotModelMapper::setLastBoxSetColumn(int lastBoxSetColumn)
{
    if (lastBoxSetColumn != lastBoxSetSection()) {
        QBoxPlotModelMapper::setLastBoxSetSection(lastBoxSetColumn);
        emit lastBoxSetColumnChanged();
    }
}

int QVBoxPlotModelMapper::firstRow() const
{
    return QBoxPlotModelMapper::first();
}

void QVBoxPlotModelMapper::setFirstRow(int firstRow)
{
    if (firstRow != first()) {
        QBoxPlotModelMapper::setFirst(firstRow);
        emit firstRowChanged();
    }
}

int QVBoxPlotModelMapper::rowCount() const
{
    return QBoxPlotModelMapper::count();
}

void QVBoxPlotModelMapper::setRowCount(int rowCount)
{
    if (rowCount != count()) {
        QBoxPlotModelMapper::setCount(rowCount);
        emit rowCountChanged();
    }
}

#include "moc_qvboxplotmodelmapper.cpp"

QT_CHARTS_END_NAMESPACE

