/****************************************************************************
**
** Copyright (C) 2016 Canonical Limited and/or its subsidiary(-ies).
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

#include "proxytestinnermodel.h"

ProxyTestInnerModel::ProxyTestInnerModel()
{
    append("Adios");
    append("Hola");
    append("Halo");
}

QModelIndex ProxyTestInnerModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid())
        return QModelIndex();

    return createIndex(row, column);
}

QModelIndex ProxyTestInnerModel::parent(const QModelIndex & /*parent*/) const
{
    return QModelIndex();
}

int ProxyTestInnerModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return m_values.count();
}

int ProxyTestInnerModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return 1;
}

QVariant ProxyTestInnerModel::data(const QModelIndex &index, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    return m_values[index.row()];
}

void ProxyTestInnerModel::append(const QString &s)
{
    beginInsertRows(QModelIndex(), m_values.count(), m_values.count());
    m_values << s;
    endInsertRows();
}

void ProxyTestInnerModel::setValue(int i, const QString &s)
{
    m_values[i] = s;
    Q_EMIT dataChanged(index(i, 0), index(i, 0));
}

void ProxyTestInnerModel::moveTwoToZero()
{
    beginMoveRows(QModelIndex(), 2, 2, QModelIndex(), 0);
    m_values.move(2, 0);
    endMoveRows();
}

void ProxyTestInnerModel::doStuff()
{
    moveTwoToZero();
    setValue(1, "Hilo");
}
