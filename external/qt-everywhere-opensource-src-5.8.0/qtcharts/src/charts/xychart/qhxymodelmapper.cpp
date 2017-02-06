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
    \brief Horizontal model mapper for QXYSeries.

    Model mappers allow you to use QAbstractItemModel derived models as a data source for a chart series.
    Horizontal model mapper is used to create a connection between QXYSeries and QAbstractItemModel derived model object.
    It is possible to use both QAbstractItemModel and QXYSeries model API. QXYModelMapper makes sure that QXYSeries and the model are kept in sync.
    Note: used model has to support adding/removing rows/columns and modifying the data of the cells.
*/
/*!
    \qmltype HXYModelMapper
    \instantiates QHXYModelMapper
    \inqmlmodule QtCharts

    \brief Horizontal model mapper for QXYSeries

    HXYModelMapper allows you to use your own QAbstractItemModel derived model with data in rows as
    a data source for XYSeries based series. It is possible to use both QAbstractItemModel and
    XYSeries data API to manipulate data. HXYModelMapper keeps the series and the model in sync.
*/

/*!
    \property QHXYModelMapper::series
    \brief Defines the QXYSeries object that is used by the mapper.

    All the data in the series is discarded when it is set to the mapper.
    When new series is specified the old series is disconnected (it preserves its data)
*/
/*!
    \qmlproperty XYSeries HXYModelMapper::series
    Defines the XYSeries object that is used by the mapper. All the data in the series is discarded when it is set to
    the mapper. When new series is specified the old series is disconnected (it preserves its data).
*/

/*!
    \property QHXYModelMapper::model
    \brief Defines the model that is used by the mapper.
*/
/*!
    \qmlproperty SomeModel HXYModelMapper::model
    The QAbstractItemModel based model that is used by the mapper. You need to implement the model
    and expose it to QML. Note: the model has to support adding/removing rows/columns and modifying
    the data of the cells.
*/

/*!
    \property QHXYModelMapper::xRow
    \brief Defines which row of the model is kept in sync with the x values of the QXYSeries.

    Default value is: -1 (invalid mapping)
*/
/*!
    \qmlproperty int HXYModelMapper::xRow
    Defines which row of the model is kept in sync with the x values of the series. Default value is -1 (invalid
    mapping).
*/

/*!
    \property QHXYModelMapper::yRow
    \brief Defines which row of the model is kept in sync with the  y values of the QXYSeries.

    Default value is: -1 (invalid mapping)
*/
/*!
    \qmlproperty int HXYModelMapper::yRow
    Defines which row of the model is kept in sync with the  y values of the series. Default value is -1
    (invalid mapping).
*/

/*!
    \property QHXYModelMapper::firstColumn
    \brief Defines which column of the model contains the data for the first point of the series.

    Minimal and default value is: 0
*/
/*!
    \qmlproperty int HXYModelMapper::firstColumn
    Defines which column of the model contains the data for the first point of the series.
    The default value is 0.
*/

/*!
    \property QHXYModelMapper::columnCount
    \brief Defines the number of columns of the model that are mapped as the data for series.

    Minimal and default value is: -1 (count limited by the number of columns in the model)
*/
/*!
    \qmlproperty int HXYModelMapper::columnCount
    Defines the number of columns of the model that are mapped as the data for series. The default value is
    -1 (count limited by the number of columns in the model)
*/

/*!
    \fn void QHXYModelMapper::seriesReplaced()

    Emitted when the series to which mapper is connected to has changed.
*/

/*!
    \fn void QHXYModelMapper::modelReplaced()

    Emitted when the model to which mapper is connected to has changed.
*/

/*!
    \fn void QHXYModelMapper::xRowChanged()

    Emitted when the xRow has changed.
*/

/*!
    \fn void QHXYModelMapper::yRowChanged()

    Emitted when the yRow has changed.
*/

/*!
    \fn void QHXYModelMapper::firstColumnChanged()
    Emitted when the firstColumn has changed.
*/

/*!
    \fn void QHXYModelMapper::columnCountChanged()
    Emitted when the columnCount has changed.
*/

/*!
    Constructs a mapper object which is a child of \a parent.
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
