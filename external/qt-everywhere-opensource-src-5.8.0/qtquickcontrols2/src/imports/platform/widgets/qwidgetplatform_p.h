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

#ifndef QWIDGETPLATFORM_P_H
#define QWIDGETPLATFORM_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/qdebug.h>
#include <QtCore/qcoreapplication.h>
#include <QtGui/qpa/qplatformtheme.h>
#include <QtGui/qpa/qplatformdialoghelper.h>
#include <QtGui/qpa/qplatformsystemtrayicon.h>
#include <QtGui/qpa/qplatformmenu.h>

#ifdef QT_WIDGETS_LIB
#include "qwidgetplatformcolordialog_p.h"
#include "qwidgetplatformfiledialog_p.h"
#include "qwidgetplatformfontdialog_p.h"
#include "qwidgetplatformmessagedialog_p.h"
#include "qwidgetplatformmenu_p.h"
#include "qwidgetplatformmenuitem_p.h"
#include "qwidgetplatformsystemtrayicon_p.h"
#endif

QT_BEGIN_NAMESPACE

#ifndef QT_WIDGETS_LIB
typedef QPlatformMenu QWidgetPlatformMenu;
typedef QPlatformMenuItem QWidgetPlatformMenuItem;
typedef QPlatformColorDialogHelper QWidgetPlatformColorDialog;
typedef QPlatformFileDialogHelper QWidgetPlatformFileDialog;
typedef QPlatformFontDialogHelper QWidgetPlatformFontDialog;
typedef QPlatformMessageDialogHelper QWidgetPlatformMessageDialog;
typedef QPlatformSystemTrayIcon QWidgetPlatformSystemTrayIcon;
#endif

namespace QWidgetPlatform
{
    static inline bool isAvailable(const char *type)
    {
        if (!qApp->inherits("QApplication")) {
            qCritical("\nERROR: No native %s implementation available."
                      "\nQt Labs Platform requires Qt Widgets on this setup."
                      "\nAdd 'QT += widgets' to .pro and create QApplication in main().\n", type);
            return false;
        }
        return true;
    }

    template<typename T>
    static inline T *createWidget(const char *name, QObject *parent)
    {
        static bool available = isAvailable(name);
#ifdef QT_WIDGETS_LIB
        if (available)
            return new T(parent);
#else
        Q_UNUSED(parent)
        Q_UNUSED(available)
#endif
        return nullptr;
    }

    static inline QPlatformMenu *createMenu(QObject *parent = nullptr) { return createWidget<QWidgetPlatformMenu>("Menu", parent); }
    static inline QPlatformMenuItem *createMenuItem(QObject *parent = nullptr) { return createWidget<QWidgetPlatformMenuItem>("MenuItem", parent); }
    static inline QPlatformSystemTrayIcon *createSystemTrayIcon(QObject *parent = nullptr) { return createWidget<QWidgetPlatformSystemTrayIcon>("SystemTrayIcon", parent); }
    static inline QPlatformDialogHelper *createDialog(QPlatformTheme::DialogType type, QObject *parent = nullptr)
    {
        switch (type) {
        case QPlatformTheme::ColorDialog: return createWidget<QWidgetPlatformColorDialog>("ColorDialog", parent);
        case QPlatformTheme::FileDialog: return createWidget<QWidgetPlatformFileDialog>("FileDialog", parent);
        case QPlatformTheme::FontDialog: return createWidget<QWidgetPlatformFontDialog>("FontDialog", parent);
        case QPlatformTheme::MessageDialog: return createWidget<QWidgetPlatformMessageDialog>("MessageDialog", parent);
        default: Q_UNREACHABLE(); break;
        return nullptr;
        }
    }
}

QT_END_NAMESPACE

#endif // QWIDGETPLATFORM_P_H
