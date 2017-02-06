/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "visualtestutil.h"

#include <QtQuick/QQuickItem>
#include <QtCore/QDebug>

bool QQuickVisualTestUtil::delegateVisible(QQuickItem *item)
{
    return item->isVisible() && !QQuickItemPrivate::get(item)->culled;
}

QQuickItem *QQuickVisualTestUtil::findVisibleChild(QQuickItem *parent, const QString &objectName)
{
    QQuickItem *item = 0;
    QList<QQuickItem*> items = parent->findChildren<QQuickItem*>(objectName);
    for (int i = 0; i < items.count(); ++i) {
        if (items.at(i)->isVisible() && !QQuickItemPrivate::get(items.at(i))->culled) {
            item = items.at(i);
            break;
        }
    }
    return item;
}

void QQuickVisualTestUtil::dumpTree(QQuickItem *parent, int depth)
{
    static QString padding("                       ");
    for (int i = 0; i < parent->childItems().count(); ++i) {
        QQuickItem *item = qobject_cast<QQuickItem*>(parent->childItems().at(i));
        if (!item)
            continue;
        qDebug() << padding.left(depth*2) << item;
        dumpTree(item, depth+1);
    }
}

