/****************************************************************************
**
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

#include "content_client_qt.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/user_agent.h"
#include "ui/base/layout.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "type_conversion.h"

#include <QCoreApplication>
#include <QFile>
#include <QLibraryInfo>
#include <QString>

#if defined(Q_OS_WIN)
#include <shlobj.h>
static QString getLocalAppDataDir()
{
    QString result;
    wchar_t path[MAX_PATH];
    if (SHGetSpecialFolderPath(0, path, CSIDL_LOCAL_APPDATA, FALSE))
        result = QDir::fromNativeSeparators(QString::fromWCharArray(path));
    return result;
}
#endif

#if defined(ENABLE_PLUGINS)

// The plugin logic is based on chrome/common/chrome_content_client.cc:
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/pepper_plugin_info.h"
#include "ppapi/shared_impl/ppapi_permissions.h"

#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

static const int32_t kPepperFlashPermissions = ppapi::PERMISSION_DEV |
                                               ppapi::PERMISSION_PRIVATE |
                                               ppapi::PERMISSION_BYPASS_USER_GESTURE |
                                               ppapi::PERMISSION_FLASH;

namespace switches {
const char kPpapiFlashPath[]    = "ppapi-flash-path";
const char kPpapiFlashVersion[] = "ppapi-flash-version";
const char kPpapiWidevinePath[] = "ppapi-widevine-path";
}

static const char kWidevineCdmPluginExtension[] = "";

static const int32_t kWidevineCdmPluginPermissions = ppapi::PERMISSION_DEV
                                                   | ppapi::PERMISSION_PRIVATE;

static QString ppapiPluginsPath()
{
    // Look for plugins in /plugins/ppapi or application dir.
    static bool initialized = false;
    static QString potentialPluginsPath = QLibraryInfo::location(QLibraryInfo::PluginsPath) % QLatin1String("/ppapi");
    if (!initialized) {
        initialized = true;
        if (!QFileInfo::exists(potentialPluginsPath))
            potentialPluginsPath = QCoreApplication::applicationDirPath();
    }
    return potentialPluginsPath;
}


content::PepperPluginInfo CreatePepperFlashInfo(const base::FilePath& path, const std::string& version)
{
    content::PepperPluginInfo plugin;

    plugin.is_out_of_process = true;
    plugin.name = content::kFlashPluginName;
    plugin.path = path;
    plugin.permissions = kPepperFlashPermissions;

    std::vector<std::string> flash_version_numbers = base::SplitString(version, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (flash_version_numbers.size() < 1)
        flash_version_numbers.push_back("11");
    else if (flash_version_numbers[0].empty())
        flash_version_numbers[0] = "11";
    if (flash_version_numbers.size() < 2)
        flash_version_numbers.push_back("2");
    if (flash_version_numbers.size() < 3)
        flash_version_numbers.push_back("999");
    if (flash_version_numbers.size() < 4)
        flash_version_numbers.push_back("999");

    // E.g., "Shockwave Flash 10.2 r154":
    plugin.description = plugin.name + " " + flash_version_numbers[0] + "." + flash_version_numbers[1] + " r" + flash_version_numbers[2];
    plugin.version = base::JoinString(flash_version_numbers, ".");
    content::WebPluginMimeType swf_mime_type(content::kFlashPluginSwfMimeType,
                                             content::kFlashPluginSwfExtension,
                                             content::kFlashPluginSwfDescription);
    plugin.mime_types.push_back(swf_mime_type);
    content::WebPluginMimeType spl_mime_type(content::kFlashPluginSplMimeType,
                                             content::kFlashPluginSplExtension,
                                             content::kFlashPluginSplDescription);
    plugin.mime_types.push_back(spl_mime_type);

    return plugin;
}

void AddPepperFlashFromSystem(std::vector<content::PepperPluginInfo>* plugins)
{
    QStringList pluginPaths;
#if defined(Q_OS_WIN)
    QString winDir = QDir::fromNativeSeparators(qgetenv("WINDIR"));
    if (winDir.isEmpty())
        winDir = QString::fromLatin1("C:/Windows");
    QDir pluginDir(winDir + "/System32/Macromed/Flash");
    pluginDir.setFilter(QDir::Files);
    Q_FOREACH (const QFileInfo &info, pluginDir.entryInfoList(QStringList("pepflashplayer*.dll")))
        pluginPaths << info.absoluteFilePath();
    pluginPaths << ppapiPluginsPath() + QStringLiteral("/pepflashplayer.dll");
#endif
#if defined(Q_OS_OSX)
    pluginPaths << "/Library/Internet Plug-Ins/PepperFlashPlayer/PepperFlashPlayer.plugin"; // Mac OS X
    pluginPaths << ppapiPluginsPath() + QStringLiteral("/PepperFlashPlayer.plugin");
#endif
#if defined(Q_OS_LINUX)
    pluginPaths << "/opt/google/chrome/PepperFlash/libpepflashplayer.so" // Google Chrome
                << "/usr/lib/pepperflashplugin-nonfree/libpepflashplayer.so" // Ubuntu, package pepperflashplugin-nonfree
                << "/usr/lib/adobe-flashplugin/libpepflashplayer.so" // Ubuntu, package adobe-flashplugin
                << "/usr/lib/PepperFlash/libpepflashplayer.so" // Arch
                << "/usr/lib64/chromium/PepperFlash/libpepflashplayer.so"; // OpenSuSE
    pluginPaths << ppapiPluginsPath() + QStringLiteral("/libpepflashplayer.so");
#endif
    std::string flash_version = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(switches::kPpapiFlashVersion);
    for (auto it = pluginPaths.constBegin(); it != pluginPaths.constEnd(); ++it) {
        if (!QFile::exists(*it))
            continue;
        plugins->push_back(CreatePepperFlashInfo(QtWebEngineCore::toFilePath(*it), flash_version));
    }
}

void AddPepperFlashFromCommandLine(std::vector<content::PepperPluginInfo>* plugins)
{
    const base::CommandLine::StringType flash_path = base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(switches::kPpapiFlashPath);
    if (flash_path.empty() || !QFile::exists(QtWebEngineCore::toQt(flash_path)))
        return;

    // Read pepper flash plugin version from command-line. (e.g. 16.0.0.235)
    std::string flash_version = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(switches::kPpapiFlashVersion);
    plugins->push_back(CreatePepperFlashInfo(base::FilePath(flash_path), flash_version));
}

void AddPepperWidevine(std::vector<content::PepperPluginInfo>* plugins)
{
#if defined(WIDEVINE_CDM_AVAILABLE) && defined(ENABLE_PEPPER_CDMS) && !defined(WIDEVINE_CDM_IS_COMPONENT)
    QStringList pluginPaths;
    const base::CommandLine::StringType widevine_argument = base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(switches::kPpapiWidevinePath);
    if (!widevine_argument.empty())
        pluginPaths << QtWebEngineCore::toQt(widevine_argument);
    else {
        pluginPaths << ppapiPluginsPath() + QStringLiteral("/") + QString::fromLatin1(kWidevineCdmAdapterFileName);
#if defined(Q_OS_OSX)
    QDir potentialWidevineDir(QDir::homePath() + "/Library/Application Support/Google/Chrome/WidevineCDM");
    if (potentialWidevineDir.exists()) {
        QFileInfoList widevineVersionDirs = potentialWidevineDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);
        for (int i = 0; i < widevineVersionDirs.size(); ++i) {
            QString versionDirPath(widevineVersionDirs.at(i).absoluteFilePath());
            QString potentialWidevinePluginPath = versionDirPath + "/_platform_specific/mac_x64/" + QString::fromLatin1(kWidevineCdmAdapterFileName);
            pluginPaths << potentialWidevinePluginPath;
        }
    }
#elif defined(Q_OS_WIN)
    QDir potentialWidevineDir(getLocalAppDataDir() + "/Google/Chrome/User Data/WidevineCDM");
    if (potentialWidevineDir.exists()) {
        QFileInfoList widevineVersionDirs = potentialWidevineDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);
        for (int i = 0; i < widevineVersionDirs.size(); ++i) {
            QString versionDirPath(widevineVersionDirs.at(i).absoluteFilePath());
#ifdef WIN64
            QString potentialWidevinePluginPath = versionDirPath + "/_platform_specific/win_x64/" + QString::fromLatin1(kWidevineCdmAdapterFileName);
#else
            QString potentialWidevinePluginPath = versionDirPath + "/_platform_specific/win_x86/" + QString::fromLatin1(kWidevineCdmAdapterFileName);
#endif
            pluginPaths << potentialWidevinePluginPath;
        }
    }
#elif defined(Q_OS_LINUX)
        pluginPaths << QStringLiteral("/opt/google/chrome/libwidevinecdmadapter.so") // Google Chrome
                    << QStringLiteral("/usr/lib/chromium/libwidevinecdmadapter.so") // Arch
                    << QStringLiteral("/usr/lib64/chromium/libwidevinecdmadapter.so"); // OpenSUSE style
#endif
    }

    Q_FOREACH (const QString &pluginPath, pluginPaths) {
        base::FilePath path = QtWebEngineCore::toFilePath(pluginPath);
        if (base::PathExists(path)) {
            content::PepperPluginInfo widevine_cdm;
            widevine_cdm.is_out_of_process = true;
            widevine_cdm.path = path;
            widevine_cdm.name = kWidevineCdmDisplayName;
            widevine_cdm.description = kWidevineCdmDescription;
            content::WebPluginMimeType widevine_cdm_mime_type(
                kWidevineCdmPluginMimeType,
                kWidevineCdmPluginExtension,
                kWidevineCdmPluginMimeTypeDescription);

            // Add the supported codecs as if they came from the component manifest.
            std::vector<std::string> codecs;
            codecs.push_back(kCdmSupportedCodecVp8);
            codecs.push_back(kCdmSupportedCodecVp9);
#if defined(USE_PROPRIETARY_CODECS)
            codecs.push_back(kCdmSupportedCodecAvc1);
#endif  // defined(USE_PROPRIETARY_CODECS)
            std::string codec_string =
                base::JoinString(codecs, ",");
            widevine_cdm_mime_type.additional_param_names.push_back(
                base::ASCIIToUTF16(kCdmSupportedCodecsParamName));
            widevine_cdm_mime_type.additional_param_values.push_back(
                base::ASCIIToUTF16(codec_string));

            widevine_cdm.mime_types.push_back(widevine_cdm_mime_type);
            widevine_cdm.permissions = kWidevineCdmPluginPermissions;
            plugins->push_back(widevine_cdm);
        }
    }
#endif  // defined(WIDEVINE_CDM_AVAILABLE) && defined(ENABLE_PEPPER_CDMS) &&
        // !defined(WIDEVINE_CDM_IS_COMPONENT)
}

namespace QtWebEngineCore {

void ContentClientQt::AddPepperPlugins(std::vector<content::PepperPluginInfo>* plugins)
{
    AddPepperFlashFromSystem(plugins);
    AddPepperFlashFromCommandLine(plugins);
    AddPepperWidevine(plugins);
}

}
#endif

#include <QCoreApplication>

namespace QtWebEngineCore {

std::string ContentClientQt::getUserAgent()
{
    // Mention the Chromium version we're based on to get passed stupid UA-string-based feature detection (several WebRTC demos need this)
    return content::BuildUserAgentFromProduct("QtWebEngine/" QTWEBENGINECORE_VERSION_STR " Chrome/" CHROMIUM_VERSION);
}

base::StringPiece ContentClientQt::GetDataResource(int resource_id, ui::ScaleFactor scale_factor) const {
    return ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(resource_id, scale_factor);
}

base::RefCountedMemory *ContentClientQt::GetDataResourceBytes(int resource_id) const
{
    return ResourceBundle::GetSharedInstance().LoadDataResourceBytes(resource_id);
}

base::string16 ContentClientQt::GetLocalizedString(int message_id) const
{
    return l10n_util::GetStringUTF16(message_id);
}

std::string ContentClientQt::GetProduct() const
{
    QString productName(qApp->applicationName() % '/' % qApp->applicationVersion());
    return productName.toStdString();
}

} // namespace QtWebEngineCore
