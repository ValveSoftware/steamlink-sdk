/****************************************************************************
**
** Copyright (C) 2013 BlackBerry Limited. All rights reserved.
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "web_engine_library_info.h"

#include "base/base_paths.h"
#include "base/file_util.h"
#include "content/public/common/content_paths.h"
#include "ui/base/ui_base_paths.h"
#include "type_conversion.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QStandardPaths>
#include <QString>
#include <QStringBuilder>

#ifndef QTWEBENGINEPROCESS_NAME
#error "No name defined for QtWebEngine's process"
#endif


namespace {

QString location(QLibraryInfo::LibraryLocation path)
{
#if defined(Q_OS_BLACKBERRY)
    // On BlackBerry, the qtwebengine may live in /usr/lib/qtwebengine.
    // If so, the QTWEBENGINEPROCESS_PATH env var is set to /usr/lib/qtwebengine/bin/QTWEBENGINEPROCESS_NAME.
    static QString webEnginePath;
    static bool initialized = false;
    if (!initialized) {
        const QByteArray fromEnv = qgetenv("QTWEBENGINEPROCESS_PATH");
        if (!fromEnv.isEmpty()) {
            QDir dir = QFileInfo(QString::fromLatin1(fromEnv)).dir();
            if (dir.cdUp())
                webEnginePath = dir.absolutePath();
        }
        initialized = true;
    }
    switch (path) {
    case QLibraryInfo::TranslationsPath:
        if (!webEnginePath.isEmpty())
            return webEnginePath % QDir::separator() % QLatin1String("translations");
        break;
    case QLibraryInfo::DataPath:
        if (!webEnginePath.isEmpty())
            return webEnginePath;
        break;
    default:
        break;
    }
#endif

    return QLibraryInfo::location(path);
}

#if defined(OS_MACOSX)
static inline CFBundleRef frameworkBundle()
{
    return CFBundleGetBundleWithIdentifier(CFSTR("org.qt-project.Qt.QtWebEngineCore"));
}

static QString getPath(CFBundleRef frameworkBundle)
{
    QString path;
    // The following is a fix for QtWebEngineProcess crashes on OS X 10.7 and before.
    // We use it for the other OS X versions as well to make sure it works and because
    // the directory structure should be the same.
    if (qApp->applicationName() == QLatin1String(QTWEBENGINEPROCESS_NAME)) {
        path = QDir::cleanPath(qApp->applicationDirPath() % QLatin1String("/../../../.."));
    } else if (frameworkBundle) {
        CFURLRef bundleUrl = CFBundleCopyBundleURL(frameworkBundle);
        CFStringRef bundlePath = CFURLCopyFileSystemPath(bundleUrl, kCFURLPOSIXPathStyle);
        path = QString::fromCFString(bundlePath);
        CFRelease(bundlePath);
        CFRelease(bundleUrl);
    }
    return path;
}

static QString getResourcesPath(CFBundleRef frameworkBundle)
{
    QString path;
    // The following is a fix for QtWebEngineProcess crashes on OS X 10.7 and before.
    // We use it for the other OS X versions as well to make sure it works and because
    // the directory structure should be the same.
    if (qApp->applicationName() == QLatin1String(QTWEBENGINEPROCESS_NAME)) {
        path = getPath(frameworkBundle) % QLatin1String("/Resources");
    } else if (frameworkBundle) {
        CFURLRef resourcesRelativeUrl = CFBundleCopyResourcesDirectoryURL(frameworkBundle);
        CFStringRef resourcesRelativePath = CFURLCopyFileSystemPath(resourcesRelativeUrl, kCFURLPOSIXPathStyle);
        path = getPath(frameworkBundle) % QLatin1Char('/') % QString::fromCFString(resourcesRelativePath);
        CFRelease(resourcesRelativePath);
        CFRelease(resourcesRelativeUrl);
    }
    return path;
}
#endif

QString subProcessPath()
{
    static bool initialized = false;
#if defined(OS_WIN)
    static QString processBinary (QLatin1String(QTWEBENGINEPROCESS_NAME) % QLatin1String(".exe"));
#else
    static QString processBinary (QLatin1String(QTWEBENGINEPROCESS_NAME));
#endif
#if defined(OS_MACOSX) && defined(QT_MAC_FRAMEWORK_BUILD)
    static QString processPath (getPath(frameworkBundle())
                                % QStringLiteral("/Helpers/" QTWEBENGINEPROCESS_NAME ".app/Contents/MacOS/" QTWEBENGINEPROCESS_NAME));
#else
    static QString processPath (location(QLibraryInfo::LibraryExecutablesPath)
                                % QDir::separator() % processBinary);
#endif
    if (!initialized) {
        // Allow overriding at runtime for the time being.
        const QByteArray fromEnv = qgetenv("QTWEBENGINEPROCESS_PATH");
        if (!fromEnv.isEmpty())
            processPath = QString::fromLatin1(fromEnv);
        if (!QFileInfo(processPath).exists()) {
            qWarning("QtWebEngineProcess not found at location %s. Trying fallback path...", qPrintable(processPath));
            processPath = QCoreApplication::applicationDirPath() % QDir::separator() % processBinary;
        }
        if (!QFileInfo(processPath).exists())
            qFatal("QtWebEngineProcess not found at location %s. Try setting the QTWEBENGINEPROCESS_PATH environment variable.", qPrintable(processPath));
        initialized = true;
    }

    return processPath;
}

QString pluginsPath()
{
#if defined(OS_MACOSX) && defined(QT_MAC_FRAMEWORK_BUILD)
    return getPath(frameworkBundle()) % QLatin1String("/Libraries");
#else
    return location(QLibraryInfo::PluginsPath) % QDir::separator() % QLatin1String("qtwebengine");
#endif
}

QString localesPath()
{
#if defined(OS_MACOSX) && defined(QT_MAC_FRAMEWORK_BUILD)
    return getResourcesPath(frameworkBundle()) % QLatin1String("/qtwebengine_locales");
#else
    return location(QLibraryInfo::TranslationsPath) % QDir::separator() % QLatin1String("qtwebengine_locales");
#endif
}

QString fallbackDir() {
    static QString directory = QDir::homePath() % QDir::separator() % QChar::fromLatin1('.') % QCoreApplication::applicationName();
    return directory;
}

} // namespace

#if defined(OS_ANDROID)
namespace base {
// Replace the Android base path provider that depends on jni.
// With this we avoid patching chromium which we would need since
// PathService registers PathProviderAndroid by default on Android.
bool PathProviderAndroid(int key, FilePath* result)
{
    *result = WebEngineLibraryInfo::getPath(key);
    return !(result->empty());
}

}
#endif // defined(OS_ANDROID)

base::FilePath WebEngineLibraryInfo::getPath(int key)
{
    QString directory;
    switch (key) {
    case QT_RESOURCES_PAK:
#if defined(OS_MACOSX) && defined(QT_MAC_FRAMEWORK_BUILD)
        return toFilePath(getResourcesPath(frameworkBundle()) % QLatin1String("/qtwebengine_resources.pak"));
#else
        return toFilePath(location(QLibraryInfo::DataPath) % QDir::separator() %  QLatin1String("qtwebengine_resources.pak"));
#endif
    case base::FILE_EXE:
    case content::CHILD_PROCESS_EXE:
        return toFilePath(subProcessPath());
#if defined(OS_POSIX)
    case base::DIR_CACHE:
        directory = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        break;
    case base::DIR_HOME:
        directory = QDir::homePath();
        break;
#endif
    case base::DIR_USER_DESKTOP:
        directory = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        break;
    case base::DIR_QT_LIBRARY_DATA:
#if defined(OS_MACOSX) && defined(QT_MAC_FRAMEWORK_BUILD)
        return toFilePath(getResourcesPath(frameworkBundle()));
#else
        return toFilePath(location(QLibraryInfo::DataPath));
#endif
#if defined(OS_ANDROID)
    case base::DIR_SOURCE_ROOT:
    case base::DIR_ANDROID_EXTERNAL_STORAGE:
    case base::DIR_ANDROID_APP_DATA:
        directory = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
        break;
#endif
    case content::DIR_MEDIA_LIBS:
        return toFilePath(pluginsPath());
    case ui::DIR_LOCALES:
        return toFilePath(localesPath());
    default:
        // Note: the path system expects this function to override the default
        // behavior. So no need to log an error if we don't support a given
        // path. The system will just use the default.
        return base::FilePath();
    }

    return toFilePath(directory.isEmpty() ? fallbackDir() : directory);
}

base::string16 WebEngineLibraryInfo::getApplicationName()
{
    return toString16(qApp->applicationName());
}
