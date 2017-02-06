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

#include <QtQml/qqmlextensionplugin.h>
#include <QtQml/qqml.h>
#include <QtCore/qloggingcategory.h>

#include "qquickplatformdialog_p.h"
#include "qquickplatformcolordialog_p.h"
#include "qquickplatformfiledialog_p.h"
#include "qquickplatformfolderdialog_p.h"
#include "qquickplatformfontdialog_p.h"
#include "qquickplatformmessagedialog_p.h"

#include "qquickplatformmenu_p.h"
#include "qquickplatformmenubar_p.h"
#include "qquickplatformmenuitem_p.h"
#include "qquickplatformmenuitemgroup_p.h"
#include "qquickplatformmenuseparator_p.h"

#include "qquickplatformstandardpaths_p.h"

#include "qquickplatformsystemtrayicon_p.h"

Q_DECLARE_METATYPE(QStandardPaths::StandardLocation)
Q_DECLARE_METATYPE(QStandardPaths::LocateOptions)

static inline void initResources()
{
#ifdef QT_STATIC
    Q_INIT_RESOURCE(qmake_Qt_labs_platform);
#endif
}

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(qtLabsPlatformDialogs, "qt.labs.platform.dialogs")
Q_LOGGING_CATEGORY(qtLabsPlatformMenus, "qt.labs.platform.menus")
Q_LOGGING_CATEGORY(qtLabsPlatformTray, "qt.labs.platform.tray")

class QtLabsPlatformPlugin: public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

public:
    QtLabsPlatformPlugin(QObject *parent = nullptr);
    void registerTypes(const char *uri) override;
};

QtLabsPlatformPlugin::QtLabsPlatformPlugin(QObject *parent) : QQmlExtensionPlugin(parent)
{
    initResources();
}

void QtLabsPlatformPlugin::registerTypes(const char *uri)
{
    qmlRegisterUncreatableType<QQuickPlatformDialog>(uri, 1, 0, "Dialog", QQuickPlatformDialog::tr("Dialog is an abstract base class"));
    qmlRegisterType<QQuickPlatformColorDialog>(uri, 1, 0, "ColorDialog");
    qmlRegisterType<QQuickPlatformFileDialog>(uri, 1, 0, "FileDialog");
    qmlRegisterType<QQuickPlatformFileNameFilter>();
    qmlRegisterType<QQuickPlatformFolderDialog>(uri, 1, 0, "FolderDialog");
    qmlRegisterType<QQuickPlatformFontDialog>(uri, 1, 0, "FontDialog");
    qmlRegisterType<QQuickPlatformMessageDialog>(uri, 1, 0, "MessageDialog");

    qmlRegisterType<QQuickPlatformMenu>(uri, 1, 0, "Menu");
    qmlRegisterType<QQuickPlatformMenuBar>(uri, 1, 0, "MenuBar");
    qmlRegisterType<QQuickPlatformMenuItem>(uri, 1, 0, "MenuItem");
    qmlRegisterType<QQuickPlatformMenuItemGroup>(uri, 1, 0, "MenuItemGroup");
    qmlRegisterType<QQuickPlatformMenuSeparator>(uri, 1, 0, "MenuSeparator");

    qmlRegisterUncreatableType<QPlatformDialogHelper>(uri, 1, 0, "StandardButton", QQuickPlatformDialog::tr("Cannot create an instance of StandardButton"));
    qmlRegisterSingletonType<QQuickPlatformStandardPaths>(uri, 1, 0, "StandardPaths", QQuickPlatformStandardPaths::create);
    qRegisterMetaType<QStandardPaths::StandardLocation>();
    qRegisterMetaType<QStandardPaths::LocateOptions>();

    qmlRegisterType<QQuickPlatformSystemTrayIcon>(uri, 1, 0, "SystemTrayIcon");
}

QT_END_NAMESPACE

#include "qtlabsplatformplugin.moc"
