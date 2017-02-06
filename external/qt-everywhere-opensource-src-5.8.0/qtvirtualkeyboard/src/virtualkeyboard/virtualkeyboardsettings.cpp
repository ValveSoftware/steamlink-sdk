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

#include "virtualkeyboardsettings.h"
#include "settings.h"
#include "virtualkeyboarddebug.h"
#include <QQmlEngine>
#include <QFileInfo>
#include <QDir>
#include <QtCore/private/qobject_p.h>

namespace QtVirtualKeyboard {

class VirtualKeyboardSettingsPrivate : public QObjectPrivate
{
public:
    VirtualKeyboardSettingsPrivate() :
        QObjectPrivate(),
        engine() {}

    QString buildStyleImportPath(const QString &path, const QString &name) const
    {
        QString importPath(path + name + "/style.qml");
        if (!importPath.startsWith("qrc:")) {
            QUrl url = QUrl::fromLocalFile(importPath);
            importPath = url.toString();
        }
        return importPath;
    }

    QString buildStyleFilePath(const QString &path, const QString &name) const
    {
        QString filePath(path);
        if (filePath.startsWith("qrc:"))
            filePath.remove(0, 3);
        return filePath + name + "/style.qml";
    }

    QString styleImportPath(const QString &name) const
    {
        QStringList styleImportPathList;
        styleImportPathList << "qrc:/QtQuick/VirtualKeyboard/content/styles/";
        const QStringList importPathList = engine->importPathList();
        // Add QML import path (Note: the QML base dir is usually the last entry in the list)
        for (int i = importPathList.size() - 1; i >= 0; --i) {
            const QString stylesPath = importPathList.at(i)
                + QStringLiteral("/QtQuick/VirtualKeyboard/Styles/");
            styleImportPathList += stylesPath;
        }

        for (const QString &styleImportPath : qAsConst(styleImportPathList)) {
            QString filePath = buildStyleFilePath(styleImportPath, name);
            bool pathExist = false;
#ifdef COMPILING_QML
            // qtquickcompiler removes *.qml file paths from qrc file, but keeps directories - QTRD-3268
            pathExist = QFileInfo(filePath).dir().exists();
#else
            pathExist = QFileInfo::exists(filePath);
#endif
            if (pathExist)
                return buildStyleImportPath(styleImportPath, name);
        }
        return QString();
    }

    QPointer<QQmlEngine> engine;
};

/*!
    \qmlmodule QtQuick.VirtualKeyboard.Settings 2.0
    \title Qt Quick Virtual Keyboard Settings QML Types
    \ingroup qmlmodules

    \brief Provides settings for Qt Virtual Keyboard.

    The QML types can be imported into your application using the following
    import statements in your .qml file:

    \badcode
    import QtQuick.VirtualKeyboard.Settings 2.0
    \endcode
*/

/*!
    \qmltype VirtualKeyboardSettings
    \inqmlmodule QtQuick.VirtualKeyboard.Settings
    \ingroup qtvirtualkeyboard-settings-qml
    \since QtQuick.VirtualKeyboard 1.2
    \brief Provides settings for virtual keyboard.

    This type provides a VirtualKeyboardSettings singleton instance,
    which can be used to configure the virtual keyboard settings.

    Please note that the settings have only effect in the current
    application's lifetime, that is, configuration changes are not
    permanent.

    For example, to change the keyboard style in application:

    \code
        Component.onCompleted: VirtualKeyboardSettings.styleName = "retro"
    \endcode
*/

/*!
    \internal
*/
QObject *VirtualKeyboardSettings::registerSettingsModule(QQmlEngine *engine, QJSEngine *jsEngine)
{
    Q_UNUSED(jsEngine);
    return new VirtualKeyboardSettings(engine);
}

/*!
    \class QtVirtualKeyboard::VirtualKeyboardSettings
    \internal
*/

/*!
    \internal
*/
VirtualKeyboardSettings::VirtualKeyboardSettings(QQmlEngine *engine) :
    QObject(*new VirtualKeyboardSettingsPrivate())
{
    Q_D(VirtualKeyboardSettings);
    d->engine = engine;
    Settings *settings = Settings::instance();
    if (settings->styleName().isEmpty())
        resetStyle();
    connect(settings, SIGNAL(styleChanged()), SIGNAL(styleChanged()));
    connect(settings, SIGNAL(styleNameChanged()), SIGNAL(styleNameChanged()));
    connect(settings, SIGNAL(localeChanged()), SIGNAL(localeChanged()));
    connect(settings, SIGNAL(availableLocalesChanged()), SIGNAL(availableLocalesChanged()));
    connect(settings, SIGNAL(activeLocalesChanged()), SIGNAL(activeLocalesChanged()));
}

/*!
    \internal
*/
QString VirtualKeyboardSettings::style() const
{
    return Settings::instance()->style();
}

/*!
    \internal
*/
QString VirtualKeyboardSettings::styleName() const
{
    return Settings::instance()->styleName();
}

/*!
    \internal
*/
void VirtualKeyboardSettings::setStyleName(const QString &styleName)
{
    Q_D(VirtualKeyboardSettings);
    Settings *settings = Settings::instance();
    QString style = d->styleImportPath(styleName);
    if (style.isEmpty()) {
        qWarning() << "WARNING: Cannot find style" << styleName << "- fallback:" << settings->styleName();
        return;
    }
    settings->setStyleName(styleName);
    settings->setStyle(style);
}

QString VirtualKeyboardSettings::locale() const
{
    return Settings::instance()->locale();
}

void VirtualKeyboardSettings::setLocale(const QString &locale)
{
    Settings::instance()->setLocale(locale);
}

QStringList VirtualKeyboardSettings::availableLocales() const
{
    return Settings::instance()->availableLocales();
}

void VirtualKeyboardSettings::setActiveLocales(const QStringList &activeLocales)
{
    Settings::instance()->setActiveLocales(activeLocales);
}

QStringList VirtualKeyboardSettings::activeLocales() const
{
    return Settings::instance()->activeLocales();
}

void VirtualKeyboardSettings::resetStyle()
{
    Q_D(VirtualKeyboardSettings);
    Settings *settings = Settings::instance();
    QString styleName = QT_VIRTUALKEYBOARD_DEFAULT_STYLE;
    QString style = d->styleImportPath(styleName);
    QString customStyleName = qgetenv("QT_VIRTUALKEYBOARD_STYLE");
    if (!customStyleName.isEmpty()) {
        bool found = false;
        QRegExp styleNameValidator("\\w+");
        if (styleNameValidator.exactMatch(customStyleName)) {
            QString customStyle = d->styleImportPath(customStyleName);
            if (!customStyle.isEmpty()) {
                styleName = customStyleName;
                style = customStyle;
                found = true;
            }
        }
        if (!found) {
            qWarning() << "WARNING: Cannot find style" << customStyleName << "- fallback:" << styleName;
        }
    }
    if (!style.isEmpty()) {
        settings->setStyleName(styleName);
        settings->setStyle(style);
    }
}

/*!
    \qmlproperty string VirtualKeyboardSettings::style
    \internal
*/

/*!
    \qmlproperty string VirtualKeyboardSettings::styleName

    This property provides the current style. Application can change
    the keyboard style by setting the styleName to different value.

    The system wide keyboard style can be affected by setting
    the QT_VIRTUALKEYBOARD_STYLE environment variable.
*/

/*!
    \qmlproperty string VirtualKeyboardSettings::locale
    \since QtQuick.VirtualKeyboard.Settings 2.0

    This property provides the default locale for the keyboard.

    When the locale is not specified, the default system locale is used instead.

    If the keyboard locale is different from the new default locale, keyboard
    language is changed immediately to reflect the new locale. If the locale setting
    is incorrect, or it is not in the list of supported locales, it is ignored and
    the default setting is used instead.

    A locale is supported if it is included in the list of availableLocales.
*/

/*!
    \qmlproperty list<string> VirtualKeyboardSettings::availableLocales
    \since QtQuick.VirtualKeyboard.Settings 2.0

    This property contains a list of languages supported by the virtual keyboard.

    This list is read-only and depends on the build-time configuration of the
    virtual keyboard.
*/

/*!
    \qmlproperty list<string> VirtualKeyboardSettings::activeLocales
    \since QtQuick.VirtualKeyboard.Settings 2.0

    This property contains a list of activated languages of the virtual keyboard.

    The list of active languages is a subset of the available languages, and can be
    used to limit the list of available languages in the application lifetime.
*/

} // namespace QtVirtualKeyboard
