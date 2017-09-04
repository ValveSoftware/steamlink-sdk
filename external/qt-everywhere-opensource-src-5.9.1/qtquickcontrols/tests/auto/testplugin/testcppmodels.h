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

#ifndef TESTCPPMODELS_H
#define TESTCPPMODELS_H

#include <QAbstractListModel>
#include <QVariant>
#include <QtGui/private/qguiapplication_p.h>
#include <QtQml/QQmlEngine>
#include <QtQml/QJSEngine>

class TestObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int value READ value CONSTANT)

public:
    TestObject(int val = 0) : m_value(val) {}
    int value() const { return m_value; }
private:
    int m_value;
};

class TestItemModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit TestItemModel(QObject *parent = 0)
        : QAbstractListModel(parent) {}

    enum {
        TestRole = Qt::UserRole + 1
    };

    Q_INVOKABLE QVariant dataAt(int index) const
    {
        return QString("Row %1").arg(index);
    }

    QVariant data(const QModelIndex &index, int role) const
    {
        if (role == TestRole)
            return dataAt(index.row());
        else
            return QVariant();
    }

    int rowCount(const QModelIndex & /*parent*/) const
    {
        return 10;
    }

    QHash<int, QByteArray> roleNames() const
    {
        QHash<int, QByteArray> rn = QAbstractItemModel::roleNames();
        rn[TestRole] = "test";
        return rn;
    }

private:
    QList<TestObject> m_testobject;
};

class TestFetchAppendModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit TestFetchAppendModel(QObject *parent = 0)
        : QAbstractListModel(parent) { }

    virtual int rowCount(const QModelIndex &parent) const
    {
        if (parent.isValid()) {
            return 0;
        }

        return m_data.size();
    }
    virtual QVariant data(const QModelIndex &index, int role) const
    {
        if (!index.isValid() || role != Qt::DisplayRole) {
            return QVariant();
        }

        return QVariant::fromValue(index.row());
    }

    virtual bool canFetchMore(const QModelIndex &parent) const
    {
        Q_UNUSED(parent)

        return true;
    }
    virtual void fetchMore(const QModelIndex & parent)
    {
        Q_UNUSED(parent)

        addMoreData();
    }

private:
    void addMoreData()
    {
        static const int insertCount = 20;

        beginInsertRows(QModelIndex(), m_data.size(), m_data.size() + insertCount - 1);
        for (int i = 0; i < insertCount; i++) {
            m_data.append(int());
        }
        endInsertRows();
    }

    QList<int> m_data;
};


#endif // TESTCPPMODELS_H

