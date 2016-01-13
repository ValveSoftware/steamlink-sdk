/****************************************************************************
 **
 ** Copyright (C) 2013 Ivan Vizir <define-true-false@yandex.com>
 ** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
 ** Contact: http://www.qt-project.org/legal
 **
 ** This file is part of the QtWinExtras module of the Qt Toolkit.
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

#ifndef QQUICKJUMPLIST_P_H
#define QQUICKJUMPLIST_P_H

#include <QObject>
#include <QQmlParserStatus>
#include <QQmlListProperty>

QT_BEGIN_NAMESPACE

class QQuickJumpListCategory;

class QQuickJumpList : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_PROPERTY(QQuickJumpListCategory *recent READ recent CONSTANT)
    Q_PROPERTY(QQuickJumpListCategory *frequent READ frequent CONSTANT)
    Q_PROPERTY(QQuickJumpListCategory *tasks READ tasks WRITE setTasks NOTIFY tasksChanged)
    Q_PROPERTY(QQmlListProperty<QQuickJumpListCategory> categories READ categories NOTIFY categoriesChanged)
    Q_PROPERTY(QQmlListProperty<QObject> data READ data)
    Q_CLASSINFO("DefaultProperty", "data")
    Q_INTERFACES(QQmlParserStatus)

public:
    explicit QQuickJumpList(QObject *parent = 0);
    ~QQuickJumpList();

    QQuickJumpListCategory *recent() const;
    QQuickJumpListCategory *frequent() const;

    QQuickJumpListCategory *tasks() const;
    void setTasks(QQuickJumpListCategory *tasks);

    QQmlListProperty<QObject> data();
    QQmlListProperty<QQuickJumpListCategory> categories();

    void classBegin();
    void componentComplete();

Q_SIGNALS:
    void tasksChanged();
    void categoriesChanged();

private Q_SLOTS:
    void rebuild();

private:
    static void data_append(QQmlListProperty<QObject> *property, QObject *object);
    static int categories_count(QQmlListProperty<QQuickJumpListCategory> *property);
    static QQuickJumpListCategory *categories_at(QQmlListProperty<QQuickJumpListCategory> *property, int index);

    QQuickJumpListCategory *m_recent;
    QQuickJumpListCategory *m_frequent;
    QQuickJumpListCategory *m_tasks;
    QList<QQuickJumpListCategory *> m_categories;
};

QT_END_NAMESPACE

#endif // QQUICKJUMPLIST_P_H
