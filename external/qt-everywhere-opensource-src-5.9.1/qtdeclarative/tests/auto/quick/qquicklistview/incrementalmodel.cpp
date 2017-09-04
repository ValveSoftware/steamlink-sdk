/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "incrementalmodel.h"
#include <QGuiApplication>
#include <QDebug>

IncrementalModel::IncrementalModel(QObject *parent)
    : QAbstractListModel(parent), count(0)
{
    for (int i = 0; i < 100; ++i)
        list.append("Item " + QString::number(i));
}

int IncrementalModel::rowCount(const QModelIndex & /* parent */) const
{
    return count;
}

QVariant IncrementalModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (index.row() >= list.size() || index.row() < 0)
        return QVariant();

    if (role == Qt::DisplayRole)
        return list.at(index.row());
    return QVariant();
}

bool IncrementalModel::canFetchMore(const QModelIndex & /* index */) const
{
    if (count < list.size())
        return true;
    else
        return false;
}

void IncrementalModel::fetchMore(const QModelIndex & /* index */)
{
    int remainder = list.size() - count;
    int itemsToFetch = qMin(5, remainder);

    beginInsertRows(QModelIndex(), count, count+itemsToFetch-1);

    count += itemsToFetch;

    endInsertRows();
}
