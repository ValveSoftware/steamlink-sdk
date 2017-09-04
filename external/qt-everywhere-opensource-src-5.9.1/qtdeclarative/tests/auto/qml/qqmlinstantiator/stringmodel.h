/****************************************************************************
**
** Copyright (C) 2016 Dmitrii Kosarev aka Kakadu <kakadu.hafanana@gmail.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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
#ifndef STRINGMODEL_H
#define STRINGMODEL_H

#include <QtCore/QObject>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QDebug>

class StringModel : public QAbstractItemModel
{
    Q_OBJECT
    QVector<QString> items;
    QHash<int, QByteArray> roles;
    QString name;

public:
    explicit StringModel(const QString& name) : QAbstractItemModel(), name(name)
    {
        roles.insert(555, "text");
    }

    void drop(int count)
    {
        beginRemoveRows(QModelIndex(), 0, count-1);
        for (int i=0; i<count; i++)
            items.pop_front();
        endRemoveRows();
    }

    Q_INVOKABLE void add(QString s)
    {
        beginInsertRows(QModelIndex(), 0, 0);
        items.push_front(s);
        endInsertRows();
    }

    int rowCount(const QModelIndex &) const
    {
        return items.count();
    }

    virtual QHash<int, QByteArray> roleNames() const Q_DECL_OVERRIDE
    {
        return roles;
    }

    virtual int columnCount(const QModelIndex &) const
    {
        return 1;
    }

    virtual bool hasChildren(const QModelIndex &) const Q_DECL_OVERRIDE
    {
        return rowCount(QModelIndex()) > 0;
    }

    virtual QModelIndex index(int row, int column, const QModelIndex &parent) const
    {
        Q_UNUSED(column);
        if (row>=0 && row<rowCount(parent))
            return createIndex(row,0);
        else
            return QModelIndex();
    }

    virtual QModelIndex parent(const QModelIndex &) const
    {
        return QModelIndex();
    }

    QVariant data (const QModelIndex & index, int role) const
    {
        int row = index.row();
        if ((row<0) || (row>=items.count()))
            return QVariant::Invalid;

        switch (role) {
        case Qt::DisplayRole:
        case 555:
            return QVariant::fromValue(items.at(row));
        default:
            return QVariant();
        }
    }
};

#endif // STRINGMODEL_H
