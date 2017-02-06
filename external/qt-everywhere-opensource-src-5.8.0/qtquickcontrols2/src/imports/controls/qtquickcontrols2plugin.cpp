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

#include <QtCore/private/qfileselector_p.h>
#include <QtQuickControls2/qquickstyle.h>
#include <QtQuickControls2/private/qquickstyle_p.h>
#include <QtQuickControls2/private/qquickstyleplugin_p.h>
#include <QtQuickControls2/private/qquickstyleselector_p.h>
#include <QtQuickControls2/private/qquickcolorimageprovider_p.h>
#include <QtQuickTemplates2/private/qquickbuttongroup_p.h>
#include <QtQuickControls2/private/qquicktumblerview_p.h>

#include "qquickbusyindicatorring_p.h"
#include "qquickdefaultstyle_p.h"
#include "qquickdialring_p.h"
#include "qquickprogressstrip_p.h"

static inline void initResources()
{
    Q_INIT_RESOURCE(qtquickcontrols2plugin);
#ifdef QT_STATIC
    Q_INIT_RESOURCE(qmake_QtQuick_Controls_2);
#endif
}

QT_BEGIN_NAMESPACE

class QtQuickControls2Plugin: public QQuickStylePlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

public:
    QtQuickControls2Plugin(QObject *parent = nullptr);
    void registerTypes(const char *uri);
    void initializeEngine(QQmlEngine *engine, const char *uri);
};

QtQuickControls2Plugin::QtQuickControls2Plugin(QObject *parent) : QQuickStylePlugin(parent)
{
    initResources();
}

void QtQuickControls2Plugin::registerTypes(const char *uri)
{
    QQuickStylePrivate::init(typeUrl());
    const QString style = QQuickStyle::name();
    if (!style.isEmpty())
        QFileSelectorPrivate::addStatics(QStringList() << style.toLower());

    QQuickStyleSelector selector;
    selector.setBaseUrl(typeUrl());

    // QtQuick.Controls 2.0 (Qt 5.7)
    qmlRegisterType(selector.select(QStringLiteral("AbstractButton.qml")), uri, 2, 0, "AbstractButton");
    qmlRegisterType(selector.select(QStringLiteral("ApplicationWindow.qml")), uri, 2, 0, "ApplicationWindow");
    qmlRegisterType(selector.select(QStringLiteral("BusyIndicator.qml")), uri, 2, 0, "BusyIndicator");
    qmlRegisterType(selector.select(QStringLiteral("Button.qml")), uri, 2, 0, "Button");
    qmlRegisterType<QQuickButtonGroup>(uri, 2, 0, "ButtonGroup");
    qmlRegisterType<QQuickButtonGroupAttached>();
    qmlRegisterType(selector.select(QStringLiteral("CheckBox.qml")), uri, 2, 0, "CheckBox");
    qmlRegisterType(selector.select(QStringLiteral("CheckDelegate.qml")), uri, 2, 0, "CheckDelegate");
    qmlRegisterType(selector.select(QStringLiteral("ComboBox.qml")), uri, 2, 0, "ComboBox");
    qmlRegisterType(selector.select(QStringLiteral("Container.qml")), uri, 2, 0, "Container");
    qmlRegisterType(selector.select(QStringLiteral("Control.qml")), uri, 2, 0, "Control");
    qmlRegisterType(selector.select(QStringLiteral("Dial.qml")), uri, 2, 0, "Dial");
    qmlRegisterType(selector.select(QStringLiteral("Drawer.qml")), uri, 2, 0, "Drawer");
    qmlRegisterType(selector.select(QStringLiteral("Frame.qml")), uri, 2, 0, "Frame");
    qmlRegisterType(selector.select(QStringLiteral("GroupBox.qml")), uri, 2, 0, "GroupBox");
    qmlRegisterType(selector.select(QStringLiteral("ItemDelegate.qml")), uri, 2, 0, "ItemDelegate");
    qmlRegisterType(selector.select(QStringLiteral("Label.qml")), uri, 2, 0, "Label");
    qmlRegisterType(selector.select(QStringLiteral("Menu.qml")), uri, 2, 0, "Menu");
    qmlRegisterType(selector.select(QStringLiteral("MenuItem.qml")), uri, 2, 0, "MenuItem");
    qmlRegisterType(selector.select(QStringLiteral("Page.qml")), uri, 2, 0, "Page");
    qmlRegisterType(selector.select(QStringLiteral("PageIndicator.qml")), uri, 2, 0, "PageIndicator");
    qmlRegisterType(selector.select(QStringLiteral("Pane.qml")), uri, 2, 0, "Pane");
    qmlRegisterType(selector.select(QStringLiteral("Popup.qml")), uri, 2, 0, "Popup");
    qmlRegisterType(selector.select(QStringLiteral("ProgressBar.qml")), uri, 2, 0, "ProgressBar");
    qmlRegisterType(selector.select(QStringLiteral("RadioButton.qml")), uri, 2, 0, "RadioButton");
    qmlRegisterType(selector.select(QStringLiteral("RadioDelegate.qml")), uri, 2, 0, "RadioDelegate");
    qmlRegisterType(selector.select(QStringLiteral("RangeSlider.qml")), uri, 2, 0, "RangeSlider");
    qmlRegisterType(selector.select(QStringLiteral("ScrollBar.qml")), uri, 2, 0, "ScrollBar");
    qmlRegisterType(selector.select(QStringLiteral("ScrollIndicator.qml")), uri, 2, 0, "ScrollIndicator");
    qmlRegisterType(selector.select(QStringLiteral("Slider.qml")), uri, 2, 0, "Slider");
    qmlRegisterType(selector.select(QStringLiteral("SpinBox.qml")), uri, 2, 0, "SpinBox");
    qmlRegisterType(selector.select(QStringLiteral("StackView.qml")), uri, 2, 0, "StackView");
    qmlRegisterType(selector.select(QStringLiteral("SwipeDelegate.qml")), uri, 2, 0, "SwipeDelegate");
    qmlRegisterType(selector.select(QStringLiteral("SwipeView.qml")), uri, 2, 0, "SwipeView");
    qmlRegisterType(selector.select(QStringLiteral("Switch.qml")), uri, 2, 0, "Switch");
    qmlRegisterType(selector.select(QStringLiteral("SwitchDelegate.qml")), uri, 2, 0, "SwitchDelegate");
    qmlRegisterType(selector.select(QStringLiteral("TabBar.qml")), uri, 2, 0, "TabBar");
    qmlRegisterType(selector.select(QStringLiteral("TabButton.qml")), uri, 2, 0, "TabButton");
    qmlRegisterType(selector.select(QStringLiteral("TextArea.qml")), uri, 2, 0, "TextArea");
    qmlRegisterType(selector.select(QStringLiteral("TextField.qml")), uri, 2, 0, "TextField");
    qmlRegisterType(selector.select(QStringLiteral("ToolBar.qml")), uri, 2, 0, "ToolBar");
    qmlRegisterType(selector.select(QStringLiteral("ToolButton.qml")), uri, 2, 0, "ToolButton");
    qmlRegisterType(selector.select(QStringLiteral("ToolTip.qml")), uri, 2, 0, "ToolTip");
    qmlRegisterType(selector.select(QStringLiteral("Tumbler.qml")), uri, 2, 0, "Tumbler");

    // QtQuick.Controls 2.1 (Qt 5.8)
    qmlRegisterType<QQuickButtonGroup,1 >(uri, 2, 1, "ButtonGroup");
    qmlRegisterType(selector.select(QStringLiteral("Dialog.qml")), uri, 2, 1, "Dialog");
    qmlRegisterType(selector.select(QStringLiteral("DialogButtonBox.qml")), uri, 2, 1, "DialogButtonBox");
    qmlRegisterType(selector.select(QStringLiteral("MenuSeparator.qml")), uri, 2, 1, "MenuSeparator");
    qmlRegisterType(selector.select(QStringLiteral("RoundButton.qml")), uri, 2, 1, "RoundButton");
    qmlRegisterType(selector.select(QStringLiteral("ToolSeparator.qml")), uri, 2, 1, "ToolSeparator");
}

static QObject *styleSingleton(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine);
    Q_UNUSED(scriptEngine);
    return new QQuickDefaultStyle;
}

void QtQuickControls2Plugin::initializeEngine(QQmlEngine *engine, const char *uri)
{
    Q_UNUSED(uri);

    engine->addImageProvider(QStringLiteral("default"), new QQuickColorImageProvider(QStringLiteral(":/qt-project.org/imports/QtQuick/Controls.2/images")));

    const QByteArray import = QByteArray(uri) + ".impl";
    qmlRegisterType<QQuickBusyIndicatorRing>(import, 2, 0, "BusyRing");
    qmlRegisterType<QQuickBusyIndicatorAnimator>(import, 2, 0, "BusyRingAnimator");
    qmlRegisterType<QQuickProgressStrip>(import, 2, 0, "ProgressStrip");
    qmlRegisterType<QQuickProgressAnimator>(import, 2, 0, "ProgressStripAnimator");
    qmlRegisterType<QQuickDialRing>(import, 2, 0, "DialRing");
    qmlRegisterType<QQuickTumblerView>(import, 2, 1, "TumblerView");
    qmlRegisterSingletonType<QQuickDefaultStyle>(import, 2, 1, "Default", styleSingleton);

    qmlRegisterType(typeUrl(QStringLiteral("CheckIndicator.qml")), import, 2, 0, "CheckIndicator");
    qmlRegisterType(typeUrl(QStringLiteral("RadioIndicator.qml")), import, 2, 0, "RadioIndicator");
    qmlRegisterType(typeUrl(QStringLiteral("SwitchIndicator.qml")), import, 2, 0, "SwitchIndicator");
}

QT_END_NAMESPACE

#include "qtquickcontrols2plugin.moc"
