/****************************************************************************
**
** Copyright (C) 2013 BlackBerry Limited. All rights reserved.
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

#include "web_engine_library_info.h"

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "content/public/common/content_paths.h"
#include "ui/base/ui_base_paths.h"
#include "ui/base/ui_base_switches.h"
#include "type_conversion.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QStandardPaths>
#include <QString>

#ifndef QTWEBENGINEPROCESS_NAME
#error "No name defined for QtWebEngine's process"
#endif

using namespace QtWebEngineCore;

namespace {

QString fallbackDir() {
    static QString directory = QDir::homePath() % QLatin1String("/.") % QCoreApplication::applicationName();
    return directory;
}

#if defined(OS_MACOSX) && defined(QT_MAC_FRAMEWORK_BUILD)
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

#if defined(OS_MACOSX)
static QString getMainApplicationResourcesPath()
{
    QString resourcesPath;
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    if (!mainBundle)
        return resourcesPath;

    // Will point to Resources inside an app bundle, or in case if the application is not packaged
    // as a bundle, will point to the application directory, where the resources are assumed to be
    // found.
    CFURLRef resourcesRelativeUrl = CFBundleCopyResourcesDirectoryURL(mainBundle);
    if (!resourcesRelativeUrl)
        return resourcesPath;

    CFURLRef resourcesAbsoluteUrl = CFURLCopyAbsoluteURL(resourcesRelativeUrl);
    CFStringRef resourcesAbolutePath = CFURLCopyFileSystemPath(resourcesAbsoluteUrl,
                                                                kCFURLPOSIXPathStyle);
    resourcesPath = QString::fromCFString(resourcesAbolutePath);
    CFRelease(resourcesAbolutePath);
    CFRelease(resourcesAbsoluteUrl);
    CFRelease(resourcesRelativeUrl);

    return resourcesPath;
}

#endif

QString subProcessPath()
{
    static QString processPath;
    if (processPath.isEmpty()) {
#if defined(OS_WIN)
        const QString processBinary = QLatin1String(QTWEBENGINEPROCESS_NAME) % QLatin1String(".exe");
#else
        const QString processBinary = QLatin1String(QTWEBENGINEPROCESS_NAME);
#endif

        QStringList candidatePaths;
        const QByteArray fromEnv = qgetenv("QTWEBENGINEPROCESS_PATH");
        if (!fromEnv.isEmpty()) {
            // Only search in QTWEBENGINEPROCESS_PATH if set
            candidatePaths << QString::fromLocal8Bit(fromEnv);
        } else {
#if defined(OS_MACOSX) && defined(QT_MAC_FRAMEWORK_BUILD)
            candidatePaths << getPath(frameworkBundle())
                              % QStringLiteral("/Helpers/" QTWEBENGINEPROCESS_NAME ".app/Contents/MacOS/" QTWEBENGINEPROCESS_NAME);
#else
            candidatePaths << QLibraryInfo::location(QLibraryInfo::LibraryExecutablesPath)
                              % QLatin1Char('/') % processBinary;
            candidatePaths << QCoreApplication::applicationDirPath()
                              % QLatin1Char('/') % processBinary;
#endif
        }

        Q_FOREACH (const QString &candidate, candidatePaths) {
            if (QFileInfo::exists(candidate)) {
                processPath = candidate;
                break;
            }
        }
        if (processPath.isEmpty())
            qFatal("Could not find %s", processBinary.toUtf8().constData());

    }


    return processPath;
}

QString localesPath()
{
#if defined(OS_MACOSX) && defined(QT_MAC_FRAMEWORK_BUILD)
    return getResourcesPath(frameworkBundle()) % QLatin1String("/qtwebengine_locales");
#else
    static bool initialized = false;
    static QString potentialLocalesPath = QLibraryInfo::location(QLibraryInfo::TranslationsPath) % QDir::separator() % QLatin1String("qtwebengine_locales");

    if (!initialized) {
        initialized = true;
        if (!QFileInfo::exists(potentialLocalesPath)) {
            qWarning("Installed Qt WebEngine locales directory not found at location %s. Trying application directory...", qPrintable(potentialLocalesPath));
            potentialLocalesPath = QCoreApplication::applicationDirPath() % QDir::separator() % QLatin1String("qtwebengine_locales");
        }
        if (!QFileInfo::exists(potentialLocalesPath)) {
            qWarning("Qt WebEngine locales directory not found at location %s. Trying fallback directory... Translations MAY NOT not be correct.", qPrintable(potentialLocalesPath));
            potentialLocalesPath = fallbackDir();
        }
    }

    return potentialLocalesPath;
#endif
}

#if defined(ENABLE_SPELLCHECK)
QString dictionariesPath()
{
    static QString potentialDictionariesPath;
    static bool initialized = false;
    QStringList candidatePaths;
    if (!initialized) {
        initialized = true;

        // First try to find dictionaries near the application.
#ifdef OS_MACOSX
        QString resourcesDictionariesPath = getMainApplicationResourcesPath()
                % QDir::separator() % QLatin1String("qtwebengine_dictionaries");
        candidatePaths << resourcesDictionariesPath;
#endif
        QString applicationDictionariesPath = QCoreApplication::applicationDirPath()
                % QDir::separator() % QLatin1String("qtwebengine_dictionaries");
        candidatePaths << applicationDictionariesPath;

        // Then try to find dictionaries near the installed library.
#if defined(OS_MACOSX) && defined(QT_MAC_FRAMEWORK_BUILD)
        QString frameworkDictionariesPath = getResourcesPath(frameworkBundle())
                % QLatin1String("/qtwebengine_dictionaries");
        candidatePaths << frameworkDictionariesPath;
#endif

        QString libraryDictionariesPath = QLibraryInfo::location(QLibraryInfo::DataPath)
                % QDir::separator() % QLatin1String("qtwebengine_dictionaries");
        candidatePaths << libraryDictionariesPath;

        Q_FOREACH (const QString &candidate, candidatePaths) {
            if (QFileInfo::exists(candidate)) {
                potentialDictionariesPath = candidate;
                break;
            }
        }
    }

    return potentialDictionariesPath;
}
#endif // ENABLE_SPELLCHECK

QString icuDataPath()
{
#if defined(OS_MACOSX) && defined(QT_MAC_FRAMEWORK_BUILD)
    return getResourcesPath(frameworkBundle());
#else
    static bool initialized = false;
    static QString potentialResourcesPath = QLibraryInfo::location(QLibraryInfo::DataPath) % QLatin1String("/resources");
    if (!initialized) {
        initialized = true;
        if (!QFileInfo::exists(potentialResourcesPath % QLatin1String("/icudtl.dat"))) {
            qWarning("Qt WebEngine ICU data not found at %s. Trying parent directory...", qPrintable(potentialResourcesPath));
            potentialResourcesPath = QLibraryInfo::location(QLibraryInfo::DataPath);
        }
        if (!QFileInfo::exists(potentialResourcesPath % QLatin1String("/icudtl.dat"))) {
            qWarning("Qt WebEngine ICU data not found at %s. Trying application directory...", qPrintable(potentialResourcesPath));
            potentialResourcesPath = QCoreApplication::applicationDirPath();
        }
        if (!QFileInfo::exists(potentialResourcesPath % QLatin1String("/icudtl.dat"))) {
            qWarning("Qt WebEngine ICU data not found at %s. Trying fallback directory... The application MAY NOT work.", qPrintable(potentialResourcesPath));
            potentialResourcesPath = fallbackDir();
        }
    }

    return potentialResourcesPath;
#endif
}

QString resourcesDataPath()
{
#if defined(OS_MACOSX) && defined(QT_MAC_FRAMEWORK_BUILD)
    return getResourcesPath(frameworkBundle());
#else
    static bool initialized = false;
    static QString potentialResourcesPath = QLibraryInfo::location(QLibraryInfo::DataPath) % QLatin1String("/resources");
    if (!initialized) {
        initialized = true;
        if (!QFileInfo::exists(potentialResourcesPath % QLatin1String("/qtwebengine_resources.pak"))) {
            qWarning("Qt WebEngine resources not found at %s. Trying parent directory...", qPrintable(potentialResourcesPath));
            potentialResourcesPath = QLibraryInfo::location(QLibraryInfo::DataPath);
        }
        if (!QFileInfo::exists(potentialResourcesPath % QLatin1String("/qtwebengine_resources.pak"))) {
            qWarning("Qt WebEngine resources not found at %s. Trying application directory...", qPrintable(potentialResourcesPath));
            potentialResourcesPath = QCoreApplication::applicationDirPath();
        }
        if (!QFileInfo::exists(potentialResourcesPath % QLatin1String("/qtwebengine_resources.pak"))) {
            qWarning("Qt WebEngine resources not found at %s. Trying fallback directory... The application MAY NOT work.", qPrintable(potentialResourcesPath));
            potentialResourcesPath = fallbackDir();
        }
    }

    return potentialResourcesPath;
#endif
}
} // namespace

base::FilePath WebEngineLibraryInfo::getPath(int key)
{
    QString directory;
    switch (key) {
    case QT_RESOURCES_PAK:
        return toFilePath(resourcesDataPath() % QLatin1String("/qtwebengine_resources.pak"));
    case QT_RESOURCES_100P_PAK:
        return toFilePath(resourcesDataPath() % QLatin1String("/qtwebengine_resources_100p.pak"));
    case QT_RESOURCES_200P_PAK:
        return toFilePath(resourcesDataPath() % QLatin1String("/qtwebengine_resources_200p.pak"));
    case QT_RESOURCES_DEVTOOLS_PAK:
        return toFilePath(resourcesDataPath() % QLatin1String("/qtwebengine_devtools_resources.pak"));
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
        return toFilePath(icuDataPath());
    case ui::DIR_LOCALES:
        return toFilePath(localesPath());
#if defined(ENABLE_SPELLCHECK)
    case base::DIR_APP_DICTIONARIES:
        return toFilePath(dictionariesPath());
#endif
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

std::string WebEngineLibraryInfo::getApplicationLocale()
{
    base::CommandLine *parsedCommandLine = base::CommandLine::ForCurrentProcess();
    if (!parsedCommandLine->HasSwitch(switches::kLang)) {
        const QString &locale = QLocale().bcp47Name();

        // QLocale::bcp47Name returns "en" for American English locale. Chromium requires the "US" suffix
        // to clarify the dialect and ignores the shorter version.
        if (locale == "en")
            return "en-US";

        return locale.toStdString();
    }

    return parsedCommandLine->GetSwitchValueASCII(switches::kLang);
}
