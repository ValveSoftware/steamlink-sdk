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

#include "qbardataproxy_p.h"
#include "qbar3dseries_p.h"

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

/*!
 * \class QBarDataProxy
 * \inmodule QtDataVisualization
 * \brief Base proxy class for Q3DBars.
 * \since QtDataVisualization 1.0
 *
 * QBarDataProxy handles adding, inserting, changing and removing rows of data.
 *
 * The data array is a list of vectors (rows) of QBarDataItem instances.
 * Each row can contain different amount of items or even be null.
 *
 * QBarDataProxy takes ownership of all QBarDataRows passed to it, whether directly or
 * in a QBarDataArray container.
 * If you use QBarDataRow pointers to directly modify data after adding the array to the proxy,
 * you must also emit proper signal to make the graph update.
 *
 * QBarDataProxy optionally keeps track of row and column labels, which QCategory3DAxis can utilize
 * to show axis labels. The row and column labels are stored in separate array from the data and
 * row manipulation methods provide an alternate versions that don't affect the row labels.
 * This enables the option of having row labels that relate to the position of the data in the
 * array rather than the data itself.
 *
 * \sa {Qt Data Visualization Data Handling}
 */

/*!
 * \typedef QtDataVisualization::QBarDataRow
 * \relates QBarDataProxy
 *
 * A vector of \l {QBarDataItem}s.
 */

/*!
 * \typedef QtDataVisualization::QBarDataArray
 * \relates QBarDataProxy
 *
 * A list of pointers to \l {QBarDataRow}s.
 */

/*!
 * \qmltype BarDataProxy
 * \inqmlmodule QtDataVisualization
 * \since QtDataVisualization 1.0
 * \ingroup datavisualization_qml
 * \instantiates QBarDataProxy
 * \inherits AbstractDataProxy
 * \brief Base proxy type for Bars3D.
 *
 * This type handles adding, inserting, changing, and removing rows of data with Qt Quick 2.
 *
 * This type is uncreatable, but contains properties that are exposed via subtypes.
 *
 * For more complete description, see QBarDataProxy.
 *
 * \sa ItemModelBarDataProxy, {Qt Data Visualization Data Handling}
 */

/*!
 * \qmlproperty int BarDataProxy::rowCount
 * Row count in the array.
 */

/*!
 * \qmlproperty list BarDataProxy::rowLabels
 *
 * Optional row labels for the array. Indexes in this array match row indexes in data array.
 * If the list is shorter than row count, all rows will not get labels.
 */

/*!
 * \qmlproperty list BarDataProxy::columnLabels
 *
 * Optional column labels for the array. Indexes in this array match column indexes in rows.
 * If the list is shorter than the longest row, all columns will not get labels.
 */

/*!
 * \qmlproperty Bar3DSeries BarDataProxy::series
 *
 * The series this proxy is attached to.
 */

/*!
 * Constructs QBarDataProxy with the given \a parent.
 */
QBarDataProxy::QBarDataProxy(QObject *parent) :
    QAbstractDataProxy(new QBarDataProxyPrivate(this), parent)
{
}

/*!
 * \internal
 */
QBarDataProxy::QBarDataProxy(QBarDataProxyPrivate *d, QObject *parent) :
    QAbstractDataProxy(d, parent)
{
}

/*!
 * Destroys QBarDataProxy.
 */
QBarDataProxy::~QBarDataProxy()
{
}

/*!
 * \property QBarDataProxy::series
 *
 *  The series this proxy is attached to.
 */
QBar3DSeries *QBarDataProxy::series() const
{
    return static_cast<QBar3DSeries *>(d_ptr->series());
}

/*!
 * Clears the existing array and row and column labels.
 */
void QBarDataProxy::resetArray()
{
    resetArray(0, QStringList(), QStringList());
    emit rowCountChanged(rowCount());
}

/*!
 * Takes ownership of the \a newArray. Clears the existing array if the \a newArray is
 * different from the existing array. If it's the same array, this just triggers arrayReset()
 * signal.
 * Passing null array deletes the old array and creates a new empty array.
 * Row and column labels are not affected.
 */
void QBarDataProxy::resetArray(QBarDataArray *newArray)
{
    dptr()->resetArray(newArray, 0, 0);
    emit arrayReset();
    emit rowCountChanged(rowCount());
}

/*!
 * Takes ownership of the \a newArray. Clears the existing array if the \a newArray is
 * different from the existing array. If it's the same array, this just triggers arrayReset()
 * signal.
 * Passing a null array deletes the old array and creates a new empty array.
 * The \a rowLabels and \a columnLabels lists specify the new labels for rows and columns.
 */
void QBarDataProxy::resetArray(QBarDataArray *newArray, const QStringList &rowLabels,
                               const QStringList &columnLabels)
{
    dptr()->resetArray(newArray, &rowLabels, &columnLabels);
    emit arrayReset();
    emit rowCountChanged(rowCount());
}

/*!
 * Changes existing rows by replacing a row at \a rowIndex with a new \a row. The \a row can be
 * the same as the existing row already stored at the \a rowIndex.
 * Existing row labels are not affected.
 */
void QBarDataProxy::setRow(int rowIndex, QBarDataRow *row)
{
    dptr()->setRow(rowIndex, row, 0);
    emit rowsChanged(rowIndex, 1);
}

/*!
 * Changes existing rows by replacing a row at \a rowIndex with \a row. The \a row can be
 * the same as the existing row already stored at the \a rowIndex.
 * Changes the row label to the \a label.
 */
void QBarDataProxy::setRow(int rowIndex, QBarDataRow *row, const QString &label)
{
    dptr()->setRow(rowIndex, row, &label);
    emit rowsChanged(rowIndex, 1);
}

/*!
 * Changes existing rows by replacing a rows starting at \a rowIndex with \a rows.
 * Existing row labels are not affected. The rows in the \a rows array can be
 * the same as the existing rows already stored at the \a rowIndex.
 */
void QBarDataProxy::setRows(int rowIndex, const QBarDataArray &rows)
{
    dptr()->setRows(rowIndex, rows, 0);
    emit rowsChanged(rowIndex, rows.size());
}

/*!
 * Changes existing rows by replacing a rows starting at \a rowIndex with \a rows.
 * The row labels are changed to \a labels. The rows in the \a rows array can be
 * the same as the existing rows already stored at the \a rowIndex.
 */
void QBarDataProxy::setRows(int rowIndex, const QBarDataArray &rows, const QStringList &labels)
{
    dptr()->setRows(rowIndex, rows, &labels);
    emit rowsChanged(rowIndex, rows.size());
}

/*!
 * Changes a single item at \a rowIndex, \a columnIndex to the \a item.
 */
void QBarDataProxy::setItem(int rowIndex, int columnIndex, const QBarDataItem &item)
{
    dptr()->setItem(rowIndex, columnIndex, item);
    emit itemChanged(rowIndex, columnIndex);
}

/*!
 * Changes a single item at \a position to the \a item.
 * The X-value of \a position indicates the row and the Y-value indicates the column.
 */
void QBarDataProxy::setItem(const QPoint &position, const QBarDataItem &item)
{
    setItem(position.x(), position.y(), item);
}

/*!
 * Adds a new \a row to the end of array.
 * Existing row labels are not affected.
 *
 * \return index of the added row.
 */
int QBarDataProxy::addRow(QBarDataRow *row)
{
    int addIndex = dptr()->addRow(row, 0);
    emit rowsAdded(addIndex, 1);
    emit rowCountChanged(rowCount());
    return addIndex;
}

/*!
 * Adds a new \a row with the \a label to the end of array.
 *
 * \return index of the added row.
 */
int QBarDataProxy::addRow(QBarDataRow *row, const QString &label)
{
    int addIndex = dptr()->addRow(row, &label);
    emit rowsAdded(addIndex, 1);
    emit rowCountChanged(rowCount());
    return addIndex;
}

/*!
 * Adds new \a rows to the end of array.
 * Existing row labels are not affected.
 *
 * \return index of the first added row.
 */
int QBarDataProxy::addRows(const QBarDataArray &rows)
{
    int addIndex = dptr()->addRows(rows, 0);
    emit rowsAdded(addIndex, rows.size());
    emit rowCountChanged(rowCount());
    return addIndex;
}

/*!
 * Adds new \a rows with \a labels to the end of array.
 *
 * \return index of the first added row.
 */
int QBarDataProxy::addRows(const QBarDataArray &rows, const QStringList &labels)
{
    int addIndex = dptr()->addRows(rows, &labels);
    emit rowsAdded(addIndex, rows.size());
    emit rowCountChanged(rowCount());
    return addIndex;
}

/*!
 * Inserts a new \a row into \a rowIndex.
 * If rowIndex is equal to array size, rows are added to end of the array.
 * Any existing row labels are not affected.
 * \note Row labels array will be out of sync with row array after this call,
 *       if there were labeled rows beyond the inserted row.
 */
void QBarDataProxy::insertRow(int rowIndex, QBarDataRow *row)
{
    dptr()->insertRow(rowIndex, row, 0);
    emit rowsInserted(rowIndex, 1);
    emit rowCountChanged(rowCount());
}

/*!
 * Inserts a new \a row with the \a label into \a rowIndex.
 * If rowIndex is equal to array size, rows are added to end of the array.
 */
void QBarDataProxy::insertRow(int rowIndex, QBarDataRow *row, const QString &label)
{
    dptr()->insertRow(rowIndex, row, &label);
    emit rowsInserted(rowIndex, 1);
    emit rowCountChanged(rowCount());
}

/*!
 * Inserts new \a rows into \a rowIndex.
 * If rowIndex is equal to array size, rows are added to end of the array.
 * Any existing row labels are not affected.
 * \note Row labels array will be out of sync with row array after this call,
 *       if there were labeled rows beyond the inserted rows.
 */
void QBarDataProxy::insertRows(int rowIndex, const QBarDataArray &rows)
{
    dptr()->insertRows(rowIndex, rows, 0);
    emit rowsInserted(rowIndex, rows.size());
    emit rowCountChanged(rowCount());
}

/*!
 * Inserts new \a rows with \a labels into \a rowIndex.
 * If rowIndex is equal to array size, rows are added to end of the array.
 */
void QBarDataProxy::insertRows(int rowIndex, const QBarDataArray &rows, const QStringList &labels)
{
    dptr()->insertRows(rowIndex, rows, &labels);
    emit rowsInserted(rowIndex, rows.size());
    emit rowCountChanged(rowCount());
}

/*!
 * Removes \a removeCount rows staring at \a rowIndex. Attempting to remove rows past the end of the
 * array does nothing. If \a removeLabels is true, corresponding row labels are also removed. Otherwise
 * the row labels are not affected.
 * \note If \a removeLabels is false, the row labels array will be out of sync with the row array
 *       if there are labeled rows beyond the removed rows.
 */
void QBarDataProxy::removeRows(int rowIndex, int removeCount, bool removeLabels)
{
    if (rowIndex < rowCount() && removeCount >= 1) {
        dptr()->removeRows(rowIndex, removeCount, removeLabels);
        emit rowsRemoved(rowIndex, removeCount);
        emit rowCountChanged(rowCount());
    }
}

/*!
 * \property QBarDataProxy::rowCount
 *
 * Row \a count in the array.
 */
int QBarDataProxy::rowCount() const
{
    return dptrc()->m_dataArray->size();
}

/*!
 * \property QBarDataProxy::rowLabels
 *
 * Optional row \a labels for the array. Indexes in this array match row indexes in data array.
 * If the list is shorter than row count, all rows will not get labels.
 */
QStringList QBarDataProxy::rowLabels() const
{
    return dptrc()->m_rowLabels;
}

void QBarDataProxy::setRowLabels(const QStringList &labels)
{
    if (dptr()->m_rowLabels != labels) {
        dptr()->m_rowLabels = labels;
        emit rowLabelsChanged();
    }
}

/*!
 * \property QBarDataProxy::columnLabels
 *
 * Optional column \a labels for the array. Indexes in this array match column indexes in rows.
 * If the list is shorter than the longest row, all columns will not get labels.
 */
QStringList QBarDataProxy::columnLabels() const
{
    return dptrc()->m_columnLabels;
}

void QBarDataProxy::setColumnLabels(const QStringList &labels)
{
    if (dptr()->m_columnLabels != labels) {
        dptr()->m_columnLabels = labels;
        emit columnLabelsChanged();
    }
}

/*!
 * \return pointer to the data array.
 */
const QBarDataArray *QBarDataProxy::array() const
{
    return dptrc()->m_dataArray;
}

/*!
 * \return pointer to the row at \a rowIndex. It is guaranteed to be valid only until the next call
 * that modifies data.
 */
const QBarDataRow *QBarDataProxy::rowAt(int rowIndex) const
{
    const QBarDataArray &dataArray = *dptrc()->m_dataArray;
    Q_ASSERT(rowIndex >= 0 && rowIndex < dataArray.size());
    return dataArray[rowIndex];
}

/*!
 * \return pointer to the item at \a rowIndex, \a columnIndex. It is guaranteed to be valid only
 * until the next call that modifies data.
 */
const QBarDataItem *QBarDataProxy::itemAt(int rowIndex, int columnIndex) const
{
    const QBarDataArray &dataArray = *dptrc()->m_dataArray;
    Q_ASSERT(rowIndex >= 0 && rowIndex < dataArray.size());
    const QBarDataRow &dataRow = *dataArray[rowIndex];
    Q_ASSERT(columnIndex >= 0 && columnIndex < dataRow.size());
    return &dataRow.at(columnIndex);
}

/*!
 * \return pointer to the item at \a position. The X-value of \a position indicates the row
 * and the Y-value indicates the column. The item is guaranteed to be valid only
 * until the next call that modifies data.
 */
const QBarDataItem *QBarDataProxy::itemAt(const QPoint &position) const
{
    return itemAt(position.x(), position.y());
}

/*!
 * \internal
 */
QBarDataProxyPrivate *QBarDataProxy::dptr()
{
    return static_cast<QBarDataProxyPrivate *>(d_ptr.data());
}

/*!
 * \internal
 */
const QBarDataProxyPrivate *QBarDataProxy::dptrc() const
{
    return static_cast<const QBarDataProxyPrivate *>(d_ptr.data());
}

/*!
 * \fn void QBarDataProxy::arrayReset()
 *
 * Emitted when data array is reset.
 * If you change the whole array contents without calling resetArray(), you need to
 * emit this signal yourself or the graph won't get updated.
 */

/*!
 * \fn void QBarDataProxy::rowsAdded(int startIndex, int count)
 *
 * Emitted when rows have been added. Provides \a startIndex and \a count of rows added.
 * If you add rows directly to the array without calling addRow() or addRows(), you
 * need to emit this signal yourself or the graph won't get updated.
 */

/*!
 * \fn void QBarDataProxy::rowsChanged(int startIndex, int count)
 *
 * Emitted when rows have changed. Provides \a startIndex and \a count of changed rows.
 * If you change rows directly in the array without calling setRow() or setRows(), you
 * need to emit this signal yourself or the graph won't get updated.
 */

/*!
 * \fn void QBarDataProxy::rowsRemoved(int startIndex, int count)
 *
 * Emitted when rows have been removed. Provides \a startIndex and \a count of rows removed.
 * Index is the current array size if rows were removed from the end of the array.
 * If you remove rows directly from the array without calling removeRows(), you
 * need to emit this signal yourself or the graph won't get updated.
 */

/*!
 * \fn void QBarDataProxy::rowsInserted(int startIndex, int count)
 *
 * Emitted when rows have been inserted. Provides \a startIndex and \a count of inserted rows.
 * If you insert rows directly into the array without calling insertRow() or insertRows(), you
 * need to emit this signal yourself or the graph won't get updated.
 */

/*!
 * \fn void QBarDataProxy::itemChanged(int rowIndex, int columnIndex)
 *
 * Emitted when an item has changed. Provides \a rowIndex and \a columnIndex of changed item.
 * If you change an item directly in the array without calling setItem(), you
 * need to emit this signal yourself or the graph won't get updated.
 */

// QBarDataProxyPrivate

QBarDataProxyPrivate::QBarDataProxyPrivate(QBarDataProxy *q)
    : QAbstractDataProxyPrivate(q, QAbstractDataProxy::DataTypeBar),
      m_dataArray(new QBarDataArray)
{
}

QBarDataProxyPrivate::~QBarDataProxyPrivate()
{
    clearArray();
}

void QBarDataProxyPrivate::resetArray(QBarDataArray *newArray, const QStringList *rowLabels,
                                      const QStringList *columnLabels)
{
    if (rowLabels)
        qptr()->setRowLabels(*rowLabels);
    if (columnLabels)
        qptr()->setColumnLabels(*columnLabels);

    if (!newArray)
        newArray = new QBarDataArray;

    if (newArray != m_dataArray) {
        clearArray();
        m_dataArray = newArray;
    }
}

void QBarDataProxyPrivate::setRow(int rowIndex, QBarDataRow *row, const QString *label)
{
    Q_ASSERT(rowIndex >= 0 && rowIndex < m_dataArray->size());

    if (label)
        fixRowLabels(rowIndex, 1, QStringList(*label), false);
    if (row != m_dataArray->at(rowIndex)) {
        clearRow(rowIndex);
        (*m_dataArray)[rowIndex] = row;
    }
}

void QBarDataProxyPrivate::setRows(int rowIndex, const QBarDataArray &rows,
                                   const QStringList *labels)
{
    QBarDataArray &dataArray = *m_dataArray;
    Q_ASSERT(rowIndex >= 0 && (rowIndex + rows.size()) <= dataArray.size());
    if (labels)
        fixRowLabels(rowIndex, rows.size(), *labels, false);
    for (int i = 0; i < rows.size(); i++) {
        if (rows.at(i) != dataArray.at(rowIndex)) {
            clearRow(rowIndex);
            dataArray[rowIndex] = rows.at(i);
        }
        rowIndex++;
    }
}

void QBarDataProxyPrivate::setItem(int rowIndex, int columnIndex, const QBarDataItem &item)
{
    Q_ASSERT(rowIndex >= 0 && rowIndex < m_dataArray->size());
    QBarDataRow &row = *(*m_dataArray)[rowIndex];
    Q_ASSERT(columnIndex < row.size());
    row[columnIndex] = item;
}

int QBarDataProxyPrivate::addRow(QBarDataRow *row, const QString *label)
{
    int currentSize = m_dataArray->size();
    if (label)
        fixRowLabels(currentSize, 1, QStringList(*label), false);
    m_dataArray->append(row);
    return currentSize;
}

int QBarDataProxyPrivate::addRows(const QBarDataArray &rows, const QStringList *labels)
{
    int currentSize = m_dataArray->size();
    if (labels)
        fixRowLabels(currentSize, rows.size(), *labels, false);
    for (int i = 0; i < rows.size(); i++)
        m_dataArray->append(rows.at(i));
    return currentSize;
}

void QBarDataProxyPrivate::insertRow(int rowIndex, QBarDataRow *row, const QString *label)
{
    Q_ASSERT(rowIndex >= 0 && rowIndex <= m_dataArray->size());
    if (label)
        fixRowLabels(rowIndex, 1, QStringList(*label), true);
    m_dataArray->insert(rowIndex, row);
}

void QBarDataProxyPrivate::insertRows(int rowIndex, const QBarDataArray &rows,
                                      const QStringList *labels)
{
    Q_ASSERT(rowIndex >= 0 && rowIndex <= m_dataArray->size());
    if (labels)
        fixRowLabels(rowIndex, rows.size(), *labels, true);
    for (int i = 0; i < rows.size(); i++)
        m_dataArray->insert(rowIndex++, rows.at(i));
}

void QBarDataProxyPrivate::removeRows(int rowIndex, int removeCount, bool removeLabels)
{
    Q_ASSERT(rowIndex >= 0);
    int maxRemoveCount = m_dataArray->size() - rowIndex;
    removeCount = qMin(removeCount, maxRemoveCount);
    bool labelsChanged = false;
    for (int i = 0; i < removeCount; i++) {
        clearRow(rowIndex);
        m_dataArray->removeAt(rowIndex);
        if (removeLabels && m_rowLabels.size() > rowIndex) {
            m_rowLabels.removeAt(rowIndex);
            labelsChanged = true;
        }
    }
    if (labelsChanged)
        emit qptr()->rowLabelsChanged();
}

QBarDataProxy *QBarDataProxyPrivate::qptr()
{
    return static_cast<QBarDataProxy *>(q_ptr);
}

void QBarDataProxyPrivate::clearRow(int rowIndex)
{
    if (m_dataArray->at(rowIndex)) {
        delete m_dataArray->at(rowIndex);
        (*m_dataArray)[rowIndex] = 0;
    }
}

void QBarDataProxyPrivate::clearArray()
{
    for (int i = 0; i < m_dataArray->size(); i++)
        clearRow(i);
    m_dataArray->clear();
    delete m_dataArray;
}

/*!
 * \internal
 * Fixes the row label array to include specified labels.
 */
void QBarDataProxyPrivate::fixRowLabels(int startIndex, int count, const QStringList &newLabels,
                                        bool isInsert)
{
    bool changed = false;
    int currentSize = m_rowLabels.size();

    int newSize = newLabels.size();
    if (startIndex >= currentSize) {
        // Adding labels past old label array, create empty strings to fill intervening space
        if (newSize) {
            for (int i = currentSize; i < startIndex; i++)
                m_rowLabels << QString();
            // Doesn't matter if insert, append, or just change when there were no existing
            // strings, just append new strings.
            m_rowLabels << newLabels;
            changed = true;
        }
    } else {
        if (isInsert) {
            int insertIndex = startIndex;
            if (count)
                changed = true;
            for (int i = 0; i < count; i++) {
                if (i < newSize)
                    m_rowLabels.insert(insertIndex++, newLabels.at(i));
                else
                    m_rowLabels.insert(insertIndex++, QString());
            }
        } else {
            // Either append or change, replace labels up to array end and then add new ones
            int lastChangeIndex = count + startIndex;
            int newIndex = 0;
            for (int i = startIndex; i < lastChangeIndex; i++) {
                if (i >= currentSize) {
                    // Label past the current size, so just append the new label
                    if (newSize < newIndex) {
                        changed = true;
                        m_rowLabels << newLabels.at(newIndex);
                    } else {
                        break; // No point appending empty strings, so just exit
                    }
                } else if (newSize > newIndex) {
                    // Replace existing label
                    if (m_rowLabels.at(i) != newLabels.at(newIndex)) {
                        changed = true;
                        m_rowLabels[i] = newLabels.at(newIndex);
                    }
                } else {
                    // No more new labels, so clear existing label
                    if (!m_rowLabels.at(i).isEmpty()) {
                        changed = true;
                        m_rowLabels[i] = QString();
                    }
                }
                newIndex++;
            }
        }
    }
    if (changed)
        emit qptr()->rowLabelsChanged();
}

QPair<GLfloat, GLfloat> QBarDataProxyPrivate::limitValues(int startRow, int endRow,
                                                          int startColumn, int endColumn) const
{
    QPair<GLfloat, GLfloat> limits = qMakePair(0.0f, 0.0f);
    endRow = qMin(endRow, m_dataArray->size() - 1);
    for (int i = startRow; i <= endRow; i++) {
        QBarDataRow *row = m_dataArray->at(i);
        if (row) {
            endColumn = qMin(endColumn, row->size() - 1);
            for (int j = startColumn; j <= endColumn; j++) {
                const QBarDataItem &item = m_dataArray->at(i)->at(j);
                float itemValue = item.value();
                if (limits.second < itemValue)
                    limits.second = itemValue;
                if (limits.first > itemValue)
                    limits.first = itemValue;
            }
        }
    }
    return limits;
}

void QBarDataProxyPrivate::setSeries(QAbstract3DSeries *series)
{
    QAbstractDataProxyPrivate::setSeries(series);
    QBar3DSeries *barSeries = static_cast<QBar3DSeries *>(series);
    emit qptr()->seriesChanged(barSeries);
}

QT_END_NAMESPACE_DATAVISUALIZATION
