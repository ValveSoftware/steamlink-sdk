/****************************************************************************
**
** Copyright (C) 2017 Ford Motor Company
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtRemoteObjects module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qremoteobjectabstractitemmodelreplica.h"
#include "qremoteobjectabstractitemmodelreplica_p.h"

#include "qremoteobjectnode.h"

#include <QDebug>
#include <QRect>
#include <QPoint>

QT_BEGIN_NAMESPACE
enum {
    DefaultRootCacheSize = 1000
};

inline QDebug operator<<(QDebug stream, const RequestedData &data)
{
    return stream.nospace() << "RequestedData[start=" << data.start << ", end=" << data.end << ", roles=" << data.roles << "]";
}

CacheData::CacheData(QAbstractItemModelReplicaPrivate *model, CacheData *parentItem)
    : replicaModel(model)
    , parent(parentItem)
    , hasChildren(false)
    , columnCount(0)
    , rowCount(0)
{
    if (parent)
        replicaModel->m_activeParents.insert(parent);
}

CacheData::~CacheData() {
    if (parent && !replicaModel->m_activeParents.empty())
        replicaModel->m_activeParents.erase(this);
}

QAbstractItemModelReplicaPrivate::QAbstractItemModelReplicaPrivate()
    : QRemoteObjectReplica()
    , m_selectionModel(0)
    , m_rootItem(this)
    , m_lastRequested(-1)
{
    m_rootItem.children.setCacheSize(DefaultRootCacheSize);
    QAbstractItemModelReplicaPrivate::registerMetatypes();
    initializeModelConnections();
    connect(this, &QAbstractItemModelReplicaPrivate::availableRolesChanged, this, [this]{
        m_availableRoles.clear();
    });
}

QAbstractItemModelReplicaPrivate::QAbstractItemModelReplicaPrivate(QRemoteObjectNode *node, const QString &name)
    : QRemoteObjectReplica(ConstructWithNode)
    , m_selectionModel(0)
    , m_rootItem(this)
    , m_lastRequested(-1)
{
    m_rootItem.children.setCacheSize(DefaultRootCacheSize);
    QAbstractItemModelReplicaPrivate::registerMetatypes();
    initializeModelConnections();
    initializeNode(node, name);
    connect(this, &QAbstractItemModelReplicaPrivate::availableRolesChanged, this, [this]{
        m_availableRoles.clear();
    });
}

QAbstractItemModelReplicaPrivate::~QAbstractItemModelReplicaPrivate()
{
    m_rootItem.clear();
    qDeleteAll(m_pendingRequests);
}

void QAbstractItemModelReplicaPrivate::initialize()
{
    QVariantList properties;
    properties << QVariant::fromValue(QVector<int>());
    properties << QVariant::fromValue(QIntHash());
    setProperties(properties);
}

void QAbstractItemModelReplicaPrivate::registerMetatypes()
{
    static bool alreadyRegistered = false;
    if (!alreadyRegistered) {
        alreadyRegistered = true;
        qRegisterMetaType<Qt::Orientation>();
        qRegisterMetaType<QVector<Qt::Orientation> >();
        qRegisterMetaTypeStreamOperators<ModelIndex>();
        qRegisterMetaTypeStreamOperators<IndexList>();
        qRegisterMetaTypeStreamOperators<DataEntries>();
        qRegisterMetaTypeStreamOperators<Qt::Orientation>();
        qRegisterMetaTypeStreamOperators<QVector<Qt::Orientation> >();
        qRegisterMetaTypeStreamOperators<QItemSelectionModel::SelectionFlags>();
        qRegisterMetaType<QItemSelectionModel::SelectionFlags>();
        qRegisterMetaType<QIntHash>();
        qRegisterMetaTypeStreamOperators<QIntHash>();
    }
}

void QAbstractItemModelReplicaPrivate::initializeModelConnections()
{
    connect(this, &QAbstractItemModelReplicaPrivate::dataChanged, this, &QAbstractItemModelReplicaPrivate::onDataChanged);
    connect(this, &QAbstractItemModelReplicaPrivate::rowsInserted, this, &QAbstractItemModelReplicaPrivate::onRowsInserted);
    connect(this, &QAbstractItemModelReplicaPrivate::columnsInserted, this, &QAbstractItemModelReplicaPrivate::onColumnsInserted);
    connect(this, &QAbstractItemModelReplicaPrivate::rowsRemoved, this, &QAbstractItemModelReplicaPrivate::onRowsRemoved);
    connect(this, &QAbstractItemModelReplicaPrivate::rowsMoved, this, &QAbstractItemModelReplicaPrivate::onRowsMoved);
    connect(this, &QAbstractItemModelReplicaPrivate::currentChanged, this, &QAbstractItemModelReplicaPrivate::onCurrentChanged);
    connect(this, &QAbstractItemModelReplicaPrivate::modelReset, this, &QAbstractItemModelReplicaPrivate::onModelReset);
    connect(this, &QAbstractItemModelReplicaPrivate::headerDataChanged, this, &QAbstractItemModelReplicaPrivate::onHeaderDataChanged);
}

inline void removeIndexFromRow(const QModelIndex &index, const QVector<int> &roles, CachedRowEntry *entry)
{
    CachedRowEntry &entryRef = *entry;
    if (index.column() < entryRef.size()) {
        CacheEntry &entry = entryRef[index.column()];
        if (roles.isEmpty()) {
            entry.data.clear();
        } else {
            Q_FOREACH (int role, roles) {
                entry.data.remove(role);
            }
        }
    }
}

void QAbstractItemModelReplicaPrivate::onReplicaCurrentChanged(const QModelIndex &current, const QModelIndex &previous)
{
    Q_UNUSED(previous)
    IndexList currentIndex = toModelIndexList(current, q);
    qCDebug(QT_REMOTEOBJECT_MODELS) << Q_FUNC_INFO << "current=" << currentIndex;
    replicaSetCurrentIndex(currentIndex, QItemSelectionModel::Clear|QItemSelectionModel::Select|QItemSelectionModel::Current);
}

void QAbstractItemModelReplicaPrivate::setModel(QAbstractItemModelReplica *model)
{
    q = model;
    setParent(model);
    m_selectionModel.reset(new QItemSelectionModel(model));
    connect(m_selectionModel.data(), &QItemSelectionModel::currentChanged, this, &QAbstractItemModelReplicaPrivate::onReplicaCurrentChanged);
}

bool QAbstractItemModelReplicaPrivate::clearCache(const IndexList &start, const IndexList &end, const QVector<int> &roles = QVector<int>())
{
    Q_ASSERT(start.size() == end.size());

    bool ok = true;
    const QModelIndex startIndex = toQModelIndex(start, q, &ok);
    if (!ok)
        return false;
    const QModelIndex endIndex = toQModelIndex(end, q, &ok);
    if (!ok)
        return false;
    Q_ASSERT(startIndex.isValid());
    Q_ASSERT(endIndex.isValid());
    Q_ASSERT(startIndex.parent() == endIndex.parent());
    Q_UNUSED(endIndex);
    QModelIndex parentIndex = startIndex.parent();
    auto parentItem = cacheData(parentIndex);

    const int startRow = start.last().row;
    const int lastRow = end.last().row;
    const int startColumn = start.last().column;
    const int lastColumn = end.last().column;
    for (int row = startRow; row <= lastRow; ++row) {
        Q_ASSERT_X(row >= 0 && row < parentItem->rowCount, __FUNCTION__, qPrintable(QString(QLatin1String("0 <= %1 < %2")).arg(row).arg(parentItem->rowCount)));
        auto item = parentItem->children.get(row);
        if (item) {
            CachedRowEntry *entry = &(item->cachedRowEntry);
            for (int column = startColumn; column <= lastColumn; ++column)
                removeIndexFromRow(q->index(row, column, parentIndex), roles, entry);
        }
    }
    return true;
}

void QAbstractItemModelReplicaPrivate::onDataChanged(const IndexList &start, const IndexList &end, const QVector<int> &roles)
{
    qCDebug(QT_REMOTEOBJECT_MODELS) << Q_FUNC_INFO << "start=" << start << "end=" << end << "roles=" << roles;

    // we need to clear the cache to make sure the new remote data is fetched if the new data call is happening
    if (clearCache(start, end, roles)) {
        bool ok = true;
        const QModelIndex startIndex = toQModelIndex(start, q, &ok);
        if (!ok)
            return;
        const QModelIndex endIndex = toQModelIndex(end, q, &ok);
        if (!ok)
            return;
        Q_ASSERT(startIndex.parent() == endIndex.parent());
        auto parentItem = cacheData(startIndex.parent());
        int startRow = start.last().row;
        int endRow = end.last().row;
        bool dataChanged = false;
        while (startRow <= endRow) {
            for (;startRow <= endRow; startRow++) {
                if (parentItem->children.exists(startRow))
                    break;
            }

            if (startRow  > endRow)
                break;

            RequestedData data;
            data.roles = roles;
            data.start = start;
            data.start.last().row = startRow;

            while (startRow <= endRow && parentItem->children.exists(startRow))
                ++startRow;

            data.end = end;
            data.end.last().row = startRow -1;

            m_requestedData.append(data);
            dataChanged = true;
        }

        if (dataChanged)
            QMetaObject::invokeMethod(this, "fetchPendingData", Qt::QueuedConnection);
    }
}

void QAbstractItemModelReplicaPrivate::onRowsInserted(const IndexList &parent, int start, int end)
{
    qCDebug(QT_REMOTEOBJECT_MODELS) << Q_FUNC_INFO << "start=" << start << "end=" << end << "parent=" << parent;

    bool treeFullyLazyLoaded = true;
    const QModelIndex parentIndex = toQModelIndex(parent, q, &treeFullyLazyLoaded, true);
    if (!treeFullyLazyLoaded)
        return;

    auto parentItem = cacheData(parentIndex);
    q->beginInsertRows(parentIndex, start, end);
    parentItem->insertChildren(start, end);
    q->endInsertRows();
    if (!parentItem->hasChildren && parentItem->columnCount > 0) {
        parentItem->hasChildren = true;
        emit q->dataChanged(parentIndex, parentIndex);
    }
}

void QAbstractItemModelReplicaPrivate::onColumnsInserted(const IndexList &parent, int start, int end)
{
    qCDebug(QT_REMOTEOBJECT_MODELS) << Q_FUNC_INFO << "start=" << start << "end=" << end << "parent=" << parent;

    bool treeFullyLazyLoaded = true;
    const QModelIndex parentIndex = toQModelIndex(parent, q, &treeFullyLazyLoaded);
    if (!treeFullyLazyLoaded)
        return;

    //Since we need to support QAIM and models that don't emit columnCountChanged
    //check if we have a constant columnCount everywhere if thats the case don't insert
    //more columns
    auto parentItem = cacheData(parentIndex);
    auto parentOfParent = parentItem->parent;
    if (parentOfParent && parentItem != &m_rootItem)
        if (parentOfParent->columnCount == parentItem->columnCount)
            return;
    q->beginInsertColumns(parentIndex, start, end);
    parentItem->columnCount += end - start + 1;
    q->endInsertColumns();
    if (!parentItem->hasChildren && parentItem->children.size() > 0) {
        parentItem->hasChildren = true;
        emit q->dataChanged(parentIndex, parentIndex);
    }

}

void QAbstractItemModelReplicaPrivate::onRowsRemoved(const IndexList &parent, int start, int end)
{
    qCDebug(QT_REMOTEOBJECT_MODELS) << Q_FUNC_INFO << "start=" << start << "end=" << end << "parent=" << parent;

    bool treeFullyLazyLoaded = true;
    const QModelIndex parentIndex = toQModelIndex(parent, q, &treeFullyLazyLoaded);
    if (!treeFullyLazyLoaded)
        return;

    auto parentItem = cacheData(parentIndex);
    q->beginRemoveRows(parentIndex, start, end);
    if (parentItem)
        parentItem->removeChildren(start, end);
    q->endRemoveRows();
}

void QAbstractItemModelReplicaPrivate::onRowsMoved(IndexList srcParent, int srcRow, int count, IndexList destParent, int destRow)
{
    qCDebug(QT_REMOTEOBJECT_MODELS) << Q_FUNC_INFO;

    const QModelIndex sourceParent = toQModelIndex(srcParent, q);
    const QModelIndex destinationParent = toQModelIndex(destParent, q);
    Q_ASSERT(!sourceParent.isValid());
    Q_ASSERT(!destinationParent.isValid());
    q->beginMoveRows(sourceParent, srcRow, count, destinationParent, destRow);
//TODO misses parents...
    IndexList start, end;
    start << ModelIndex(srcRow, 0);
    end << ModelIndex(srcRow + count, q->columnCount(sourceParent)-1);
    clearCache(start, end);
    IndexList start2, end2;
    start2 << ModelIndex(destRow, 0);
    end2 << ModelIndex(destRow + count, q->columnCount(destinationParent)-1);
    clearCache(start2, end2);
    q->endMoveRows();
}

void QAbstractItemModelReplicaPrivate::onCurrentChanged(IndexList current, IndexList previous)
{
    qCDebug(QT_REMOTEOBJECT_MODELS) << Q_FUNC_INFO << "current=" << current << "previous=" << previous;
    Q_UNUSED(previous);
    Q_ASSERT(m_selectionModel);
    bool ok;
    // If we have several tree models sharing a selection model, we
    // can't guarantee that all Replicas have the selected cell
    // available.
    const QModelIndex currentIndex = toQModelIndex(current, q, &ok);
    // Ignore selection if we can't find the desired cell.
    if (ok)
        m_selectionModel->setCurrentIndex(currentIndex, QItemSelectionModel::Clear|QItemSelectionModel::Select|QItemSelectionModel::Current);
}

void QAbstractItemModelReplicaPrivate::handleInitDone(QRemoteObjectPendingCallWatcher *watcher)
{
    qCDebug(QT_REMOTEOBJECT_MODELS) << Q_FUNC_INFO;

    handleModelResetDone(watcher);

    emit q->initialized();
}

void QAbstractItemModelReplicaPrivate::handleModelResetDone(QRemoteObjectPendingCallWatcher *watcher)
{
    const QSize size = watcher->returnValue().toSize();
    qCDebug(QT_REMOTEOBJECT_MODELS) << Q_FUNC_INFO << "size=" << size;

    q->beginResetModel();
    m_rootItem.clear();
    if (size.height() > 0) {
        m_rootItem.rowCount = size.height();
        m_rootItem.hasChildren = true;
    }

    m_rootItem.columnCount = size.width();
    m_headerData[0].resize(size.width());
    m_headerData[1].resize(size.height());
    q->endResetModel();
    m_pendingRequests.removeAll(watcher);
    delete watcher;
}

void QAbstractItemModelReplicaPrivate::handleSizeDone(QRemoteObjectPendingCallWatcher *watcher)
{
    SizeWatcher *sizeWatcher = static_cast<SizeWatcher*>(watcher);
    const QSize size = sizeWatcher->returnValue().toSize();
    auto parentItem = cacheData(sizeWatcher->parentList);
    const QModelIndex parent = toQModelIndex(sizeWatcher->parentList, q);

    if (size.width() != parentItem->columnCount) {
        const int columnCount = std::max(0, parentItem->columnCount);
        Q_ASSERT_X(size.width() >= parentItem->columnCount, __FUNCTION__, qPrintable(QStringLiteral("The column count should only shrink in columnsRemoved!!")));
        parentItem->columnCount = size.width();
        if (size.width() > columnCount) {
            Q_ASSERT(size.width() > 0);
            q->beginInsertColumns(parent, columnCount, size.width() - 1);
            q->endInsertColumns();
        } else {
            Q_ASSERT_X(size.width() == columnCount, __FUNCTION__, qPrintable(QString(QLatin1String("%1 != %2")).arg(size.width()).arg(columnCount)));
        }
    }

    Q_ASSERT_X(size.height() >= parentItem->rowCount, __FUNCTION__, qPrintable(QStringLiteral("The new size and the current size should match!!")));
    if (!parentItem->rowCount) {
        if (size.height() > 0) {
            q->beginInsertRows(parent, 0, size.height() - 1);
            parentItem->rowCount = size.height();
            q->endInsertRows();
        }
    } else {
        Q_ASSERT_X(parentItem->rowCount == size.height(), __FUNCTION__, qPrintable(QString(QLatin1String("%1 != %2")).arg(parentItem->rowCount).arg(size.height())));
    }
    m_pendingRequests.removeAll(watcher);
    delete watcher;
}

void QAbstractItemModelReplicaPrivate::init()
{
    qCDebug(QT_REMOTEOBJECT_MODELS) << Q_FUNC_INFO;
    SizeWatcher *watcher = doModelReset();
    m_pendingRequests.push_back(watcher);
    connect(watcher, &SizeWatcher::finished, this, &QAbstractItemModelReplicaPrivate::handleInitDone);
}

SizeWatcher* QAbstractItemModelReplicaPrivate::doModelReset()
{
    qDeleteAll(m_pendingRequests);
    m_pendingRequests.clear();
    IndexList parentList;
    QRemoteObjectPendingReply<QSize> reply = replicaSizeRequest(parentList);
    SizeWatcher *watcher = new SizeWatcher(parentList, reply);
    m_pendingRequests.push_back(watcher);
    return watcher;
}

inline void fillCacheEntry(CacheEntry *entry, const IndexValuePair &pair, const QVector<int> &roles)
{
    Q_ASSERT(entry);

    const QVariantList &data = pair.data;
    Q_ASSERT(roles.size() == data.size());

    entry->flags = pair.flags;

    qCDebug(QT_REMOTEOBJECT_MODELS) << Q_FUNC_INFO << "data.size=" << data.size();
    for (int i = 0; i < data.size(); ++i) {
        const int role = roles[i];
        const QVariant dataVal = data[i];
        qCDebug(QT_REMOTEOBJECT_MODELS) << Q_FUNC_INFO << "role=" << role << "data=" << dataVal;
        entry->data[role] = dataVal;
    }
}

inline void fillRow(CacheData *item, const IndexValuePair &pair, const QAbstractItemModel *model, const QVector<int> &roles)
{
    CachedRowEntry &rowRef = item->cachedRowEntry;
    const QModelIndex index = toQModelIndex(pair.index, model);
    Q_ASSERT(index.isValid());
    qCDebug(QT_REMOTEOBJECT_MODELS) << Q_FUNC_INFO << "row=" << index.row() << "column=" << index.column();
    if (index.column() == 0)
        item->hasChildren = pair.hasChildren;
    bool existed = false;
    for (int i = 0; i < rowRef.size(); ++i) {
        if (i == index.column()) {
            fillCacheEntry(&rowRef[i], pair, roles);
            existed = true;
        }
    }
    qCDebug(QT_REMOTEOBJECT_MODELS) << Q_FUNC_INFO << "existed=" << existed;
    if (!existed) {
        CacheEntry entries;
        fillCacheEntry(&entries, pair, roles);
        rowRef.append(entries);
    }
}

int collectEntriesForRow(DataEntries* filteredEntries, int row, const DataEntries &entries, int startIndex)
{
    Q_ASSERT(filteredEntries);
    const int size = entries.data.size();
    for (int i = startIndex; i < size; ++i)
    {
        const IndexValuePair &pair = entries.data[i];
        if (pair.index.last().row == row)
            filteredEntries->data << pair;
        else
            return i;
    }
    return size;
}

void QAbstractItemModelReplicaPrivate::requestedData(QRemoteObjectPendingCallWatcher *qobject)
{
    RowWatcher *watcher = static_cast<RowWatcher *>(qobject);
    Q_ASSERT(watcher);
    Q_ASSERT(watcher->start.size() == watcher->end.size());

    qCDebug(QT_REMOTEOBJECT_MODELS) << Q_FUNC_INFO << "start=" << watcher->start << "end=" << watcher->end;

    IndexList parentList = watcher->start;
    Q_ASSERT(!parentList.isEmpty());
    parentList.pop_back();
    auto parentItem = cacheData(parentList);
    DataEntries entries = watcher->returnValue().value<DataEntries>();

    const int rowCount = parentItem->rowCount;
    const int columnCount = parentItem->columnCount;

    if (rowCount < 1 || columnCount < 1)
        return;

    const int startRow =  std::min(watcher->start.last().row, rowCount - 1);
    const int endRow = std::min(watcher->end.last().row, rowCount - 1);
    const int startColumn = std::min(watcher->start.last().column, columnCount - 1);
    const int endColumn = std::min(watcher->end.last().column, columnCount - 1);
    Q_ASSERT_X(startRow >= 0 && startRow < parentItem->rowCount, __FUNCTION__, qPrintable(QString(QLatin1String("0 <= %1 < %2")).arg(startRow).arg(parentItem->rowCount)));
    Q_ASSERT_X(endRow >= 0 && endRow < parentItem->rowCount, __FUNCTION__, qPrintable(QString(QLatin1String("0 <= %1 < %2")).arg(endRow).arg(parentItem->rowCount)));

    for (int i = 0; i < entries.data.size(); ++i) {
        IndexValuePair pair = entries.data[i];
        if (auto item = createCacheData(pair.index))
            fillRow(item, pair, q, watcher->roles);
    }

    const QModelIndex parentIndex = toQModelIndex(parentList, q);
    const QModelIndex startIndex = q->index(startRow, startColumn, parentIndex);
    const QModelIndex endIndex = q->index(endRow, endColumn, parentIndex);
    Q_ASSERT(startIndex.isValid());
    Q_ASSERT(endIndex.isValid());
    emit q->dataChanged(startIndex, endIndex, watcher->roles);
    m_pendingRequests.removeAll(watcher);
    delete watcher;
}

void QAbstractItemModelReplicaPrivate::fetchPendingData()
{
    if (m_requestedData.isEmpty())
        return;

    qCDebug(QT_REMOTEOBJECT_MODELS) << Q_FUNC_INFO << "m_requestedData.size=" << m_requestedData.size();

    std::vector<RequestedData> finalRequests;
    RequestedData curData;
    Q_FOREACH (const RequestedData &data, m_requestedData) {
        qCDebug(QT_REMOTEOBJECT_MODELS) << Q_FUNC_INFO << "REQUESTED start=" << data.start << "end=" << data.end << "roles=" << data.roles;

        Q_ASSERT(!data.start.isEmpty());
        Q_ASSERT(!data.end.isEmpty());
        Q_ASSERT(data.start.size() == data.end.size());
        if (curData.start.isEmpty() || curData.start.last().row == -1 || curData.start.last().column == -1)
            curData = data;
        if (curData.start.size() != data.start.size()) {
            finalRequests.push_back(curData);
            curData = data;
        } else {
            if (data.start.size() > 1) {
                for (int i = 0; i < data.start.size() - 1; ++i) {
                    if (curData.start[i].row != data.start[i].row ||
                        curData.start[i].column != data.start[i].column) {
                        finalRequests.push_back(curData);
                        curData = data;
                    }
                }
            }

            const ModelIndex curIndStart = curData.start.last();
            const ModelIndex curIndEnd = curData.end.last();
            const ModelIndex dataIndStart = data.start.last();
            const ModelIndex dataIndEnd = data.end.last();
            const ModelIndex resStart(std::min(curIndStart.row, dataIndStart.row), std::min(curIndStart.column, dataIndStart.column));
            const ModelIndex resEnd(std::max(curIndEnd.row, dataIndEnd.row), std::max(curIndEnd.column, dataIndEnd.column));
            QVector<int> roles = curData.roles;
            if (!curData.roles.isEmpty())
                Q_FOREACH (int role, data.roles)
                    if (!curData.roles.contains(role))
                        roles.append(role);
            QRect firstRect( QPoint(curIndStart.row, curIndStart.column), QPoint(curIndEnd.row, curIndEnd.column));
            QRect secondRect( QPoint(dataIndStart.row, dataIndStart.column), QPoint(dataIndEnd.row, dataIndEnd.column));

            const bool borders = (qAbs(curIndStart.row - dataIndStart.row) == 1) ||
                                 (qAbs(curIndStart.column - dataIndStart.column) == 1) ||
                                 (qAbs(curIndEnd.row - dataIndEnd.row) == 1) ||
                                 (qAbs(curIndEnd.column - dataIndEnd.column) == 1);

            if ((resEnd.row - resStart.row < 100) && (firstRect.intersects(secondRect) || borders)) {
                IndexList start = curData.start;
                start.pop_back();
                start.push_back(resStart);
                IndexList end = curData.end;
                end.pop_back();
                end.push_back(resEnd);
                curData.start = start;
                curData.end = end;
                curData.roles = roles;
                Q_ASSERT(!start.isEmpty());
                Q_ASSERT(!end.isEmpty());
            } else {
                finalRequests.push_back(curData);
                curData = data;
            }
        }
    }
    finalRequests.push_back(curData);
    //qCDebug(QT_REMOTEOBJECT_MODELS) << "Final requests" << finalRequests;
    int rows = 0;
                                                                        // There is no point to eat more than can chew
    for (auto it = finalRequests.rbegin(); it != finalRequests.rend() && size_t(rows) < m_rootItem.children.cacheSize; ++it) {
        qCDebug(QT_REMOTEOBJECT_MODELS) << Q_FUNC_INFO << "FINAL start=" << it->start << "end=" << it->end << "roles=" << it->roles;

        QRemoteObjectPendingReply<DataEntries> reply = replicaRowRequest(it->start, it->end, it->roles);
        RowWatcher *watcher = new RowWatcher(it->start, it->end, it->roles, reply);
        rows += 1 + it->end.first().row - it->start.first().row;
        m_pendingRequests.push_back(watcher);
        connect(watcher, &RowWatcher::finished, this, &QAbstractItemModelReplicaPrivate::requestedData);
    }
    m_requestedData.clear();
}

void QAbstractItemModelReplicaPrivate::onModelReset()
{
    qCDebug(QT_REMOTEOBJECT_MODELS) << Q_FUNC_INFO;
    SizeWatcher *watcher = doModelReset();
    connect(watcher, &SizeWatcher::finished, this, &QAbstractItemModelReplicaPrivate::handleModelResetDone);
}

void QAbstractItemModelReplicaPrivate::onHeaderDataChanged(Qt::Orientation orientation, int first, int last)
{
    // TODO clean cache
    const int index = orientation == Qt::Horizontal ? 0 : 1;
    QVector<CacheEntry> &entries = m_headerData[index];
    for (int i = first; i < last; ++i )
        entries[i].data.clear();
    emit q->headerDataChanged(orientation, first, last);
}

void QAbstractItemModelReplicaPrivate::fetchPendingHeaderData()
{
    QVector<int> roles;
    QVector<int> sections;
    QVector<Qt::Orientation> orientations;
    Q_FOREACH (const RequestedHeaderData &data, m_requestedHeaderData) {
        roles.push_back(data.role);
        sections.push_back(data.section);
        orientations.push_back(data.orientation);
    }
    QRemoteObjectPendingReply<QVariantList> reply = replicaHeaderRequest(orientations, sections, roles);
    HeaderWatcher *watcher = new HeaderWatcher(orientations, sections, roles, reply);
    connect(watcher, &HeaderWatcher::finished, this, &QAbstractItemModelReplicaPrivate::requestedHeaderData);
    m_requestedHeaderData.clear();
    m_pendingRequests.push_back(watcher);
}

static inline QVector<QPair<int, int> > listRanges(const QVector<int> &list)
{
    QVector<QPair<int, int> > result;
    if (!list.isEmpty()) {
        QPair<int, int> currentElem = qMakePair(list.first(), list.first());
        QVector<int>::const_iterator end = list.end();
        for (QVector<int>::const_iterator it = list.constBegin() + 1; it != end; ++it) {
            if (currentElem.first == *it +1)
                currentElem.first = *it;
            else if ( currentElem.second == *it -1)
                currentElem.second = *it;
            else if (currentElem.first <= *it && currentElem.second >= *it)
                continue;
            else {
                result.push_back(currentElem);
                currentElem.first = *it;
                currentElem.second = *it;
            }
        }
        result.push_back(currentElem);
    }
    return result;
}

void QAbstractItemModelReplicaPrivate::requestedHeaderData(QRemoteObjectPendingCallWatcher *qobject)
{
    HeaderWatcher *watcher = static_cast<HeaderWatcher *>(qobject);
    Q_ASSERT(watcher);

    QVariantList data = watcher->returnValue().value<QVariantList>();
    Q_ASSERT(watcher->orientations.size() == data.size());
    Q_ASSERT(watcher->sections.size() == data.size());
    Q_ASSERT(watcher->roles.size() == data.size());
    QVector<int> horizontalSections;
    QVector<int> verticalSections;

    for (int i = 0; i < data.size(); ++i) {
        if (watcher->orientations[i] == Qt::Horizontal)
            horizontalSections.append(watcher->sections[i]);
        else
            verticalSections.append(watcher->sections[i]);
        const int index = watcher->orientations[i] == Qt::Horizontal ? 0 : 1;
        const int role = watcher->roles[i];
        QHash<int, QVariant> &dat = m_headerData[index][watcher->sections[i]].data;
        dat[role] = data[i];
    }
    QVector<QPair<int, int> > horRanges = listRanges(horizontalSections);
    QVector<QPair<int, int> > verRanges = listRanges(verticalSections);

    for (int i = 0; i < horRanges.size(); ++i)
        emit q->headerDataChanged(Qt::Horizontal, horRanges[i].first, horRanges[i].second);
    for (int i = 0; i < verRanges.size(); ++i)
        emit q->headerDataChanged(Qt::Vertical, verRanges[i].first, verRanges[i].second);
    m_pendingRequests.removeAll(watcher);
    delete watcher;
}

QAbstractItemModelReplica::QAbstractItemModelReplica(QAbstractItemModelReplicaPrivate *rep)
    : QAbstractItemModel()
    , d(rep)
{
    rep->setModel(this);

    connect(rep, &QAbstractItemModelReplicaPrivate::initialized, d.data(), &QAbstractItemModelReplicaPrivate::init);
}

QAbstractItemModelReplica::~QAbstractItemModelReplica()
{
}

static QVariant findData(const CachedRowEntry &row, const QModelIndex &index, int role, bool *cached = 0)
{
    if (index.column() < row.size()) {
        const CacheEntry &entry = row[index.column()];
        QHash<int, QVariant>::ConstIterator it = entry.data.constFind(role);
        if (it != entry.data.constEnd()) {
            if (cached)
                *cached = true;
            return it.value();
        }
    }
    if (cached)
        *cached = false;
    return QVariant();
}

QItemSelectionModel* QAbstractItemModelReplica::selectionModel() const
{
    return d->m_selectionModel.data();
}

bool QAbstractItemModelReplica::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role == Qt::UserRole - 1) {
        auto parent = d->cacheData(index);
        if (!parent)
            return false;
        bool ok = true;
        auto row = value.toInt(&ok);
        if (ok)
            parent->ensureChildren(row, row);
        return ok;
    }
    if (!index.isValid())
        return false;
    if (index.row() < 0 || index.row() >= rowCount(index.parent()))
        return false;
    if (index.column() < 0 || index.column() >= columnCount(index.parent()))
        return false;

    const QVector<int > &availRoles = availableRoles();
    const QVector<int>::const_iterator res = std::find(availRoles.begin(), availRoles.end(), role);
    if (res == availRoles.end()) {
        qCWarning(QT_REMOTEOBJECT_MODELS) << "Tried to setData for index" << index << "on a not supported role" << role;
        return false;
    }
    // sendInvocationRequest to change server side data;
    d->replicaSetData(toModelIndexList(index, this), value, role);
    return true;
}

QVariant QAbstractItemModelReplica::data(const QModelIndex & index, int role) const
{

    if (!d->isInitialized()) {
        qCDebug(QT_REMOTEOBJECT_MODELS)<<"Data not initialized yet";
        return QVariant();
    }

    if (!index.isValid())
        return QVariant();

    if (!availableRoles().contains(role))
        return QVariant();

    auto item = d->cacheData(index);
    if (item) {
        bool cached = false;
        QVariant result = findData(item->cachedRowEntry, index, role, &cached);
        if (cached)
            return result;
    }

    auto parentItem = d->cacheData(index.parent());
    Q_ASSERT(parentItem);
    Q_ASSERT(index.row() < parentItem->rowCount);
    const int row = index.row();
    IndexList parentList = toModelIndexList(index.parent(), this);
    IndexList start = IndexList() << parentList << ModelIndex(row, 0);
    IndexList end = IndexList() << parentList << ModelIndex(row, std::max(0, parentItem->columnCount - 1));
    Q_ASSERT(toQModelIndex(start, this).isValid());

    RequestedData data;
    QVector<int> roles;
    roles << role;
    data.start = start;
    data.end = end;
    data.roles = roles;
    d->m_requestedData.push_back(data);
    qCDebug(QT_REMOTEOBJECT_MODELS) << "FETCH PENDING DATA" << start << end << roles;
    QMetaObject::invokeMethod(d.data(), "fetchPendingData", Qt::QueuedConnection);
    return QVariant{};
}
QModelIndex QAbstractItemModelReplica::parent(const QModelIndex &index) const
{
    if (!index.isValid() || !index.internalPointer())
        return QModelIndex();
    auto parent = static_cast<CacheData*>(index.internalPointer());
    Q_ASSERT(parent);
    if (parent == &d->m_rootItem)
        return QModelIndex();
    if (d->m_activeParents.find(parent) == d->m_activeParents.end() || d->m_activeParents.find(parent->parent) == d->m_activeParents.end())
        return QModelIndex();
    int row = parent->parent->children.find(parent);
    Q_ASSERT(row >= 0);
    return createIndex(row, 0, parent->parent);
}
QModelIndex QAbstractItemModelReplica::index(int row, int column, const QModelIndex &parent) const
{
    auto parentItem = d->cacheData(parent);
    if (!parentItem)
        return QModelIndex();

    Q_ASSERT_X((parent.isValid() && parentItem && parentItem != &d->m_rootItem) || (!parent.isValid() && parentItem == &d->m_rootItem), __FUNCTION__, qPrintable(QString(QLatin1String("isValid=%1 equals=%2")).arg(parent.isValid()).arg(parentItem == &d->m_rootItem)));

    // hmpf, following works around a Q_ASSERT-bug in QAbstractItemView::setModel which does just call
    // d->model->index(0,0) without checking the range before-hand what triggers our assert in case the
    // model is empty when view::setModel is called :-/ So, work around with the following;
    if (row < 0 || row >= parentItem->rowCount)
        return QModelIndex();

    if (column < 0 || column >= parentItem->columnCount)
        return QModelIndex();

    if (parentItem != &d->m_rootItem)
        parentItem->ensureChildren(row, row);
    return createIndex(row, column, reinterpret_cast<void*>(parentItem));
}
bool QAbstractItemModelReplica::hasChildren(const QModelIndex &parent) const
{
    auto parentItem = d->cacheData(parent);
    if (parent.isValid() && parent.column() != 0)
        return false;
    else
        return parentItem ? parentItem->hasChildren : false;
}
int QAbstractItemModelReplica::rowCount(const QModelIndex &parent) const
{
    auto parentItem = d->cacheData(parent);
    const bool canHaveChildren = parentItem && parentItem->hasChildren && !parentItem->rowCount && parent.column() == 0;
    if (canHaveChildren) {
        IndexList parentList = toModelIndexList(parent, this);
        QRemoteObjectPendingReply<QSize> reply = d->replicaSizeRequest(parentList);
        SizeWatcher *watcher = new SizeWatcher(parentList, reply);
        connect(watcher, &SizeWatcher::finished, d.data(), &QAbstractItemModelReplicaPrivate::handleSizeDone);
    } else if (parent.column() > 0) {
        return 0;
    }

    return parentItem ? parentItem->rowCount : 0;
}
int QAbstractItemModelReplica::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid() && parent.column() > 0)
        return 0;
    auto parentItem = d->cacheData(parent);
    if (!parentItem)
        return 0;
    while (parentItem->columnCount < 0 && parentItem->parent)
        parentItem = parentItem->parent;
    return std::max(0, parentItem->columnCount);
}

QVariant QAbstractItemModelReplica::headerData(int section, Qt::Orientation orientation, int role) const
{
    const int index = orientation == Qt::Horizontal ? 0 : 1;
    const QVector<CacheEntry> elem = d->m_headerData[index];
    if (section >= elem.size())
        return QVariant();

    const QHash<int, QVariant> &dat = elem.at(section).data;
    QHash<int, QVariant>::ConstIterator it = dat.constFind(role);
    if (it != dat.constEnd())
        return it.value();

    RequestedHeaderData data;
    data.role = role;
    data.section = section;
    data.orientation = orientation;
    d->m_requestedHeaderData.push_back(data);
    QMetaObject::invokeMethod(d.data(), "fetchPendingHeaderData", Qt::QueuedConnection);
    return QVariant();
}

Qt::ItemFlags QAbstractItemModelReplica::flags(const QModelIndex &index) const
{
    CacheEntry *entry = d->cacheEntry(index);
    return entry ? entry->flags : Qt::NoItemFlags;
}

bool QAbstractItemModelReplica::isInitialized() const
{
    return d->isInitialized();
}

bool QAbstractItemModelReplica::hasData(const QModelIndex &index, int role) const
{
    if (!d->isInitialized() || !index.isValid())
        return false;
    auto item = d->cacheData(index);
    if (!item)
        return false;
    bool cached = false;
    const CachedRowEntry &entry = item->cachedRowEntry;
    QVariant result = findData(entry, index, role, &cached);
    Q_UNUSED(result);
    return cached;
}

size_t QAbstractItemModelReplica::rootCacheSize() const
{
    return d->m_rootItem.children.cacheSize;
}

void QAbstractItemModelReplica::setRootCacheSize(size_t rootCacheSize)
{
    d->m_rootItem.children.setCacheSize(rootCacheSize);
}

QVector<int> QAbstractItemModelReplica::availableRoles() const
{
    return d->availableRoles();
}

QHash<int, QByteArray> QAbstractItemModelReplica::roleNames() const
{
    return d->roleNames();
}

QT_END_NAMESPACE
