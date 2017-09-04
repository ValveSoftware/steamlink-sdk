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

#include <QtCharts/QVBarModelMapper>

QT_CHARTS_BEGIN_NAMESPACE

/*!
    \class QVBarModelMapper
    \inmodule Qt Charts
    \brief The QVBarModelMapper class is a vertical model mapper for bar series.

    Model mappers enable using a data model derived from the QAbstractItemModel class
    as a data source for a chart. A vertical model mapper is used to create a connection
    between a data model and QAbstractBarSeries, so that each column in the data model
    defines a bar set and each row maps to a category in a bar series.

    Both model and bar series properties can be used to manipulate the data. The model mapper
    keeps the bar series and the data model in sync.

    The model mapper ensures that all the bar sets in the bar series have equal sizes.
    Therefore, adding or removing a value from a bar set causes the same change to be
    made in all the bar sets in the bar series.

    For more information, see \l{BarModelMapper Example}.

    \sa QHBarModelMapper
*/
/*!
    \qmltype VBarModelMapper
    \instantiates QVBarModelMapper
    \inqmlmodule QtCharts

    \inherits BarModelMapper

    \brief Vertical model mapper for bar series.

    The VBarModelMapper type enables using a data model derived from the QAbstractItemModel
    class as a data source for a chart. A vertical model mapper is used to create a connection
    between a data model and QAbstractBarSeries, so that each column in the data model
    defines a bar set and each row maps to a category in a bar series. You need to implement
    the data model and expose it to QML.

    Both model and bar series properties can be used to manipulate the data. The model mapper
    keeps the bar series and the data model in sync.

    The model mapper ensures that all the bar sets in the bar series have equal sizes.
    Therefore, adding or removing a value from a bar set causes the same change to be
    made in all the bar sets in the bar series.

    The following QML code snippet creates a bar series with three bar sets (assuming the model
    has at least four columns). Each bar set contains data starting from row 1. The name
    of a bar set is defined by the column header.
    \code
        BarSeries {
            VBarModelMapper {
                model: myCustomModel // QAbstractItemModel derived implementation
                firstBarSetColumn: 1
                lastBarSetColumn: 3
                firstRow: 1
            }
        }
    \endcode

    \sa HBarModelMapper
*/

/*!
    \property QVBarModelMapper::series
    \brief The bar series that is used by the mapper.

    All the data in the series is discarded when it is set to the mapper.
    When a new series is specified, the old series is disconnected (but it preserves its data).
*/
/*!
    \qmlproperty AbstractBarSeries VBarModelMapper::series
    The bar series that is used by the mapper. All the data in the series is discarded when it is
    set to the mapper. When the new series is specified, the old series is disconnected (but it
    preserves its data).
*/

/*!
    \property QVBarModelMapper::model
    \brief The data model that is used by the mapper.
*/
/*!
    \qmlproperty SomeModel VBarModelMapper::model
    The data model that is used by the mapper. You need to implement the model and expose it to QML.

    \note The model has to support adding and removing rows or columns and modifying
    the data in the cells.
*/

/*!
    \property QVBarModelMapper::firstBarSetColumn
    \brief The column of the model that is used as the data source for the first bar set.

    The default value is -1 (invalid mapping).
*/
/*!
    \qmlproperty int VBarModelMapper::firstBarSetColumn
    The column of the model that is used as the data source for the first bar set. The default value
    is -1 (invalid mapping).
*/

/*!
    \property QVBarModelMapper::lastBarSetColumn
    \brief The column of the model that is used as the data source for the last bar set.

    The default value is -1 (invalid mapping).
*/
/*!
    \qmlproperty int VBarModelMapper::lastBarSetColumn
    The column of the model that is used as the data source for the last bar set. The default
    value is -1 (invalid mapping).
*/

/*!
    \property QVBarModelMapper::firstRow
    \brief The row of the model that contains the first values of the bar sets in the bar series.

    The minimum and default value is 0.
*/
/*!
    \qmlproperty int VBarModelMapper::firstRow
    The row of the model that contains the first values of the bar sets in the bar series.
    The default value is 0.
*/

/*!
    \property QVBarModelMapper::rowCount
    \brief The number of rows of the model that are mapped as the data for the bar series.

    The minimum and default value is -1 (number limited to the number of rows in the model).
*/
/*!
    \qmlproperty int VBarModelMapper::rowCount
    The number of rows of the model that are mapped as the data for the bar series. The default
    value is -1 (number limited to the number of rows in the model).
*/

/*!
    \fn void QVBarModelMapper::seriesReplaced()

    This signal is emitted when the bar series that the mapper is connected to changes.
*/

/*!
    \fn void QVBarModelMapper::modelReplaced()

    This signal is emitted when the model that the mapper is connected to changes.
*/

/*!
    \fn void QVBarModelMapper::firstBarSetColumnChanged()
    This signal is emitted when the first bar set column changes.
*/

/*!
    \fn void QVBarModelMapper::lastBarSetColumnChanged()
    This signal is emitted when the last bar set column changes.
*/

/*!
    \fn void QVBarModelMapper::firstRowChanged()
    This signal is emitted when the first row changes.
*/

/*!
    \fn void QVBarModelMapper::rowCountChanged()
    This signal is emitted when the number of rows changes.
*/

/*!
    Constructs a mapper object that is a child of \a parent.
*/
QVBarModelMapper::QVBarModelMapper(QObject *parent) :
    QBarModelMapper(parent)
{
    QBarModelMapper::setOrientation(Qt::Vertical);
}

QAbstractItemModel *QVBarModelMapper::model() const
{
    return QBarModelMapper::model();
}

void QVBarModelMapper::setModel(QAbstractItemModel *model)
{
    if (model != QBarModelMapper::model()) {
        QBarModelMapper::setModel(model);
        emit modelReplaced();
    }
}

QAbstractBarSeries *QVBarModelMapper::series() const
{
    return QBarModelMapper::series();
}

void QVBarModelMapper::setSeries(QAbstractBarSeries *series)
{
    if (series != QBarModelMapper::series()) {
        QBarModelMapper::setSeries(series);
        emit seriesReplaced();
    }
}

int QVBarModelMapper::firstBarSetColumn() const
{
    return QBarModelMapper::firstBarSetSection();
}

void QVBarModelMapper::setFirstBarSetColumn(int firstBarSetColumn)
{
    if (firstBarSetColumn != firstBarSetSection()) {
        QBarModelMapper::setFirstBarSetSection(firstBarSetColumn);
        emit firstBarSetColumnChanged();
    }
}

int QVBarModelMapper::lastBarSetColumn() const
{
    return QBarModelMapper::lastBarSetSection();
}

void QVBarModelMapper::setLastBarSetColumn(int lastBarSetColumn)
{
    if (lastBarSetColumn != lastBarSetSection()) {
        QBarModelMapper::setLastBarSetSection(lastBarSetColumn);
        emit lastBarSetColumnChanged();
    }
}

int QVBarModelMapper::firstRow() const
{
    return QBarModelMapper::first();
}

void QVBarModelMapper::setFirstRow(int firstRow)
{
    if (firstRow != first()) {
        QBarModelMapper::setFirst(firstRow);
        emit firstRowChanged();
    }
}

int QVBarModelMapper::rowCount() const
{
    return QBarModelMapper::count();
}

void QVBarModelMapper::setRowCount(int rowCount)
{
    if (rowCount != count()) {
        QBarModelMapper::setCount(rowCount);
        emit rowCountChanged();
    }
}

#include "moc_qvbarmodelmapper.cpp"

QT_CHARTS_END_NAMESPACE
