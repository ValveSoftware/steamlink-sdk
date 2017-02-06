/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Data Visualization module of the Qt Toolkit.
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

#include "qscatterdataproxy_p.h"
#include "qscatter3dseries_p.h"
#include "qabstract3daxis_p.h"

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

/*!
 * \class QScatterDataProxy
 * \inmodule QtDataVisualization
 * \brief Base proxy class for Q3DScatter.
 * \since QtDataVisualization 1.0
 *
 * QScatterDataProxy handles adding, inserting, changing, and removing data items.
 *
 * QScatterDataProxy takes ownership of all QScatterDataArrays and QScatterDataItems passed to it.
 *
 * \sa {Qt Data Visualization Data Handling}
 */

/*!
 * \typedef QtDataVisualization::QScatterDataArray
 * \relates QScatterDataProxy
 *
 * A vector of \l {QScatterDataItem}s.
 */

/*!
 * \qmltype ScatterDataProxy
 * \inqmlmodule QtDataVisualization
 * \since QtDataVisualization 1.0
 * \ingroup datavisualization_qml
 * \instantiates QScatterDataProxy
 * \inherits AbstractDataProxy
 * \brief Base proxy class for Scatter3D.
 *
 * This type handles adding, inserting, changing, and removing data items.
 *
 * This type is uncreatable, but contains properties that are exposed via subtypes.
 *
 * For more complete description, see QScatterDataProxy.
 *
 * \sa ItemModelScatterDataProxy, {Qt Data Visualization Data Handling}
 */

/*!
 * \qmlproperty int ScatterDataProxy::itemCount
 * Item count in the array.
 */

/*!
 * \qmlproperty Scatter3DSeries ScatterDataProxy::series
 *
 * The series this proxy is attached to.
 */

/*!
 * Constructs QScatterDataProxy with the given \a parent.
 */
QScatterDataProxy::QScatterDataProxy(QObject *parent) :
    QAbstractDataProxy(new QScatterDataProxyPrivate(this), parent)
{
}

/*!
 * \internal
 */
QScatterDataProxy::QScatterDataProxy(QScatterDataProxyPrivate *d, QObject *parent) :
    QAbstractDataProxy(d, parent)
{
}

/*!
 * Destroys QScatterDataProxy.
 */
QScatterDataProxy::~QScatterDataProxy()
{
}

/*!
 * \property QScatterDataProxy::series
 *
 *  The series this proxy is attached to.
 */
QScatter3DSeries *QScatterDataProxy::series() const
{
    return static_cast<QScatter3DSeries *>(d_ptr->series());
}

/*!
 * Takes ownership of the \a newArray. Clears the existing array if the \a newArray is
 * different from the existing array. If it's the same array, this just triggers arrayReset()
 * signal.
 * Passing a null array deletes the old array and creates a new empty array.
 */
void QScatterDataProxy::resetArray(QScatterDataArray *newArray)
{
    if (dptr()->m_dataArray != newArray)
        dptr()->resetArray(newArray);

    emit arrayReset();
    emit itemCountChanged(itemCount());
}

/*!
 * Changes a single item at \a index with \a item.
 */
void QScatterDataProxy::setItem(int index, const QScatterDataItem &item)
{
    dptr()->setItem(index, item);
    emit itemsChanged(index, 1);
}

/*!
 * Changes items starting from \a index with \a items.
 */
void QScatterDataProxy::setItems(int index, const QScatterDataArray &items)
{
    dptr()->setItems(index, items);
    emit itemsChanged(index, items.size());
}

/*!
 * Adds a single \a item to the end of the array.
 *
 * \return index of the added item.
 */
int QScatterDataProxy::addItem(const QScatterDataItem &item)
{
    int addIndex = dptr()->addItem(item);
    emit itemsAdded(addIndex, 1);
    emit itemCountChanged(itemCount());
    return addIndex;
}

/*!
 * Adds \a items to the end of the array.
 *
 * \return index of the first added item.
 */
int QScatterDataProxy::addItems(const QScatterDataArray &items)
{
    int addIndex = dptr()->addItems(items);
    emit itemsAdded(addIndex, items.size());
    emit itemCountChanged(itemCount());
    return addIndex;
}

/*!
 * Inserts a single \a item to \a index. If index is equal to data array size, item is added to
 * the array.
 */
void QScatterDataProxy::insertItem(int index, const QScatterDataItem &item)
{
    dptr()->insertItem(index, item);
    emit itemsInserted(index, 1);
    emit itemCountChanged(itemCount());
}

/*!
 * Inserts \a items to \a index. If index is equal to data array size, items are added to the array.
 */
void QScatterDataProxy::insertItems(int index, const QScatterDataArray &items)
{
    dptr()->insertItems(index, items);
    emit itemsInserted(index, items.size());
    emit itemCountChanged(itemCount());
}

/*!
 * Removes \a removeCount items starting from \a index. Attempting to remove items past the end of
 * the array does nothing.
 */
void QScatterDataProxy::removeItems(int index, int removeCount)
{
    if (index >= dptr()->m_dataArray->size())
        return;

    dptr()->removeItems(index, removeCount);
    emit itemsRemoved(index, removeCount);
    emit itemCountChanged(itemCount());
}

/*!
 * \property QScatterDataProxy::itemCount
 *
 * \return item count in the array.
 */
int QScatterDataProxy::itemCount() const
{
    return dptrc()->m_dataArray->size();
}

/*!
 * \return pointer to the data array.
 */
const QScatterDataArray *QScatterDataProxy::array() const
{
    return dptrc()->m_dataArray;
}

/*!
 * \return pointer to the item at \a index. It is guaranteed to be valid only until next call
 * that modifies data.
 */
const QScatterDataItem *QScatterDataProxy::itemAt(int index) const
{
    return &dptrc()->m_dataArray->at(index);
}

/*!
 * \internal
 */
QScatterDataProxyPrivate *QScatterDataProxy::dptr()
{
    return static_cast<QScatterDataProxyPrivate *>(d_ptr.data());
}

/*!
 * \internal
 */
const QScatterDataProxyPrivate *QScatterDataProxy::dptrc() const
{
    return static_cast<const QScatterDataProxyPrivate *>(d_ptr.data());
}

/*!
 * \fn void QScatterDataProxy::arrayReset()
 *
 * Emitted when data array is reset.
 * If you change the whole array contents without calling resetArray(), you need to
 * emit this signal yourself or the graph won't get updated.
 */

/*!
 * \fn void QScatterDataProxy::itemsAdded(int startIndex, int count)
 *
 * Emitted when items have been added. Provides \a startIndex and \a count of items added.
 * If you add items directly to the array without calling addItem() or addItems(), you
 * need to emit this signal yourself or the graph won't get updated.
 */

/*!
 * \fn void QScatterDataProxy::itemsChanged(int startIndex, int count)
 *
 * Emitted when items have changed. Provides \a startIndex and \a count of changed items.
 * If you change items directly in the array without calling setItem() or setItems(), you
 * need to emit this signal yourself or the graph won't get updated.
 */

/*!
 * \fn void QScatterDataProxy::itemsRemoved(int startIndex, int count)
 *
 * Emitted when items have been removed. Provides \a startIndex and \a count of items removed.
 * Index may be over current array size if removed from end.
 * If you remove items directly from the array without calling removeItems(), you
 * need to emit this signal yourself or the graph won't get updated.
 */

/*!
 * \fn void QScatterDataProxy::itemsInserted(int startIndex, int count)
 *
 * Emitted when items have been inserted. Provides \a startIndex and \a count of inserted items.
 * If you insert items directly into the array without calling insertItem() or insertItems(), you
 * need to emit this signal yourself or the graph won't get updated.
 */

// QScatterDataProxyPrivate

QScatterDataProxyPrivate::QScatterDataProxyPrivate(QScatterDataProxy *q)
    : QAbstractDataProxyPrivate(q, QAbstractDataProxy::DataTypeScatter),
      m_dataArray(new QScatterDataArray)
{
}

QScatterDataProxyPrivate::~QScatterDataProxyPrivate()
{
    m_dataArray->clear();
    delete m_dataArray;
}

void QScatterDataProxyPrivate::resetArray(QScatterDataArray *newArray)
{
    if (!newArray)
        newArray = new QScatterDataArray;

    if (newArray != m_dataArray) {
        m_dataArray->clear();
        delete m_dataArray;
        m_dataArray = newArray;
    }
}

void QScatterDataProxyPrivate::setItem(int index, const QScatterDataItem &item)
{
    Q_ASSERT(index >= 0 && index < m_dataArray->size());
    (*m_dataArray)[index] = item;
}

void QScatterDataProxyPrivate::setItems(int index, const QScatterDataArray &items)
{
    Q_ASSERT(index >= 0 && (index + items.size()) <= m_dataArray->size());
    for (int i = 0; i < items.size(); i++)
        (*m_dataArray)[index++] = items[i];
}

int QScatterDataProxyPrivate::addItem(const QScatterDataItem &item)
{
    int currentSize = m_dataArray->size();
    m_dataArray->append(item);
    return currentSize;
}

int QScatterDataProxyPrivate::addItems(const QScatterDataArray &items)
{
    int currentSize = m_dataArray->size();
    (*m_dataArray) += items;
    return currentSize;
}

void QScatterDataProxyPrivate::insertItem(int index, const QScatterDataItem &item)
{
    Q_ASSERT(index >= 0 && index <= m_dataArray->size());
    m_dataArray->insert(index, item);
}

void QScatterDataProxyPrivate::insertItems(int index, const QScatterDataArray &items)
{
    Q_ASSERT(index >= 0 && index <= m_dataArray->size());
    for (int i = 0; i < items.size(); i++)
        m_dataArray->insert(index++, items.at(i));
}

void QScatterDataProxyPrivate::removeItems(int index, int removeCount)
{
    Q_ASSERT(index >= 0);
    int maxRemoveCount = m_dataArray->size() - index;
    removeCount = qMin(removeCount, maxRemoveCount);
    m_dataArray->remove(index, removeCount);
}

void QScatterDataProxyPrivate::limitValues(QVector3D &minValues, QVector3D &maxValues,
                                           QAbstract3DAxis *axisX, QAbstract3DAxis *axisY,
                                           QAbstract3DAxis *axisZ) const
{
    if (m_dataArray->isEmpty())
        return;

    const QVector3D &firstPos = m_dataArray->at(0).position();

    float minX = firstPos.x();
    float maxX = minX;
    float minY = firstPos.y();
    float maxY = minY;
    float minZ = firstPos.z();
    float maxZ = minZ;

    if (m_dataArray->size() > 1) {
        for (int i = 1; i < m_dataArray->size(); i++) {
            const QVector3D &pos = m_dataArray->at(i).position();

            float value = pos.x();
            if (qIsNaN(value) || qIsInf(value))
                continue;
            if (isValidValue(minX, value, axisX))
                minX = value;
            if (maxX < value)
                maxX = value;

            value = pos.y();
            if (qIsNaN(value) || qIsInf(value))
                continue;
            if (isValidValue(minY, value, axisY))
                minY = value;
            if (maxY < value)
                maxY = value;

            value = pos.z();
            if (qIsNaN(value) || qIsInf(value))
                continue;
            if (isValidValue(minZ, value, axisZ))
                minZ = value;
            if (maxZ < value)
                maxZ = value;
        }
    }

    minValues.setX(minX);
    minValues.setY(minY);
    minValues.setZ(minZ);

    maxValues.setX(maxX);
    maxValues.setY(maxY);
    maxValues.setZ(maxZ);
}

bool QScatterDataProxyPrivate::isValidValue(float axisValue, float value,
                                            QAbstract3DAxis *axis) const
{
    return (axisValue > value && (value > 0.0f
                                  || (value == 0.0f && axis->d_ptr->allowZero())
                                  || (value < 0.0f && axis->d_ptr->allowNegatives())));
}

void QScatterDataProxyPrivate::setSeries(QAbstract3DSeries *series)
{
    QAbstractDataProxyPrivate::setSeries(series);
    QScatter3DSeries *scatterSeries = static_cast<QScatter3DSeries *>(series);
    emit qptr()->seriesChanged(scatterSeries);
}

QScatterDataProxy *QScatterDataProxyPrivate::qptr()
{
    return static_cast<QScatterDataProxy *>(q_ptr);
}

QT_END_NAMESPACE_DATAVISUALIZATION
