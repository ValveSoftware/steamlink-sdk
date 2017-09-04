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

#include <QtCharts/QHPieModelMapper>

QT_CHARTS_BEGIN_NAMESPACE

/*!
    \class QHPieModelMapper
    \inmodule Qt Charts
    \brief The QHPieModelMapper is a horizontal model mapper for pie series.

    Model mappers enable using a data model derived from the QAbstractItemModel class
    as a data source for a chart. A horizontal model mapper is used to create a connection
    between a data model and QPieSeries, so that each column in the data model defines a
    pie slice and each row maps to the label or the value of the pie slice.

    Both model and pie series properties can be used to manipulate the data. The model
    mapper keeps the pie series and the data model in sync.
*/
/*!
    \qmltype HPieModelMapper
    \instantiates QHPieModelMapper
    \inqmlmodule QtCharts

    \brief Horizontal model mapper for pie series.

    Model mappers enable using a data model derived from the QAbstractItemModel class
    as a data source for a chart. A horizontal model mapper is used to create a connection
    between a data model and PieSeries, so that each column in the data model defines a
    pie slice and each row maps to the label or the value of the pie slice.

    Both model and pie series properties can be used to manipulate the data. The model
    mapper keeps the pie series and the data model in sync.

    The following QML example creates a pie series with four slices (assuming the model has
    at least five columns). Each slice gets a label from row 1 and a value from row 2.
    \code
        HPieModelMapper {
            series: pieSeries
            model: customModel
            labelsRow: 1
            valuesRow: 2
            firstColumn: 1
            columnCount: 4
        }
    \endcode
*/

/*!
    \property QHPieModelMapper::series
    \brief The pie series that is used by the mapper.

    All the data in the series is discarded when it is set to the mapper.
    When a new series is specified, the old series is disconnected (but it preserves its data).
*/
/*!
    \qmlproperty PieSeries HPieModelMapper::series
    The PieSeries object that is used by the mapper. If you define the mapper element as a child for a
    PieSeries, leave this property undefined. All the data in the series is discarded when it is set to the mapper.
    When a new series is specified, the old series is disconnected (but it preserves its data).
*/

/*!
    \property QHPieModelMapper::model
    \brief The model that is used by the mapper.
*/
/*!
    \qmlproperty SomeModel HPieModelMapper::model
    The QAbstractItemModel based model that is used by the mapper. You need to implement the model
    and expose it to QML.

    \note The model has to support adding and removing rows or columns and modifying
    the data in the cells.
*/

/*!
    \property QHPieModelMapper::valuesRow
    \brief The row of the model that is kept in sync with the values of the pie's slices.

    The default value is -1 (invalid mapping).
*/
/*!
    \qmlproperty int HPieModelMapper::valuesRow
    The row of the model that is kept in sync with the values of the pie's slices.
    The default value is -1 (invalid mapping).
*/

/*!
    \property QHPieModelMapper::labelsRow
    \brief The row of the model that is kept in sync with the labels of the pie's slices.

    The default value is -1 (invalid mapping).
*/
/*!
    \qmlproperty int HPieModelMapper::labelsRow
    The row of the model that is kept in sync with the labels of the pie's slices.
    The default value is -1 (invalid mapping).
*/

/*!
    \property QHPieModelMapper::firstColumn
    \brief The column of the model that contains the first slice value.

    The minimum and default value is 0.
*/
/*!
    \qmlproperty int HPieModelMapper::firstColumn
    The column of the model that contains the first slice value.
    The default value is 0.
*/

/*!
    \property QHPieModelMapper::columnCount
    \brief The number of columns of the model that are mapped as the data for the pie series.

    The minimum and default value is -1 (number limited to the number of columns in the model).
*/
/*!
    \qmlproperty int HPieModelMapper::columnCount
    The number of columns of the model that are mapped as the data for the pie series.
    The default value is -1 (number limited by the number of columns in the model).
*/

/*!
    \fn void QHPieModelMapper::seriesReplaced()
    This signal is emitted when the series that the mapper is connected to changes.
*/

/*!
    \fn void QHPieModelMapper::modelReplaced()
    This signal is emitted when the model that the mapper is connected to changes.
*/

/*!
    \fn void QHPieModelMapper::valuesRowChanged()
    This signal is emitted when the values row changes.
*/

/*!
    \fn void QHPieModelMapper::labelsRowChanged()
    This signal is emitted when the labels row changes.
*/

/*!
    \fn void QHPieModelMapper::firstColumnChanged()
    This signal is emitted when the first column changes.
*/

/*!
    \fn void QHPieModelMapper::columnCountChanged()
    This signal is emitted when the number of columns changes.
*/

/*!
    Constructs a mapper object that is a child of \a parent.
*/
QHPieModelMapper::QHPieModelMapper(QObject *parent) :
    QPieModelMapper(parent)
{
    setOrientation(Qt::Horizontal);
}

QAbstractItemModel *QHPieModelMapper::model() const
{
    return QPieModelMapper::model();
}

void QHPieModelMapper::setModel(QAbstractItemModel *model)
{
    if (model != QPieModelMapper::model()) {
        QPieModelMapper::setModel(model);
        emit modelReplaced();
    }
}

QPieSeries *QHPieModelMapper::series() const
{
    return QPieModelMapper::series();
}

void QHPieModelMapper::setSeries(QPieSeries *series)
{
    if (series != QPieModelMapper::series()) {
        QPieModelMapper::setSeries(series);
        emit seriesReplaced();
    }
}

/*!
    Returns the row of the model that is kept in sync with the values of the pie's slices.
*/
int QHPieModelMapper::valuesRow() const
{
    return valuesSection();
}

/*!
    Sets the model row that is kept in sync with the pie slices' values to \a valuesRow.
*/
void QHPieModelMapper::setValuesRow(int valuesRow)
{
    if (valuesRow != valuesSection()) {
        setValuesSection(valuesRow);
        emit valuesRowChanged();
    }
}

/*!
    Returns the row of the model that is kept in sync with the labels of the pie's slices.
*/
int QHPieModelMapper::labelsRow() const
{
    return labelsSection();
}

/*!
    Sets the model row that is kept in sync with the pie slices' labels to \a labelsRow.
*/
void QHPieModelMapper::setLabelsRow(int labelsRow)
{
    if (labelsRow != labelsSection()) {
        setLabelsSection(labelsRow);
        emit labelsRowChanged();
    }
}

int QHPieModelMapper::firstColumn() const
{
    return first();
}

void QHPieModelMapper::setFirstColumn(int firstColumn)
{
    if (firstColumn != first()) {
        setFirst(firstColumn);
        emit firstColumnChanged();
    }
}

int QHPieModelMapper::columnCount() const
{
    return count();
}

void QHPieModelMapper::setColumnCount(int columnCount)
{
    if (columnCount != count()) {
        setCount(columnCount);
        emit columnCountChanged();
    }
}

#include "moc_qhpiemodelmapper.cpp"

QT_CHARTS_END_NAMESPACE
