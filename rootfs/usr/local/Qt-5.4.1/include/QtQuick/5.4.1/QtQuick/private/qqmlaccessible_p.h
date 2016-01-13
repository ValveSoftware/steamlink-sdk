/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#ifndef QQMLACCESSIBLE_H
#define QQMLACCESSIBLE_H

#include <QtGui/qaccessibleobject.h>
#include <QtQml/qqmlproperty.h>

QT_BEGIN_NAMESPACE

#ifndef QT_NO_ACCESSIBILITY

/*
    -- Declarative Accessibility Overview. --

    * Item interface classes:
    QAccessibleDeclarativeItem for Qt Quick 1
    QAccessibleQuickItem for for Qt Quick 2
    Common base class: QQmlAccessible

    * View interface classes.

    These are the root of the QML accessible tree and connects it to the widget hierarchy.

    QAccessbileDeclarativeView is the root for the QGraphicsView implementation
    QAccessbileQuickView is the root for the SceneGraph implementation

*/
class QQmlAccessible: public QAccessibleObject, public QAccessibleActionInterface
{
public:
    ~QQmlAccessible();
    void *interface_cast(QAccessible::InterfaceType t);

    virtual QRect viewRect() const = 0;
    QAccessible::State state() const;

    QStringList actionNames() const;
    void doAction(const QString &actionName);
    QStringList keyBindingsForAction(const QString &actionName) const;

protected:
    // For subclasses, use instantiateObject factory method outside the class.
    QQmlAccessible(QObject *object);
};

#endif // QT_NO_ACCESSIBILITY

QT_END_NAMESPACE

#endif // QQMLACCESSIBLE_H
