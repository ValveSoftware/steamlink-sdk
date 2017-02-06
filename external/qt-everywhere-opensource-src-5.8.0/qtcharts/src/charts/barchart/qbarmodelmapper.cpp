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

#include <QtCharts/QBarModelMapper>
#include <private/qbarmodelmapper_p.h>
#include <QtCharts/QAbstractBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QChart>
#include <QtCore/QAbstractItemModel>

QT_CHARTS_BEGIN_NAMESPACE

/*!
    \class QBarModelMapper
    \inmodule Qt Charts
    \brief The QBarModelMapper class is the base class for model mapper classes.
    \internal

    Model mappers enable using a data model derived from the QAbstractItemModel class
    as a data source for a chart.
*/

QBarModelMapper::QBarModelMapper(QObject *parent) :
    QObject(parent),
    d_ptr(new QBarModelMapperPrivate(this))
{
}

QAbstractItemModel *QBarModelMapper::model() const
{
    Q_D(const QBarModelMapper);
    return d->m_model;
}

void QBarModelMapper::setModel(QAbstractItemModel *model)
{
    if (model == 0)
        return;

    Q_D(QBarModelMapper);
    if (d->m_model)
        disconnect(d->m_model, 0, d, 0);

    d->m_model = model;
    d->initializeBarFromModel();
    // connect signals from the model
    connect(d->m_model, SIGNAL(modelReset()), d, SLOT(initializeBarFromModel()));
    connect(d->m_model, SIGNAL(dataChanged(QModelIndex,QModelIndex)), d, SLOT(modelUpdated(QModelIndex,QModelIndex)));
    connect(d->m_model, SIGNAL(headerDataChanged(Qt::Orientation,int,int)), d, SLOT(modelHeaderDataUpdated(Qt::Orientation,int,int)));
    connect(d->m_model, SIGNAL(rowsInserted(QModelIndex,int,int)), d, SLOT(modelRowsAdded(QModelIndex,int,int)));
    connect(d->m_model, SIGNAL(rowsRemoved(QModelIndex,int,int)), d, SLOT(modelRowsRemoved(QModelIndex,int,int)));
    connect(d->m_model, SIGNAL(columnsInserted(QModelIndex,int,int)), d, SLOT(modelColumnsAdded(QModelIndex,int,int)));
    connect(d->m_model, SIGNAL(columnsRemoved(QModelIndex,int,int)), d, SLOT(modelColumnsRemoved(QModelIndex,int,int)));
    connect(d->m_model, SIGNAL(destroyed()), d, SLOT(handleModelDestroyed()));
}

QAbstractBarSeries *QBarModelMapper::series() const
{
    Q_D(const QBarModelMapper);
    return d->m_series;
}

void QBarModelMapper::setSeries(QAbstractBarSeries *series)
{
    Q_D(QBarModelMapper);
    if (d->m_series)
        disconnect(d->m_series, 0, d, 0);

    if (series == 0)
        return;

    d->m_series = series;
    d->initializeBarFromModel();
    // connect the signals from the series
    connect(d->m_series, SIGNAL(barsetsAdded(QList<QBarSet*>)), d, SLOT(barSetsAdded(QList<QBarSet*>)));
    connect(d->m_series, SIGNAL(barsetsRemoved(QList<QBarSet*>)), d, SLOT(barSetsRemoved(QList<QBarSet*>)));
    connect(d->m_series, SIGNAL(destroyed()), d, SLOT(handleSeriesDestroyed()));
}

/*!
    Returns which row/column of the model contains the first values of the QBarSets in the series.
    The default value is 0.
*/
int QBarModelMapper::first() const
{
    Q_D(const QBarModelMapper);
    return d->m_first;
}

/*!
    Sets which row of the model contains the \a first values of the QBarSets in the series.
    The default value is 0.
*/
void QBarModelMapper::setFirst(int first)
{
    Q_D(QBarModelMapper);
    d->m_first = qMax(first, 0);
    d->initializeBarFromModel();
}

/*!
    Returns the number of rows/columns of the model that are mapped as the data for QAbstractBarSeries
    Minimal and default value is: -1 (count limited by the number of rows/columns in the model)
*/
int QBarModelMapper::count() const
{
    Q_D(const QBarModelMapper);
    return d->m_count;
}

/*!
    Sets the \a count of rows/columns of the model that are mapped as the data for QAbstractBarSeries
    Minimal and default value is: -1 (count limited by the number of rows/columns in the model)
*/
void QBarModelMapper::setCount(int count)
{
    Q_D(QBarModelMapper);
    d->m_count = qMax(count, -1);
    d->initializeBarFromModel();
}

/*!
    Returns the orientation that is used when QBarModelMapper accesses the model.
    This mean whether the consecutive values of the bar set are read from row (Qt::Horizontal)
    or from columns (Qt::Vertical)
*/
Qt::Orientation QBarModelMapper::orientation() const
{
    Q_D(const QBarModelMapper);
    return d->m_orientation;
}

/*!
    Returns the \a orientation that is used when QBarModelMapper accesses the model.
    This mean whether the consecutive values of the pie are read from row (Qt::Horizontal)
    or from columns (Qt::Vertical)
*/
void QBarModelMapper::setOrientation(Qt::Orientation orientation)
{
    Q_D(QBarModelMapper);
    d->m_orientation = orientation;
    d->initializeBarFromModel();
}

/*!
    Returns which section of the model is used as the data source for the first bar set
*/
int QBarModelMapper::firstBarSetSection() const
{
    Q_D(const QBarModelMapper);
    return d->m_firstBarSetSection;
}

/*!
    Sets the model section that is used as the data source for the first bar set
    Parameter \a firstBarSetSection specifies the section of the model.
*/
void QBarModelMapper::setFirstBarSetSection(int firstBarSetSection)
{
    Q_D(QBarModelMapper);
    d->m_firstBarSetSection = qMax(-1, firstBarSetSection);
    d->initializeBarFromModel();
}

/*!
    Returns which section of the model is used as the data source for the last bar set
*/
int QBarModelMapper::lastBarSetSection() const
{
    Q_D(const QBarModelMapper);
    return d->m_lastBarSetSection;
}

/*!
    Sets the model section that is used as the data source for the last bar set
    Parameter \a lastBarSetSection specifies the section of the model.
*/
void QBarModelMapper::setLastBarSetSection(int lastBarSetSection)
{
    Q_D(QBarModelMapper);
    d->m_lastBarSetSection = qMax(-1, lastBarSetSection);
    d->initializeBarFromModel();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

QBarModelMapperPrivate::QBarModelMapperPrivate(QBarModelMapper *q) :
    QObject(q),
    m_series(0),
    m_model(0),
    m_first(0),
    m_count(-1),
    m_orientation(Qt::Vertical),
    m_firstBarSetSection(-1),
    m_lastBarSetSection(-1),
    m_seriesSignalsBlock(false),
    m_modelSignalsBlock(false),
    q_ptr(q)
{
}

void QBarModelMapperPrivate::blockModelSignals(bool block)
{
    m_modelSignalsBlock = block;
}

void QBarModelMapperPrivate::blockSeriesSignals(bool block)
{
    m_seriesSignalsBlock = block;
}

QBarSet *QBarModelMapperPrivate::barSet(QModelIndex index)
{
    if (!index.isValid())
        return 0;

    if (m_orientation == Qt::Vertical && index.column() >= m_firstBarSetSection && index.column() <= m_lastBarSetSection) {
        if (index.row() >= m_first && (m_count == - 1 || index.row() < m_first + m_count)) {
            return m_series->barSets().at(index.column() - m_firstBarSetSection);
        }
    } else if (m_orientation == Qt::Horizontal && index.row() >= m_firstBarSetSection && index.row() <= m_lastBarSetSection) {
        if (index.column() >= m_first && (m_count == - 1 || index.column() < m_first + m_count))
            return m_series->barSets().at(index.row() - m_firstBarSetSection);
    }
    return 0; // This part of model has not been mapped to any slice
}

QModelIndex QBarModelMapperPrivate::barModelIndex(int barSection, int posInBar)
{
    if (m_count != -1 && posInBar >= m_count)
        return QModelIndex(); // invalid

    if (barSection < m_firstBarSetSection || barSection > m_lastBarSetSection)
        return QModelIndex(); // invalid

    if (m_orientation == Qt::Vertical)
        return m_model->index(posInBar + m_first, barSection);
    else
        return m_model->index(barSection, posInBar + m_first);
}

void QBarModelMapperPrivate::handleSeriesDestroyed()
{
    m_series = 0;
}

void QBarModelMapperPrivate::modelUpdated(QModelIndex topLeft, QModelIndex bottomRight)
{
    Q_UNUSED(topLeft)
    Q_UNUSED(bottomRight)

    if (m_model == 0 || m_series == 0)
        return;

    if (m_modelSignalsBlock)
        return;

    blockSeriesSignals();
    QModelIndex index;
    for (int row = topLeft.row(); row <= bottomRight.row(); row++) {
        for (int column = topLeft.column(); column <= bottomRight.column(); column++) {
            index = topLeft.sibling(row, column);
            QBarSet *bar = barSet(index);
            if (bar) {
                if (m_orientation == Qt::Vertical)
                    bar->replace(row - m_first, m_model->data(index).toReal());
                else
                    bar->replace(column - m_first, m_model->data(index).toReal());
            }
        }
    }
    blockSeriesSignals(false);
}

void QBarModelMapperPrivate::modelHeaderDataUpdated(Qt::Orientation orientation, int first, int last)
{
    if (m_model == 0 || m_series == 0)
        return;

    if (m_modelSignalsBlock)
        return;

    blockSeriesSignals();
    if (orientation != m_orientation) {
        for (int section = first; section <= last; section++) {
            if (section >= m_firstBarSetSection && section <= m_lastBarSetSection) {
                QBarSet *bar = m_series->barSets().at(section - m_firstBarSetSection);
                if (bar)
                    bar->setLabel(m_model->headerData(section, orientation).toString());
            }
        }
    }
    blockSeriesSignals(false);
}

void QBarModelMapperPrivate::modelRowsAdded(QModelIndex parent, int start, int end)
{
    Q_UNUSED(parent)
    if (m_modelSignalsBlock)
        return;

    blockSeriesSignals();
    if (m_orientation == Qt::Vertical)
        insertData(start, end);
    else if (start <= m_firstBarSetSection || start <= m_lastBarSetSection) // if the changes affect the map - reinitialize
        initializeBarFromModel();
    blockSeriesSignals(false);
}

void QBarModelMapperPrivate::modelRowsRemoved(QModelIndex parent, int start, int end)
{
    Q_UNUSED(parent)
    if (m_modelSignalsBlock)
        return;

    blockSeriesSignals();
    if (m_orientation == Qt::Vertical)
        removeData(start, end);
    else if (start <= m_firstBarSetSection || start <= m_lastBarSetSection) // if the changes affect the map - reinitialize
        initializeBarFromModel();
    blockSeriesSignals(false);
}

void QBarModelMapperPrivate::modelColumnsAdded(QModelIndex parent, int start, int end)
{
    Q_UNUSED(parent)
    if (m_modelSignalsBlock)
        return;

    blockSeriesSignals();
    if (m_orientation == Qt::Horizontal)
        insertData(start, end);
    else if (start <= m_firstBarSetSection || start <= m_lastBarSetSection) // if the changes affect the map - reinitialize
        initializeBarFromModel();
    blockSeriesSignals(false);
}

void QBarModelMapperPrivate::modelColumnsRemoved(QModelIndex parent, int start, int end)
{
    Q_UNUSED(parent)
    if (m_modelSignalsBlock)
        return;

    blockSeriesSignals();
    if (m_orientation == Qt::Horizontal)
        removeData(start, end);
    else if (start <= m_firstBarSetSection || start <= m_lastBarSetSection) // if the changes affect the map - reinitialize
        initializeBarFromModel();
    blockSeriesSignals(false);
}

void QBarModelMapperPrivate::handleModelDestroyed()
{
    m_model = 0;
}

void QBarModelMapperPrivate::insertData(int start, int end)
{
    Q_UNUSED(end)
    Q_UNUSED(start)
    Q_UNUSED(end)
    // Currently barchart needs to be fully recalculated when change is made.
    // Re-initialize
    initializeBarFromModel();
}

void QBarModelMapperPrivate::removeData(int start, int end)
{
    Q_UNUSED(end)
    Q_UNUSED(start)
    Q_UNUSED(end)
    // Currently barchart needs to be fully recalculated when change is made.
    // Re-initialize
    initializeBarFromModel();
}

void QBarModelMapperPrivate::barSetsAdded(QList<QBarSet *> sets)
{
    if (m_seriesSignalsBlock)
        return;

    if (sets.count() == 0)
        return;

    int firstIndex = m_series->barSets().indexOf(sets.at(0));
    if (firstIndex == -1)
        return;

    int maxCount = 0;
    for (int i = 0; i < sets.count(); i++) {
        if (sets.at(i)->count() > m_count)
            maxCount = sets.at(i)->count();
    }

    if (m_count != -1 && m_count < maxCount)
        m_count = maxCount;

    m_lastBarSetSection += sets.count();

    blockModelSignals();
    int modelCapacity = m_orientation == Qt::Vertical ? m_model->rowCount() - m_first : m_model->columnCount() - m_first;
    if (maxCount > modelCapacity) {
        if (m_orientation == Qt::Vertical)
            m_model->insertRows(m_model->rowCount(), maxCount - modelCapacity);
        else
            m_model->insertColumns(m_model->columnCount(), maxCount - modelCapacity);
    }

    if (m_orientation == Qt::Vertical)
        m_model->insertColumns(firstIndex + m_firstBarSetSection, sets.count());
    else
        m_model->insertRows(firstIndex + m_firstBarSetSection, sets.count());


    for (int i = firstIndex + m_firstBarSetSection; i < firstIndex + m_firstBarSetSection + sets.count(); i++) {
        m_model->setHeaderData(i, m_orientation == Qt::Vertical ? Qt::Horizontal : Qt::Vertical, sets.at(i - firstIndex - m_firstBarSetSection)->label());
        for (int j = 0; j < sets.at(i - firstIndex - m_firstBarSetSection)->count(); j++)
            m_model->setData(barModelIndex(i, j), sets.at(i - firstIndex - m_firstBarSetSection)->at(j));
    }
    blockModelSignals(false);
    initializeBarFromModel();
}

void QBarModelMapperPrivate::barSetsRemoved(QList<QBarSet *> sets)
{
    if (m_seriesSignalsBlock)
        return;

    if (sets.count() == 0)
        return;

    int firstIndex = m_barSets.indexOf(sets.at(0));
    if (firstIndex == -1)
        return;

    m_lastBarSetSection -= sets.count();

    for (int i = firstIndex + sets.count() - 1; i >= firstIndex; i--)
        m_barSets.removeAt(i);

    blockModelSignals();
    if (m_orientation == Qt::Vertical)
        m_model->removeColumns(firstIndex + m_firstBarSetSection, sets.count());
    else
        m_model->removeRows(firstIndex + m_firstBarSetSection, sets.count());
    blockModelSignals(false);
    initializeBarFromModel();
}

void QBarModelMapperPrivate::valuesAdded(int index, int count)
{
    if (m_seriesSignalsBlock)
        return;

    if (m_count != -1)
        m_count += count;

    int barSetIndex = m_barSets.indexOf(qobject_cast<QBarSet *>(QObject::sender()));

    blockModelSignals();
    if (m_orientation == Qt::Vertical)
        m_model->insertRows(index + m_first, count);
    else
        m_model->insertColumns(index + m_first, count);

    for (int j = index; j < index + count; j++)
        m_model->setData(barModelIndex(barSetIndex + m_firstBarSetSection, j), m_barSets.at(barSetIndex)->at(j));

    blockModelSignals(false);
    initializeBarFromModel();
}

void QBarModelMapperPrivate::valuesRemoved(int index, int count)
{
    if (m_seriesSignalsBlock)
        return;

    if (m_count != -1)
        m_count -= count;

    blockModelSignals();
    if (m_orientation == Qt::Vertical)
        m_model->removeRows(index + m_first, count);
    else
        m_model->removeColumns(index + m_first, count);

    blockModelSignals(false);
    initializeBarFromModel();
}

void QBarModelMapperPrivate::barLabelChanged()
{
    if (m_seriesSignalsBlock)
        return;

    int barSetIndex = m_barSets.indexOf(qobject_cast<QBarSet *>(QObject::sender()));

    blockModelSignals();
    m_model->setHeaderData(barSetIndex + m_firstBarSetSection, m_orientation == Qt::Vertical ? Qt::Horizontal : Qt::Vertical, m_barSets.at(barSetIndex)->label());
    blockModelSignals(false);
    initializeBarFromModel();
}

void QBarModelMapperPrivate::barValueChanged(int index)
{
    if (m_seriesSignalsBlock)
        return;

    int barSetIndex = m_barSets.indexOf(qobject_cast<QBarSet *>(QObject::sender()));

    blockModelSignals();
    m_model->setData(barModelIndex(barSetIndex + m_firstBarSetSection, index), m_barSets.at(barSetIndex)->at(index));
    blockModelSignals(false);
    initializeBarFromModel();
}

void QBarModelMapperPrivate::initializeBarFromModel()
{
    if (m_model == 0 || m_series == 0)
        return;

    blockSeriesSignals();
    // clear current content
    m_series->clear();
    m_barSets.clear();

    // create the initial bar sets
    for (int i = m_firstBarSetSection; i <= m_lastBarSetSection; i++) {
        int posInBar = 0;
        QModelIndex barIndex = barModelIndex(i, posInBar);
        // check if there is such model index
        if (barIndex.isValid()) {
            QBarSet *barSet = new QBarSet(m_model->headerData(i, m_orientation == Qt::Vertical ? Qt::Horizontal : Qt::Vertical).toString());
            while (barIndex.isValid()) {
                barSet->append(m_model->data(barIndex, Qt::DisplayRole).toDouble());
                posInBar++;
                barIndex = barModelIndex(i, posInBar);
            }
            connect(barSet, SIGNAL(valuesAdded(int,int)), this, SLOT(valuesAdded(int,int)));
            connect(barSet, SIGNAL(valuesRemoved(int,int)), this, SLOT(valuesRemoved(int,int)));
            connect(barSet, SIGNAL(valueChanged(int)), this, SLOT(barValueChanged(int)));
            connect(barSet, SIGNAL(labelChanged()), this, SLOT(barLabelChanged()));
            m_series->append(barSet);
            m_barSets.append(barSet);
        } else {
            break;
        }
    }
    blockSeriesSignals(false);
}

#include "moc_qbarmodelmapper.cpp"
#include "moc_qbarmodelmapper_p.cpp"

QT_CHARTS_END_NAMESPACE
