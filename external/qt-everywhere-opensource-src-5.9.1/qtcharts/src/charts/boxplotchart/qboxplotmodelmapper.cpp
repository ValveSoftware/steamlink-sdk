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

#include <QtCharts/QBoxPlotModelMapper>
#include <private/qboxplotmodelmapper_p.h>
#include <QtCharts/QBoxPlotSeries>
#include <QtCharts/QBoxSet>
#include <QtCharts/QChart>
#include <QtCore/QAbstractItemModel>

QT_CHARTS_BEGIN_NAMESPACE

/*!
    \class QBoxPlotModelMapper
    \inmodule Qt Charts
    \brief The QBoxPlotModelMapper class is the base class for box plot model
    mapper classes.
    \internal

    Model mappers enable using a data model derived from the QAbstractItemModel
    class as a data source for a chart.
*/

QBoxPlotModelMapper::QBoxPlotModelMapper(QObject *parent) :
    QObject(parent),
    d_ptr(new QBoxPlotModelMapperPrivate(this))
{
}

QAbstractItemModel *QBoxPlotModelMapper::model() const
{
    Q_D(const QBoxPlotModelMapper);
    return d->m_model;
}

void QBoxPlotModelMapper::setModel(QAbstractItemModel *model)
{
    if (model == 0)
        return;

    Q_D(QBoxPlotModelMapper);
    if (d->m_model)
        disconnect(d->m_model, 0, d, 0);

    d->m_model = model;
    d->initializeBoxFromModel();
    // connect signals from the model
    connect(d->m_model, SIGNAL(modelReset()), d, SLOT(initializeBoxFromModel()));
    connect(d->m_model, SIGNAL(dataChanged(QModelIndex,QModelIndex)), d, SLOT(modelUpdated(QModelIndex,QModelIndex)));
    connect(d->m_model, SIGNAL(headerDataChanged(Qt::Orientation,int,int)), d, SLOT(modelHeaderDataUpdated(Qt::Orientation,int,int)));
    connect(d->m_model, SIGNAL(rowsInserted(QModelIndex,int,int)), d, SLOT(modelRowsAdded(QModelIndex,int,int)));
    connect(d->m_model, SIGNAL(rowsRemoved(QModelIndex,int,int)), d, SLOT(modelRowsRemoved(QModelIndex,int,int)));
    connect(d->m_model, SIGNAL(columnsInserted(QModelIndex,int,int)), d, SLOT(modelColumnsAdded(QModelIndex,int,int)));
    connect(d->m_model, SIGNAL(columnsRemoved(QModelIndex,int,int)), d, SLOT(modelColumnsRemoved(QModelIndex,int,int)));
    connect(d->m_model, SIGNAL(destroyed()), d, SLOT(handleModelDestroyed()));
}

QBoxPlotSeries *QBoxPlotModelMapper::series() const
{
    Q_D(const QBoxPlotModelMapper);
    return d->m_series;
}

void QBoxPlotModelMapper::setSeries(QBoxPlotSeries *series)
{
    Q_D(QBoxPlotModelMapper);
    if (d->m_series)
        disconnect(d->m_series, 0, d, 0);

    if (series == 0)
        return;

    d->m_series = series;
    d->initializeBoxFromModel();
    // connect the signals from the series
    connect(d->m_series, SIGNAL(boxsetsAdded(QList<QBoxSet *>)), d, SLOT(boxSetsAdded(QList<QBoxSet *>)));
    connect(d->m_series, SIGNAL(boxsetsRemoved(QList<QBoxSet *>)), d, SLOT(boxSetsRemoved(QList<QBoxSet *>)));
    connect(d->m_series, SIGNAL(destroyed()), d, SLOT(handleSeriesDestroyed()));
}

/*!
    Returns which row/column of the model contains the first values of the QBoxSets in the series.
    The default value is 0.
*/
int QBoxPlotModelMapper::first() const
{
    Q_D(const QBoxPlotModelMapper);
    return d->m_first;
}

/*!
    Sets which row/column of the model contains the \a first values of the QBoxSets in the series.
    The default value is 0.
*/
void QBoxPlotModelMapper::setFirst(int first)
{
    Q_D(QBoxPlotModelMapper);
    d->m_first = qMax(first, 0);
    d->initializeBoxFromModel();
}

/*!
    Returns the number of rows/columns of the model that are mapped as the data for QBoxPlotSeries
    Minimal and default value is: -1 (count limited by the number of rows/columns in the model)
*/
int QBoxPlotModelMapper::count() const
{
    Q_D(const QBoxPlotModelMapper);
    return d->m_count;
}

/*!
    Sets the \a count of rows/columns of the model that are mapped as the data for QBoxPlotSeries
    Minimal and default value is: -1 (count limited by the number of rows/columns in the model)
*/
void QBoxPlotModelMapper::setCount(int count)
{
    Q_D(QBoxPlotModelMapper);
    d->m_count = qMax(count, -1);
    d->initializeBoxFromModel();
}

/*!
    Returns the orientation that is used when QBoxPlotModelMapper accesses the model.
    This means whether the consecutive values of the box-and-whiskers set are read from row (Qt::Horizontal)
    or from columns (Qt::Vertical)
*/
Qt::Orientation QBoxPlotModelMapper::orientation() const
{
    Q_D(const QBoxPlotModelMapper);
    return d->m_orientation;
}

/*!
    Returns the \a orientation that is used when QBoxPlotModelMapper accesses the model.
    This mean whether the consecutive values of the box-and-whiskers set are read from row (Qt::Horizontal)
    or from columns (Qt::Vertical)
*/
void QBoxPlotModelMapper::setOrientation(Qt::Orientation orientation)
{
    Q_D(QBoxPlotModelMapper);
    d->m_orientation = orientation;
    d->initializeBoxFromModel();
}

/*!
    Returns which section of the model is used as the data source for the first box set
*/
int QBoxPlotModelMapper::firstBoxSetSection() const
{
    Q_D(const QBoxPlotModelMapper);
    return d->m_firstBoxSetSection;
}

/*!
    Sets the model section that is used as the data source for the first box set
    Parameter \a firstBoxSetSection specifies the section of the model.
*/
void QBoxPlotModelMapper::setFirstBoxSetSection(int firstBoxSetSection)
{
    Q_D(QBoxPlotModelMapper);
    d->m_firstBoxSetSection = qMax(-1, firstBoxSetSection);
    d->initializeBoxFromModel();
}

/*!
    Returns which section of the model is used as the data source for the last box set
*/
int QBoxPlotModelMapper::lastBoxSetSection() const
{
    Q_D(const QBoxPlotModelMapper);
    return d->m_lastBoxSetSection;
}

/*!
    Sets the model section that is used as the data source for the last box set
    Parameter \a lastBoxSetSection specifies the section of the model.
*/
void QBoxPlotModelMapper::setLastBoxSetSection(int lastBoxSetSection)
{
    Q_D(QBoxPlotModelMapper);
    d->m_lastBoxSetSection = qMax(-1, lastBoxSetSection);
    d->initializeBoxFromModel();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

QBoxPlotModelMapperPrivate::QBoxPlotModelMapperPrivate(QBoxPlotModelMapper *q) :
    QObject(q),
    m_series(0),
    m_model(0),
    m_first(0),
    m_count(-1),
    m_orientation(Qt::Vertical),
    m_firstBoxSetSection(-1),
    m_lastBoxSetSection(-1),
    m_seriesSignalsBlock(false),
    m_modelSignalsBlock(false),
    q_ptr(q)
{
}

void QBoxPlotModelMapperPrivate::blockModelSignals(bool block)
{
    m_modelSignalsBlock = block;
}

void QBoxPlotModelMapperPrivate::blockSeriesSignals(bool block)
{
    m_seriesSignalsBlock = block;
}

QBoxSet *QBoxPlotModelMapperPrivate::boxSet(QModelIndex index)
{
    if (!index.isValid())
        return 0;

    if (m_orientation == Qt::Vertical && index.column() >= m_firstBoxSetSection && index.column() <= m_lastBoxSetSection) {
        if (index.row() >= m_first && (m_count == - 1 || index.row() < m_first + m_count))
            return m_series->boxSets().at(index.column() - m_firstBoxSetSection);
    } else if (m_orientation == Qt::Horizontal && index.row() >= m_firstBoxSetSection && index.row() <= m_lastBoxSetSection) {
        if (index.column() >= m_first && (m_count == - 1 || index.column() < m_first + m_count))
            return m_series->boxSets().at(index.row() - m_firstBoxSetSection);
    }
    return 0; // This part of model has not been mapped to any boxset
}

QModelIndex QBoxPlotModelMapperPrivate::boxModelIndex(int boxSection, int posInBar)
{
    if (m_count != -1 && posInBar >= m_count)
        return QModelIndex(); // invalid

    if (boxSection < m_firstBoxSetSection || boxSection > m_lastBoxSetSection)
        return QModelIndex(); // invalid

    if (m_orientation == Qt::Vertical)
        return m_model->index(posInBar + m_first, boxSection);
    else
        return m_model->index(boxSection, posInBar + m_first);
}

void QBoxPlotModelMapperPrivate::handleSeriesDestroyed()
{
    m_series = 0;
}

void QBoxPlotModelMapperPrivate::modelUpdated(QModelIndex topLeft, QModelIndex bottomRight)
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
            QBoxSet *box = boxSet(index);
            if (box) {
                if (m_orientation == Qt::Vertical)
                    box->setValue(row - m_first, m_model->data(index).toReal());
                else
                    box->setValue(column - m_first, m_model->data(index).toReal());
            }
        }
    }
    blockSeriesSignals(false);
}

void QBoxPlotModelMapperPrivate::modelHeaderDataUpdated(Qt::Orientation orientation, int first, int last)
{
    Q_UNUSED(orientation);
    Q_UNUSED(first);
    Q_UNUSED(last);
}

void QBoxPlotModelMapperPrivate::modelRowsAdded(QModelIndex parent, int start, int end)
{
    Q_UNUSED(parent)
    if (m_modelSignalsBlock)
        return;

    blockSeriesSignals();
    if (m_orientation == Qt::Vertical)
        insertData(start, end);
    else if (start <= m_firstBoxSetSection || start <= m_lastBoxSetSection) // if the changes affect the map - reinitialize
        initializeBoxFromModel();
    blockSeriesSignals(false);
}

void QBoxPlotModelMapperPrivate::modelRowsRemoved(QModelIndex parent, int start, int end)
{
    Q_UNUSED(parent)
    if (m_modelSignalsBlock)
        return;

    blockSeriesSignals();
    if (m_orientation == Qt::Vertical)
        removeData(start, end);
    else if (start <= m_firstBoxSetSection || start <= m_lastBoxSetSection) // if the changes affect the map - reinitialize
        initializeBoxFromModel();
    blockSeriesSignals(false);
}

void QBoxPlotModelMapperPrivate::modelColumnsAdded(QModelIndex parent, int start, int end)
{
    Q_UNUSED(parent)
    if (m_modelSignalsBlock)
        return;

    blockSeriesSignals();
    if (m_orientation == Qt::Horizontal)
        insertData(start, end);
    else if (start <= m_firstBoxSetSection || start <= m_lastBoxSetSection) // if the changes affect the map - reinitialize
        initializeBoxFromModel();
    blockSeriesSignals(false);
}

void QBoxPlotModelMapperPrivate::modelColumnsRemoved(QModelIndex parent, int start, int end)
{
    Q_UNUSED(parent)
    if (m_modelSignalsBlock)
        return;

    blockSeriesSignals();
    if (m_orientation == Qt::Horizontal)
        removeData(start, end);
    else if (start <= m_firstBoxSetSection || start <= m_lastBoxSetSection) // if the changes affect the map - reinitialize
        initializeBoxFromModel();
    blockSeriesSignals(false);
}

void QBoxPlotModelMapperPrivate::handleModelDestroyed()
{
    m_model = 0;
}

void QBoxPlotModelMapperPrivate::insertData(int start, int end)
{
    Q_UNUSED(end)
    Q_UNUSED(start)
    Q_UNUSED(end)
    // Currently boxplotchart needs to be fully recalculated when change is made.
    // Re-initialize
    initializeBoxFromModel();
}

void QBoxPlotModelMapperPrivate::removeData(int start, int end)
{
    Q_UNUSED(end)
    Q_UNUSED(start)
    Q_UNUSED(end)
    // Currently boxplotchart needs to be fully recalculated when change is made.
    // Re-initialize
    initializeBoxFromModel();
}

void QBoxPlotModelMapperPrivate::boxSetsAdded(QList<QBoxSet *> sets)
{
    if (m_seriesSignalsBlock)
        return;

    if (sets.count() == 0)
        return;

    int firstIndex = m_series->boxSets().indexOf(sets.at(0));
    if (firstIndex == -1)
        return;

    int maxCount = 0;
    for (int i = 0; i < sets.count(); i++) {
        if (sets.at(i)->count() > m_count)
            maxCount = sets.at(i)->count();
    }

    if (m_count != -1 && m_count < maxCount)
        m_count = maxCount;

    m_lastBoxSetSection += sets.count();

    blockModelSignals();
    int modelCapacity = m_orientation == Qt::Vertical ? m_model->rowCount() - m_first : m_model->columnCount() - m_first;
    if (maxCount > modelCapacity) {
        if (m_orientation == Qt::Vertical)
            m_model->insertRows(m_model->rowCount(), maxCount - modelCapacity);
        else
            m_model->insertColumns(m_model->columnCount(), maxCount - modelCapacity);
    }

    if (m_orientation == Qt::Vertical)
        m_model->insertColumns(firstIndex + m_firstBoxSetSection, sets.count());
    else
        m_model->insertRows(firstIndex + m_firstBoxSetSection, sets.count());


    for (int i = firstIndex + m_firstBoxSetSection; i < firstIndex + m_firstBoxSetSection + sets.count(); i++) {
        for (int j = 0; j < sets.at(i - firstIndex - m_firstBoxSetSection)->count(); j++)
            m_model->setData(boxModelIndex(i, j), sets.at(i - firstIndex - m_firstBoxSetSection)->at(j));
    }
    blockModelSignals(false);
    initializeBoxFromModel();
}

void QBoxPlotModelMapperPrivate::boxSetsRemoved(QList<QBoxSet *> sets)
{
    if (m_seriesSignalsBlock)
        return;

    if (sets.count() == 0)
        return;

    int firstIndex = m_boxSets.indexOf(sets.at(0));
    if (firstIndex == -1)
        return;

    m_lastBoxSetSection -= sets.count();

    for (int i = firstIndex + sets.count() - 1; i >= firstIndex; i--)
        m_boxSets.removeAt(i);

    blockModelSignals();
    if (m_orientation == Qt::Vertical)
        m_model->removeColumns(firstIndex + m_firstBoxSetSection, sets.count());
    else
        m_model->removeRows(firstIndex + m_firstBoxSetSection, sets.count());
    blockModelSignals(false);
    initializeBoxFromModel();
}

void QBoxPlotModelMapperPrivate::boxValueChanged(int index)
{
    if (m_seriesSignalsBlock)
        return;

    int boxSetIndex = m_boxSets.indexOf(qobject_cast<QBoxSet *>(QObject::sender()));

    blockModelSignals();
    m_model->setData(boxModelIndex(boxSetIndex + m_firstBoxSetSection, index), m_boxSets.at(boxSetIndex)->at(index));
    blockModelSignals(false);
    initializeBoxFromModel();
}

void QBoxPlotModelMapperPrivate::initializeBoxFromModel()
{
    if (m_model == 0 || m_series == 0)
        return;

    blockSeriesSignals();
    // clear current content
    m_series->clear();
    m_boxSets.clear();

    // create the initial box-and-whiskers sets
    for (int i = m_firstBoxSetSection; i <= m_lastBoxSetSection; i++) {
        int posInBar = 0;
        QModelIndex boxIndex = boxModelIndex(i, posInBar);
        // check if there is such model index
        if (boxIndex.isValid()) {
            QBoxSet *boxSet = new QBoxSet();
            while (boxIndex.isValid()) {
                boxSet->append(m_model->data(boxIndex, Qt::DisplayRole).toDouble());
                posInBar++;
                boxIndex = boxModelIndex(i, posInBar);
            }
            connect(boxSet, SIGNAL(valueChanged(int)), this, SLOT(boxValueChanged(int)));
            m_series->append(boxSet);
            m_boxSets.append(boxSet);
        } else {
            break;
        }
    }
    blockSeriesSignals(false);
}

#include "moc_qboxplotmodelmapper.cpp"
#include "moc_qboxplotmodelmapper_p.cpp"

QT_CHARTS_END_NAMESPACE

