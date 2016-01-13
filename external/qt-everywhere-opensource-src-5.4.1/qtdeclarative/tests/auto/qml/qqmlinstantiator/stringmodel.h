/****************************************************************************
**
** Copyright (C) 2013 Dmitrii Kosarev aka Kakadu <kakadu.hafanana@gmail.com>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
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
