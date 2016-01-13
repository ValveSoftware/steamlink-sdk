/****************************************************************************
**
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

static base::StringPiece PlatformResourceProvider(int key) {
    if (key == IDR_DIR_HEADER_HTML) {
        base::StringPiece html_data = ui::ResourceBundle::GetSharedInstance().GetRawDataResource(IDR_DIR_HEADER_HTML);
        return html_data;
    }
    return base::StringPiece();
}

void ContentMainDelegateQt::PreSandboxStartup()
{
    net::NetModule::SetResourceProvider(PlatformResourceProvider);
    ui::ResourceBundle::InitSharedInstanceWithLocale(l10n_util::GetApplicationLocale(std::string("en-US")), 0);

    // Suppress info, warning and error messages per default.
    int logLevel = logging::LOG_FATAL;

    CommandLine* parsedCommandLine = CommandLine::ForCurrentProcess();
    if (parsedCommandLine->HasSwitch(switches::kLoggingLevel)) {
        std::string logLevelValue = parsedCommandLine->GetSwitchValueASCII(switches::kLoggingLevel);
        int level = 0;
        if (base::StringToInt(logLevelValue, &level) && level >= logging::LOG_INFO && level < logging::LOG_NUM_SEVERITIES)
            logLevel = level;
    }

    logging::SetMinLogLevel(logLevel);
}

content::ContentBrowserClient *ContentMainDelegateQt::CreateContentBrowserClient()
{
    m_browserClient.reset(new ContentBrowserClientQt);
    return m_browserClient.get();
}

content::ContentRendererClient *ContentMainDelegateQt::CreateContentRendererClient()
{
    return new ContentRendererClientQt;
}

bool ContentMainDelegateQt::BasicStartupComplete(int *exit_code)
{
    PathService::Override(base::FILE_EXE, WebEngineLibraryInfo::getPath(base::FILE_EXE));
    PathService::Override(base::DIR_QT_LIBRARY_DATA, WebEngineLibraryInfo::getPath(base::DIR_QT_LIBRARY_DATA));
    PathService::Override(content::DIR_MEDIA_LIBS, WebEngineLibraryInfo::getPath(content::DIR_MEDIA_LIBS));
    PathService::Override(ui::DIR_LOCALES, WebEngineLibraryInfo::getPath(ui::DIR_LOCALES));

    SetContentClient(new ContentClientQt);
    return false;
}

