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

#include <QtCharts/QVPieModelMapper>

QT_CHARTS_BEGIN_NAMESPACE

/*!
    \class QVPieModelMapper
    \inmodule Qt Charts
    \brief The QVPieModelMapper is a vertical model mapper for pie series.

    Model mappers enable using a data model derived from the QAbstractItemModel class
    as a data source for a chart. A vertical model mapper is used to create a connection
    between a data model and QPieSeries, so that each row in the data model defines a
    pie slice and each column maps to the label or the value of the pie slice.

    Both model and pie series properties can be used to manipulate the data. The model
    mapper keeps the pie series and the data model in sync.
*/
/*!
    \qmltype VPieModelMapper
    \instantiates QVPieModelMapper
    \inqmlmodule QtCharts

    \brief Vertical model mapper for pie series.

    Model mappers enable using a data model derived from the QAbstractItemModel class
    as a data source for a chart. A vertical model mapper is used to create a connection
    between a data model and PieSeries, so that each row in the data model defines a
    pie slice and each column maps to the label or the value of the pie slice.

    Both model and pie series properties can be used to manipulate the data. The model
    mapper keeps the pie series and the data model in sync.

    The following QML example creates a pie series with four slices (assuming the model has at
    least five rows). Each slice gets a label from column 1 and a value from column 2.
    \code
        VPieModelMapper {
            series: pieSeries
            model: customModel
            labelsColumn: 1
            valuesColumn: 2
            firstRow: 1
            rowCount: 4
        }
    \endcode
*/

/*!
    \property QVPieModelMapper::series
    \brief The pie series that is used by the mapper.

    All the data in the series is discarded when it is set to the mapper.
    When a new series is specified, the old series is disconnected (but it preserves its data).
*/
/*!
    \qmlproperty PieSeries VPieModelMapper::series
    The pie series that is used by the mapper. If you define the mapper element as a child for a
    PieSeries, leave this property undefined. All the data in the series is discarded when it is set to the mapper.
    When new series is specified the old series is disconnected (but it preserves its data).
*/

/*!
    \property QVPieModelMapper::model
    \brief The model that is used by the mapper.
*/
/*!
    \qmlproperty SomeModel VPieModelMapper::model
    The QAbstractItemModel based model that is used by the mapper. You need to implement the model
    and expose it to QML.

    \note The model has to support adding and removing rows or columns and modifying
    the data in the cells.
*/

/*!
    \property QVPieModelMapper::valuesColumn
    \brief The column of the model that is kept in sync with the values of the pie's slices.

    The default value is -1 (invalid mapping).
*/
/*!
    \qmlproperty int VPieModelMapper::valuesColumn
    The column of the model that is kept in sync with the values of the pie's slices.
    The default value is -1 (invalid mapping).
*/

/*!
    \property QVPieModelMapper::labelsColumn
    \brief The column of the model that is kept in sync with the labels of the pie's slices.

    The default value is -1 (invalid mapping).
*/
/*!
    \qmlproperty int VPieModelMapper::labelsColumn
    The column of the model that is kept in sync with the labels of the pie's slices.
    The default value is -1 (invalid mapping).
*/

/*!
    \property QVPieModelMapper::firstRow
    \brief The row of the model that contains the first slice value.

    The minimum and default value is 0.
*/
/*!
    \qmlproperty int VPieModelMapper::firstRow
    The row of the model that contains the first slice value.
    The default value is 0.
*/

/*!
    \property QVPieModelMapper::rowCount
    \brief The number of rows of the model that are mapped as the data for a pie series.

    The minimum and default value is -1 (number limited by the number of rows in the model).
*/
/*!
    \qmlproperty int VPieModelMapper::rowCount
    The number of rows of the model that are mapped as the data for a pie series.
    The default value is -1 (number limited by the number of rows in the model).
*/

/*!
    \fn void QVPieModelMapper::seriesReplaced()

    This signal is emitted when the series that the mapper is connected to changes.
*/

/*!
    \fn void QVPieModelMapper::modelReplaced()

    This signal is emitted when the model that the mapper is connected to changes.
*/

/*!
    \fn void QVPieModelMapper::valuesColumnChanged()

    This signal is emitted when the values column changes.
*/

/*!
    \fn void QVPieModelMapper::labelsColumnChanged()

    This signal is emitted when the labels column changes.
*/

/*!
    \fn void QVPieModelMapper::firstRowChanged()
    This signal is emitted when the first row changes.
*/

/*!
    \fn void QVPieModelMapper::rowCountChanged()
    This signal is emitted when the number of rows changes.
*/

/*!
    Constructs a mapper object that is a child of \a parent.
*/
QVPieModelMapper::QVPieModelMapper(QObject *parent) :
    QPieModelMapper(parent)
{
    QPieModelMapper::setOrientation(Qt::Vertical);
}

QAbstractItemModel *QVPieModelMapper::model() const
{
    return QPieModelMapper::model();
}

void QVPieModelMapper::setModel(QAbstractItemModel *model)
{
    if (model != QPieModelMapper::model()) {
        QPieModelMapper::setModel(model);
        emit modelReplaced();
    }
}

QPieSeries *QVPieModelMapper::series() const
{
    return QPieModelMapper::series();
}

void QVPieModelMapper::setSeries(QPieSeries *series)
{
    if (series != QPieModelMapper::series()) {
        QPieModelMapper::setSeries(series);
        emit seriesReplaced();
    }
}

/*!
    Returns the column of the model that is kept in sync with the values of the pie's slices.
*/
int QVPieModelMapper::valuesColumn() const
{
    return QPieModelMapper::valuesSection();
}

/*!
    Sets the model column that is kept in sync with the pie slices' values to \a valuesColumn.
*/
void QVPieModelMapper::setValuesColumn(int valuesColumn)
{
    if (valuesColumn != valuesSection()) {
        QPieModelMapper::setValuesSection(valuesColumn);
        emit valuesColumnChanged();
    }
}

/*!
    Returns the column of the model that is kept in sync with the labels of the pie's slices.
*/
int QVPieModelMapper::labelsColumn() const
{
    return QPieModelMapper::labelsSection();
}

/*!
    Sets the model column that is kept in sync with the pies slices' labels to \a labelsColumn.
*/
void QVPieModelMapper::setLabelsColumn(int labelsColumn)
{
    if (labelsColumn != labelsSection()) {
        QPieModelMapper::setLabelsSection(labelsColumn);
        emit labelsColumnChanged();
    }
}

int QVPieModelMapper::firstRow() const
{
    return first();
}

void QVPieModelMapper::setFirstRow(int firstRow)
{
    if (firstRow != first()) {
        setFirst(firstRow);
        emit firstRowChanged();
    }
}

int QVPieModelMapper::rowCount() const
{
    return count();
}

void QVPieModelMapper::setRowCount(int rowCount)
{
    if (rowCount != count()) {
        setCount(rowCount);
        emit rowCountChanged();
    }
}

#include "moc_qvpiemodelmapper.cpp"

QT_CHARTS_END_NAMESPACE
