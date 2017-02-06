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

#include "qsurfacedataproxy_p.h"
#include "qsurface3dseries_p.h"
#include "qabstract3daxis_p.h"

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

/*!
 * \class QSurfaceDataProxy
 * \inmodule QtDataVisualization
 * \brief Base proxy class for Q3DSurface.
 * \since QtDataVisualization 1.0
 *
 * QSurfaceDataProxy takes care of surface related data handling. The QSurfaceDataProxy handles the
 * data in rows and for this it provides two auxiliary typedefs. QSurfaceDataArray is a QList for
 * controlling the rows. For rows there is a QVector QSurfaceDataRow which contains QSurfaceDataItem
 * objects. See Q3DSurface documentation and basic sample code there how to feed the data for the
 * QSurfaceDataProxy.
 *
 * All rows must have the same number of items.
 *
 * QSurfaceDataProxy takes ownership of all QSurfaceDataRows passed to it, whether directly or
 * in a QSurfaceDataArray container.
 * If you use QSurfaceDataRow pointers to directly modify data after adding the array to the proxy,
 * you must also emit proper signal to make the graph update.
 *
 * To make a sensible surface, the X-value of each successive item in all rows must be
 * either ascending or descending throughout the row.
 * Similarly, the Z-value of each successive item in all columns must be either ascending or
 * descending throughout the column.
 *
 * \note Currently only surfaces with straight rows and columns are fully supported. Any row
 * with items that do not have the exact same Z-value or any column with items that do not have
 * the exact same X-value may get clipped incorrectly if the whole surface doesn't completely fit
 * in the visible X or Z axis ranges.
 *
 * \note Surfaces with less than two rows or columns are not considered valid surfaces and will
 * not be rendered.
 *
 * \note On some environments, surfaces with a lot of visible vertices may not render, because
 * they exceed the per-draw vertex count supported by the graphics driver.
 * This is mostly an issue on 32bit and/or OpenGL ES2 platforms.
 *
 * \sa {Qt Data Visualization Data Handling}
 */

/*!
 * \typedef QtDataVisualization::QSurfaceDataRow
 * \relates QSurfaceDataProxy
 *
 * A vector of \l {QSurfaceDataItem}s.
 */

/*!
 * \typedef QtDataVisualization::QSurfaceDataArray
 * \relates QSurfaceDataProxy
 *
 * A list of pointers to \l {QSurfaceDataRow}s.
 */

/*!
 * \qmltype SurfaceDataProxy
 * \inqmlmodule QtDataVisualization
 * \since QtDataVisualization 1.0
 * \ingroup datavisualization_qml
 * \instantiates QSurfaceDataProxy
 * \inherits AbstractDataProxy
 * \brief Base proxy class for Surface3D.
 *
 * This type handles surface data items. The data is arranged into rows and columns, and all rows must have
 * the same number of columns.
 *
 * This type is uncreatable, but contains properties that are exposed via subtypes.
 *
 * For a more complete description, see QSurfaceDataProxy.
 *
 * \sa ItemModelSurfaceDataProxy, {Qt Data Visualization Data Handling}
 */

/*!
 * \qmlproperty int SurfaceDataProxy::rowCount
 * Number of the rows in the array.
 */

/*!
 * \qmlproperty int SurfaceDataProxy::columnCount
 * Number of the columns in the array.
 */

/*!
 * \qmlproperty Surface3DSeries SurfaceDataProxy::series
 *
 * The series this proxy is attached to.
 */

/*!
 * Constructs QSurfaceDataProxy with the given \a parent.
 */
QSurfaceDataProxy::QSurfaceDataProxy(QObject *parent) :
    QAbstractDataProxy(new QSurfaceDataProxyPrivate(this), parent)
{
}

/*!
 * \internal
 */
QSurfaceDataProxy::QSurfaceDataProxy(QSurfaceDataProxyPrivate *d, QObject *parent) :
    QAbstractDataProxy(d, parent)
{
}

/*!
 * Destroys QSurfaceDataProxy.
 */
QSurfaceDataProxy::~QSurfaceDataProxy()
{
}

/*!
 * \property QSurfaceDataProxy::series
 *
 *  The series this proxy is attached to.
 */
QSurface3DSeries *QSurfaceDataProxy::series() const
{
    return static_cast<QSurface3DSeries *>(d_ptr->series());
}

/*!
 * Takes ownership of the \a newArray. Clears the existing array if the \a newArray is
 * different from the existing array. If it's the same array, this just triggers arrayReset()
 * signal.
 * Passing a null array deletes the old array and creates a new empty array.
 * All rows in \a newArray must be of same length.
 */
void QSurfaceDataProxy::resetArray(QSurfaceDataArray *newArray)
{
    if (dptr()->m_dataArray != newArray) {
        dptr()->resetArray(newArray);
    }
    emit arrayReset();
    emit rowCountChanged(rowCount());
    emit columnCountChanged(columnCount());
}

/*!
 * Changes existing row by replacing a row at \a rowIndex with a new \a row. The \a row can be
 * the same as the existing row already stored at the \a rowIndex. The new \a row must have
 * the same number of columns as the row it is replacing.
 */
void QSurfaceDataProxy::setRow(int rowIndex, QSurfaceDataRow *row)
{
    dptr()->setRow(rowIndex, row);
    emit rowsChanged(rowIndex, 1);
}

/*!
 * Changes existing rows by replacing a rows starting at \a rowIndex with \a rows.
 * The rows in the \a rows array can be the same as the existing rows already
 * stored at the \a rowIndex. The new rows must have the same number of columns
 * as the rows they are replacing.
 */
void QSurfaceDataProxy::setRows(int rowIndex, const QSurfaceDataArray &rows)
{
    dptr()->setRows(rowIndex, rows);
    emit rowsChanged(rowIndex, rows.size());
}

/*!
 * Changes a single item at \a rowIndex, \a columnIndex to the \a item.
 */
void QSurfaceDataProxy::setItem(int rowIndex, int columnIndex, const QSurfaceDataItem &item)
{
    dptr()->setItem(rowIndex, columnIndex, item);
    emit itemChanged(rowIndex, columnIndex);
}

/*!
 * Changes a single item at \a position to the \a item.
 * The X-value of \a position indicates the row and the Y-value indicates the column.
 */
void QSurfaceDataProxy::setItem(const QPoint &position, const QSurfaceDataItem &item)
{
    setItem(position.x(), position.y(), item);
}

/*!
 * Adds a new \a row to the end of array. The new \a row must have
 * the same number of columns as the rows at the initial array.
 *
 * \return index of the added row.
 */
int QSurfaceDataProxy::addRow(QSurfaceDataRow *row)
{
    int addIndex = dptr()->addRow(row);
    emit rowsAdded(addIndex, 1);
    emit rowCountChanged(rowCount());
    return addIndex;
}

/*!
 * Adds new \a rows to the end of array. The new rows must have the same number of columns
 * as the rows at the initial array.
 *
 * \return index of the first added row.
 */
int QSurfaceDataProxy::addRows(const QSurfaceDataArray &rows)
{
    int addIndex = dptr()->addRows(rows);
    emit rowsAdded(addIndex, rows.size());
    emit rowCountChanged(rowCount());
    return addIndex;
}

/*!
 * Inserts a new \a row into \a rowIndex.
 * If rowIndex is equal to array size, rows are added to end of the array. The new \a row must have
 * the same number of columns as the rows at the initial array.
 */
void QSurfaceDataProxy::insertRow(int rowIndex, QSurfaceDataRow *row)
{
    dptr()->insertRow(rowIndex, row);
    emit rowsInserted(rowIndex, 1);
    emit rowCountChanged(rowCount());
}

/*!
 * Inserts new \a rows into \a rowIndex.
 * If rowIndex is equal to array size, rows are added to end of the array. The new \a rows must have
 * the same number of columns as the rows at the initial array.
 */
void QSurfaceDataProxy::insertRows(int rowIndex, const QSurfaceDataArray &rows)
{
    dptr()->insertRows(rowIndex, rows);
    emit rowsInserted(rowIndex, rows.size());
    emit rowCountChanged(rowCount());
}

/*!
 * Removes \a removeCount rows staring at \a rowIndex. Attempting to remove rows past the end of the
 * array does nothing.
 */
void QSurfaceDataProxy::removeRows(int rowIndex, int removeCount)
{
    if (rowIndex < rowCount() && removeCount >= 1) {
        dptr()->removeRows(rowIndex, removeCount);
        emit rowsRemoved(rowIndex, removeCount);
        emit rowCountChanged(rowCount());
    }
}

/*!
 * \return pointer to the data array.
 */
const QSurfaceDataArray *QSurfaceDataProxy::array() const
{
    return dptrc()->m_dataArray;
}

/*!
 * \return pointer to the item at \a rowIndex, \a columnIndex. It is guaranteed to be valid only
 * until the next call that modifies data.
 */
const QSurfaceDataItem *QSurfaceDataProxy::itemAt(int rowIndex, int columnIndex) const
{
    const QSurfaceDataArray &dataArray = *dptrc()->m_dataArray;
    Q_ASSERT(rowIndex >= 0 && rowIndex < dataArray.size());
    const QSurfaceDataRow &dataRow = *dataArray[rowIndex];
    Q_ASSERT(columnIndex >= 0 && columnIndex < dataRow.size());
    return &dataRow.at(columnIndex);
}

/*!
 * \return pointer to the item at \a position. The X-value of \a position indicates the row
 * and the Y-value indicates the column. The item is guaranteed to be valid only
 * until the next call that modifies data.
 */
const QSurfaceDataItem *QSurfaceDataProxy::itemAt(const QPoint &position) const
{
    return itemAt(position.x(), position.y());
}

/*!
 * \property QSurfaceDataProxy::rowCount
 *
 * \return number of rows in the data.
 */
int QSurfaceDataProxy::rowCount() const
{
    return dptrc()->m_dataArray->size();
}

/*!
 * \property QSurfaceDataProxy::columnCount
 *
 * \return number of items in the columns.
 */
int QSurfaceDataProxy::columnCount() const
{
    if (dptrc()->m_dataArray->size() > 0)
        return dptrc()->m_dataArray->at(0)->size();
    else
        return 0;
}

/*!
 * \internal
 */
QSurfaceDataProxyPrivate *QSurfaceDataProxy::dptr()
{
    return static_cast<QSurfaceDataProxyPrivate *>(d_ptr.data());
}

/*!
 * \internal
 */
const QSurfaceDataProxyPrivate *QSurfaceDataProxy::dptrc() const
{
    return static_cast<const QSurfaceDataProxyPrivate *>(d_ptr.data());
}

/*!
 * \fn void QSurfaceDataProxy::arrayReset()
 *
 * Emitted when the data array is reset.
 * If you change the whole array contents without calling resetArray(), you need to
 * emit this signal yourself or the graph won't get updated.
 */

/*!
 * \fn void QSurfaceDataProxy::rowsAdded(int startIndex, int count)
 *
 * Emitted when rows have been added. Provides \a startIndex and \a count of rows added.
 * If you add rows directly to the array without calling addRow() or addRows(), you
 * need to emit this signal yourself or the graph won't get updated.
 */

/*!
 * \fn void QSurfaceDataProxy::rowsChanged(int startIndex, int count)
 *
 * Emitted when rows have changed. Provides \a startIndex and \a count of changed rows.
 * If you change rows directly in the array without calling setRow() or setRows(), you
 * need to emit this signal yourself or the graph won't get updated.
 */

/*!
 * \fn void QSurfaceDataProxy::rowsRemoved(int startIndex, int count)
 *
 * Emitted when rows have been removed. Provides \a startIndex and \a count of rows removed.
 * Index is the current array size if rows were removed from the end of the array.
 * If you remove rows directly from the array without calling removeRows(), you
 * need to emit this signal yourself or the graph won't get updated.
 */

/*!
 * \fn void QSurfaceDataProxy::rowsInserted(int startIndex, int count)
 *
 * Emitted when rows have been inserted. Provides \a startIndex and \a count of inserted rows.
 * If you insert rows directly into the array without calling insertRow() or insertRows(), you
 * need to emit this signal yourself or the graph won't get updated.
 */

/*!
 * \fn void QSurfaceDataProxy::itemChanged(int rowIndex, int columnIndex)
 *
 * Emitted when an item has changed. Provides \a rowIndex and \a columnIndex of changed item.
 * If you change an item directly in the array without calling setItem(), you
 * need to emit this signal yourself or the graph won't get updated.
 */

//  QSurfaceDataProxyPrivate

QSurfaceDataProxyPrivate::QSurfaceDataProxyPrivate(QSurfaceDataProxy *q)
    : QAbstractDataProxyPrivate(q, QAbstractDataProxy::DataTypeSurface),
      m_dataArray(new QSurfaceDataArray)
{
}

QSurfaceDataProxyPrivate::~QSurfaceDataProxyPrivate()
{
    clearArray();
}

void QSurfaceDataProxyPrivate::resetArray(QSurfaceDataArray *newArray)
{
    if (!newArray)
        newArray = new QSurfaceDataArray;

    if (newArray != m_dataArray) {
        clearArray();
        m_dataArray = newArray;
    }
}

void QSurfaceDataProxyPrivate::setRow(int rowIndex, QSurfaceDataRow *row)
{
    Q_ASSERT(rowIndex >= 0 && rowIndex < m_dataArray->size());
    Q_ASSERT(m_dataArray->at(rowIndex)->size() == row->size());

    if (row != m_dataArray->at(rowIndex)) {
        clearRow(rowIndex);
        (*m_dataArray)[rowIndex] = row;
    }
}

void QSurfaceDataProxyPrivate::setRows(int rowIndex, const QSurfaceDataArray &rows)
{
    QSurfaceDataArray &dataArray = *m_dataArray;
    Q_ASSERT(rowIndex >= 0 && (rowIndex + rows.size()) <= dataArray.size());

    for (int i = 0; i < rows.size(); i++) {
        Q_ASSERT(m_dataArray->at(rowIndex)->size() == rows.at(i)->size());
        if (rows.at(i) != dataArray.at(rowIndex)) {
            clearRow(rowIndex);
            dataArray[rowIndex] = rows.at(i);
        }
        rowIndex++;
    }
}

void QSurfaceDataProxyPrivate::setItem(int rowIndex, int columnIndex, const QSurfaceDataItem &item)
{
    Q_ASSERT(rowIndex >= 0 && rowIndex < m_dataArray->size());
    QSurfaceDataRow &row = *(*m_dataArray)[rowIndex];
    Q_ASSERT(columnIndex < row.size());
    row[columnIndex] = item;
}

int QSurfaceDataProxyPrivate::addRow(QSurfaceDataRow *row)
{
    Q_ASSERT(m_dataArray->at(0)->size() == row->size());
    int currentSize = m_dataArray->size();
    m_dataArray->append(row);
    return currentSize;
}

int QSurfaceDataProxyPrivate::addRows(const QSurfaceDataArray &rows)
{
    int currentSize = m_dataArray->size();
    for (int i = 0; i < rows.size(); i++) {
        Q_ASSERT(m_dataArray->at(0)->size() == rows.at(i)->size());
        m_dataArray->append(rows.at(i));
    }
    return currentSize;
}

void QSurfaceDataProxyPrivate::insertRow(int rowIndex, QSurfaceDataRow *row)
{
    Q_ASSERT(rowIndex >= 0 && rowIndex <= m_dataArray->size());
    Q_ASSERT(m_dataArray->at(0)->size() == row->size());
    m_dataArray->insert(rowIndex, row);
}

void QSurfaceDataProxyPrivate::insertRows(int rowIndex, const QSurfaceDataArray &rows)
{
    Q_ASSERT(rowIndex >= 0 && rowIndex <= m_dataArray->size());

    for (int i = 0; i < rows.size(); i++) {
        Q_ASSERT(m_dataArray->at(0)->size() == rows.at(i)->size());
        m_dataArray->insert(rowIndex++, rows.at(i));
    }
}

void QSurfaceDataProxyPrivate::removeRows(int rowIndex, int removeCount)
{
    Q_ASSERT(rowIndex >= 0);
    int maxRemoveCount = m_dataArray->size() - rowIndex;
    removeCount = qMin(removeCount, maxRemoveCount);
    for (int i = 0; i < removeCount; i++) {
        clearRow(rowIndex);
        m_dataArray->removeAt(rowIndex);
    }
}

QSurfaceDataProxy *QSurfaceDataProxyPrivate::qptr()
{
    return static_cast<QSurfaceDataProxy *>(q_ptr);
}

void QSurfaceDataProxyPrivate::limitValues(QVector3D &minValues, QVector3D &maxValues,
                                           QAbstract3DAxis *axisX, QAbstract3DAxis *axisY,
                                           QAbstract3DAxis *axisZ) const
{
    float min = 0.0f;
    float max = 0.0f;

    int rows = m_dataArray->size();
    int columns = 0;
    if (rows)
        columns = m_dataArray->at(0)->size();

    if (rows && columns) {
        min = m_dataArray->at(0)->at(0).y();
        max = m_dataArray->at(0)->at(0).y();
    }

    for (int i = 0; i < rows; i++) {
        QSurfaceDataRow *row = m_dataArray->at(i);
        if (row) {
            for (int j = 0; j < columns; j++) {
                float itemValue = m_dataArray->at(i)->at(j).y();
                if (qIsNaN(itemValue) || qIsInf(itemValue))
                    continue;
                if (min > itemValue && isValidValue(itemValue, axisY))
                    min = itemValue;
                if (max < itemValue)
                    max = itemValue;
            }
        }
    }

    minValues.setY(min);
    maxValues.setY(max);

    if (columns) {
        // Have some defaults
        float xLow = m_dataArray->at(0)->at(0).x();
        float xHigh = m_dataArray->at(0)->last().x();
        float zLow = m_dataArray->at(0)->at(0).z();
        float zHigh = m_dataArray->last()->at(0).z();
        for (int i = 0; i < columns; i++) {
            float zItemValue = m_dataArray->at(0)->at(i).z();
            if (qIsNaN(zItemValue) || qIsInf(zItemValue))
                continue;
            else if (isValidValue(zItemValue, axisZ))
                zLow = qMin(zLow,zItemValue);
        }
        for (int i = 0; i < columns; i++) {
            float zItemValue = m_dataArray->last()->at(i).z();
            if (qIsNaN(zItemValue) || qIsInf(zItemValue))
                continue;
            else if (isValidValue(zItemValue, axisZ))
                zHigh = qMax(zHigh, zItemValue);
        }
        for (int i = 0; i < rows; i++) {
            float xItemValue = m_dataArray->at(i)->at(0).x();
            if (qIsNaN(xItemValue) || qIsInf(xItemValue))
                continue;
            else if (isValidValue(xItemValue, axisX))
                xLow = qMin(xLow, xItemValue);
        }
        for (int i = 0; i < rows; i++) {
            float xItemValue = m_dataArray->at(i)->last().x();
            if (qIsNaN(xItemValue) || qIsInf(xItemValue))
                continue;
            else if (isValidValue(xItemValue, axisX))
                xHigh = qMax(xHigh, xItemValue);
        }
        minValues.setX(xLow);
        minValues.setZ(zLow);
        maxValues.setX(xHigh);
        maxValues.setZ(zHigh);
    } else {
        minValues.setX(axisX->d_ptr->allowZero() ? 0.0f : 1.0f);
        minValues.setZ(axisZ->d_ptr->allowZero() ? 0.0f : 1.0f);
        maxValues.setX(axisX->d_ptr->allowZero() ? 0.0f : 1.0f);
        maxValues.setZ(axisZ->d_ptr->allowZero() ? 0.0f : 1.0f);
    }
}

bool QSurfaceDataProxyPrivate::isValidValue(float value, QAbstract3DAxis *axis) const
{
    return (value > 0.0f || (value == 0.0f && axis->d_ptr->allowZero())
            || (value < 0.0f && axis->d_ptr->allowNegatives()));
}

void QSurfaceDataProxyPrivate::clearRow(int rowIndex)
{
    if (m_dataArray->at(rowIndex)) {
        delete m_dataArray->at(rowIndex);
        (*m_dataArray)[rowIndex] = 0;
    }
}

void QSurfaceDataProxyPrivate::clearArray()
{
    for (int i = 0; i < m_dataArray->size(); i++)
        clearRow(i);
    m_dataArray->clear();
    delete m_dataArray;
}

void QSurfaceDataProxyPrivate::setSeries(QAbstract3DSeries *series)
{
    QAbstractDataProxyPrivate::setSeries(series);
    QSurface3DSeries *surfaceSeries = static_cast<QSurface3DSeries *>(series);
    emit qptr()->seriesChanged(surfaceSeries);
}

QT_END_NAMESPACE_DATAVISUALIZATION
