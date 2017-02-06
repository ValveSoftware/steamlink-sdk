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

#include "qwidgetplatformmenuitem_p.h"

#include <QtWidgets/qmenu.h>
#include <QtWidgets/qaction.h>

QT_BEGIN_NAMESPACE

QWidgetPlatformMenuItem::QWidgetPlatformMenuItem(QObject *parent)
    : m_action(new QAction)
{
    setParent(parent);
    connect(m_action.data(), &QAction::hovered, this, &QPlatformMenuItem::hovered);
    connect(m_action.data(), &QAction::triggered, this, &QPlatformMenuItem::activated);
}

QWidgetPlatformMenuItem::~QWidgetPlatformMenuItem()
{
}

QAction *QWidgetPlatformMenuItem::action() const
{
    return m_action.data();
}

quintptr QWidgetPlatformMenuItem::tag() const
{
    return 0;
}

void QWidgetPlatformMenuItem::setTag(quintptr tag)
{
    Q_UNUSED(tag);
}

void QWidgetPlatformMenuItem::setText(const QString &text)
{
    m_action->setText(text);
}

void QWidgetPlatformMenuItem::setIcon(const QIcon &icon)
{
    m_action->setIcon(icon);
}

void QWidgetPlatformMenuItem::setMenu(QPlatformMenu *menu)
{
    m_action->setMenu(qobject_cast<QMenu *>(menu));
}

void QWidgetPlatformMenuItem::setVisible(bool visible)
{
    m_action->setVisible(visible);
}

void QWidgetPlatformMenuItem::setIsSeparator(bool separator)
{
    m_action->setSeparator(separator);
}

void QWidgetPlatformMenuItem::setFont(const QFont &font)
{
    m_action->setFont(font);
}

void QWidgetPlatformMenuItem::setRole(MenuRole role)
{
    m_action->setMenuRole(static_cast<QAction::MenuRole>(role));
}

void QWidgetPlatformMenuItem::setCheckable(bool checkable)
{
    m_action->setCheckable(checkable);
}

void QWidgetPlatformMenuItem::setChecked(bool checked)
{
    m_action->setChecked(checked);
}

void QWidgetPlatformMenuItem::setShortcut(const QKeySequence &shortcut)
{
    m_action->setShortcut(shortcut);
}

void QWidgetPlatformMenuItem::setEnabled(bool enabled)
{
    m_action->setEnabled(enabled);
}

void QWidgetPlatformMenuItem::setIconSize(int size)
{
    Q_UNUSED(size);
}

QT_END_NAMESPACE
