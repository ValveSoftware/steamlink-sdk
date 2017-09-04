/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
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
#include "Private/qquicktreemodeladaptor_p.h"
#include "Private/qquicksceneposlistener_p.h"

#ifdef QT_WIDGETS_LIB
#include <QtQuick/qquickimageprovider.h>
#include "Private/qquickstyleitem_p.h"
#endif

#ifndef QT_NO_TRANSLATION
#include <QtCore/qcoreapplication.h>
#include <QtCore/qlibraryinfo.h>
#include <QtCore/qlocale.h>
#endif

static void initResources()
{
#ifdef QT_STATIC
    Q_INIT_RESOURCE(qmake_QtQuick_Controls);
#endif
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

    { "TextArea", 1, 3 },

    { "TreeView", 1, 4 },

    { "TextArea", 1, 5 },
    { "TreeView", 1, 5 }
};

QtQuickControls1Plugin::QtQuickControls1Plugin(QObject *parent) : QQmlExtensionPlugin(parent)
{
    initResources();
}

void QtQuickControls1Plugin::registerTypes(const char *uri)
{
    qmlRegisterType<QQuickAction1>(uri, 1, 0, "Action");
    qmlRegisterType<QQuickExclusiveGroup1>(uri, 1, 0, "ExclusiveGroup");
    qmlRegisterType<QQuickMenuItem1>(uri, 1, 0, "MenuItem");
    qmlRegisterUncreatableType<QQuickMenuItemType1>(uri, 1, 0, "MenuItemType",
                                                   QLatin1String("Do not create objects of type MenuItemType"));
    qmlRegisterType<QQuickMenuSeparator1>(uri, 1, 0, "MenuSeparator");
    qmlRegisterUncreatableType<QQuickMenuBase1>(uri, 1, 0, "MenuBase",
                                               QLatin1String("Do not create objects of type MenuBase"));

    qmlRegisterUncreatableType<QQuickStack1>(uri, 1, 0, "Stack", QLatin1String("Do not create objects of type Stack"));
    qmlRegisterUncreatableType<QQuickSelectionMode1>(uri, 1, 1, "SelectionMode", QLatin1String("Do not create objects of type SelectionMode"));

    const QString filesLocation = fileLocation();
    for (int i = 0; i < int(sizeof(qmldir)/sizeof(qmldir[0])); i++)
        qmlRegisterType(QUrl(filesLocation + "/" + qmldir[i].type + ".qml"), uri, qmldir[i].major, qmldir[i].minor, qmldir[i].type);
}

void QtQuickControls1Plugin::initializeEngine(QQmlEngine *engine, const char *uri)
{
    Q_UNUSED(uri);

    // Register private API. Note that to use these types outside of the
    // Qt Quick Controls module, both the public and private imports must be used.
    const char *private_uri = "QtQuick.Controls.Private";
    qmlRegisterType<QQuickAbstractStyle1>(private_uri, 1, 0, "AbstractStyle");
    qmlRegisterType<QQuickCalendarModel1>(private_uri, 1, 0, "CalendarModel");
    qmlRegisterType<QQuickPadding1>(private_uri, 1, 0, "Padding");
    qmlRegisterType<QQuickRangedDate1>(private_uri, 1, 0, "RangedDate");
    qmlRegisterType<QQuickRangeModel1>(private_uri, 1, 0, "RangeModel");
    qmlRegisterType<QQuickWheelArea1>(private_uri, 1, 0, "WheelArea");
#if QT_CONFIG(validator)
    qmlRegisterType<QQuickSpinBoxValidator1>(private_uri, 1, 0, "SpinBoxValidator");
#endif
    qmlRegisterSingletonType<QQuickTooltip1>(private_uri, 1, 0, "Tooltip", QQuickControlsPrivate1::registerTooltipModule);
    qmlRegisterSingletonType<QQuickControlSettings1>(private_uri, 1, 0, "Settings", QQuickControlsPrivate1::registerSettingsModule);

    qmlRegisterUncreatableType<QQuickControlsPrivate1>(private_uri, 1, 0, "Controls", QLatin1String("Controls is an abstract type."));
    qmlRegisterType<QQuickControlsPrivate1Attached>();

    qmlRegisterType<QQuickTreeModelAdaptor1>(private_uri, 1, 0, "TreeModelAdaptor");
    qmlRegisterType<QQuickScenePosListener1>(private_uri, 1, 0, "ScenePosListener");

    qmlRegisterType<QQuickMenu1>(private_uri, 1, 0, "MenuPrivate");
    qmlRegisterType<QQuickMenuBar1>(private_uri, 1, 0, "MenuBarPrivate");
    qmlRegisterType<QQuickPopupWindow1>(private_uri, 1, 0, "PopupWindow");

    qmlRegisterUncreatableType<QAbstractItemModel>(private_uri, 1, 0, "AbstractItemModel",
                                                   QLatin1String("AbstractItemModel is an abstract type."));

#ifdef QT_WIDGETS_LIB
    qmlRegisterType<QQuickStyleItem1>(private_uri, 1, 0, "StyleItem");
    engine->addImageProvider("__tablerow", new QQuickTableRowImageProvider1);
#endif
    engine->addImageProvider("desktoptheme", new QQuickDesktopIconProvider1);
    if (isLoadedFromResource())
        engine->addImportPath(QStringLiteral("qrc:/"));

#ifndef QT_NO_TRANSLATION
    if (m_translator.load(QLocale(), QLatin1String("qtquickcontrols"),
                          QLatin1String("_"), QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        QCoreApplication::installTranslator(&m_translator);
#endif
}

QString QtQuickControls1Plugin::fileLocation() const
{
#ifndef QT_STATIC
    if (isLoadedFromResource())
        return "qrc:/QtQuick/Controls";
    return baseUrl().toString();
#else
    return "qrc:/qt-project.org/imports/QtQuick/Controls";
#endif
}

bool QtQuickControls1Plugin::isLoadedFromResource() const
{
#ifdef QT_STATIC
    // When static it is included automatically
    // for us.
    return false;
#endif
#if defined(ALWAYS_LOAD_FROM_RESOURCES)
    return true;
#else
    // If one file is missing, it will load all the files from the resource
    QFile file(baseUrl().toLocalFile() + "/ApplicationWindow.qml");
    if (!file.exists())
        return true;
    return false;
#endif
}

QT_END_NAMESPACE
