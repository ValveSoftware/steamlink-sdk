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

#include "qquickcontrolsettings_p.h"
#include <qquickitem.h>
#include <qcoreapplication.h>
#include <qdebug.h>
#include <qqmlengine.h>
#include <qfileinfo.h>
#if QT_CONFIG(library)
#include <qlibrary.h>
#endif
#include <qdir.h>
#include <QTouchDevice>
#include <QGuiApplication>
#include <QStyleHints>
#if defined(Q_OS_ANDROID) && !defined(Q_OS_ANDROID_EMBEDDED)
#include <private/qjnihelpers_p.h>
#endif

QT_BEGIN_NAMESPACE

static QString defaultStyleName()
{
    //Only enable QStyle support when we are using QApplication
#if defined(QT_WIDGETS_LIB) && !defined(Q_OS_IOS) && !defined(Q_OS_ANDROID) && !defined(Q_OS_BLACKBERRY) && !defined(Q_OS_QNX) && !defined(Q_OS_WINRT)
    if (QCoreApplication::instance()->inherits("QApplication"))
        return QLatin1String("Desktop");
#elif defined(Q_OS_ANDROID) && !defined(Q_OS_ANDROID_EMBEDDED)
    if (QtAndroidPrivate::androidSdkVersion() >= 11)
        return QLatin1String("Android");
#elif defined(Q_OS_IOS)
    return QLatin1String("iOS");
#elif defined(Q_OS_WINRT) && 0 // Enable once style is ready
    return QLatin1String("WinRT");
#endif
    return QLatin1String("Base");
}

static QString styleEnvironmentVariable()
{
    QString style = qgetenv("QT_QUICK_CONTROLS_1_STYLE");
    if (style.isEmpty())
        style = qgetenv("QT_QUICK_CONTROLS_STYLE");
    return style;
}

static QString styleImportName()
{
    QString name = styleEnvironmentVariable();
    if (name.isEmpty())
        name = defaultStyleName();
    return QFileInfo(name).fileName();
}

static bool fromResource(const QString &path)
{
    return path.startsWith(":/");
}

bool QQuickControlSettings1::hasTouchScreen() const
{
    const auto devices = QTouchDevice::devices();
    for (const QTouchDevice *dev : devices)
        if (dev->type() == QTouchDevice::TouchScreen)
            return true;
    return false;
}

bool QQuickControlSettings1::isMobile() const
{
#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID) || defined(Q_OS_BLACKBERRY) || defined(Q_OS_QNX) || defined(Q_OS_WINRT)
    return true;
#else
    if (qEnvironmentVariableIsSet("QT_QUICK_CONTROLS_MOBILE")) {
        return true;
    }
    return false;
#endif
}

bool QQuickControlSettings1::hoverEnabled() const
{
    return !isMobile() || !hasTouchScreen();
}

QString QQuickControlSettings1::makeStyleComponentPath(const QString &controlStyleName, const QString &styleDirPath)
{
    return styleDirPath + QStringLiteral("/") + controlStyleName;
}

QUrl QQuickControlSettings1::makeStyleComponentUrl(const QString &controlStyleName, const QString &styleDirPath)
{
    QString styleFilePath = makeStyleComponentPath(controlStyleName, styleDirPath);

    if (styleDirPath.startsWith(QLatin1String(":/")))
        return QUrl(QStringLiteral("qrc") + styleFilePath);

    return QUrl::fromLocalFile(styleFilePath);
}

QQmlComponent *QQuickControlSettings1::styleComponent(const QUrl &styleDirUrl, const QString &controlStyleName, QObject *control)
{
    Q_UNUSED(styleDirUrl); // required for hack that forces this function to be re-called from within QML when style changes

    // QUrl doesn't consider qrc-based URLs as local files, so bypass it here.
    QString styleFilePath = makeStyleComponentPath(controlStyleName, m_styleMap.value(m_name).m_styleDirPath);
    QUrl styleFileUrl;
    if (QFile::exists(styleFilePath)) {
        styleFileUrl = makeStyleComponentUrl(controlStyleName, m_styleMap.value(m_name).m_styleDirPath);
    } else {
        // It's OK for a style to pick and choose which controls it wants to provide style files for.
        styleFileUrl = makeStyleComponentUrl(controlStyleName, m_styleMap.value(QStringLiteral("Base")).m_styleDirPath);
    }

    return new QQmlComponent(qmlEngine(control), styleFileUrl);
}

static QString relativeStyleImportPath(QQmlEngine *engine, const QString &styleName)
{
    QString path;
#ifndef QT_STATIC
    bool found = false;
    const auto importPathList = engine->importPathList(); // ideally we'd call QQmlImportDatabase::importPathList(Local) here, but it's not exported
    for (QString import : importPathList) {
        bool localPath = QFileInfo(import).isAbsolute();
        if (import.startsWith(QLatin1String("qrc:/"), Qt::CaseInsensitive)) {
            import = QLatin1Char(':') + import.mid(4);
            localPath = true;
        }
        if (localPath) {
            QDir dir(import + QStringLiteral("/QtQuick/Controls/Styles"));
            if (dir.exists(styleName)) {
                found = true;
                path = dir.absolutePath();
                break;
            }
        }
    }
    if (!found)
        path = ":/QtQuick/Controls/Styles";
#else
    Q_UNUSED(engine);
    Q_UNUSED(styleName);
    path = ":/qt-project.org/imports/QtQuick/Controls/Styles";
#endif
    return path;
}

static QString styleImportPath(QQmlEngine *engine, const QString &styleName)
{
    QString path = styleEnvironmentVariable();
    QFileInfo info(path);
    if (fromResource(path)) {
        path = info.path();
    } else if (info.isRelative()) {
        path = relativeStyleImportPath(engine, styleName);
    } else {
#ifndef QT_STATIC
        path = info.absolutePath();
#else
        path = "qrc:/qt-project.org/imports/QtQuick/Controls/Styles";
#endif
    }
    return path;
}

QQuickControlSettings1::QQuickControlSettings1(QQmlEngine *engine)
    : m_engine(engine)
{
    // First, register all style paths in the default style location.
    QDir dir;
    const QString defaultStyle = defaultStyleName();
#ifndef QT_STATIC
    dir.setPath(relativeStyleImportPath(engine, defaultStyle));
#else
    dir.setPath(":/qt-project.org/imports/QtQuick/Controls/Styles");
#endif
    dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    const auto list = dir.entryList();
    for (const QString &styleDirectory : list) {
        findStyle(engine, styleDirectory);
    }

    m_name = styleImportName();

    // If the style name is a path..
    const QString styleNameFromEnvVar = styleEnvironmentVariable();
    if (QFile::exists(styleNameFromEnvVar)) {
        StyleData styleData;
        styleData.m_styleDirPath = styleNameFromEnvVar;
        m_styleMap[m_name] = styleData;
    }

    // Then check if the style the user wanted is known to us. Otherwise, use the fallback style.
    if (m_styleMap.contains(m_name)) {
        m_path = m_styleMap.value(m_name).m_styleDirPath;
    } else {
        m_path = m_styleMap.value(defaultStyle).m_styleDirPath;
        // Maybe the requested style is not next to the default style, but elsewhere in the import path
        findStyle(engine, m_name);
        if (!m_styleMap.contains(m_name)) {
            QString unknownStyle = m_name;
            m_name = defaultStyle;
            qWarning() << "WARNING: Cannot find style" << unknownStyle << "- fallback:" << styleFilePath();
        }
    }

    // Can't really do anything about this failing here, so don't bother checking...
    resolveCurrentStylePath();

    connect(this, SIGNAL(styleNameChanged()), SIGNAL(styleChanged()));
    connect(this, SIGNAL(stylePathChanged()), SIGNAL(styleChanged()));
}

bool QQuickControlSettings1::resolveCurrentStylePath()
{
#if QT_CONFIG(library)
    if (!m_styleMap.contains(m_name)) {
        qWarning() << "WARNING: Cannot find style" << m_name;
        return false;
    }

    StyleData styleData = m_styleMap.value(m_name);

    if (styleData.m_stylePluginPath.isEmpty())
        return true; // It's not a plugin; don't have to do anything.

    typedef bool (*StyleInitFunc)();
    typedef const char *(*StylePathFunc)();

    QLibrary lib(styleData.m_stylePluginPath);
    if (!lib.load()) {
        qWarning().nospace() << "WARNING: Cannot load plugin " << styleData.m_stylePluginPath
            << " for style " << m_name << ": " << lib.errorString();
        return false;
    }

    // Check for the existence of this first, as we don't want to init if this doesn't exist.
    StyleInitFunc initFunc = (StyleInitFunc) lib.resolve("qt_quick_controls_style_init");
    if (initFunc)
        initFunc();
    StylePathFunc pathFunc = (StylePathFunc) lib.resolve("qt_quick_controls_style_path");
    if (pathFunc) {
        styleData.m_styleDirPath = QString::fromLocal8Bit(pathFunc());
        m_styleMap[m_name] = styleData;
        m_path = styleData.m_styleDirPath;
    }
#endif // QT_CONFIG(library)
    return true;
}

void QQuickControlSettings1::findStyle(QQmlEngine *engine, const QString &styleName)
{
    QString path = styleImportPath(engine, styleName);
    QDir dir;
    dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
    dir.setPath(path);
    if (!dir.cd(styleName))
        return;

    StyleData styleData;

#if QT_CONFIG(library) && !defined(QT_STATIC)
    const auto list = dir.entryList();
    for (const QString &fileName : list) {
        // This assumes that there is only one library in the style directory,
        // which should be a safe assumption. If at some point it's determined
        // not to be safe, we'll have to resolve the init and path functions
        // here, to be sure that it is the correct library.
        if (QLibrary::isLibrary(fileName)) {
            styleData.m_stylePluginPath = dir.absoluteFilePath(fileName);
            break;
        }
    }
#endif

    // If there's no plugin for the style, then the style's files are
    // contained in this directory (which contains a qmldir file instead).
    styleData.m_styleDirPath = dir.absolutePath();

    m_styleMap[styleName] = styleData;
}

QUrl QQuickControlSettings1::style() const
{
    QUrl result;
    QString path = styleFilePath();
    if (fromResource(path)) {
        result.setScheme("qrc");
        path.remove(0, 1); // remove ':' prefix
        result.setPath(path);
    } else
        result = QUrl::fromLocalFile(path);
    return result;
}

QString QQuickControlSettings1::styleName() const
{
    return m_name;
}

void QQuickControlSettings1::setStyleName(const QString &name)
{
    if (m_name != name) {
        QString oldName = m_name;
        m_name = name;

        if (!m_styleMap.contains(name)) {
            // Maybe this style is not next to the default style, but elsewhere in the import path
            findStyle(m_engine, name);
        }

        // Don't change the style if it can't be resolved.
        if (!resolveCurrentStylePath())
            m_name = oldName;
        else
            emit styleNameChanged();
    }
}

QString QQuickControlSettings1::stylePath() const
{
    return m_path;
}

void QQuickControlSettings1::setStylePath(const QString &path)
{
    if (m_path != path) {
        m_path = path;
        emit stylePathChanged();
    }
}

QString QQuickControlSettings1::styleFilePath() const
{
    return m_path;
}

extern Q_GUI_EXPORT int qt_defaultDpiX();

qreal QQuickControlSettings1::dpiScaleFactor() const
{
#ifndef Q_OS_MAC
    return (qreal(qt_defaultDpiX()) / 96.0);
#endif
    return 1.0;
}

qreal QQuickControlSettings1::dragThreshold() const
{
    return qApp->styleHints()->startDragDistance();
}


QT_END_NAMESPACE
