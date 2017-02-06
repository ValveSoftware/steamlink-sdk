/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
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

#include <QtCharts/QHBoxPlotModelMapper>

QT_CHARTS_BEGIN_NAMESPACE

/*!
    \class QHBoxPlotModelMapper
    \inmodule Qt Charts
    \brief The QHBoxPlotModelMapper class is a horizontal model mapper for box
    plot series.

    Model mappers enable using a data model derived from the QAbstractItemModel
    class as a data source for a chart. A horizontal model mapper is used to
    create a connection between a data model and QBoxPlotSeries object, so that
    each row in the data model defines a box-and-whiskers item and each column
    maps to the range and three median values of the box-and-whiskers item.

    Both model and series properties can be used to manipulate the
    data. The model mapper keeps the series and the data model in sync.

    The model mapper ensures that all the box-and-whiskers items in the box plot
    series have equal sizes. Therefore, adding or removing a value from a
    box-and-whiskers item causes the same change to be made in all the
    box-and-whiskers items in the box plot series.

    \sa QVBoxPlotModelMapper
*/
/*!
    \qmltype HBoxPlotModelMapper
    \instantiates QHBoxPlotModelMapper
    \inqmlmodule QtCharts

    \brief Horizontal model mapper for box plot series.

    The HBoxPlotModelMapper type enables using a data model derived from the
    QAbstractItemModel class as a data source for a chart. A horizontal model
    mapper is used to create a connection between a data model and a
    BoxPlotSeries type, so that each row in the data model defines a
    box-and-whiskers item and each column maps to the range and three median
    values of the box-and-whiskers item.

    Both model and series properties can be used to manipulate the data. The
    model mapper keeps the series and the data model in sync.

    The model mapper ensures that all the box-and-whiskers items in the box plot
    series have equal sizes. Therefore, adding or removing a value from a
    box-and-whiskers item causes the same change to be made in all the
    box-and-whiskers items in the box plot series.

    The following QML code snippet creates a box plot series with three
    box-and-whiskers items (assuming the model has at least four rows). Each
    box-and-whiskers item contains data starting from column 1. The name of an
    item is defined by the row header.
    \code
        BoxPlotSeries {
            HBoxPlotModelMapper {
                model: myCustomModel // QAbstractItemModel derived implementation
                firstBoxSetRow: 1
                lastBoxSetRow: 3
                firstColumn: 1
            }
        }
    \endcode

    \sa VBoxPlotModelMapper
*/

/*!
    \property QHBoxPlotModelMapper::series
    \brief The box plot series that is used by the mapper.

    All the data in the series is discarded when it is set to the mapper.
    When a new series is specified, the old series is disconnected (but it
    preserves its data).
*/
/*!
    \qmlproperty AbstractBarSeries HBoxPlotModelMapper::series
    The box plot series that is used by the mapper. All the data in the series
    is discarded when it is set to the mapper. When the new series is specified,
    the old series is disconnected (but it preserves its data).
*/

/*!
    \property QHBoxPlotModelMapper::model
    \brief The model that is used by the mapper.
*/
/*!
    \qmlproperty SomeModel HBoxPlotModelMapper::model
    The data model that is used by the mapper. You need to implement the model
    and expose it to QML.

    \note The model has to support adding and removing rows or columns and
    modifying the data in the cells.
*/

/*!
    \property QHBoxPlotModelMapper::firstBoxSetRow
    \brief The row of the model that is used as the data source for the first
    box-and-whiskers item.

    The default value is -1 (invalid mapping).
*/
/*!
    \qmlproperty int HBoxPlotModelMapper::firstBoxSetRow
    The row of the model is used as the data source for the first
    box-and-whiskers item. The default value is -1 (invalid mapping).
*/

/*!
    \property QHBoxPlotModelMapper::lastBoxSetRow
    \brief The row of the model that is used as the data source for the last
    box-and-whiskers item.

    The default value is -1 (invalid mapping).
*/
/*!
    \qmlproperty int HBoxPlotModelMapper::lastBoxSetRow
    The row of the model is used as the data source for the last
    box-and-whiskers item. The default value is -1 (invalid mapping).
*/

/*!
    \property QHBoxPlotModelMapper::firstColumn
    \brief The column of the model that contains the first values of the
    box-and-whiskers items in the box plot series.

    The minimum and default value is 0.
*/
/*!
    \qmlproperty int HBoxPlotModelMapper::firstColumn
    The column of the model that contains the first values of the
    box-and-whiskers items in the box plot series.
    The default value is 0.
*/

/*!
    \property QHBoxPlotModelMapper::columnCount
    \brief The number of columns of the model that are mapped as the data for
    the box plot series.

    The minimum and default value is  -1 (number limited to the number of
    columns in the model).
*/
/*!
    \qmlproperty int HBoxPlotModelMapper::columnCount
    The number of columns of the model that are mapped as the data for
    the box plot series. The minimum and default value is  -1 (number limited to
    the number of columns in the model).
*/

/*!
    \fn void QHBoxPlotModelMapper::seriesReplaced()

    This signal is emitted when the series that the mapper is connected to
    changes.
*/

/*!
    \fn void QHBoxPlotModelMapper::modelReplaced()

    This signal is emitted when the model that the mapper is connected to
    changes.
*/

/*!
    \fn void QHBoxPlotModelMapper::firstBoxSetRowChanged()
    This signal is emitted when the first box-and-whiskers item row changes.
*/

/*!
    \fn void QHBoxPlotModelMapper::lastBoxSetRowChanged()
    This signal is emitted when the last box-and-whiskers item row changes.
*/

/*!
    \fn void QHBoxPlotModelMapper::firstColumnChanged()
    This signal is emitted when the first column changes.
*/

/*!
    \fn void QHBoxPlotModelMapper::columnCountChanged()
    This signal is emitted when the number of columns changes.
*/

/*!
    Constructs a mapper object that is a child of \a parent.
*/
QHBoxPlotModelMapper::QHBoxPlotModelMapper(QObject *parent) :
    QBoxPlotModelMapper(parent)
{
    QBoxPlotModelMapper::setOrientation(Qt::Horizontal);
}

QAbstractItemModel *QHBoxPlotModelMapper::model() const
{
    return QBoxPlotModelMapper::model();
}

void QHBoxPlotModelMapper::setModel(QAbstractItemModel *model)
{
    if (model != QBoxPlotModelMapper::model()) {
        QBoxPlotModelMapper::setModel(model);
        emit modelReplaced();
    }
}

QBoxPlotSeries *QHBoxPlotModelMapper::series() const
{
    return QBoxPlotModelMapper::series();
}

void QHBoxPlotModelMapper::setSeries(QBoxPlotSeries *series)
{
    if (series != QBoxPlotModelMapper::series()) {
        QBoxPlotModelMapper::setSeries(series);
        emit seriesReplaced();
    }
}

int QHBoxPlotModelMapper::firstBoxSetRow() const
{
    return QBoxPlotModelMapper::firstBoxSetSection();
}

void QHBoxPlotModelMapper::setFirstBoxSetRow(int firstBoxSetRow)
{
    if (firstBoxSetRow != firstBoxSetSection()) {
        QBoxPlotModelMapper::setFirstBoxSetSection(firstBoxSetRow);
        emit firstBoxSetRowChanged();
    }
}

int QHBoxPlotModelMapper::lastBoxSetRow() const
{
    return QBoxPlotModelMapper::lastBoxSetSection();
}

void QHBoxPlotModelMapper::setLastBoxSetRow(int lastBoxSetRow)
{
    if (lastBoxSetRow != lastBoxSetSection()) {
        QBoxPlotModelMapper::setLastBoxSetSection(lastBoxSetRow);
        emit lastBoxSetRowChanged();
    }
}

int QHBoxPlotModelMapper::firstColumn() const
{
    return QBoxPlotModelMapper::first();
}

void QHBoxPlotModelMapper::setFirstColumn(int firstColumn)
{
    if (firstColumn != first()) {
        QBoxPlotModelMapper::setFirst(firstColumn);
        emit firstColumnChanged();
    }
}

int QHBoxPlotModelMapper::columnCount() const
{
    return QBoxPlotModelMapper::count();
}

void QHBoxPlotModelMapper::setColumnCount(int columnCount)
{
    if (columnCount != count()) {
        QBoxPlotModelMapper::setCount(columnCount);
        emit columnCountChanged();
    }
}

#include "moc_qhboxplotmodelmapper.cpp"

QT_CHARTS_END_NAMESPACE

