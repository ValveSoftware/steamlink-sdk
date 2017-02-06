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

#include "qquickcontainer_p.h"
#include "qquickcontainer_p_p.h"

#include <QtQuick/private/qquickflickable_p.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype Container
    \inherits Control
    \instantiates QQuickContainer
    \inqmlmodule QtQuick.Controls
    \since 5.7
    \ingroup qtquickcontrols2-containers
    \brief Abstract base type providing functionality common to containers.

    Container is the base type of container-like user interface controls that
    allow dynamic insertion and removal of items.

    \section2 Using Containers

    Typically, items are statically declared as children of Container, but it
    is also possible to \l {addItem}{add}, \l {insertItem}{insert},
    \l {moveItem}{move} and \l {removeItem}{remove} items dynamically. The
    items in a container can be accessed using \l itemAt() or
    \l contentChildren.

    Most containers have the concept of a "current" item. The current item is
    specified via the \l currentIndex property, and can be accessed using the
    read-only \l currentItem property.

    The following example illustrates dynamic insertion of items to a \l TabBar,
    which is one of the concrete implementations of Container.

    \code
    Row {
        TabBar {
            id: tabBar

            currentIndex: 0
            width: parent.width - addButton.width

            TabButton { text: "TabButton" }
        }

        Component {
            id: tabButton
            TabButton { text: "TabButton" }
        }

        Button {
            id: addButton
            text: "+"
            flat: true
            onClicked: {
                tabBar.addItem(tabButton.createObject(tabBar))
                console.log("added:", tabBar.itemAt(tabBar.count - 1))
            }
        }
    }
    \endcode

    \section2 Implementing Containers

    Container does not provide any default visualization. It is used to implement
    such containers as \l SwipeView and \l TabBar. When implementing a custom
    container, the most important part of the API is \l contentModel, which provides
    the contained items in a way that it can be used as a delegate model for item
    views and repeaters.

    \code
    Container {
        id: container

        contentItem: ListView {
            model: container.contentModel
            snapMode: ListView.SnapOneItem
            orientation: ListView.Horizontal
        }

        Text {
            text: "Page 1"
            width: container.width
            height: container.height
        }

        Text {
            text: "Page 2"
            width: container.width
            height: container.height
        }
    }
    \endcode

    Notice how the sizes of the page items are set by hand. This is because the
    example uses a plain Container, which does not make any assumptions on the
    visual layout. It is typically not necessary to specify sizes for items in
    concrete Container implementations, such as \l SwipeView and \l TabBar.

    \sa {Container Controls}
*/

static QQuickItem *effectiveContentItem(QQuickItem *item)
{
    QQuickFlickable *flickable = qobject_cast<QQuickFlickable *>(item);
    if (flickable)
        return flickable->contentItem();
    return item;
}

QQuickContainerPrivate::QQuickContainerPrivate() : contentModel(nullptr), currentIndex(-1), updatingCurrent(false),
    changeTypes(Destroyed | Parent | SiblingOrder)
{
}

void QQuickContainerPrivate::init()
{
    Q_Q(QQuickContainer);
    contentModel = new QQmlObjectModel(q);
    QObject::connect(contentModel, &QQmlObjectModel::countChanged, q, &QQuickContainer::countChanged);
    QObject::connect(contentModel, &QQmlObjectModel::childrenChanged, q, &QQuickContainer::contentChildrenChanged);
}

void QQuickContainerPrivate::cleanup()
{
    Q_Q(QQuickContainer);
    // ensure correct destruction order (QTBUG-46798)
    const int count = contentModel->count();
    for (int i = 0; i < count; ++i) {
        QQuickItem *item = itemAt(i);
        if (item)
            QQuickItemPrivate::get(item)->removeItemChangeListener(this, changeTypes);
    }

    if (contentItem) {
        QQuickItem *focusItem = QQuickItemPrivate::get(contentItem)->subFocusItem;
        if (focusItem && window)
            QQuickWindowPrivate::get(window)->clearFocusInScope(contentItem, focusItem, Qt::OtherFocusReason);

        q->contentItemChange(nullptr, contentItem);
        delete contentItem;
    }

    QObject::disconnect(contentModel, &QQmlObjectModel::countChanged, q, &QQuickContainer::countChanged);
    QObject::disconnect(contentModel, &QQmlObjectModel::childrenChanged, q, &QQuickContainer::contentChildrenChanged);
    delete contentModel;
}

QQuickItem *QQuickContainerPrivate::itemAt(int index) const
{
    return qobject_cast<QQuickItem *>(contentModel->get(index));
}

void QQuickContainerPrivate::insertItem(int index, QQuickItem *item)
{
    Q_Q(QQuickContainer);
    if (!q->isContent(item))
        return;
    contentData.append(item);

    updatingCurrent = true;

    item->setParentItem(effectiveContentItem(contentItem));
    QQuickItemPrivate::get(item)->addItemChangeListener(this, changeTypes);
    contentModel->insert(index, item);

    q->itemAdded(index, item);

    if (contentModel->count() == 1 && currentIndex == -1)
        q->setCurrentIndex(index);

    updatingCurrent = false;
}

void QQuickContainerPrivate::moveItem(int from, int to)
{
    Q_Q(QQuickContainer);
    int oldCurrent = currentIndex;
    contentModel->move(from, to);

    updatingCurrent = true;

    if (from == oldCurrent)
        q->setCurrentIndex(to);
    else if (from < oldCurrent && to >= oldCurrent)
        q->setCurrentIndex(oldCurrent - 1);
    else if (from > oldCurrent && to <= oldCurrent)
        q->setCurrentIndex(oldCurrent + 1);

    updatingCurrent = false;
}

void QQuickContainerPrivate::removeItem(int index, QQuickItem *item)
{
    Q_Q(QQuickContainer);
    if (!q->isContent(item))
        return;
    contentData.removeOne(item);

    updatingCurrent = true;

    bool currentChanged = false;
    if (index == currentIndex) {
        q->setCurrentIndex(currentIndex - 1);
    } else if (index < currentIndex) {
        --currentIndex;
        currentChanged = true;
    }

    QQuickItemPrivate::get(item)->removeItemChangeListener(this, changeTypes);
    item->setParentItem(nullptr);
    contentModel->remove(index);

    q->itemRemoved(index, item);

    if (currentChanged)
        emit q->currentIndexChanged();

    updatingCurrent = false;
}

void QQuickContainerPrivate::_q_currentIndexChanged()
{
    Q_Q(QQuickContainer);
    if (!updatingCurrent)
        q->setCurrentIndex(contentItem ? contentItem->property("currentIndex").toInt() : -1);
}

void QQuickContainerPrivate::itemChildAdded(QQuickItem *, QQuickItem *child)
{
    // add dynamically reparented items (eg. by a Repeater)
    if (!QQuickItemPrivate::get(child)->isTransparentForPositioner() && !contentData.contains(child))
        insertItem(contentModel->count(), child);
}

void QQuickContainerPrivate::itemParentChanged(QQuickItem *item, QQuickItem *parent)
{
    // remove dynamically unparented items (eg. by a Repeater)
    if (!parent)
        removeItem(contentModel->indexOf(item, nullptr), item);
}

void QQuickContainerPrivate::itemSiblingOrderChanged(QQuickItem *)
{
    // reorder the restacked items (eg. by a Repeater)
    Q_Q(QQuickContainer);
    QList<QQuickItem *> siblings = effectiveContentItem(contentItem)->childItems();
    for (int i = 0; i < siblings.count(); ++i) {
        QQuickItem* sibling = siblings.at(i);
        int index = contentModel->indexOf(sibling, nullptr);
        q->moveItem(index, i);
    }
}

void QQuickContainerPrivate::itemDestroyed(QQuickItem *item)
{
    int index = contentModel->indexOf(item, nullptr);
    if (index != -1)
        removeItem(index, item);
}

void QQuickContainerPrivate::contentData_append(QQmlListProperty<QObject> *prop, QObject *obj)
{
    QQuickContainerPrivate *p = static_cast<QQuickContainerPrivate *>(prop->data);
    QQuickContainer *q = static_cast<QQuickContainer *>(prop->object);
    QQuickItem *item = qobject_cast<QQuickItem *>(obj);
    if (item) {
        if (QQuickItemPrivate::get(item)->isTransparentForPositioner())
            item->setParentItem(effectiveContentItem(p->contentItem));
        else if (p->contentModel->indexOf(item, nullptr) == -1)
            q->addItem(item);
    } else {
        p->contentData.append(obj);
    }
}

int QQuickContainerPrivate::contentData_count(QQmlListProperty<QObject> *prop)
{
    QQuickContainerPrivate *p = static_cast<QQuickContainerPrivate *>(prop->data);
    return p->contentData.count();
}

QObject *QQuickContainerPrivate::contentData_at(QQmlListProperty<QObject> *prop, int index)
{
    QQuickContainerPrivate *p = static_cast<QQuickContainerPrivate *>(prop->data);
    return p->contentData.value(index);
}

void QQuickContainerPrivate::contentData_clear(QQmlListProperty<QObject> *prop)
{
    QQuickContainerPrivate *p = static_cast<QQuickContainerPrivate *>(prop->data);
    p->contentData.clear();
}

void QQuickContainerPrivate::contentChildren_append(QQmlListProperty<QQuickItem> *prop, QQuickItem *item)
{
    QQuickContainer *q = static_cast<QQuickContainer *>(prop->object);
    q->addItem(item);
}

int QQuickContainerPrivate::contentChildren_count(QQmlListProperty<QQuickItem> *prop)
{
    QQuickContainerPrivate *p = static_cast<QQuickContainerPrivate *>(prop->data);
    return p->contentModel->count();
}

QQuickItem *QQuickContainerPrivate::contentChildren_at(QQmlListProperty<QQuickItem> *prop, int index)
{
    QQuickContainer *q = static_cast<QQuickContainer *>(prop->object);
    return q->itemAt(index);
}

void QQuickContainerPrivate::contentChildren_clear(QQmlListProperty<QQuickItem> *prop)
{
    QQuickContainerPrivate *p = static_cast<QQuickContainerPrivate *>(prop->data);
    p->contentModel->clear();
}

QQuickContainer::QQuickContainer(QQuickItem *parent) :
    QQuickControl(*(new QQuickContainerPrivate), parent)
{
    Q_D(QQuickContainer);
    d->init();
}

QQuickContainer::QQuickContainer(QQuickContainerPrivate &dd, QQuickItem *parent) :
    QQuickControl(dd, parent)
{
    Q_D(QQuickContainer);
    d->init();
}

QQuickContainer::~QQuickContainer()
{
    Q_D(QQuickContainer);
    d->cleanup();
}

/*!
    \qmlproperty int QtQuick.Controls::Container::count
    \readonly

    This property holds the number of items.
*/
int QQuickContainer::count() const
{
    Q_D(const QQuickContainer);
    return d->contentModel->count();
}

/*!
    \qmlmethod Item QtQuick.Controls::Container::itemAt(int index)

    Returns the item at \a index, or \c null if it does not exist.
*/
QQuickItem *QQuickContainer::itemAt(int index) const
{
    Q_D(const QQuickContainer);
    return d->itemAt(index);
}

/*!
    \qmlmethod void QtQuick.Controls::Container::addItem(Item item)

    Adds an \a item.
*/
void QQuickContainer::addItem(QQuickItem *item)
{
    Q_D(QQuickContainer);
    insertItem(d->contentModel->count(), item);
}

/*!
    \qmlmethod void QtQuick.Controls::Container::insertItem(int index, Item item)

    Inserts an \a item at \a index.
*/
void QQuickContainer::insertItem(int index, QQuickItem *item)
{
    Q_D(QQuickContainer);
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
    \qmlmethod void QtQuick.Controls::Container::moveItem(int from, int to)

    Moves an item \a from one index \a to another.
*/
void QQuickContainer::moveItem(int from, int to)
{
    Q_D(QQuickContainer);
    const int count = d->contentModel->count();
    if (from < 0 || from > count - 1)
        return;
    if (to < 0 || to > count - 1)
        to = count - 1;

    if (from != to)
        d->moveItem(from, to);
}

/*!
    \qmlmethod void QtQuick.Controls::Container::removeItem(int index)

    Removes an item at \a index.

    \note The ownership of the item is transferred to the caller.
*/
void QQuickContainer::removeItem(int index)
{
    Q_D(QQuickContainer);
    const int count = d->contentModel->count();
    if (index < 0 || index >= count)
        return;

    QQuickItem *item = itemAt(index);
    if (item)
        d->removeItem(index, item);
}

/*!
    \qmlproperty model QtQuick.Controls::Container::contentModel
    \readonly

    This property holds the content model of items.

    The content model is provided for visualization purposes. It can be assigned
    as a model to a content item that presents the contents of the container.

    \code
    Container {
        id: container
        contentItem: ListView {
            model: container.contentModel
        }
    }
    \endcode

    \sa contentData, contentChildren
*/
QVariant QQuickContainer::contentModel() const
{
    Q_D(const QQuickContainer);
    return QVariant::fromValue(d->contentModel);
}

/*!
    \qmlproperty list<Object> QtQuick.Controls::Container::contentData
    \default

    This property holds the list of content data.

    The list contains all objects that have been declared in QML as children
    of the container, and also items that have been dynamically added or
    inserted using the \l addItem() and \l insertItem() methods, respectively.

    \note Unlike \c contentChildren, \c contentData does include non-visual QML
    objects. It is not re-ordered when items are inserted or moved.

    \sa Item::data, contentChildren
*/
QQmlListProperty<QObject> QQuickContainer::contentData()
{
    Q_D(QQuickContainer);
    return QQmlListProperty<QObject>(this, d,
                                     QQuickContainerPrivate::contentData_append,
                                     QQuickContainerPrivate::contentData_count,
                                     QQuickContainerPrivate::contentData_at,
                                     QQuickContainerPrivate::contentData_clear);
}

/*!
    \qmlproperty list<Item> QtQuick.Controls::Container::contentChildren

    This property holds the list of content children.

    The list contains all items that have been declared in QML as children
    of the container, and also items that have been dynamically added or
    inserted using the \l addItem() and \l insertItem() methods, respectively.

    \note Unlike \c contentData, \c contentChildren does not include non-visual
    QML objects. It is re-ordered when items are inserted or moved.

    \sa Item::children, contentData
*/
QQmlListProperty<QQuickItem> QQuickContainer::contentChildren()
{
    Q_D(QQuickContainer);
    return QQmlListProperty<QQuickItem>(this, d,
                                        QQuickContainerPrivate::contentChildren_append,
                                        QQuickContainerPrivate::contentChildren_count,
                                        QQuickContainerPrivate::contentChildren_at,
                                        QQuickContainerPrivate::contentChildren_clear);
}

/*!
    \qmlproperty int QtQuick.Controls::Container::currentIndex

    This property holds the index of the current item.

    \sa currentItem, incrementCurrentIndex(), decrementCurrentIndex()
*/
int QQuickContainer::currentIndex() const
{
    Q_D(const QQuickContainer);
    return d->currentIndex;
}

void QQuickContainer::setCurrentIndex(int index)
{
    Q_D(QQuickContainer);
    if (d->currentIndex == index)
        return;

    d->currentIndex = index;
    emit currentIndexChanged();
    emit currentItemChanged();
}

/*!
    \qmlmethod void QtQuick.Controls::Container::incrementCurrentIndex()
    \since QtQuick.Controls 2.1

    Increments the current index of the container.

    \sa currentIndex
*/
void QQuickContainer::incrementCurrentIndex()
{
    Q_D(QQuickContainer);
    if (d->currentIndex < count() - 1)
        setCurrentIndex(d->currentIndex + 1);
}

/*!
    \qmlmethod void QtQuick.Controls::Container::decrementCurrentIndex()
    \since QtQuick.Controls 2.1

    Decrements the current index of the container.

    \sa currentIndex
*/
void QQuickContainer::decrementCurrentIndex()
{
    Q_D(QQuickContainer);
    if (d->currentIndex > 0)
        setCurrentIndex(d->currentIndex - 1);
}

/*!
    \qmlproperty Item QtQuick.Controls::Container::currentItem
    \readonly

    This property holds the current item.

    \sa currentIndex
*/
QQuickItem *QQuickContainer::currentItem() const
{
    Q_D(const QQuickContainer);
    return itemAt(d->currentIndex);
}

void QQuickContainer::itemChange(ItemChange change, const ItemChangeData &data)
{
    Q_D(QQuickContainer);
    QQuickControl::itemChange(change, data);
    if (change == QQuickItem::ItemChildAddedChange && isComponentComplete() && data.item != d->background && data.item != d->contentItem) {
        if (!QQuickItemPrivate::get(data.item)->isTransparentForPositioner() && d->contentModel->indexOf(data.item, nullptr) == -1)
            addItem(data.item);
    }
}

void QQuickContainer::contentItemChange(QQuickItem *newItem, QQuickItem *oldItem)
{
    Q_D(QQuickContainer);
    QQuickControl::contentItemChange(newItem, oldItem);

    static const int slotIndex = metaObject()->indexOfSlot("_q_currentIndexChanged()");

    if (oldItem) {
        QQuickItemPrivate::get(oldItem)->removeItemChangeListener(d, QQuickItemPrivate::Children);
        QQuickItem *oldContentItem = effectiveContentItem(oldItem);
        if (oldContentItem != oldItem)
            QQuickItemPrivate::get(oldContentItem)->removeItemChangeListener(d, QQuickItemPrivate::Children);

        int signalIndex = oldItem->metaObject()->indexOfSignal("currentIndexChanged()");
        if (signalIndex != -1)
            QMetaObject::disconnect(oldItem, signalIndex, this, slotIndex);
    }

    if (newItem) {
        QQuickItemPrivate::get(newItem)->addItemChangeListener(d, QQuickItemPrivate::Children);
        QQuickItem *newContentItem = effectiveContentItem(newItem);
        if (newContentItem != newItem)
            QQuickItemPrivate::get(newContentItem)->addItemChangeListener(d, QQuickItemPrivate::Children);

        int signalIndex = newItem->metaObject()->indexOfSignal("currentIndexChanged()");
        if (signalIndex != -1)
            QMetaObject::connect(newItem, signalIndex, this, slotIndex);
    }
}

bool QQuickContainer::isContent(QQuickItem *item) const
{
    // If the item has a QML context associated to it (it was created in QML),
    // we add it to the content model. Otherwise, it's probably the default
    // highlight item that is always created by the item views, which we need
    // to exclude.
    //
    // TODO: Find a better way to identify/exclude the highlight item...
    return qmlContext(item);
}

void QQuickContainer::itemAdded(int index, QQuickItem *item)
{
    Q_UNUSED(index);
    Q_UNUSED(item);
}

void QQuickContainer::itemRemoved(int index, QQuickItem *item)
{
    Q_UNUSED(index);
    Q_UNUSED(item);
}

QT_END_NAMESPACE

#include "moc_qquickcontainer_p.cpp"
