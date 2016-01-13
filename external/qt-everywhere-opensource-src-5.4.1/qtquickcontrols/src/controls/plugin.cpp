/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "plugin.h"

#include "qquickaction_p.h"
#include "qquickexclusivegroup_p.h"
#include "qquickmenu_p.h"
#include "qquickmenubar_p.h"
#include "qquickpopupwindow_p.h"
#include "qquickstack_p.h"
#include "qquickdesktopiconprovider_p.h"
#include "qquickselectionmode_p.h"

#include "Private/qquickcalendarmodel_p.h"
#include "Private/qquickrangeddate_p.h"
#include "Private/qquickrangemodel_p.h"
#include "Private/qquickwheelarea_p.h"
#include "Private/qquicktooltip_p.h"
#include "Private/qquickcontrolsettings_p.h"
#include "Private/qquickspinboxvalidator_p.h"
#include "Private/qquickabstractstyle_p.h"
#include "Private/qquickcontrolsprivate_p.h"

#ifdef QT_WIDGETS_LIB
#include <QtQuick/qquickimageprovider.h>
#include "Private/qquickstyleitem_p.h"
#endif

#ifndef QT_NO_TRANSLATION
#include <QtCore/qlibraryinfo.h>
#include <QtCore/qlocale.h>
#endif

static void initResources()
{
    Q_INIT_RESOURCE(controls);
}

QT_BEGIN_NAMESPACE

static const struct {
    const char *type;
    int major, minor;
} qmldir [] = {
    { "ApplicationWindow", 1, 0 },
    { "Button", 1, 0 },
    { "Calendar", 1, 2 },
    { "CheckBox", 1, 0 },
    { "ComboBox", 1, 0 },
    { "GroupBox", 1, 0 },
    { "Label", 1, 0 },
    { "MenuBar", 1, 0 },
    { "Menu", 1, 0 },
    { "StackView", 1, 0 },
    { "ProgressBar", 1, 0 },
    { "RadioButton", 1, 0 },
    { "ScrollView", 1, 0 },
    { "Slider", 1, 0 },
    { "SpinBox", 1, 0 },
    { "SplitView", 1, 0 },
    { "StackViewDelegate", 1, 0 },
    { "StackViewTransition", 1, 0 },
    { "StatusBar", 1, 0 },
    { "Switch", 1, 1 },
    { "Tab", 1, 0 },
    { "TabView", 1, 0 },
    { "TableView", 1, 0 },
    { "TableViewColumn", 1, 0 },
    { "TextArea", 1, 0 },
    { "TextField", 1, 0 },
    { "ToolBar", 1, 0 },
    { "ToolButton", 1, 0 },

    { "BusyIndicator", 1, 1 },

    { "TextArea", 1, 3 }
};

void QtQuickControlsPlugin::registerTypes(const char *uri)
{
    initResources();
    qmlRegisterType<QQuickAction>(uri, 1, 0, "Action");
    qmlRegisterType<QQuickExclusiveGroup>(uri, 1, 0, "ExclusiveGroup");
    qmlRegisterType<QQuickMenuItem>(uri, 1, 0, "MenuItem");
    qmlRegisterUncreatableType<QQuickMenuItemType>(uri, 1, 0, "MenuItemType",
                                                   QLatin1String("Do not create objects of type MenuItemType"));
    qmlRegisterType<QQuickMenuSeparator>(uri, 1, 0, "MenuSeparator");
    qmlRegisterUncreatableType<QQuickMenuBase>(uri, 1, 0, "MenuBase",
                                               QLatin1String("Do not create objects of type MenuBase"));

    qmlRegisterUncreatableType<QQuickStack>(uri, 1, 0, "Stack", QLatin1String("Do not create objects of type Stack"));
    qmlRegisterUncreatableType<QQuickSelectionMode>(uri, 1, 1, "SelectionMode", QLatin1String("Do not create objects of type SelectionMode"));

    const QString filesLocation = fileLocation();
    for (int i = 0; i < int(sizeof(qmldir)/sizeof(qmldir[0])); i++)
        qmlRegisterType(QUrl(filesLocation + "/" + qmldir[i].type + ".qml"), uri, qmldir[i].major, qmldir[i].minor, qmldir[i].type);
}

void QtQuickControlsPlugin::initializeEngine(QQmlEngine *engine, const char *uri)
{
    Q_UNUSED(uri);

    // Register private API. Note that to use these types outside of the
    // Qt Quick Controls module, both the public and private imports must be used.
    const char *private_uri = "QtQuick.Controls.Private";
    qmlRegisterType<QQuickAbstractStyle>(private_uri, 1, 0, "AbstractStyle");
    qmlRegisterType<QQuickCalendarModel>(private_uri, 1, 0, "CalendarModel");
    qmlRegisterType<QQuickPadding>(private_uri, 1, 0, "Padding");
    qmlRegisterType<QQuickRangedDate>(private_uri, 1, 0, "RangedDate");
    qmlRegisterType<QQuickRangeModel>(private_uri, 1, 0, "RangeModel");
    qmlRegisterType<QQuickWheelArea>(private_uri, 1, 0, "WheelArea");
    qmlRegisterType<QQuickSpinBoxValidator>(private_uri, 1, 0, "SpinBoxValidator");
    qmlRegisterSingletonType<QQuickTooltip>(private_uri, 1, 0, "Tooltip", QQuickControlsPrivate::registerTooltipModule);
    qmlRegisterSingletonType<QQuickControlSettings>(private_uri, 1, 0, "Settings", QQuickControlsPrivate::registerSettingsModule);

    qmlRegisterType<QQuickMenu>(private_uri, 1, 0, "MenuPrivate");
    qmlRegisterType<QQuickMenuBar>(private_uri, 1, 0, "MenuBarPrivate");
    qmlRegisterType<QQuickPopupWindow>(private_uri, 1, 0, "PopupWindow");

#ifdef QT_WIDGETS_LIB
    qmlRegisterType<QQuickStyleItem>(private_uri, 1, 0, "StyleItem");
    engine->addImageProvider("__tablerow", new QQuickTableRowImageProvider);
#endif
    engine->addImageProvider("desktoptheme", new QQuickDesktopIconProvider);
    if (isLoadedFromResource())
        engine->addImportPath(QStringLiteral("qrc:/"));

#ifndef QT_NO_TRANSLATION
    if (m_translator.load(QLocale(), QLatin1String("qtquickcontrols"),
                          QLatin1String("_"), QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        QCoreApplication::installTranslator(&m_translator);
#endif
}

QString QtQuickControlsPlugin::fileLocation() const
{
    if (isLoadedFromResource())
        return "qrc:/QtQuick/Controls";
    return baseUrl().toString();
}

bool QtQuickControlsPlugin::isLoadedFromResource() const
{
    // If one file is missing, it will load all the files from the resource
    QFile file(baseUrl().toLocalFile() + "/ApplicationWindow.qml");
    if (!file.exists())
        return true;
    return false;
}

QT_END_NAMESPACE
