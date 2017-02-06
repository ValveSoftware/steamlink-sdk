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

#include "browser_message_filter_qt.h"

#include "common/qt_messages.h"
#include "content/public/browser/plugin_service.h"
#include "type_conversion.h"

namespace QtWebEngineCore {

BrowserMessageFilterQt::BrowserMessageFilterQt(int /*render_process_id*/)
    : BrowserMessageFilter(QtMsgStart)
{
}

// The following is based on chrome/browser/plugins/plugin_info_message_filter.cc:
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

bool BrowserMessageFilterQt::OnMessageReceived(const IPC::Message& message)
{
    IPC_BEGIN_MESSAGE_MAP(BrowserMessageFilterQt, message)
#if defined(ENABLE_PEPPER_CDMS)
        IPC_MESSAGE_HANDLER(
            QtWebEngineHostMsg_IsInternalPluginAvailableForMimeType,
            OnIsInternalPluginAvailableForMimeType)
#endif
        IPC_MESSAGE_UNHANDLED(return false)
    IPC_END_MESSAGE_MAP()
    return true;
}

#if defined(ENABLE_PEPPER_CDMS)
void BrowserMessageFilterQt::OnIsInternalPluginAvailableForMimeType(
    const std::string& mime_type, bool* is_available,
    std::vector<base::string16>* additional_param_names,
    std::vector<base::string16>* additional_param_values)
{
    std::vector<content::WebPluginInfo> plugins;
    content::PluginService::GetInstance()->GetInternalPlugins(&plugins);

    for (size_t i = 0; i < plugins.size(); ++i) {
        const content::WebPluginInfo& plugin = plugins[i];
        const std::vector<content::WebPluginMimeType>& mime_types = plugin.mime_types;
        for (size_t j = 0; j < mime_types.size(); ++j) {
            if (mime_types[j].mime_type == mime_type) {
                *is_available = true;
                *additional_param_names = mime_types[j].additional_param_names;
                *additional_param_values = mime_types[j].additional_param_values;
                return;
            }
        }
    }

    *is_available = false;
}

#endif // defined(ENABLE_PEPPER_CDMS)

} // namespace QtWebEngineCore
