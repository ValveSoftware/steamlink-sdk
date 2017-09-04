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

#include <QtCharts/QCandlestickModelMapper>
#include <QtCharts/QCandlestickSeries>
#include <QtCharts/QCandlestickSet>
#include <QtCore/QAbstractItemModel>
#include <private/qcandlestickmodelmapper_p.h>

QT_CHARTS_BEGIN_NAMESPACE

/*!
    \class QCandlestickModelMapper
    \since 5.8
    \inmodule Qt Charts
    \brief Abstract model mapper class for candlestick series.

    Model mappers allow the use of a QAbstractItemModel-derived model as a data source for a chart
    series, creating a connection between a QCandlestickSeries and the model object. A model mapper
    maintains an equal size across all \l {QCandlestickSet} {QCandlestickSets}.

    \note The model used must support adding and removing rows/columns and modifying the data of the
    cells.
*/

/*!
    \property QCandlestickModelMapper::model
    \brief Defines the model that is used by the mapper.
*/

/*!
    \property QCandlestickModelMapper::series
    \brief Defines the QCandlestickSeries object that is used by the mapper.

    \note All data in the series is discarded when it is set to the mapper. When a new series is
    specified, the old series is disconnected (preserving its data).
*/

/*!
    \fn Qt::Orientation QCandlestickModelMapper::orientation() const
    Returns the orientation that is used when QCandlestickModelMapper accesses the model. This
    determines whether the consecutive values of the set are read from rows (Qt::Horizontal) or from
    columns (Qt::Vertical).
*/

/*!
    \fn void QCandlestickModelMapper::modelReplaced()
    \brief Emitted when the model, to which the mapper is connected, has changed.
    \sa model
*/

/*!
    \fn void QCandlestickModelMapper::seriesReplaced()
    \brief Emitted when the series to which mapper is connected to has changed.
    \sa series
*/

/*!
    Constructs a model mapper object as a child of \a parent.
*/
QCandlestickModelMapper::QCandlestickModelMapper(QObject *parent)
    : QObject(parent),
      d_ptr(new QCandlestickModelMapperPrivate(this))
{
}

void QCandlestickModelMapper::setModel(QAbstractItemModel *model)
{
    Q_D(QCandlestickModelMapper);

    if (d->m_model == model)
        return;

    if (d->m_model)
        disconnect(d->m_model, 0, d, 0);

    d->m_model = model;
    emit modelReplaced();

    if (!d->m_model)
        return;

    d->initializeCandlestickFromModel();
    // connect signals from the model
    connect(d->m_model, SIGNAL(modelReset()), d, SLOT(initializeCandlestickFromModel()));
    connect(d->m_model, SIGNAL(dataChanged(QModelIndex, QModelIndex)),
            d, SLOT(modelDataUpdated(QModelIndex, QModelIndex)));
    connect(d->m_model, SIGNAL(headerDataChanged(Qt::Orientation, int, int)),
            d, SLOT(modelHeaderDataUpdated(Qt::Orientation, int, int)));
    connect(d->m_model, SIGNAL(rowsInserted(QModelIndex, int, int)),
            d, SLOT(modelRowsInserted(QModelIndex, int, int)));
    connect(d->m_model, SIGNAL(rowsRemoved(QModelIndex, int, int)),
            d, SLOT(modelRowsRemoved(QModelIndex, int, int)));
    connect(d->m_model, SIGNAL(columnsInserted(QModelIndex, int, int)),
            d, SLOT(modelColumnsInserted(QModelIndex, int, int)));
    connect(d->m_model, SIGNAL(columnsRemoved(QModelIndex, int, int)),
            d, SLOT(modelColumnsRemoved(QModelIndex, int, int)));
    connect(d->m_model, SIGNAL(destroyed()), d, SLOT(modelDestroyed()));
}

QAbstractItemModel *QCandlestickModelMapper::model() const
{
    Q_D(const QCandlestickModelMapper);

    return d->m_model;
}

void QCandlestickModelMapper::setSeries(QCandlestickSeries *series)
{
    Q_D(QCandlestickModelMapper);

    if (d->m_series == series)
        return;

    if (d->m_series)
        disconnect(d->m_series, 0, d, 0);

    d->m_series = series;
    emit seriesReplaced();

    if (!d->m_series)
        return;

    d->initializeCandlestickFromModel();
    // connect the signals from the series
    connect(d->m_series, SIGNAL(candlestickSetsAdded(QList<QCandlestickSet *>)),
            d, SLOT(candlestickSetsAdded(QList<QCandlestickSet *>)));
    connect(d->m_series, SIGNAL(candlestickSetsRemoved(QList<QCandlestickSet*>)),
            d, SLOT(candlestickSetsRemoved(QList<QCandlestickSet *>)));
    connect(d->m_series, SIGNAL(destroyed()), d, SLOT(seriesDestroyed()));
}

QCandlestickSeries *QCandlestickModelMapper::series() const
{
    Q_D(const QCandlestickModelMapper);

    return d->m_series;
}

/*!
    Sets the row/column of the model that contains the \a timestamp values of the sets in the
    series. Default value is -1 (invalid mapping).
*/
void QCandlestickModelMapper::setTimestamp(int timestamp)
{
    Q_D(QCandlestickModelMapper);

    timestamp = qMax(timestamp, -1);

    if (d->m_timestamp == timestamp)
        return;

    d->m_timestamp = timestamp;
    emit d->timestampChanged();
    d->initializeCandlestickFromModel();
}

/*!
    Returns the row/column of the model that contains the timestamp values of the sets in the
    series. Default value is -1 (invalid mapping).
*/
int QCandlestickModelMapper::timestamp() const
{
    Q_D(const QCandlestickModelMapper);

    return d->m_timestamp;
}

/*!
    Sets the row/column of the model that contains the \a open values of the sets in the series.
    Default value is -1 (invalid mapping).
*/
void QCandlestickModelMapper::setOpen(int open)
{
    Q_D(QCandlestickModelMapper);

    open = qMax(open, -1);

    if (d->m_open == open)
        return;

    d->m_open = open;
    emit d->openChanged();
    d->initializeCandlestickFromModel();
}

/*!
    Returns the row/column of the model that contains the open values of the sets in the series.
    Default value is -1 (invalid mapping).
*/
int QCandlestickModelMapper::open() const
{
    Q_D(const QCandlestickModelMapper);

    return d->m_open;
}

/*!
    Sets the row/column of the model that contains the \a high values of the sets in the series.
    Default value is -1 (invalid mapping).
*/
void QCandlestickModelMapper::setHigh(int high)
{
    Q_D(QCandlestickModelMapper);

    high = qMax(high, -1);

    if (d->m_high == high)
        return;

    d->m_high = high;
    emit d->highChanged();
    d->initializeCandlestickFromModel();
}

/*!
    Returns the row/column of the model that contains the high values of the sets in the series.
    Default value is -1 (invalid mapping).
*/
int QCandlestickModelMapper::high() const
{
    Q_D(const QCandlestickModelMapper);

    return d->m_high;
}

/*!
    Sets the row/column of the model that contains the \a low values of the sets in the series.
    Default value is -1 (invalid mapping).
*/
void QCandlestickModelMapper::setLow(int low)
{
    Q_D(QCandlestickModelMapper);

    low = qMax(low, -1);

    if (d->m_low == low)
        return;

    d->m_low = low;
    emit d->lowChanged();
    d->initializeCandlestickFromModel();
}

/*!
    Returns the row/column of the model that contains the low values of the sets in the series.
    Default value is -1 (invalid mapping).
*/
int QCandlestickModelMapper::low() const
{
    Q_D(const QCandlestickModelMapper);

    return d->m_low;
}

/*!
    Sets the row/column of the model that contains the \a close values of the sets in the series.
    Default value is -1 (invalid mapping).
*/
void QCandlestickModelMapper::setClose(int close)
{
    Q_D(QCandlestickModelMapper);

    close = qMax(close, -1);

    if (d->m_close == close)
        return;

    d->m_close = close;
    emit d->closeChanged();
    d->initializeCandlestickFromModel();
}

/*!
    Returns the row/column of the model that contains the close values of the sets in the series.
    Default value is -1 (invalid mapping).
*/
int QCandlestickModelMapper::close() const
{
    Q_D(const QCandlestickModelMapper);

    return d->m_close;
}

/*!
    Sets the section of the model that is used as the data source for the first candlestick set.
    Parameter \a firstSetSection specifies the section of the model. Default value is -1.
*/
void QCandlestickModelMapper::setFirstSetSection(int firstSetSection)
{
    Q_D(QCandlestickModelMapper);

    firstSetSection = qMax(firstSetSection, -1);

    if (d->m_firstSetSection == firstSetSection)
        return;

    d->m_firstSetSection = firstSetSection;
    emit d->firstSetSectionChanged();
    d->initializeCandlestickFromModel();
}

/*!
    Returns the section of the model that is used as the data source for the first candlestick set.
    Default value is -1 (invalid mapping).
*/
int QCandlestickModelMapper::firstSetSection() const
{
    Q_D(const QCandlestickModelMapper);

    return d->m_firstSetSection;
}

/*!
    Sets the section of the model that is used as the data source for the last candlestick set.
    Parameter \a lastSetSection specifies the section of the model. Default value is -1.
*/
void QCandlestickModelMapper::setLastSetSection(int lastSetSection)
{
    Q_D(QCandlestickModelMapper);

    lastSetSection = qMax(lastSetSection, -1);

    if (d->m_lastSetSection == lastSetSection)
        return;

    d->m_lastSetSection = lastSetSection;
    emit d->lastSetSectionChanged();
    d->initializeCandlestickFromModel();
}

/*!
    Returns the section of the model that is used as the data source for the last candlestick set.
    Default value is -1 (invalid mapping).
*/
int QCandlestickModelMapper::lastSetSection() const
{
    Q_D(const QCandlestickModelMapper);

    return d->m_lastSetSection;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

QCandlestickModelMapperPrivate::QCandlestickModelMapperPrivate(QCandlestickModelMapper *q)
    : QObject(q),
      m_model(nullptr),
      m_series(nullptr),
      m_timestamp(-1),
      m_open(-1),
      m_high(-1),
      m_low(-1),
      m_close(-1),
      m_firstSetSection(-1),
      m_lastSetSection(-1),
      m_modelSignalsBlock(false),
      m_seriesSignalsBlock(false),
      q_ptr(q)
{
}

void QCandlestickModelMapperPrivate::initializeCandlestickFromModel()
{
    if (!m_model || !m_series)
        return;

    blockSeriesSignals();
    // clear current content
    m_series->clear();
    m_sets.clear();

    // create the initial candlestick sets
    QList<QCandlestickSet *> sets;
    for (int i = m_firstSetSection; i <= m_lastSetSection; ++i) {
        QModelIndex timestampIndex = candlestickModelIndex(i, m_timestamp);
        QModelIndex openIndex = candlestickModelIndex(i, m_open);
        QModelIndex highIndex = candlestickModelIndex(i, m_high);
        QModelIndex lowIndex = candlestickModelIndex(i, m_low);
        QModelIndex closeIndex = candlestickModelIndex(i, m_close);
        if (timestampIndex.isValid()
            && openIndex.isValid()
            && highIndex.isValid()
            && lowIndex.isValid()
            && closeIndex.isValid()) {
            QCandlestickSet *set = new QCandlestickSet();
            set->setTimestamp(m_model->data(timestampIndex, Qt::DisplayRole).toReal());
            set->setOpen(m_model->data(openIndex, Qt::DisplayRole).toReal());
            set->setHigh(m_model->data(highIndex, Qt::DisplayRole).toReal());
            set->setLow(m_model->data(lowIndex, Qt::DisplayRole).toReal());
            set->setClose(m_model->data(closeIndex, Qt::DisplayRole).toReal());

            connect(set, SIGNAL(timestampChanged()), this, SLOT(candlestickSetChanged()));
            connect(set, SIGNAL(openChanged()), this, SLOT(candlestickSetChanged()));
            connect(set, SIGNAL(highChanged()), this, SLOT(candlestickSetChanged()));
            connect(set, SIGNAL(lowChanged()), this, SLOT(candlestickSetChanged()));
            connect(set, SIGNAL(closeChanged()), this, SLOT(candlestickSetChanged()));

            sets.append(set);
        } else {
            break;
        }
    }
    m_series->append(sets);
    m_sets.append(sets);
    blockSeriesSignals(false);
}

void QCandlestickModelMapperPrivate::modelDataUpdated(QModelIndex topLeft, QModelIndex bottomRight)
{
    Q_Q(QCandlestickModelMapper);

    if (!m_model || !m_series)
        return;

    if (m_modelSignalsBlock)
        return;

    blockSeriesSignals();
    QModelIndex index;
    for (int row = topLeft.row(); row <= bottomRight.row(); ++row) {
        for (int column = topLeft.column(); column <= bottomRight.column(); ++column) {
            index = topLeft.sibling(row, column);
            QCandlestickSet *set = candlestickSet(index);
            if (set) {
                int pos = (q->orientation() == Qt::Vertical) ? row : column;
                if (pos == m_timestamp)
                    set->setTimestamp(m_model->data(index).toReal());
                else if (pos == m_open)
                    set->setOpen(m_model->data(index).toReal());
                else if (pos == m_high)
                    set->setHigh(m_model->data(index).toReal());
                else if (pos == m_low)
                    set->setLow(m_model->data(index).toReal());
                else if (pos == m_close)
                    set->setClose(m_model->data(index).toReal());
            }
        }
    }
    blockSeriesSignals(false);
}

void QCandlestickModelMapperPrivate::modelHeaderDataUpdated(Qt::Orientation orientation, int first,
                                                            int last)
{
    Q_UNUSED(orientation);
    Q_UNUSED(first);
    Q_UNUSED(last);
}

void QCandlestickModelMapperPrivate::modelRowsInserted(QModelIndex parent, int start, int end)
{
    Q_UNUSED(parent)

    Q_Q(QCandlestickModelMapper);

    if (m_modelSignalsBlock)
        return;

    blockSeriesSignals();
    if (q->orientation() == Qt::Vertical)
        insertData(start, end);
    else if (start <= m_firstSetSection || start <= m_lastSetSection)
        initializeCandlestickFromModel();  // if the changes affect the map - reinitialize
    blockSeriesSignals(false);
}

void QCandlestickModelMapperPrivate::modelRowsRemoved(QModelIndex parent, int start, int end)
{
    Q_UNUSED(parent)

    Q_Q(QCandlestickModelMapper);

    if (m_modelSignalsBlock)
        return;

    blockSeriesSignals();
    if (q->orientation() == Qt::Vertical)
        removeData(start, end);
    else if (start <= m_firstSetSection || start <= m_lastSetSection)
        initializeCandlestickFromModel();  // if the changes affect the map - reinitialize
    blockSeriesSignals(false);
}

void QCandlestickModelMapperPrivate::modelColumnsInserted(QModelIndex parent, int start, int end)
{
    Q_UNUSED(parent)

    Q_Q(QCandlestickModelMapper);

    if (m_modelSignalsBlock)
        return;

    blockSeriesSignals();
    if (q->orientation() == Qt::Horizontal)
        insertData(start, end);
    else if (start <= m_firstSetSection || start <= m_lastSetSection)
        initializeCandlestickFromModel();  // if the changes affect the map - reinitialize
    blockSeriesSignals(false);
}

void QCandlestickModelMapperPrivate::modelColumnsRemoved(QModelIndex parent, int start, int end)
{
    Q_UNUSED(parent)

    Q_Q(QCandlestickModelMapper);

    if (m_modelSignalsBlock)
        return;

    blockSeriesSignals();
    if (q->orientation() == Qt::Horizontal)
        removeData(start, end);
    else if (start <= m_firstSetSection || start <= m_lastSetSection)
        initializeCandlestickFromModel();  // if the changes affect the map - reinitialize
    blockSeriesSignals(false);
}

void QCandlestickModelMapperPrivate::modelDestroyed()
{
    m_model = 0;
}

void QCandlestickModelMapperPrivate::candlestickSetsAdded(const QList<QCandlestickSet *> &sets)
{
    Q_Q(QCandlestickModelMapper);

    if (m_seriesSignalsBlock)
        return;

    if (sets.isEmpty())
        return;

    int firstIndex = m_series->sets().indexOf(sets.at(0));
    if (firstIndex == -1)
        return;

    m_lastSetSection += sets.count();

    blockModelSignals();
    if (q->orientation() == Qt::Vertical)
        m_model->insertColumns(firstIndex + m_firstSetSection, sets.count());
    else
        m_model->insertRows(firstIndex + m_firstSetSection, sets.count());

    for (int i = 0; i < sets.count(); ++i) {
        int section = i + firstIndex + m_firstSetSection;
        m_model->setData(candlestickModelIndex(section, m_timestamp), sets.at(i)->timestamp());
        m_model->setData(candlestickModelIndex(section, m_open), sets.at(i)->open());
        m_model->setData(candlestickModelIndex(section, m_high), sets.at(i)->high());
        m_model->setData(candlestickModelIndex(section, m_low), sets.at(i)->low());
        m_model->setData(candlestickModelIndex(section, m_close), sets.at(i)->close());
    }
    blockModelSignals(false);
    initializeCandlestickFromModel();
}

void QCandlestickModelMapperPrivate::candlestickSetsRemoved(const QList<QCandlestickSet *> &sets)
{
    Q_Q(QCandlestickModelMapper);

    if (m_seriesSignalsBlock)
        return;

    if (sets.isEmpty())
        return;

    int firstIndex = m_sets.indexOf(sets.at(0));
    if (firstIndex == -1)
        return;

    m_lastSetSection -= sets.count();

    for (int i = firstIndex + sets.count() - 1; i >= firstIndex; --i)
        m_sets.removeAt(i);

    blockModelSignals();
    if (q->orientation() == Qt::Vertical)
        m_model->removeColumns(firstIndex + m_firstSetSection, sets.count());
    else
        m_model->removeRows(firstIndex + m_firstSetSection, sets.count());
    blockModelSignals(false);
    initializeCandlestickFromModel();
}

void QCandlestickModelMapperPrivate::candlestickSetChanged()
{
    if (m_seriesSignalsBlock)
        return;

    QCandlestickSet *set = qobject_cast<QCandlestickSet *>(QObject::sender());
    if (!set)
        return;

    int section = m_series->sets().indexOf(set);
    if (section < 0)
        return;

    section += m_firstSetSection;

    blockModelSignals();
    m_model->setData(candlestickModelIndex(section, m_timestamp), set->timestamp());
    m_model->setData(candlestickModelIndex(section, m_open), set->open());
    m_model->setData(candlestickModelIndex(section, m_high), set->high());
    m_model->setData(candlestickModelIndex(section, m_low), set->low());
    m_model->setData(candlestickModelIndex(section, m_close), set->close());
    blockModelSignals(false);
}

void QCandlestickModelMapperPrivate::seriesDestroyed()
{
    m_series = 0;
}

QCandlestickSet *QCandlestickModelMapperPrivate::candlestickSet(QModelIndex index)
{
    Q_Q(QCandlestickModelMapper);

    if (!index.isValid())
        return 0;

    int section = (q->orientation() == Qt::Vertical) ? index.column() : index.row();
    int pos = (q->orientation() == Qt::Vertical) ? index.row() : index.column();

    if (section < m_firstSetSection || section > m_lastSetSection)
        return 0; // This part of model has not been mapped to any candlestick set.

    if (pos != m_timestamp && pos != m_open && pos != m_high && pos != m_low && pos != m_close)
        return 0; // This part of model has not been mapped to any candlestick set.

    return m_series->sets().at(section - m_firstSetSection);
}

QModelIndex QCandlestickModelMapperPrivate::candlestickModelIndex(int section, int pos)
{
    Q_Q(QCandlestickModelMapper);

    if (section < m_firstSetSection || section > m_lastSetSection)
        return QModelIndex(); // invalid

    if (pos != m_timestamp && pos != m_open && pos != m_high && pos != m_low && pos != m_close)
        return QModelIndex(); // invalid

    if (q->orientation() == Qt::Vertical)
        return m_model->index(pos, section);
    else
        return m_model->index(section, pos);
}

void QCandlestickModelMapperPrivate::insertData(int start, int end)
{
    Q_UNUSED(start)
    Q_UNUSED(end)

    // Currently candlestickchart needs to be fully recalculated when change is made.
    initializeCandlestickFromModel();
}

void QCandlestickModelMapperPrivate::removeData(int start, int end)
{
    Q_UNUSED(start)
    Q_UNUSED(end)

    // Currently candlestickchart needs to be fully recalculated when change is made.
    initializeCandlestickFromModel();
}

void QCandlestickModelMapperPrivate::blockModelSignals(bool block)
{
    m_modelSignalsBlock = block;
}

void QCandlestickModelMapperPrivate::blockSeriesSignals(bool block)
{
    m_seriesSignalsBlock = block;
}

#include "moc_qcandlestickmodelmapper.cpp"
#include "moc_qcandlestickmodelmapper_p.cpp"

QT_CHARTS_END_NAMESPACE
