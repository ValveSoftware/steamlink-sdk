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

#ifndef QQUICKVISUALTESTUTIL_H
#define QQUICKVISUALTESTUTIL_H

#include <QtQuick/QQuickItem>
#include <QtQml/QQmlExpression>

#include <QtQuick/private/qquickitem_p.h>

namespace QQuickVisualTestUtil
{
    QQuickItem *findVisibleChild(QQuickItem *parent, const QString &objectName);

    void dumpTree(QQuickItem *parent, int depth = 0);

    bool delegateVisible(QQuickItem *item);

    /*
       Find an item with the specified objectName.  If index is supplied then the
       item must also evaluate the {index} expression equal to index
    */
    template<typename T>
    T *findItem(QQuickItem *parent, const QString &objectName, int index = -1)
    {
        const QMetaObject &mo = T::staticMetaObject;
        for (int i = 0; i < parent->childItems().count(); ++i) {
            QQuickItem *item = qobject_cast<QQuickItem*>(parent->childItems().at(i));
            if (!item)
                continue;
            if (mo.cast(item) && (objectName.isEmpty() || item->objectName() == objectName)) {
                if (index != -1) {
                    QQmlExpression e(qmlContext(item), item, "index");
                    if (e.evaluate().toInt() == index)
                        return static_cast<T*>(item);
                } else {
                    return static_cast<T*>(item);
                }
            }
            item = findItem<T>(item, objectName, index);
            if (item)
                return static_cast<T*>(item);
        }

        return 0;
    }

    template<typename T>
    QList<T*> findItems(QQuickItem *parent, const QString &objectName, bool visibleOnly = true)
    {
        QList<T*> items;
        const QMetaObject &mo = T::staticMetaObject;
        for (int i = 0; i < parent->childItems().count(); ++i) {
            QQuickItem *item = qobject_cast<QQuickItem*>(parent->childItems().at(i));
            if (!item || (visibleOnly && (!item->isVisible() || QQuickItemPrivate::get(item)->culled)))
                continue;
            if (mo.cast(item) && (objectName.isEmpty() || item->objectName() == objectName))
                items.append(static_cast<T*>(item));
            items += findItems<T>(item, objectName);
        }

        return items;
    }

    template<typename T>
    QList<T*> findItems(QQuickItem *parent, const QString &objectName, const QList<int> &indexes)
    {
        QList<T*> items;
        for (int i=0; i<indexes.count(); i++)
            items << qobject_cast<QQuickItem*>(findItem<T>(parent, objectName, indexes[i]));
        return items;
    }

    bool compareImages(const QImage &ia, const QImage &ib);
}

#define QQUICK_VERIFY_POLISH(item) \
    QTRY_COMPARE(QQuickItemPrivate::get(item)->polishScheduled, false)

#endif // QQUICKVISUALTESTUTIL_H
