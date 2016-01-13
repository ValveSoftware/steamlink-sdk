/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
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

#ifndef TESTCPPMODELS_H
#define TESTCPPMODELS_H

#include <QAbstractListModel>
#include <QVariant>
#include <QtGui/private/qguiapplication_p.h>
#include <qpa/qplatformtheme.h>
#include <QtQml/QQmlEngine>
#include <QtQml/QJSEngine>

class SystemInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool tabAllWidgets READ tabAllWidgets CONSTANT)

public:
    SystemInfo() {}
    bool tabAllWidgets() const {
        if (const QPlatformTheme *theme = QGuiApplicationPrivate::platformTheme())
            return theme->themeHint(QPlatformTheme::TabAllWidgets).toBool();
        return true;
    }
};

static QObject *systeminfo_provider(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)

    SystemInfo *systemInfo = new SystemInfo();
    return systemInfo;
}

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


#endif // TESTCPPMODELS_H

