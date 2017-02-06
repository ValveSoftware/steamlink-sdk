/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Virtual Keyboard module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "plugin.h"
#include "inputcontext.h"
#include "inputengine.h"
#include "shifthandler.h"
#include "plaininputmethod.h"
#ifdef HAVE_HUNSPELL
#include "hunspellinputmethod.h"
#endif
#ifdef HAVE_PINYIN
#include "pinyininputmethod.h"
#endif
#ifdef HAVE_TCIME
#include "tcinputmethod.h"
#endif
#ifdef HAVE_HANGUL
#include "hangulinputmethod.h"
#endif
#ifdef HAVE_OPENWNN
#include "openwnninputmethod.h"
#endif
#ifdef HAVE_LIPI_TOOLKIT
#include "lipiinputmethod.h"
#endif
#ifdef HAVE_T9WRITE
#include "t9writeinputmethod.h"
#endif
#include "inputmethod.h"
#include "selectionlistmodel.h"
#include "enterkeyaction.h"
#include "enterkeyactionattachedtype.h"
#include "virtualkeyboardsettings.h"
#include "trace.h"
#if defined(QT_STATICPLUGIN)
#include <QtPlugin>
// This macro is similar to Q_IMPORT_PLUGIN, except it does not
// register duplicate entries as static plugins.
// The check is required since the application may already have
// initialized the plugin by its own dependencies.
#define Q_VKB_IMPORT_PLUGIN(PLUGIN) \
        extern const QT_PREPEND_NAMESPACE(QStaticPlugin) qt_static_plugin_##PLUGIN(); \
        if (!QPluginLoader::staticInstances().contains(qt_static_plugin_##PLUGIN().instance())) \
            qRegisterStaticPluginFunction(qt_static_plugin_##PLUGIN());
#endif

using namespace QtVirtualKeyboard;

Q_LOGGING_CATEGORY(qlcVirtualKeyboard, "qt.virtualkeyboard")

static const char pluginName[] = "qtvirtualkeyboard";
static const char inputMethodEnvVarName[] = "QT_IM_MODULE";
static const char pluginUri[] = "QtQuick.VirtualKeyboard";
static const char pluginSettingsUri[] = "QtQuick.VirtualKeyboard.Settings";

static QPointer<PlatformInputContext> platformInputContext;

static QObject *createInputContextModule(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine);
    Q_UNUSED(scriptEngine);
    QQmlContext *rootContext = engine->rootContext();
    QStringList inputMethodList = QStringList()
            << QLatin1String("PlainInputMethod")
            << QLatin1String("HunspellInputMethod")
#ifdef HAVE_PINYIN
            << QLatin1String("PinyinInputMethod")
#endif
#ifdef HAVE_TCIME
            << QLatin1String("TCInputMethod")
#endif
#ifdef HAVE_HANGUL
            << QLatin1String("HangulInputMethod")
#endif
#ifdef HAVE_OPENWNN
            << QLatin1String("JapaneseInputMethod")
#endif
#if defined(HAVE_LIPI_TOOLKIT) || defined(HAVE_T9WRITE)
            << QLatin1String("HandwritingInputMethod")
#endif
               ;
    rootContext->setContextProperty(QStringLiteral("VirtualKeyboardInputMethods"), inputMethodList);
    return new InputContext(platformInputContext);
}

QStringList QVirtualKeyboardPlugin::keys() const
{
    return QStringList(QLatin1String(pluginName));
}

QPlatformInputContext *QVirtualKeyboardPlugin::create(const QString &system, const QStringList &paramList)
{
    Q_UNUSED(paramList);
    Q_INIT_RESOURCE(content);
    Q_INIT_RESOURCE(default_style);
    Q_INIT_RESOURCE(retro_style);
#ifdef HAVE_T9WRITE
    Q_INIT_RESOURCE(t9write_db);
#endif
    Q_INIT_RESOURCE(layouts);

    if (!qEnvironmentVariableIsSet(inputMethodEnvVarName) || qgetenv(inputMethodEnvVarName) != pluginName)
        return Q_NULLPTR;

#if defined(QT_STATICPLUGIN)
    Q_VKB_IMPORT_PLUGIN(QtQuick2Plugin)
    Q_VKB_IMPORT_PLUGIN(QtQuick2WindowPlugin)
    Q_VKB_IMPORT_PLUGIN(QtQuickLayoutsPlugin)
    Q_VKB_IMPORT_PLUGIN(QmlFolderListModelPlugin)
    Q_VKB_IMPORT_PLUGIN(QtVirtualKeyboardStylesPlugin)
#endif

    qmlRegisterSingletonType<InputContext>(pluginUri, 1, 0, "InputContext", createInputContextModule);
    qmlRegisterSingletonType<InputContext>(pluginUri, 2, 0, "InputContext", createInputContextModule);
    qmlRegisterUncreatableType<InputEngine>(pluginUri, 1, 0, "InputEngine", QLatin1String("Cannot create input method engine"));
    qmlRegisterUncreatableType<InputEngine>(pluginUri, 2, 0, "InputEngine", QLatin1String("Cannot create input method engine"));
    qmlRegisterUncreatableType<ShiftHandler>(pluginUri, 1, 0, "ShiftHandler", QLatin1String("Cannot create shift handler"));
    qmlRegisterUncreatableType<ShiftHandler>(pluginUri, 2, 0, "ShiftHandler", QLatin1String("Cannot create shift handler"));
    qmlRegisterUncreatableType<SelectionListModel>(pluginUri, 1, 0, "SelectionListModel", QLatin1String("Cannot create selection list model"));
    qmlRegisterUncreatableType<SelectionListModel>(pluginUri, 2, 0, "SelectionListModel", QLatin1String("Cannot create selection list model"));
    qmlRegisterUncreatableType<AbstractInputMethod>(pluginUri, 1, 0, "AbstractInputMethod", QLatin1String("Cannot create abstract input method"));
    qmlRegisterUncreatableType<AbstractInputMethod>(pluginUri, 2, 0, "AbstractInputMethod", QLatin1String("Cannot create abstract input method"));
    qmlRegisterType<PlainInputMethod>(pluginUri, 1, 0, "PlainInputMethod");
    qmlRegisterType<PlainInputMethod>(pluginUri, 2, 0, "PlainInputMethod");
    qmlRegisterType<InputMethod>(pluginUri, 1, 0, "InputMethod");
    qmlRegisterType<InputMethod>(pluginUri, 2, 0, "InputMethod");
#ifdef HAVE_HUNSPELL
    qmlRegisterType<HunspellInputMethod>(pluginUri, 1, 0, "HunspellInputMethod");
    qmlRegisterType<HunspellInputMethod>(pluginUri, 2, 0, "HunspellInputMethod");
#endif
#ifdef HAVE_PINYIN
    qmlRegisterType<PinyinInputMethod>(pluginUri, 1, 1, "PinyinInputMethod");
    qmlRegisterType<PinyinInputMethod>(pluginUri, 2, 0, "PinyinInputMethod");
#endif
#ifdef HAVE_TCIME
    qmlRegisterType<TCInputMethod>(pluginUri, 2, 0, "TCInputMethod");
#endif
#ifdef HAVE_HANGUL
    qmlRegisterType<HangulInputMethod>(pluginUri, 1, 3, "HangulInputMethod");
    qmlRegisterType<HangulInputMethod>(pluginUri, 2, 0, "HangulInputMethod");
#endif
#ifdef HAVE_OPENWNN
    qmlRegisterType<OpenWnnInputMethod>(pluginUri, 1, 3, "JapaneseInputMethod");
    qmlRegisterType<OpenWnnInputMethod>(pluginUri, 2, 0, "JapaneseInputMethod");
#endif
#ifdef HAVE_LIPI_TOOLKIT
    qmlRegisterType<LipiInputMethod>(pluginUri, 2, 0, "HandwritingInputMethod");
#endif
#ifdef HAVE_T9WRITE
    qmlRegisterType<T9WriteInputMethod>(pluginUri, 2, 0, "HandwritingInputMethod");
#endif
    qmlRegisterType<EnterKeyActionAttachedType>();
    qmlRegisterType<EnterKeyAction>(pluginUri, 1, 0, "EnterKeyAction");
    qmlRegisterType<EnterKeyAction>(pluginUri, 2, 0, "EnterKeyAction");
    qmlRegisterType<Trace>(pluginUri, 2, 0, "Trace");
    qmlRegisterSingletonType<VirtualKeyboardSettings>(pluginSettingsUri, 1, 0, "VirtualKeyboardSettings", VirtualKeyboardSettings::registerSettingsModule);
    qmlRegisterSingletonType<VirtualKeyboardSettings>(pluginSettingsUri, 1, 1, "VirtualKeyboardSettings", VirtualKeyboardSettings::registerSettingsModule);
    qmlRegisterSingletonType<VirtualKeyboardSettings>(pluginSettingsUri, 1, 2, "VirtualKeyboardSettings", VirtualKeyboardSettings::registerSettingsModule);
    qmlRegisterSingletonType<VirtualKeyboardSettings>(pluginSettingsUri, 2, 0, "VirtualKeyboardSettings", VirtualKeyboardSettings::registerSettingsModule);
    qmlRegisterSingletonType<VirtualKeyboardSettings>(pluginSettingsUri, 2, 1, "VirtualKeyboardSettings", VirtualKeyboardSettings::registerSettingsModule);

    const QString path(QStringLiteral("qrc:///QtQuick/VirtualKeyboard/content/"));
    qmlRegisterType(QUrl(path + QLatin1String("InputPanel.qml")), pluginUri, 1, 0, "InputPanel");
    qmlRegisterType(QUrl(path + QLatin1String("InputPanel.qml")), pluginUri, 1, 2, "InputPanel");
    qmlRegisterType(QUrl(path + QLatin1String("InputPanel.qml")), pluginUri, 1, 3, "InputPanel");
    qmlRegisterType(QUrl(path + QLatin1String("InputPanel.qml")), pluginUri, 2, 0, "InputPanel");
    qmlRegisterType(QUrl(path + QLatin1String("InputPanel.qml")), pluginUri, 2, 1, "InputPanel");
    qmlRegisterType(QUrl(path + QLatin1String("HandwritingInputPanel.qml")), pluginUri, 2, 0, "HandwritingInputPanel");
    const QString componentsPath = path + QStringLiteral("components/");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("AlternativeKeys.qml")), pluginUri, 1, 0, "AlternativeKeys");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("AlternativeKeys.qml")), pluginUri, 2, 0, "AlternativeKeys");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("BackspaceKey.qml")), pluginUri, 1, 0, "BackspaceKey");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("BackspaceKey.qml")), pluginUri, 2, 0, "BackspaceKey");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("BaseKey.qml")), pluginUri, 1, 0, "BaseKey");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("BaseKey.qml")), pluginUri, 2, 0, "BaseKey");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("ChangeLanguageKey.qml")), pluginUri, 1, 0, "ChangeLanguageKey");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("ChangeLanguageKey.qml")), pluginUri, 2, 0, "ChangeLanguageKey");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("CharacterPreviewBubble.qml")), pluginUri, 1, 0, "CharacterPreviewBubble");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("CharacterPreviewBubble.qml")), pluginUri, 2, 0, "CharacterPreviewBubble");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("EnterKey.qml")), pluginUri, 1, 0, "EnterKey");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("EnterKey.qml")), pluginUri, 2, 0, "EnterKey");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("FillerKey.qml")), pluginUri, 1, 0, "FillerKey");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("FillerKey.qml")), pluginUri, 2, 0, "FillerKey");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("HideKeyboardKey.qml")), pluginUri, 1, 0, "HideKeyboardKey");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("HideKeyboardKey.qml")), pluginUri, 2, 0, "HideKeyboardKey");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("KeyboardColumn.qml")), pluginUri, 1, 0, "KeyboardColumn");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("KeyboardColumn.qml")), pluginUri, 2, 0, "KeyboardColumn");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("KeyboardLayout.qml")), pluginUri, 1, 0, "KeyboardLayout");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("KeyboardLayout.qml")), pluginUri, 2, 0, "KeyboardLayout");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("KeyboardLayoutLoader.qml")), pluginUri, 1, 1, "KeyboardLayoutLoader");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("KeyboardLayoutLoader.qml")), pluginUri, 2, 0, "KeyboardLayoutLoader");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("Keyboard.qml")), pluginUri, 1, 0, "Keyboard");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("Keyboard.qml")), pluginUri, 2, 0, "Keyboard");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("KeyboardRow.qml")), pluginUri, 1, 0, "KeyboardRow");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("KeyboardRow.qml")), pluginUri, 2, 0, "KeyboardRow");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("Key.qml")), pluginUri, 1, 0, "Key");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("Key.qml")), pluginUri, 2, 0, "Key");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("ModeKey.qml")), pluginUri, 2, 0, "ModeKey");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("MultiSoundEffect.qml")), pluginUri, 1, 1, "MultiSoundEffect");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("MultiSoundEffect.qml")), pluginUri, 2, 0, "MultiSoundEffect");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("MultitapInputMethod.qml")), pluginUri, 1, 0, "MultitapInputMethod");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("MultitapInputMethod.qml")), pluginUri, 2, 0, "MultitapInputMethod");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("NumberKey.qml")), pluginUri, 1, 0, "NumberKey");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("NumberKey.qml")), pluginUri, 2, 0, "NumberKey");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("ShiftKey.qml")), pluginUri, 1, 0, "ShiftKey");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("ShiftKey.qml")), pluginUri, 2, 0, "ShiftKey");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("SpaceKey.qml")), pluginUri, 1, 0, "SpaceKey");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("SpaceKey.qml")), pluginUri, 2, 0, "SpaceKey");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("SymbolModeKey.qml")), pluginUri, 1, 0, "SymbolModeKey");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("SymbolModeKey.qml")), pluginUri, 2, 0, "SymbolModeKey");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("HandwritingModeKey.qml")), pluginUri, 2, 0, "HandwritingModeKey");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("TraceInputArea.qml")), pluginUri, 2, 0, "TraceInputArea");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("TraceInputKey.qml")), pluginUri, 2, 0, "TraceInputKey");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("WordCandidatePopupList.qml")), pluginUri, 2, 0, "WordCandidatePopupList");
    qmlRegisterType(QUrl(componentsPath + QLatin1String("SelectionControl.qml")), pluginUri, 2, 1, "SelectionControl");

    if (system.compare(system, QLatin1String(pluginName), Qt::CaseInsensitive) == 0) {
        platformInputContext = new PlatformInputContext();
    }
    return platformInputContext;
}
