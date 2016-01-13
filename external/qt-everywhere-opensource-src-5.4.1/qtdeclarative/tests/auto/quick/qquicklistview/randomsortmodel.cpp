/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#include "randomsortmodel.h"

RandomSortModel::RandomSortModel(QObject* parent):
    QAbstractListModel(parent)
{
    for (int i = 0; i < 10; ++i) {
        mData.append(qMakePair(QString::fromLatin1("Item %1").arg(i), i * 10));
    }
}

QHash<int, QByteArray> RandomSortModel::roleNames() const
{
    QHash<int,QByteArray> roles = QAbstractItemModel::roleNames();
    roles[Qt::UserRole] = "SortRole";
    return roles;
}


int RandomSortModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid())
        return mData.count();

    return 0;
}

QVariant RandomSortModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (index.row() >= mData.count()) {
        return QVariant();
    }

    if (role == Qt::DisplayRole) {
        return QString::fromLatin1("%1 (weight %2)").arg(mData[index.row()].first).arg(mData[index.row()].second);
    } else if (role == Qt::UserRole) {
        return mData[index.row()].second;
    }

    return QVariant();
}

void RandomSortModel::randomize()
{
    const int row = qrand() % mData.count();
    int random;
    bool exists = false;
    // Make sure we won't end up with two items with the same weight, as that
    // would make unit-testing much harder
    do {
        exists = false;
        random = qrand() % (mData.count() * 10);
        QList<QPair<QString, int> >::ConstIterator iter, end;
        for (iter = mData.constBegin(), end = mData.constEnd(); iter != end; ++iter) {
            if ((*iter).second == random) {
                exists = true;
                break;
            }
        }
    } while (exists);
    mData[row].second = random;
    Q_EMIT dataChanged(index(row, 0), index(row, 0));
}
