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

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "content/public/common/content_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/data_pack.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_switches.h"

#include "web_engine_library_info.h"

#if defined(OS_LINUX)
#include "base/posix/global_descriptors.h"
#include "global_descriptors_qt.h"
#endif

namespace ui {

void ResourceBundle::LoadCommonResources()
{
    // We repacked the resources we need and installed them. now let chromium mmap that file.
    AddDataPackFromPath(WebEngineLibraryInfo::getPath(QT_RESOURCES_PAK), SCALE_FACTOR_NONE);
    AddDataPackFromPath(WebEngineLibraryInfo::getPath(QT_RESOURCES_100P_PAK), SCALE_FACTOR_100P);
    AddDataPackFromPath(WebEngineLibraryInfo::getPath(QT_RESOURCES_200P_PAK), SCALE_FACTOR_200P);
    AddOptionalDataPackFromPath(WebEngineLibraryInfo::getPath(QT_RESOURCES_DEVTOOLS_PAK), SCALE_FACTOR_NONE);
}

gfx::Image& ResourceBundle::GetNativeImageNamed(int resource_id)
{
    LOG(WARNING) << "Unable to load image with id " << resource_id;
    NOTREACHED();  // Want to assert in debug mode.
    return GetEmptyImage();
}

bool ResourceBundle::LocaleDataPakExists(const std::string& locale)
{
#if defined(OS_LINUX)
    base::CommandLine *parsed_command_line = base::CommandLine::ForCurrentProcess();
    std::string process_type = parsed_command_line->GetSwitchValueASCII(switches::kProcessType);
    if (process_type == switches::kRendererProcess) {
        // The Renderer Process is sandboxed thus only one locale is available in it.
        // The particular one is passed by the --lang command line option.
        if (!parsed_command_line->HasSwitch(switches::kLang) || parsed_command_line->GetSwitchValueASCII(switches::kLang) != locale)
            return false;

        auto global_descriptors = base::GlobalDescriptors::GetInstance();
        return global_descriptors->MaybeGet(kWebEngineLocale) != -1;
    }
#endif

    return !GetLocaleFilePath(locale, true).empty();
}

std::string ResourceBundle::LoadLocaleResources(const std::string& pref_locale)
{
    DCHECK(!locale_resources_data_.get()) << "locale.pak already loaded";

    std::string app_locale = l10n_util::GetApplicationLocale(pref_locale);

#if defined(OS_LINUX)
    int locale_fd = base::GlobalDescriptors::GetInstance()->MaybeGet(kWebEngineLocale);
    if (locale_fd > -1) {
        std::unique_ptr<DataPack> data_pack(new DataPack(SCALE_FACTOR_100P));
        data_pack->LoadFromFile(base::File(locale_fd));
        locale_resources_data_.reset(data_pack.release());
        return app_locale;
    }
#endif

    base::FilePath locale_file_path = GetOverriddenPakPath();
    if (locale_file_path.empty())
        locale_file_path = GetLocaleFilePath(app_locale, true);

    if (locale_file_path.empty()) {
        // It's possible that there is no locale.pak.
        LOG(WARNING) << "locale_file_path.empty() for locale " << app_locale;
        return std::string();
    }

    std::unique_ptr<DataPack> data_pack(new DataPack(SCALE_FACTOR_100P));
    if (!data_pack->LoadFromPath(locale_file_path)) {
        UMA_HISTOGRAM_ENUMERATION("ResourceBundle.LoadLocaleResourcesError",
                                  logging::GetLastSystemErrorCode(), 16000);
        LOG(ERROR) << "failed to load locale.pak";
        NOTREACHED();
        return std::string();
    }

    locale_resources_data_.reset(data_pack.release());
    return app_locale;
}

}  // namespace ui
