/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Labs Templates module of the Qt Toolkit.
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

#include "qquickplatformmenuitemgroup_p.h"
#include "qquickplatformmenuitem_p.h"

QT_BEGIN_NAMESPACE

/*!
    \qmltype MenuItemGroup
    \inherits QtObject
    \instantiates QQuickPlatformMenuItemGroup
    \inqmlmodule Qt.labs.platform
    \since 5.8
    \brief A group for managing native menu items.

    The MenuItemGroup groups native menu items together.

    MenuItemGroup is exclusive by default. In an exclusive menu item
    group, only one item can be checked at any time; checking another
    item automatically unchecks the previously checked one. MenuItemGroup
    can be configured as non-exclusive, which is particularly useful for
    showing, hiding, enabling and disabling items together as a group.

    The most straight-forward way to use MenuItemGroup is to assign
    a list of items.

    \code
    Menu {
        id: verticalMenu
        title: qsTr("Vertical")

        MenuItemGroup {
            id: verticalGroup
            items: verticalMenu.items
        }

        MenuItem { text: qsTr("Top"); checkable: true }
        MenuItem { text: qsTr("Center"); checked: true }
        MenuItem { text: qsTr("Bottom"); checkable: true }
    }
    \endcode

    The same menu may sometimes contain items that should not be included
    in the same exclusive group. Such cases are best handled using the
    \l {MenuItem::group}{group} property.

    \code
    Menu {
        id: horizontalMenu
        title: qsTr("Horizontal")

        MenuItemGroup {
            id: horizontalGroup
        }

        MenuItem {
            checked: true
            text: qsTr("Left")
            group: horizontalGroup
        }
        MenuItem {
            checkable: true
            text: qsTr("Center")
            group: horizontalGroup
        }
        MenuItem {
            text: qsTr("Right")
            checkable: true
            group: horizontalGroup
        }

        MenuItem { separator: true }
        MenuItem { text: qsTr("Justify"); checkable: true }
        MenuItem { text: qsTr("Absolute"); checkable: true }
    }
    \endcode

    More advanced use cases can be handled using the addItem() and
    removeItem() methods.

    \labs

    \sa MenuItem
*/

/*!
    \qmlsignal Qt.labs.platform::MenuItemGroup::triggered(MenuItem item)

    This signal is emitted when an \a item in the group is triggered by the user.

    \sa MenuItem::triggered()
*/

/*!
    \qmlsignal Qt.labs.platform::MenuItemGroup::hovered(MenuItem item)

    This signal is emitted when an \a item in the group is hovered by the user.

    \sa MenuItem::hovered()
*/

QQuickPlatformMenuItemGroup::QQuickPlatformMenuItemGroup(QObject *parent)
    : QObject(parent), m_enabled(true), m_visible(true), m_exclusive(true), m_checkedItem(nullptr)
{
}

QQuickPlatformMenuItemGroup::~QQuickPlatformMenuItemGroup()
{
    clear();
}

/*!
    \qmlproperty bool Qt.labs.platform::MenuItemGroup::enabled

    This property holds whether the group is enabled. The default value is \c true.

    The enabled state of the group affects the enabled state of each item in the group,
    except that explicitly disabled items are not enabled even if the group is enabled.
*/
bool QQuickPlatformMenuItemGroup::isEnabled() const
{
    return m_enabled;
}

void QQuickPlatformMenuItemGroup::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;

    m_enabled = enabled;
    emit enabledChanged();

    for (QQuickPlatformMenuItem *item : m_items) {
        if (item->m_enabled) {
            item->sync();
            emit item->enabledChanged();
        }
    }
}

/*!
    \qmlproperty bool Qt.labs.platform::MenuItemGroup::visible

    This property holds whether the group is visible. The default value is \c true.

    The visibility of the group affects the visibility of each item in the group,
    except that explicitly hidden items are not visible even if the group is visible.
*/
bool QQuickPlatformMenuItemGroup::isVisible() const
{
    return m_visible;
}

void QQuickPlatformMenuItemGroup::setVisible(bool visible)
{
    if (m_visible == visible)
        return;

    m_visible = visible;
    emit visibleChanged();

    for (QQuickPlatformMenuItem *item : m_items) {
        if (item->m_visible) {
            item->sync();
            emit item->visibleChanged();
        }
    }
}

/*!
    \qmlproperty bool Qt.labs.platform::MenuItemGroup::exclusive

    This property holds whether the group is exclusive. The default value is \c true.

    In an exclusive menu item group, only one item can be checked at any time;
    checking another item automatically unchecks the previously checked one.
*/
bool QQuickPlatformMenuItemGroup::isExclusive() const
{
    return m_exclusive;
}

void QQuickPlatformMenuItemGroup::setExclusive(bool exclusive)
{
    if (m_exclusive == exclusive)
        return;

    m_exclusive = exclusive;
    emit exclusiveChanged();

    for (QQuickPlatformMenuItem *item : m_items)
        item->sync();
}

/*!
    \qmlproperty MenuItem Qt.labs.platform::MenuItemGroup::checkedItem

    This property holds the currently checked item in the group, or \c null if no item is checked.
*/
QQuickPlatformMenuItem *QQuickPlatformMenuItemGroup::checkedItem() const
{
    return m_checkedItem;
}

void QQuickPlatformMenuItemGroup::setCheckedItem(QQuickPlatformMenuItem *item)
{
    if (m_checkedItem == item)
        return;

    if (m_checkedItem)
        m_checkedItem->setChecked(false);

    m_checkedItem = item;
    emit checkedItemChanged();

    if (item)
        item->setChecked(true);
}

/*!
    \qmlproperty list<MenuItem> Qt.labs.platform::MenuItemGroup::items

    This property holds the list of items in the group.
*/
QQmlListProperty<QQuickPlatformMenuItem> QQuickPlatformMenuItemGroup::items()
{
    return QQmlListProperty<QQuickPlatformMenuItem>(this, nullptr, items_append, items_count, items_at, items_clear);
}

/*!
    \qmlmethod void Qt.labs.platform::MenuItemGroup::addItem(MenuItem item)

    Adds an \a item to the group.
*/
void QQuickPlatformMenuItemGroup::addItem(QQuickPlatformMenuItem *item)
{
    if (!item || m_items.contains(item))
        return;

    m_items.append(item);
    item->setGroup(this);

    connect(item, &QQuickPlatformMenuItem::checkedChanged, this, &QQuickPlatformMenuItemGroup::updateCurrent);
    connect(item, &QQuickPlatformMenuItem::triggered, this, &QQuickPlatformMenuItemGroup::activateItem);
    connect(item, &QQuickPlatformMenuItem::hovered, this, &QQuickPlatformMenuItemGroup::hoverItem);

    if (m_exclusive && item->isChecked())
        setCheckedItem(item);

    emit itemsChanged();
}

/*!
    \qmlmethod void Qt.labs.platform::MenuItemGroup::removeItem(MenuItem item)

    Removes an \a item from the group.
*/
void QQuickPlatformMenuItemGroup::removeItem(QQuickPlatformMenuItem *item)
{
    if (!item || !m_items.contains(item))
        return;

    m_items.removeOne(item);
    item->setGroup(nullptr);

    disconnect(item, &QQuickPlatformMenuItem::checkedChanged, this, &QQuickPlatformMenuItemGroup::updateCurrent);
    disconnect(item, &QQuickPlatformMenuItem::triggered, this, &QQuickPlatformMenuItemGroup::activateItem);
    disconnect(item, &QQuickPlatformMenuItem::hovered, this, &QQuickPlatformMenuItemGroup::hoverItem);

    if (m_checkedItem == item)
        setCheckedItem(nullptr);

    emit itemsChanged();
}

/*!
    \qmlmethod void Qt.labs.platform::MenuItemGroup::clear()

    Removes all items from the group.
*/
void QQuickPlatformMenuItemGroup::clear()
{
    if (m_items.isEmpty())
        return;

    for (QQuickPlatformMenuItem *item : m_items) {
        item->setGroup(nullptr);
        disconnect(item, &QQuickPlatformMenuItem::checkedChanged, this, &QQuickPlatformMenuItemGroup::updateCurrent);
        disconnect(item, &QQuickPlatformMenuItem::triggered, this, &QQuickPlatformMenuItemGroup::activateItem);
        disconnect(item, &QQuickPlatformMenuItem::hovered, this, &QQuickPlatformMenuItemGroup::hoverItem);
    }

    setCheckedItem(nullptr);

    m_items.clear();
    emit itemsChanged();
}

QQuickPlatformMenuItem *QQuickPlatformMenuItemGroup::findCurrent() const
{
    for (QQuickPlatformMenuItem *item : m_items) {
        if (item->isChecked())
            return item;
    }
    return nullptr;
}

void QQuickPlatformMenuItemGroup::updateCurrent()
{
    if (!m_exclusive)
        return;

    QQuickPlatformMenuItem *item = qobject_cast<QQuickPlatformMenuItem*>(sender());
    if (item && item->isChecked())
        setCheckedItem(item);
}

void QQuickPlatformMenuItemGroup::activateItem()
{
    QQuickPlatformMenuItem *item = qobject_cast<QQuickPlatformMenuItem*>(sender());
    if (item)
        emit triggered(item);
}

void QQuickPlatformMenuItemGroup::hoverItem()
{
    QQuickPlatformMenuItem *item = qobject_cast<QQuickPlatformMenuItem*>(sender());
    if (item)
        emit hovered(item);
}

void QQuickPlatformMenuItemGroup::items_append(QQmlListProperty<QQuickPlatformMenuItem> *prop, QQuickPlatformMenuItem *item)
{
    QQuickPlatformMenuItemGroup *group = static_cast<QQuickPlatformMenuItemGroup *>(prop->object);
    group->addItem(item);
}

int QQuickPlatformMenuItemGroup::items_count(QQmlListProperty<QQuickPlatformMenuItem> *prop)
{
    QQuickPlatformMenuItemGroup *group = static_cast<QQuickPlatformMenuItemGroup *>(prop->object);
    return group->m_items.count();
}

QQuickPlatformMenuItem *QQuickPlatformMenuItemGroup::items_at(QQmlListProperty<QQuickPlatformMenuItem> *prop, int index)
{
    QQuickPlatformMenuItemGroup *group = static_cast<QQuickPlatformMenuItemGroup *>(prop->object);
    return group->m_items.value(index);
}

void QQuickPlatformMenuItemGroup::items_clear(QQmlListProperty<QQuickPlatformMenuItem> *prop)
{
    QQuickPlatformMenuItemGroup *group = static_cast<QQuickPlatformMenuItemGroup *>(prop->object);
    group->clear();
}

QT_END_NAMESPACE
