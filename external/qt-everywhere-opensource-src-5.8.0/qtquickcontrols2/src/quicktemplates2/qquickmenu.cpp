/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Templates 2 module of the Qt Toolkit.
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

#include "qquickmenu_p.h"
#include "qquickmenu_p_p.h"
#include "qquickmenuitem_p.h"

#include <QtGui/qevent.h>
#include <QtQml/private/qqmlobjectmodel_p.h>
#include <QtQuick/private/qquickitem_p.h>
#include <QtQuick/private/qquickitemchangelistener_p.h>
#include <QtQuick/private/qquickitemview_p.h>
#include <QtQuick/private/qquickevents_p_p.h>
#include <QtQuick/private/qquickwindow_p.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype Menu
    \inherits Popup
    \instantiates QQuickMenu
    \inqmlmodule QtQuick.Controls
    \since 5.7
    \ingroup qtquickcontrols2-menus
    \ingroup qtquickcontrols2-popups
    \brief Menu popup that can be used as a context menu or popup menu.

    \image qtquickcontrols2-menu.png

    Menu has two main use cases:
    \list
        \li Context menus; for example, a menu that is shown after right clicking
        \li Popup menus; for example, a menu that is shown after clicking a button
    \endlist

    \code
    Button {
        id: fileButton
        text: "File"
        onClicked: menu.open()

        Menu {
            id: menu
            y: fileButton.height

            MenuItem {
                text: "New..."
            }
            MenuItem {
                text: "Open..."
            }
            MenuItem {
                text: "Save"
            }
        }
    }
    \endcode

    Typically, menu items are statically declared as children of the menu, but
    Menu also provides API to \l {addItem}{add}, \l {insertItem}{insert},
    \l {moveItem}{move} and \l {removeItem}{remove} items dynamically. The
    items in a menu can be accessed using \l itemAt() or
    \l {Popup::}{contentChildren}.

    Although \l {MenuItem}{MenuItems} are most commonly used with Menu, it can
    contain any type of item.

    \sa {Customizing Menu}, {Menu Controls}, {Popup Controls}
*/

QQuickMenuPrivate::QQuickMenuPrivate() :
    contentItem(nullptr),
    contentModel(nullptr)
{
    Q_Q(QQuickMenu);
    contentModel = new QQmlObjectModel(q);
}

QQuickItem *QQuickMenuPrivate::itemAt(int index) const
{
    return qobject_cast<QQuickItem *>(contentModel->get(index));
}

void QQuickMenuPrivate::insertItem(int index, QQuickItem *item)
{
    contentData.append(item);
    item->setParentItem(contentItem);
    if (qobject_cast<QQuickItemView *>(contentItem))
        QQuickItemPrivate::get(item)->setCulled(true); // QTBUG-53262
    if (complete)
        resizeItem(item);
    QQuickItemPrivate::get(item)->addItemChangeListener(this, QQuickItemPrivate::Destroyed | QQuickItemPrivate::Parent);
    contentModel->insert(index, item);

    QQuickMenuItem *menuItem = qobject_cast<QQuickMenuItem *>(item);
    if (menuItem) {
        Q_Q(QQuickMenu);
        QObjectPrivate::connect(menuItem, &QQuickMenuItem::pressed, this, &QQuickMenuPrivate::onItemPressed);
        QObject::connect(menuItem, &QQuickMenuItem::triggered, q, &QQuickPopup::close);
        QObjectPrivate::connect(menuItem, &QQuickItem::activeFocusChanged, this, &QQuickMenuPrivate::onItemActiveFocusChanged);
    }
}

void QQuickMenuPrivate::moveItem(int from, int to)
{
    contentModel->move(from, to);
}

void QQuickMenuPrivate::removeItem(int index, QQuickItem *item)
{
    contentData.removeOne(item);

    QQuickItemPrivate::get(item)->removeItemChangeListener(this, QQuickItemPrivate::Destroyed | QQuickItemPrivate::Parent);
    item->setParentItem(nullptr);
    contentModel->remove(index);

    QQuickMenuItem *menuItem = qobject_cast<QQuickMenuItem *>(item);
    if (menuItem) {
        Q_Q(QQuickMenu);
        QObjectPrivate::disconnect(menuItem, &QQuickMenuItem::pressed, this, &QQuickMenuPrivate::onItemPressed);
        QObject::disconnect(menuItem, &QQuickMenuItem::triggered, q, &QQuickPopup::close);
        QObjectPrivate::disconnect(menuItem, &QQuickItem::activeFocusChanged, this, &QQuickMenuPrivate::onItemActiveFocusChanged);
    }
}

void QQuickMenuPrivate::resizeItem(QQuickItem *item)
{
    if (!item || !contentItem)
        return;

    QQuickItemPrivate *p = QQuickItemPrivate::get(item);
    if (!p->widthValid) {
        item->setWidth(contentItem->width());
        p->widthValid = false;
    }
}

void QQuickMenuPrivate::resizeItems()
{
    if (!contentModel)
        return;

    for (int i = 0; i < contentModel->count(); ++i)
        resizeItem(itemAt(i));
}

void QQuickMenuPrivate::itemChildAdded(QQuickItem *, QQuickItem *child)
{
    // add dynamically reparented items (eg. by a Repeater)
    if (!QQuickItemPrivate::get(child)->isTransparentForPositioner() && !contentData.contains(child))
        insertItem(contentModel->count(), child);
}

void QQuickMenuPrivate::itemParentChanged(QQuickItem *item, QQuickItem *parent)
{
    // remove dynamically unparented items (eg. by a Repeater)
    if (!parent)
        removeItem(contentModel->indexOf(item, nullptr), item);
}

void QQuickMenuPrivate::itemSiblingOrderChanged(QQuickItem *)
{
    // reorder the restacked items (eg. by a Repeater)
    Q_Q(QQuickMenu);
    QList<QQuickItem *> siblings = contentItem->childItems();
    for (int i = 0; i < siblings.count(); ++i) {
        QQuickItem* sibling = siblings.at(i);
        int index = contentModel->indexOf(sibling, nullptr);
        q->moveItem(index, i);
    }
}

void QQuickMenuPrivate::itemDestroyed(QQuickItem *item)
{
    QQuickPopupPrivate::itemDestroyed(item);
    int index = contentModel->indexOf(item, nullptr);
    if (index != -1)
        removeItem(index, item);
}

void QQuickMenuPrivate::itemGeometryChanged(QQuickItem *, QQuickGeometryChange, const QRectF &)
{
    if (complete)
        resizeItems();
}

void QQuickMenuPrivate::onItemPressed()
{
    Q_Q(QQuickMenu);
    QQuickItem *item = qobject_cast<QQuickItem*>(q->sender());
    if (item)
        item->forceActiveFocus();
}

void QQuickMenuPrivate::onItemActiveFocusChanged()
{
    Q_Q(QQuickMenu);
    QQuickItem *item = qobject_cast<QQuickItem*>(q->sender());
    if (!item->hasActiveFocus())
        return;

    int indexOfItem = contentModel->indexOf(item, nullptr);
    setCurrentIndex(indexOfItem);
}

int QQuickMenuPrivate::currentIndex() const
{
    QVariant index = contentItem->property("currentIndex");
    if (!index.isValid())
        return -1;
    return index.toInt();
}

void QQuickMenuPrivate::setCurrentIndex(int index)
{
    contentItem->setProperty("currentIndex", index);
}

void QQuickMenuPrivate::contentData_append(QQmlListProperty<QObject> *prop, QObject *obj)
{
    QQuickMenuPrivate *p = static_cast<QQuickMenuPrivate *>(prop->data);
    QQuickMenu *q = static_cast<QQuickMenu *>(prop->object);
    QQuickItem *item = qobject_cast<QQuickItem *>(obj);
    if (item) {
        if (QQuickItemPrivate::get(item)->isTransparentForPositioner()) {
            QQuickItemPrivate::get(item)->addItemChangeListener(p, QQuickItemPrivate::SiblingOrder);
            item->setParentItem(p->contentItem);
        } else if (p->contentModel->indexOf(item, nullptr) == -1) {
            q->addItem(item);
        }
    } else {
        p->contentData.append(obj);
    }
}

int QQuickMenuPrivate::contentData_count(QQmlListProperty<QObject> *prop)
{
    QQuickMenuPrivate *p = static_cast<QQuickMenuPrivate *>(prop->data);
    return p->contentData.count();
}

QObject *QQuickMenuPrivate::contentData_at(QQmlListProperty<QObject> *prop, int index)
{
    QQuickMenuPrivate *p = static_cast<QQuickMenuPrivate *>(prop->data);
    return p->contentData.value(index);
}

void QQuickMenuPrivate::contentData_clear(QQmlListProperty<QObject> *prop)
{
    QQuickMenuPrivate *p = static_cast<QQuickMenuPrivate *>(prop->data);
    p->contentData.clear();
}

QQuickMenu::QQuickMenu(QObject *parent) :
    QQuickPopup(*(new QQuickMenuPrivate), parent)
{
    setFocus(true);
    setClosePolicy(CloseOnEscape | CloseOnPressOutside | CloseOnReleaseOutside);
}

/*!
    \qmlmethod Item QtQuick.Controls::Menu::itemAt(int index)

    Returns the item at \a index, or \c null if it does not exist.
*/
QQuickItem *QQuickMenu::itemAt(int index) const
{
    Q_D(const QQuickMenu);
    return d->itemAt(index);
}

/*!
    \qmlmethod void QtQuick.Controls::Menu::addItem(Item item)

    Adds \a item to the end of the list of items.
*/
void QQuickMenu::addItem(QQuickItem *item)
{
    Q_D(QQuickMenu);
    insertItem(d->contentModel->count(), item);
}

/*!
    \qmlmethod void QtQuick.Controls::Menu::insertItem(int index, Item item)

    Inserts \a item at \a index.
*/
void QQuickMenu::insertItem(int index, QQuickItem *item)
{
    Q_D(QQuickMenu);
    if (!item)
        return;
    const int count = d->contentModel->count();
    if (index < 0 || index > count)
        index = count;

    int oldIndex = d->contentModel->indexOf(item, nullptr);
    if (oldIndex != -1) {
        if (oldIndex < index)
            --index;
        if (oldIndex != index)
            d->moveItem(oldIndex, index);
    } else {
        d->insertItem(index, item);
    }
}

/*!
    \qmlmethod void QtQuick.Controls::Menu::moveItem(int from, int to)

    Moves an item \a from one index \a to another.
*/
void QQuickMenu::moveItem(int from, int to)
{
    Q_D(QQuickMenu);
    const int count = d->contentModel->count();
    if (from < 0 || from > count - 1)
        return;
    if (to < 0 || to > count - 1)
        to = count - 1;

    if (from != to)
        d->moveItem(from, to);
}

/*!
    \qmlmethod void QtQuick.Controls::Menu::removeItem(int index)

    Removes the item at \a index.

    \note The ownership of the item is transferred to the caller.
*/
void QQuickMenu::removeItem(int index)
{
    Q_D(QQuickMenu);
    const int count = d->contentModel->count();
    if (index < 0 || index >= count)
        return;

    QQuickItem *item = itemAt(index);
    if (item)
        d->removeItem(index, item);
}

/*!
    \qmlproperty model QtQuick.Controls::Menu::contentModel
    \readonly

    This property holds the model used to display menu items.

    The content model is provided for visualization purposes. It can be assigned
    as a model to a content item that presents the contents of the menu.

    \code
    Menu {
        id: menu
        contentItem: ListView {
            model: menu.contentModel
        }
    }
    \endcode

    The model allows menu items to be statically declared as children of the
    menu.
*/
QVariant QQuickMenu::contentModel() const
{
    Q_D(const QQuickMenu);
    return QVariant::fromValue(d->contentModel);
}

/*!
    \qmlproperty list<Object> QtQuick.Controls::Menu::contentData
    \default

    This property holds the list of content data.

    The list contains all objects that have been declared in QML as children
    of the menu, and also items that have been dynamically added or
    inserted using the \l addItem() and \l insertItem() methods, respectively.

    \note Unlike \c contentChildren, \c contentData does include non-visual QML
    objects. It is not re-ordered when items are inserted or moved.

    \sa Item::data, {Popup::}{contentChildren}
*/
QQmlListProperty<QObject> QQuickMenu::contentData()
{
    Q_D(QQuickMenu);
    return QQmlListProperty<QObject>(this, d,
        QQuickMenuPrivate::contentData_append,
        QQuickMenuPrivate::contentData_count,
        QQuickMenuPrivate::contentData_at,
        QQuickMenuPrivate::contentData_clear);
}

/*!
    \qmlproperty string QtQuick.Controls::Menu::title

    This property holds the title for the menu.

    The title of a menu is often displayed in the text of a menu item when the
    menu is a submenu, and in the text of a tool button when it is in a
    menubar.
*/
QString QQuickMenu::title() const
{
    Q_D(const QQuickMenu);
    return d->title;
}

void QQuickMenu::setTitle(QString &title)
{
    Q_D(QQuickMenu);
    if (title == d->title)
        return;
    d->title = title;
    emit titleChanged();
}

void QQuickMenu::componentComplete()
{
    Q_D(QQuickMenu);
    QQuickPopup::componentComplete();
    d->resizeItems();
}

void QQuickMenu::contentItemChange(QQuickItem *newItem, QQuickItem *oldItem)
{
    Q_D(QQuickMenu);
    Q_UNUSED(oldItem);
    QQuickPopup::contentItemChange(newItem, oldItem);
    d->contentItem = newItem;
}

void QQuickMenu::itemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &data)
{
    Q_D(QQuickMenu);
    QQuickPopup::itemChange(change, data);

    if (change == QQuickItem::ItemVisibleHasChanged) {
        if (!data.boolValue) {
            // Ensure that when the menu isn't visible, there's no current item
            // the next time it's opened.
            QQuickItem *focusItem = QQuickItemPrivate::get(d->contentItem)->subFocusItem;
            if (focusItem) {
                QQuickWindow *window = QQuickPopup::window();
                if (window)
                    QQuickWindowPrivate::get(window)->clearFocusInScope(d->contentItem, focusItem, Qt::OtherFocusReason);
            }
            d->setCurrentIndex(-1);
        }
    }
}

void QQuickMenu::keyReleaseEvent(QKeyEvent *event)
{
    Q_D(QQuickMenu);
    QQuickPopup::keyReleaseEvent(event);
    if (d->contentModel->count() == 0)
        return;

    // QTBUG-17051
    // Work around the fact that ListView has no way of distinguishing between
    // mouse and keyboard interaction, thanks to the "interactive" bool in Flickable.
    // What we actually want is to have a way to always allow keyboard interaction but
    // only allow flicking with the mouse when there are too many menu items to be
    // shown at once.
    switch (event->key()) {
    case Qt::Key_Up:
        if (d->contentItem->metaObject()->indexOfMethod("decrementCurrentIndex()") != -1)
            QMetaObject::invokeMethod(d->contentItem, "decrementCurrentIndex");
        break;

    case Qt::Key_Down:
        if (d->contentItem->metaObject()->indexOfMethod("incrementCurrentIndex()") != -1)
            QMetaObject::invokeMethod(d->contentItem, "incrementCurrentIndex");
        break;

    default:
        break;
    }

    int index = d->currentIndex();
    QQuickItem *item = itemAt(index);
    if (item)
        item->forceActiveFocus();
}

#ifndef QT_NO_ACCESSIBILITY
QAccessible::Role QQuickMenu::accessibleRole() const
{
    return QAccessible::PopupMenu;
}
#endif // QT_NO_ACCESSIBILITY

QT_END_NAMESPACE

#include "moc_qquickmenu_p.cpp"
