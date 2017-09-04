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

#include "qremoteobjectabstractitemmodeladapter_p.h"

#include <QItemSelectionModel>

// consider evaluating performance difference with item data
inline QVariantList collectData(const QModelIndex &index, const QAbstractItemModel *model, const QVector<int> &roles)
{
    QVariantList result;
    Q_FOREACH (int role, roles)
        result << model->data(index, role);
    return result;
}

inline QVector<int> filterRoles(const QVector<int> &roles, const QVector<int> &availableRoles)
{
    if (roles.isEmpty())
        return availableRoles;

    QVector<int> neededRoles;
    foreach (int inRole, roles) {
        foreach (int availableRole, availableRoles)
            if (inRole == availableRole) {
                neededRoles << inRole;
                continue;
            }
    }
    return neededRoles;
}

QAbstractItemModelSourceAdapter::QAbstractItemModelSourceAdapter(QAbstractItemModel *obj, QItemSelectionModel *sel, const QVector<int> &roles)
    : QObject(obj),
      m_model(obj),
      m_availableRoles(roles)
{
    registerTypes();
    m_selectionModel = sel;
    connect(m_model, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)), this, SLOT(sourceDataChanged(QModelIndex,QModelIndex,QVector<int>)));
    connect(m_model, SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(sourceRowsInserted(QModelIndex,int,int)));
    connect(m_model, SIGNAL(columnsInserted(QModelIndex,int,int)), this, SLOT(sourceColumnsInserted(QModelIndex,int,int)));
    connect(m_model, SIGNAL(rowsRemoved(QModelIndex,int,int)), this, SLOT(sourceRowsRemoved(QModelIndex,int,int)));
    connect(m_model, SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)), this, SLOT(sourceRowsMoved(QModelIndex,int,int,QModelIndex,int)));
    if (m_selectionModel)
        connect(m_selectionModel, SIGNAL(currentChanged(QModelIndex,QModelIndex)), this, SLOT(sourceCurrentChanged(QModelIndex,QModelIndex)));
}

void QAbstractItemModelSourceAdapter::registerTypes()
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
        qRegisterMetaType<QItemSelectionModel::SelectionFlags>();
        qRegisterMetaTypeStreamOperators<QItemSelectionModel::SelectionFlags>();
        qRegisterMetaType<QSize>();
        qRegisterMetaType<QIntHash>();
        qRegisterMetaTypeStreamOperators<QIntHash>();
    }
}

QItemSelectionModel* QAbstractItemModelSourceAdapter::selectionModel() const
{
    return m_selectionModel;
}

QSize QAbstractItemModelSourceAdapter::replicaSizeRequest(IndexList parentList)
{
    QModelIndex parent = toQModelIndex(parentList, m_model);
    const int rowCount = m_model->rowCount(parent);
    const int columnCount = m_model->columnCount(parent);
    const QSize size(columnCount, rowCount);
    qCDebug(QT_REMOTEOBJECT_MODELS) << "parent" << parentList << "size=" << size;
    return size;
}

void QAbstractItemModelSourceAdapter::replicaSetData(const IndexList &index, const QVariant &value, int role)
{
    const QModelIndex modelIndex = toQModelIndex(index, m_model);
    Q_ASSERT(modelIndex.isValid());
    const bool result = m_model->setData(modelIndex, value, role);
    Q_ASSERT(result);
    Q_UNUSED(result);
}

DataEntries QAbstractItemModelSourceAdapter::replicaRowRequest(IndexList start, IndexList end, QVector<int> roles)
{
    qCDebug(QT_REMOTEOBJECT_MODELS) << "Requested rows" << "start=" << start << "end=" << end << "roles=" << roles;

    Q_ASSERT(start.size() == end.size());
    Q_ASSERT(!start.isEmpty());

    if (roles.isEmpty())
        roles << Qt::DisplayRole << Qt::BackgroundRole; //FIX

    IndexList parentList = start;
    Q_ASSERT(!parentList.isEmpty());
    parentList.pop_back();
    QModelIndex parent = toQModelIndex(parentList, m_model);

    const int startRow = start.last().row;
    const int startColumn = start.last().column;
    const int rowCount = m_model->rowCount(parent);
    const int columnCount = m_model->columnCount(parent);

    DataEntries entries;
    if (rowCount <= 0)
        return entries;
    const int endRow = std::min(end.last().row, rowCount - 1);
    const int endColumn = std::min(end.last().column, columnCount - 1);
    Q_ASSERT_X(endRow >= 0 && endRow < rowCount, __FUNCTION__, qPrintable(QString(QLatin1String("0 <= %1 < %2")).arg(endRow).arg(rowCount)));
    Q_ASSERT_X(endColumn >= 0 && endColumn < columnCount, __FUNCTION__, qPrintable(QString(QLatin1String("0 <= %1 < %2")).arg(endColumn).arg(columnCount)));

    for (int row = startRow; row <= endRow; ++row) {
        for (int column = startColumn; column <= endColumn; ++column) {
            const QModelIndex current = m_model->index(row, column, parent);
            Q_ASSERT(current.isValid());
            const IndexList currentList = toModelIndexList(current, m_model);
            const QVariantList data = collectData(current, m_model, roles);
            const bool hasChildren = m_model->hasChildren(current);
            const Qt::ItemFlags flags = m_model->flags(current);
            qCDebug(QT_REMOTEOBJECT_MODELS) << Q_FUNC_INFO << "current=" << currentList << "data=" << data;
            entries.data << IndexValuePair(currentList, data, hasChildren, flags);
        }
    }
    return entries;
}

QVariantList QAbstractItemModelSourceAdapter::replicaHeaderRequest(QVector<Qt::Orientation> orientations, QVector<int> sections, QVector<int> roles)
{
    qCDebug(QT_REMOTEOBJECT_MODELS) << Q_FUNC_INFO << "orientations=" << orientations << "sections=" << sections << "roles=" << roles;
    QVariantList data;
    Q_ASSERT(roles.size() == sections.size());
    Q_ASSERT(roles.size() == orientations.size());
    for (int i = 0; i < roles.size(); ++i) {
        data << m_model->headerData(sections[i], orientations[i], roles[i]);
    }
    return data;
}

void QAbstractItemModelSourceAdapter::replicaSetCurrentIndex(IndexList index, QItemSelectionModel::SelectionFlags command)
{
    if (m_selectionModel)
        m_selectionModel->setCurrentIndex(toQModelIndex(index, m_model), command);
}

void QAbstractItemModelSourceAdapter::sourceDataChanged(const QModelIndex & topLeft, const QModelIndex & bottomRight, const QVector<int> & roles) const
{
    QVector<int> neededRoles = filterRoles(roles, availableRoles());
    if (neededRoles.isEmpty()) {
        qCDebug(QT_REMOTEOBJECT_MODELS) << Q_FUNC_INFO << "Needed roles is empty!";
        return;
    }
    Q_ASSERT(topLeft.isValid());
    Q_ASSERT(bottomRight.isValid());
    IndexList start = toModelIndexList(topLeft, m_model);
    IndexList end = toModelIndexList(bottomRight, m_model);
    qCDebug(QT_REMOTEOBJECT_MODELS) << Q_FUNC_INFO << "start=" << start << "end=" << end << "neededRoles=" << neededRoles;
    emit dataChanged(start, end, neededRoles);
}

void QAbstractItemModelSourceAdapter::sourceRowsInserted(const QModelIndex & parent, int start, int end)
{
    IndexList parentList = toModelIndexList(parent, m_model);
    emit rowsInserted(parentList, start, end);
}

void QAbstractItemModelSourceAdapter::sourceColumnsInserted(const QModelIndex & parent, int start, int end)
{
    IndexList parentList = toModelIndexList(parent, m_model);
    emit columnsInserted(parentList, start, end);
}

void QAbstractItemModelSourceAdapter::sourceRowsRemoved(const QModelIndex & parent, int start, int end)
{
    IndexList parentList = toModelIndexList(parent, m_model);
    emit rowsRemoved(parentList, start, end);
}

void QAbstractItemModelSourceAdapter::sourceRowsMoved(const QModelIndex & sourceParent, int sourceRow, int count, const QModelIndex & destinationParent, int destinationChild) const
{
    emit rowsMoved(toModelIndexList(sourceParent, m_model), sourceRow, count, toModelIndexList(destinationParent, m_model), destinationChild);
}

void QAbstractItemModelSourceAdapter::sourceCurrentChanged(const QModelIndex & current, const QModelIndex & previous)
{
    IndexList currentIndex = toModelIndexList(current, m_model);
    IndexList previousIndex = toModelIndexList(previous, m_model);
    qCDebug(QT_REMOTEOBJECT_MODELS) << Q_FUNC_INFO << "current=" << currentIndex << "previous=" << previousIndex;
    emit currentChanged(currentIndex, previousIndex);
}
