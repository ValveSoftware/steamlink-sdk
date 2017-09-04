/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#include "qquickmaterialtheme_p.h"

#include <QtGui/qpa/qplatformdialoghelper.h>
#include <QtGui/qfont.h>
#include <QtGui/qfontinfo.h>

QT_BEGIN_NAMESPACE

QQuickMaterialTheme::QQuickMaterialTheme(QPlatformTheme *theme)
    : QQuickProxyTheme(theme)
{
    QFont font;
    font.setFamily(QLatin1String("Roboto"));
    QString family = QFontInfo(font).family();

    if (family != QLatin1String("Roboto")) {
        font.setFamily(QLatin1String("Noto"));
        family = QFontInfo(font).family();
    }

    if (family == QLatin1String("Roboto") || family == QLatin1String("Noto")) {
        systemFont.setFamily(family);
        buttonFont.setFamily(family);
        toolTipFont.setFamily(family);
        itemViewFont.setFamily(family);
        listViewFont.setFamily(family);
        menuItemFont.setFamily(family);
        editorFont.setFamily(family);
    }

    systemFont.setPixelSize(14);

    buttonFont.setPixelSize(14);
    buttonFont.setCapitalization(QFont::AllUppercase);
    buttonFont.setWeight(QFont::Medium);

    toolTipFont.setPixelSize(14);
    toolTipFont.setWeight(QFont::Medium);

    itemViewFont.setPixelSize(14);
    itemViewFont.setWeight(QFont::Medium);

    listViewFont.setPixelSize(16);

    menuItemFont.setPixelSize(16);

    editorFont.setPixelSize(16);
}

const QFont *QQuickMaterialTheme::font(QPlatformTheme::Font type) const
{
    switch (type) {
    case QPlatformTheme::TabButtonFont:
    case QPlatformTheme::PushButtonFont:
    case QPlatformTheme::ToolButtonFont:
        return &buttonFont;
    case QPlatformTheme::TipLabelFont:
        return &toolTipFont;
    case QPlatformTheme::ItemViewFont:
        return &itemViewFont;
    case QPlatformTheme::ListViewFont:
        return &listViewFont;
    case QPlatformTheme::MenuItemFont:
    case QPlatformTheme::ComboMenuItemFont:
        return &menuItemFont;
    case QPlatformTheme::EditorFont:
        return &editorFont;
    default:
        return &systemFont;
    }
}

QVariant QQuickMaterialTheme::themeHint(ThemeHint hint) const
{
    switch (hint) {
    case QPlatformTheme::DialogButtonBoxLayout:
        // https://material.io/guidelines/components/dialogs.html#dialogs-specs
        // As per spec, affirmative actions are placed to the right, dismissive
        // actions are placed directly to the left of affirmative actions.
        // In the Android sources, there are additional type of actions -
        // neutral, which are placed to the left.
        // Rules for macOS seems to be the most suitable here and are also used
        // in the Android QPA plugin.
        return QVariant(QPlatformDialogHelper::MacLayout);
    default:
        return QQuickProxyTheme::themeHint(hint);
    }
}

QT_END_NAMESPACE
