/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls 2 module of the Qt Toolkit.
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

#include "qquickproxytheme_p.h"

#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/qpixmap.h>
#include <QtGui/qfont.h>

QT_BEGIN_NAMESPACE

QQuickProxyTheme::QQuickProxyTheme(QPlatformTheme *theme)
    : m_theme(theme ? theme : QGuiApplicationPrivate::platform_theme)
{
}

QQuickProxyTheme::~QQuickProxyTheme()
{
    if (QGuiApplicationPrivate::platform_theme == this)
        QGuiApplicationPrivate::platform_theme = m_theme;
}

QPlatformTheme *QQuickProxyTheme::theme() const
{
    return m_theme;
}

QPlatformMenuItem *QQuickProxyTheme::createPlatformMenuItem() const
{
    if (m_theme)
        return m_theme->createPlatformMenuItem();
    return QPlatformTheme::createPlatformMenuItem();
}

QPlatformMenu *QQuickProxyTheme::createPlatformMenu() const
{
    if (m_theme)
        return m_theme->createPlatformMenu();
    return QPlatformTheme::createPlatformMenu();
}

QPlatformMenuBar *QQuickProxyTheme::createPlatformMenuBar() const
{
    if (m_theme)
        return m_theme->createPlatformMenuBar();
    return QPlatformTheme::createPlatformMenuBar();
}

void QQuickProxyTheme::showPlatformMenuBar()
{
    if (m_theme)
        m_theme->showPlatformMenuBar();
    QPlatformTheme::showPlatformMenuBar();
}

bool QQuickProxyTheme::usePlatformNativeDialog(QPlatformTheme::DialogType type) const
{
    if (m_theme)
        return m_theme->usePlatformNativeDialog(type);
    return QPlatformTheme::usePlatformNativeDialog(type);
}

QPlatformDialogHelper *QQuickProxyTheme::createPlatformDialogHelper(QPlatformTheme::DialogType type) const
{
    if (m_theme)
        return m_theme->createPlatformDialogHelper(type);
    return QPlatformTheme::createPlatformDialogHelper(type);
}

#ifndef QT_NO_SYSTEMTRAYICON
QPlatformSystemTrayIcon *QQuickProxyTheme::createPlatformSystemTrayIcon() const
{
    if (m_theme)
        return m_theme->createPlatformSystemTrayIcon();
    return QPlatformTheme::createPlatformSystemTrayIcon();
}
#endif

const QPalette *QQuickProxyTheme::palette(QPlatformTheme::Palette type) const
{
    if (m_theme)
        return m_theme->palette(type);
    return QPlatformTheme::palette(type);
}

const QFont *QQuickProxyTheme::font(QPlatformTheme::Font type) const
{
    if (m_theme)
        return m_theme->font(type);
    return QPlatformTheme::font(type);
}

QVariant QQuickProxyTheme::themeHint(QPlatformTheme::ThemeHint hint) const
{
    if (m_theme)
        return m_theme->themeHint(hint);
    return QPlatformTheme::themeHint(hint);
}

QPixmap QQuickProxyTheme::standardPixmap(QPlatformTheme::StandardPixmap sp, const QSizeF &size) const
{
    if (m_theme)
        return m_theme->standardPixmap(sp, size);
    return QPlatformTheme::standardPixmap(sp, size);
}

QIcon QQuickProxyTheme::fileIcon(const QFileInfo &fileInfo, QPlatformTheme::IconOptions iconOptions) const
{
    if (m_theme)
        return m_theme->fileIcon(fileInfo, iconOptions);
    return QPlatformTheme::fileIcon(fileInfo, iconOptions);
}

QIconEngine *QQuickProxyTheme::createIconEngine(const QString &iconName) const
{
    if (m_theme)
        return m_theme->createIconEngine(iconName);
    return QPlatformTheme::createIconEngine(iconName);
}

QList<QKeySequence> QQuickProxyTheme::keyBindings(QKeySequence::StandardKey key) const
{
    if (m_theme)
        return m_theme->keyBindings(key);
    return QPlatformTheme::keyBindings(key);
}

QString QQuickProxyTheme::standardButtonText(int button) const
{
    if (m_theme)
        return m_theme->standardButtonText(button);
    return QPlatformTheme::standardButtonText(button);
}

QT_END_NAMESPACE
