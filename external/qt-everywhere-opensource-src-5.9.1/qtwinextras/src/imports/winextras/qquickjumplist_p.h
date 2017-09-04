/****************************************************************************
 **
 ** Copyright (C) 2016 Ivan Vizir <define-true-false@yandex.com>
 ** Copyright (C) 2016 The Qt Company Ltd.
 ** Contact: https://www.qt.io/licensing/
 **
 ** This file is part of the QtWinExtras module of the Qt Toolkit.
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

#ifndef QQUICKJUMPLIST_P_H
#define QQUICKJUMPLIST_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

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
