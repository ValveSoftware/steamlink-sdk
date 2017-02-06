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

#include "content_main_delegate_qt.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_paths.h"
#include "ui/base/resource/resource_bundle.h"
#include "grit/net_resources.h"
#include "net/base/net_module.h"

#include "content_client_qt.h"
#include "renderer/content_renderer_client_qt.h"
#include "web_engine_library_info.h"

#if defined(ARCH_CPU_ARM_FAMILY) && (defined(OS_ANDROID) || defined(OS_LINUX))
#include "base/cpu.h"
#endif

#if defined(OS_LINUX)
#include "ui/base/ui_base_switches.h"
#endif

namespace QtWebEngineCore {

static base::StringPiece PlatformResourceProvider(int key) {
    if (key == IDR_DIR_HEADER_HTML) {
        base::StringPiece html_data = ui::ResourceBundle::GetSharedInstance().GetRawDataResource(IDR_DIR_HEADER_HTML);
        return html_data;
    }
    return base::StringPiece();
}

// Logging logic is based on chrome/common/logging_chrome.cc:
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

static logging::LoggingDestination DetermineLogMode(const base::CommandLine& command_line)
{
#ifdef NDEBUG
    bool enable_logging = false;
    const char *kInvertLoggingSwitch = switches::kEnableLogging;
#else
    bool enable_logging = true;
    const char *kInvertLoggingSwitch = switches::kDisableLogging;
#endif

    if (command_line.HasSwitch(kInvertLoggingSwitch))
        enable_logging = !enable_logging;

    if (enable_logging)
        return logging::LOG_TO_SYSTEM_DEBUG_LOG;
    else
        return logging::LOG_NONE;
}

void ContentMainDelegateQt::PreSandboxStartup()
{
#if defined(ARCH_CPU_ARM_FAMILY) && (defined(OS_ANDROID) || defined(OS_LINUX))
    // Create an instance of the CPU class to parse /proc/cpuinfo and cache
    // cpu_brand info.
    base::CPU cpu_info;
#endif

    net::NetModule::SetResourceProvider(PlatformResourceProvider);
    ui::ResourceBundle::InitSharedInstanceWithLocale(WebEngineLibraryInfo::getApplicationLocale(), 0, ui::ResourceBundle::LOAD_COMMON_RESOURCES);

    base::CommandLine* parsedCommandLine = base::CommandLine::ForCurrentProcess();
    logging::LoggingSettings settings;
    settings.logging_dest = DetermineLogMode(*parsedCommandLine);
    logging::InitLogging(settings);
    // view the logs with process/thread IDs and timestamps
    logging::SetLogItems(true, //enable_process_id
                         true, //enable_thread_id
                         true, //enable_timestamp
                         false //enable_tickcount
                        );

    if (logging::GetMinLogLevel() >= logging::LOG_INFO) {
        if (parsedCommandLine->HasSwitch(switches::kLoggingLevel)) {
            std::string logLevelValue = parsedCommandLine->GetSwitchValueASCII(switches::kLoggingLevel);
            int level = 0;
            if (base::StringToInt(logLevelValue, &level) && level >= logging::LOG_INFO && level < logging::LOG_NUM_SEVERITIES)
                logging::SetMinLogLevel(level);
        }
    }
}

content::ContentBrowserClient *ContentMainDelegateQt::CreateContentBrowserClient()
{
    m_browserClient.reset(new ContentBrowserClientQt);
    return m_browserClient.get();
}

content::ContentRendererClient *ContentMainDelegateQt::CreateContentRendererClient()
{
#if defined(OS_LINUX)
    base::CommandLine *parsedCommandLine = base::CommandLine::ForCurrentProcess();

    if (parsedCommandLine->HasSwitch(switches::kLang)) {
        const std::string &locale = parsedCommandLine->GetSwitchValueASCII(switches::kLang);
        ui::ResourceBundle::GetSharedInstance().ReloadLocaleResources(locale);
    }
#endif

    return new ContentRendererClientQt;
}

// see icu_util.cc
#define ICU_UTIL_DATA_FILE   0
#define ICU_UTIL_DATA_SHARED 1
#define ICU_UTIL_DATA_STATIC 2

bool ContentMainDelegateQt::BasicStartupComplete(int *exit_code)
{
    PathService::Override(base::FILE_EXE, WebEngineLibraryInfo::getPath(base::FILE_EXE));
#if ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE
    PathService::Override(base::DIR_QT_LIBRARY_DATA, WebEngineLibraryInfo::getPath(base::DIR_QT_LIBRARY_DATA));
#endif
    PathService::Override(ui::DIR_LOCALES, WebEngineLibraryInfo::getPath(ui::DIR_LOCALES));
#if defined(ENABLE_SPELLCHECK)
    PathService::Override(base::DIR_APP_DICTIONARIES, WebEngineLibraryInfo::getPath(base::DIR_APP_DICTIONARIES));
#endif
    SetContentClient(new ContentClientQt);
    return false;
}

} // namespace QtWebEngineCore
