/****************************************************************************
**
** Copyright (C) 2014 Ford Motor Company
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
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
