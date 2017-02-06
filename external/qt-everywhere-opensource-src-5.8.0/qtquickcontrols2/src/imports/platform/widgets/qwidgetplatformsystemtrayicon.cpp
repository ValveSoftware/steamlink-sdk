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

#include "qwidgetplatformsystemtrayicon_p.h"
#include "qwidgetplatformmenu_p.h"

#include <QtWidgets/qsystemtrayicon.h>

QT_BEGIN_NAMESPACE

QWidgetPlatformSystemTrayIcon::QWidgetPlatformSystemTrayIcon(QObject *parent)
    : m_systray(new QSystemTrayIcon)
{
    setParent(parent);

    connect(m_systray.data(), &QSystemTrayIcon::messageClicked, this, &QPlatformSystemTrayIcon::messageClicked);
    connect(m_systray.data(), &QSystemTrayIcon::activated, [this](QSystemTrayIcon::ActivationReason reason) {
        emit activated(static_cast<ActivationReason>(reason));
    });
}

QWidgetPlatformSystemTrayIcon::~QWidgetPlatformSystemTrayIcon()
{
}

void QWidgetPlatformSystemTrayIcon::init()
{
    m_systray->show();
}

void QWidgetPlatformSystemTrayIcon::cleanup()
{
    m_systray->hide();
}

void QWidgetPlatformSystemTrayIcon::updateIcon(const QIcon &icon)
{
    m_systray->setIcon(icon);
}

void QWidgetPlatformSystemTrayIcon::updateToolTip(const QString &tooltip)
{
    m_systray->setToolTip(tooltip);
}

void QWidgetPlatformSystemTrayIcon::updateMenu(QPlatformMenu *menu)
{
    QWidgetPlatformMenu *widgetMenu = qobject_cast<QWidgetPlatformMenu *>(menu);
    if (!widgetMenu)
        return;

    m_systray->setContextMenu(widgetMenu->menu());
}

QRect QWidgetPlatformSystemTrayIcon::geometry() const
{
    return m_systray->geometry();
}

void QWidgetPlatformSystemTrayIcon::showMessage(const QString &title, const QString &msg, const QIcon &icon, MessageIcon iconType, int msecs)
{
    Q_UNUSED(icon);
    m_systray->showMessage(title, msg, static_cast<QSystemTrayIcon::MessageIcon>(iconType), msecs);
}

bool QWidgetPlatformSystemTrayIcon::isSystemTrayAvailable() const
{
    return QSystemTrayIcon::isSystemTrayAvailable();
}

bool QWidgetPlatformSystemTrayIcon::supportsMessages() const
{
    return QSystemTrayIcon::supportsMessages();
}

QPlatformMenu *QWidgetPlatformSystemTrayIcon::createMenu() const
{
    return new QWidgetPlatformMenu;
}

QT_END_NAMESPACE
