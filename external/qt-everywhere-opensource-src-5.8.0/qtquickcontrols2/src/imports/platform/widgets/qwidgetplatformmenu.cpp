/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Labs Platform module of the Qt Toolkit.
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

#include "qwidgetplatformmenu_p.h"
#include "qwidgetplatformmenuitem_p.h"

#include <QtGui/qwindow.h>
#include <QtWidgets/qmenu.h>
#include <QtWidgets/qaction.h>

QT_BEGIN_NAMESPACE

QWidgetPlatformMenu::QWidgetPlatformMenu(QObject *parent)
    : m_menu(new QMenu)
{
    setParent(parent);

    connect(m_menu.data(), &QMenu::aboutToShow, this, &QPlatformMenu::aboutToShow);
    connect(m_menu.data(), &QMenu::aboutToHide, this, &QPlatformMenu::aboutToHide);
}

QWidgetPlatformMenu::~QWidgetPlatformMenu()
{
}

QMenu *QWidgetPlatformMenu::menu() const
{
    return m_menu.data();
}

void QWidgetPlatformMenu::insertMenuItem(QPlatformMenuItem *item, QPlatformMenuItem *before)
{
    QWidgetPlatformMenuItem *widgetItem = qobject_cast<QWidgetPlatformMenuItem *>(item);
    if (!widgetItem)
        return;

    QWidgetPlatformMenuItem *widgetBefore = qobject_cast<QWidgetPlatformMenuItem *>(before);
    m_menu->insertAction(widgetBefore ? widgetBefore->action() : nullptr, widgetItem->action());
}

void QWidgetPlatformMenu::removeMenuItem(QPlatformMenuItem *item)
{
    QWidgetPlatformMenuItem *widgetItem = qobject_cast<QWidgetPlatformMenuItem *>(item);
    if (!widgetItem)
        return;

    m_menu->removeAction(widgetItem->action());
}

void QWidgetPlatformMenu::syncMenuItem(QPlatformMenuItem *item)
{
    Q_UNUSED(item);
}

void QWidgetPlatformMenu::syncSeparatorsCollapsible(bool enable)
{
    Q_UNUSED(enable);
}

quintptr QWidgetPlatformMenu::tag() const
{
    return 0;
}

void QWidgetPlatformMenu::setTag(quintptr tag)
{
    Q_UNUSED(tag);
}

void QWidgetPlatformMenu::setText(const QString &text)
{
    m_menu->setTitle(text);
}

void QWidgetPlatformMenu::setIcon(const QIcon &icon)
{
    m_menu->setIcon(icon);
}

void QWidgetPlatformMenu::setEnabled(bool enabled)
{
    m_menu->menuAction()->setEnabled(enabled);
}

bool QWidgetPlatformMenu::isEnabled() const
{
    return m_menu->menuAction()->isEnabled();
}

void QWidgetPlatformMenu::setVisible(bool visible)
{
    m_menu->menuAction()->setVisible(visible);
}

void QWidgetPlatformMenu::setMinimumWidth(int width)
{
    if (width > 0)
        m_menu->setMinimumWidth(width);
}

void QWidgetPlatformMenu::setFont(const QFont &font)
{
    m_menu->setFont(font);
}

void QWidgetPlatformMenu::setMenuType(MenuType type)
{
    Q_UNUSED(type);
}

void QWidgetPlatformMenu::showPopup(const QWindow *window, const QRect &targetRect, const QPlatformMenuItem *item)
{
    m_menu->createWinId();
    QWindow *handle = m_menu->windowHandle();
    Q_ASSERT(handle);
    handle->setTransientParent(const_cast<QWindow *>(window));

    QPoint targetPos = targetRect.bottomLeft();
    if (window)
        targetPos = window->mapToGlobal(targetPos);

    const QWidgetPlatformMenuItem *widgetItem = qobject_cast<const QWidgetPlatformMenuItem *>(item);
    m_menu->popup(targetPos, widgetItem ? widgetItem->action() : nullptr);
}

void QWidgetPlatformMenu::dismiss()
{
    m_menu->close();
}

QPlatformMenuItem *QWidgetPlatformMenu::menuItemAt(int position) const
{
    Q_UNUSED(position);
    return nullptr;
}

QPlatformMenuItem *QWidgetPlatformMenu::menuItemForTag(quintptr tag) const
{
    Q_UNUSED(tag);
    return nullptr;
}

QPlatformMenuItem *QWidgetPlatformMenu::createMenuItem() const
{
    return nullptr;
}

QPlatformMenu *QWidgetPlatformMenu::createSubMenu() const
{
    return nullptr;
}

QT_END_NAMESPACE
