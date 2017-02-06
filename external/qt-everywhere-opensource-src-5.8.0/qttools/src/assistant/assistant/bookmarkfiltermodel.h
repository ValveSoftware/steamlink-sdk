/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Assistant of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
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
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#ifndef BOOKMARKFILTERMODEL_H
#define BOOKMARKFILTERMODEL_H

#include <QtCore/QPersistentModelIndex>

#include <QtCore/QAbstractProxyModel>
#include <QtCore/QSortFilterProxyModel>

QT_BEGIN_NAMESPACE

class BookmarkItem;
class BookmarkModel;

typedef QList<QPersistentModelIndex> PersistentModelIndexCache;

class BookmarkFilterModel : public QAbstractProxyModel
{
    Q_OBJECT
public:
    explicit BookmarkFilterModel(QObject *parent = 0);

    void setSourceModel(QAbstractItemModel *sourceModel);

    int rowCount(const QModelIndex &index) const;
    int columnCount(const QModelIndex &index) const;

    QModelIndex mapToSource(const QModelIndex &proxyIndex) const;
    QModelIndex mapFromSource(const QModelIndex &sourceIndex) const;

    QModelIndex parent(const QModelIndex &child) const;
    QModelIndex index(int row, int column, const QModelIndex &parent) const;

    Qt::DropActions supportedDropActions () const;
    Qt::ItemFlags flags(const QModelIndex &index) const;

    QVariant data(const QModelIndex &index, int role) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role);

    void filterBookmarks();
    void filterBookmarkFolders();

private slots:
    void changed(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    void rowsInserted(const QModelIndex &parent, int start, int end);
    void rowsAboutToBeRemoved(const QModelIndex &parent, int start, int end);
    void rowsRemoved(const QModelIndex &parent, int start, int end);
    void layoutAboutToBeChanged();
    void layoutChanged();
    void modelAboutToBeReset();
    void modelReset();

private:
    void setupCache(const QModelIndex &parent);
    void collectItems(const QModelIndex &parent);

private:
    bool hideBookmarks;
    BookmarkModel *sourceModel;
    PersistentModelIndexCache cache;
    QPersistentModelIndex indexToRemove;
};

// -- BookmarkTreeModel

class BookmarkTreeModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    BookmarkTreeModel(QObject *parent = 0);
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const;
};

QT_END_NAMESPACE

#endif // BOOKMARKFILTERMODEL_H
