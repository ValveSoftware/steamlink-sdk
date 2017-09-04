/****************************************************************************
**
** Copyright (C) 2016 Ford Motor Company
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#ifndef QQMLCHILDRENPRIVATE_H
#define QQMLCHILDRENPRIVATE_H

#include <QAbstractState>
#include <QAbstractTransition>
#include <QStateMachine>
#include <QQmlInfo>
#include <QQmlListProperty>

template <class T>
class ChildrenPrivate
{
public:
    ChildrenPrivate()
    {}

    static void append(QQmlListProperty<QObject> *prop, QObject *item)
    {
        QAbstractState *state = qobject_cast<QAbstractState*>(item);
        if (state) {
            item->setParent(prop->object);
        } else {
            QAbstractTransition *trans = qobject_cast<QAbstractTransition*>(item);
            if (trans)
                static_cast<T *>(prop->object)->addTransition(trans);
        }
        static_cast<ChildrenPrivate<T>*>(prop->data)->children.append(item);
        emit static_cast<T *>(prop->object)->childrenChanged();
    }

    static void appendNoTransition(QQmlListProperty<QObject> *prop, QObject *item)
    {
        QAbstractState *state = qobject_cast<QAbstractState*>(item);
        if (state) {
            item->setParent(prop->object);
        }
        static_cast<ChildrenPrivate<T>*>(prop->data)->children.append(item);
        emit static_cast<T *>(prop->object)->childrenChanged();
    }

    static int count(QQmlListProperty<QObject> *prop)
    {
        return static_cast<ChildrenPrivate<T>*>(prop->data)->children.count();
    }

    static QObject *at(QQmlListProperty<QObject> *prop, int index)
    {
        return static_cast<ChildrenPrivate<T>*>(prop->data)->children.at(index);
    }

    static void clear(QQmlListProperty<QObject> *prop)
    {
        static_cast<ChildrenPrivate<T>*>(prop->data)->children.clear();
        emit static_cast<T *>(prop->object)->childrenChanged();
    }

private:
    QList<QObject *> children;
};

#endif
