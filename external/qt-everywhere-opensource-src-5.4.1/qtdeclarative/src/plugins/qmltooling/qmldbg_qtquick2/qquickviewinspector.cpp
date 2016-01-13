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

#include "qquickviewinspector.h"

#include "highlight.h"
#include "inspecttool.h"

#include <QtQml/private/qqmlengine_p.h>
#include <QtQml/private/qqmldebugservice_p.h>
#include <QtQuick/private/qquickitem_p.h>

#include <QtQuick/QQuickView>
#include <QtQuick/QQuickItem>

#include <cfloat>

namespace QmlJSDebugger {
namespace QtQuick2 {

/*
 * Collects all the items at the given position, from top to bottom.
 */
static void collectItemsAt(QQuickItem *item, const QPointF &pos,
                           QQuickItem *overlay, QList<QQuickItem *> &resultList)
{
    if (item == overlay)
        return;

    if (item->flags() & QQuickItem::ItemClipsChildrenToShape) {
        if (!QRectF(0, 0, item->width(), item->height()).contains(pos))
            return;
    }

    QList<QQuickItem *> children = QQuickItemPrivate::get(item)->paintOrderChildItems();
    for (int i = children.count() - 1; i >= 0; --i) {
        QQuickItem *child = children.at(i);
        collectItemsAt(child, item->mapToItem(child, pos), overlay, resultList);
    }

    if (!QRectF(0, 0, item->width(), item->height()).contains(pos))
        return;

    resultList.append(item);
}

/*
 * Returns the first visible item at the given position, or 0 when no such
 * child exists.
 */
static QQuickItem *itemAt(QQuickItem *item, const QPointF &pos,
                          QQuickItem *overlay)
{
    if (item == overlay)
        return 0;

    if (!item->isVisible() || item->opacity() == 0.0)
        return 0;

    if (item->flags() & QQuickItem::ItemClipsChildrenToShape) {
        if (!QRectF(0, 0, item->width(), item->height()).contains(pos))
            return 0;
    }

    QList<QQuickItem *> children = QQuickItemPrivate::get(item)->paintOrderChildItems();
    for (int i = children.count() - 1; i >= 0; --i) {
        QQuickItem *child = children.at(i);
        if (QQuickItem *betterCandidate = itemAt(child, item->mapToItem(child, pos),
                                                 overlay))
            return betterCandidate;
    }

    if (!(item->flags() & QQuickItem::ItemHasContents))
        return 0;

    if (!QRectF(0, 0, item->width(), item->height()).contains(pos))
        return 0;

    return item;
}


QQuickViewInspector::QQuickViewInspector(QQuickView *view, QObject *parent) :
    AbstractViewInspector(parent),
    m_view(view),
    m_overlay(new QQuickItem),
    m_inspectTool(new InspectTool(this, view)),
    m_sendQmlReloadedMessage(false)
{
    // Try to make sure the overlay is always on top
    m_overlay->setZ(FLT_MAX);

    if (QQuickItem *root = view->contentItem())
        m_overlay->setParentItem(root);

    view->installEventFilter(this);
    appendTool(m_inspectTool);
    connect(view, SIGNAL(statusChanged(QQuickView::Status)),
            this, SLOT(onViewStatus(QQuickView::Status)));
}

void QQuickViewInspector::changeCurrentObjects(const QList<QObject*> &objects)
{
    QList<QQuickItem*> items;
    foreach (QObject *obj, objects)
        if (QQuickItem *item = qobject_cast<QQuickItem*>(obj))
            items << item;

    syncSelectedItems(items);
}

void QQuickViewInspector::reparentQmlObject(QObject *object, QObject *newParent)
{
    if (!newParent)
        return;

    object->setParent(newParent);
    QQuickItem *newParentItem = qobject_cast<QQuickItem*>(newParent);
    QQuickItem *item = qobject_cast<QQuickItem*>(object);
    if (newParentItem && item)
        item->setParentItem(newParentItem);
}

QWindow *getMasterWindow(QWindow *w)
{
    QWindow *p = w->parent();
    while (p) {
        w = p;
        p = p->parent();
    }
    return w;
}

Qt::WindowFlags QQuickViewInspector::windowFlags() const
{
    return getMasterWindow(m_view)->flags();
}

void QQuickViewInspector::setWindowFlags(Qt::WindowFlags flags)
{
    QWindow *w = getMasterWindow(m_view);
    w->setFlags(flags);
    // make flags are applied
    w->setVisible(false);
    w->setVisible(true);
}

QQmlEngine *QQuickViewInspector::declarativeEngine() const
{
    return m_view->engine();
}

QQuickItem *QQuickViewInspector::topVisibleItemAt(const QPointF &pos) const
{
    QQuickItem *root = m_view->contentItem();
    return itemAt(root, root->mapFromScene(pos), m_overlay);
}

QList<QQuickItem *> QQuickViewInspector::itemsAt(const QPointF &pos) const
{
    QQuickItem *root = m_view->contentItem();
    QList<QQuickItem *> resultList;
    collectItemsAt(root, root->mapFromScene(pos), m_overlay,
                   resultList);
    return resultList;
}

QList<QQuickItem*> QQuickViewInspector::selectedItems() const
{
    QList<QQuickItem *> selection;
    foreach (const QPointer<QQuickItem> &selectedItem, m_selectedItems) {
        if (selectedItem)
            selection << selectedItem;
    }
    return selection;
}

void QQuickViewInspector::setSelectedItems(const QList<QQuickItem *> &items)
{
    if (!syncSelectedItems(items))
        return;

    QList<QObject*> objectList;
    foreach (QQuickItem *item, items)
        objectList << item;

    sendCurrentObjects(objectList);
}

bool QQuickViewInspector::syncSelectedItems(const QList<QQuickItem *> &items)
{
    bool selectionChanged = false;

    // Disconnect and remove items that are no longer selected
    foreach (const QPointer<QQuickItem> &item, m_selectedItems) {
        if (!item) // Don't see how this can happen due to handling of destroyed()
            continue;
        if (items.contains(item))
            continue;

        selectionChanged = true;
        item->disconnect(this);
        m_selectedItems.removeOne(item);
        delete m_highlightItems.take(item);
    }

    // Connect and add newly selected items
    foreach (QQuickItem *item, items) {
        if (m_selectedItems.contains(item))
            continue;

        selectionChanged = true;
        connect(item, SIGNAL(destroyed(QObject*)), this, SLOT(removeFromSelectedItems(QObject*)));
        m_selectedItems.append(item);
        SelectionHighlight *selectionHighlightItem;
        selectionHighlightItem = new SelectionHighlight(titleForItem(item), item, m_overlay);
        m_highlightItems.insert(item, selectionHighlightItem);
    }

    return selectionChanged;
}

void QQuickViewInspector::showSelectedItemName(QQuickItem *item, const QPointF &point)
{
    SelectionHighlight *highlightItem = m_highlightItems.value(item, 0);
    if (highlightItem)
        highlightItem->showName(point);
}

void QQuickViewInspector::removeFromSelectedItems(QObject *object)
{
    if (QQuickItem *item = qobject_cast<QQuickItem*>(object)) {
        if (m_selectedItems.removeOne(item))
            delete m_highlightItems.take(item);
    }
}

bool QQuickViewInspector::eventFilter(QObject *obj, QEvent *event)
{
    if (obj != m_view)
        return QObject::eventFilter(obj, event);

    return AbstractViewInspector::eventFilter(obj, event);
}

bool QQuickViewInspector::mouseMoveEvent(QMouseEvent *event)
{
    // TODO
//    if (QQuickItem *item = topVisibleItemAt(event->pos()))
//        m_view->setToolTip(titleForItem(item));
//    else
//        m_view->setToolTip(QString());

    return AbstractViewInspector::mouseMoveEvent(event);
}

QString QQuickViewInspector::titleForItem(QQuickItem *item) const
{
    QString className = QLatin1String(item->metaObject()->className());
    QString objectStringId = idStringForObject(item);

    className.remove(QRegExp(QLatin1String("_QMLTYPE_\\d+")));
    className.remove(QRegExp(QLatin1String("_QML_\\d+")));
    if (className.startsWith(QLatin1String("QQuick")))
        className = className.mid(6);

    QString constructedName;

    if (!objectStringId.isEmpty()) {
        constructedName = objectStringId + QLatin1String(" (") + className + QLatin1Char(')');
    } else if (!item->objectName().isEmpty()) {
        constructedName = item->objectName() + QLatin1String(" (") + className + QLatin1Char(')');
    } else {
        constructedName = className;
    }

    return constructedName;
}

void QQuickViewInspector::reloadQmlFile(const QHash<QString, QByteArray> &changesHash)
{
    clearComponentCache();

    // Reset the selection since we are reloading the main qml
    setSelectedItems(QList<QQuickItem *>());

    QQmlDebugService::clearObjectsFromHash();

    QHash<QUrl, QByteArray> debugCache;

    foreach (const QString &str, changesHash.keys())
        debugCache.insert(QUrl(str), changesHash.value(str, QByteArray()));

    // Updating the cache in engine private such that the QML Data loader
    // gets the changes from the cache.
    QQmlEnginePrivate::get(declarativeEngine())->setDebugChangesCache(debugCache);

    m_sendQmlReloadedMessage = true;
    // reloading the view such that the changes done for the files are
    // reflected in view
    view()->setSource(view()->source());
}

void QQuickViewInspector::setShowAppOnTop(bool appOnTop)
{
    m_appOnTop = appOnTop;
    // Hack for QTCREATORBUG-6295.
    // TODO: The root cause to be identified and fixed later.
    QTimer::singleShot(100, this, SLOT(applyAppOnTop()));
}

void QQuickViewInspector::onViewStatus(QQuickView::Status status)
{
    bool success = false;
    switch (status) {
    case QQuickView::Loading:
        return;
    case QQuickView::Ready:
        if (view()->errors().count())
            break;
        success = true;
        break;
    case QQuickView::Null:
    case QQuickView::Error:
        break;
    default:
        break;
    }
    if (m_sendQmlReloadedMessage) {
        m_sendQmlReloadedMessage = false;
        sendQmlFileReloaded(success);
    }
}

void QQuickViewInspector::applyAppOnTop()
{
    Qt::WindowFlags flags = windowFlags();
    if (m_appOnTop)
        flags |= Qt::WindowStaysOnTopHint;
    else
        flags &= ~Qt::WindowStaysOnTopHint;

    setWindowFlags(flags);
}

} // namespace QtQuick2
} // namespace QmlJSDebugger
