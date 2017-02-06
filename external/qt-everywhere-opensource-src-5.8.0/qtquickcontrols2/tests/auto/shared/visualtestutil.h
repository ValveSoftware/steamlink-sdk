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

#ifndef QQUICKVISUALTESTUTIL_H
#define QQUICKVISUALTESTUTIL_H

#include <QtQuick/QQuickItem>
#include <QtQml/QQmlExpression>

#include <QtQuick/private/qquickitem_p.h>

#include <QtQuickTemplates2/private/qquickapplicationwindow_p.h>

#include "util.h"

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

    class QQuickApplicationHelper
    {
    public:
        QQuickApplicationHelper(QQmlDataTest *testCase, const QString &testFilePath) :
            component(&engine)
        {
            component.loadUrl(testCase->testFileUrl(testFilePath));
            QObject *rootObject = component.create();
            cleanup.reset(rootObject);
            QVERIFY2(rootObject, qPrintable(QString::fromLatin1("Failed to create window: %1").arg(component.errorString())));

            window = qobject_cast<QQuickWindow*>(rootObject);
            appWindow = qobject_cast<QQuickApplicationWindow*>(rootObject);
            QVERIFY(window);
            QVERIFY(!window->isVisible());
        }

        QQmlEngine engine;
        QQmlComponent component;
        QScopedPointer<QObject> cleanup;
        QQuickApplicationWindow *appWindow;
        QQuickWindow *window;
    };
}

#define QQUICK_VERIFY_POLISH(item) \
    QTRY_COMPARE(QQuickItemPrivate::get(item)->polishScheduled, false)

#endif // QQUICKVISUALTESTUTIL_H
