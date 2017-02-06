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

#ifndef PEPPER_ISOLATED_FILE_SYSTEM_MESSAGE_FILTER_H
#define PEPPER_ISOLATED_FILE_SYSTEM_MESSAGE_FILTER_H

#include "base/macros.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/private/ppb_isolated_file_system_private.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/host/resource_message_filter.h"

namespace content {
class BrowserPpapiHost;
}

namespace ppapi {
namespace host {
struct HostMessageContext;
}  // namespace host
}  // namespace ppapi

namespace QtWebEngineCore {

class PepperIsolatedFileSystemMessageFilter : public ppapi::host::ResourceMessageFilter {
public:
    static PepperIsolatedFileSystemMessageFilter *Create(PP_Instance instance, content::BrowserPpapiHost *host);

    // ppapi::host::ResourceMessageFilter implementation.
    scoped_refptr<base::TaskRunner> OverrideTaskRunnerForMessage(const IPC::Message &msg) override;
    int32_t OnResourceMessageReceived(const IPC::Message &msg, ppapi::host::HostMessageContext *context) override;

private:
    PepperIsolatedFileSystemMessageFilter(int render_process_id, ppapi::host::PpapiHost *ppapi_host);

    ~PepperIsolatedFileSystemMessageFilter() override;


    int32_t OnOpenFileSystem(ppapi::host::HostMessageContext *context, PP_IsolatedFileSystemType_Private type);
    int32_t OpenPluginPrivateFileSystem(ppapi::host::HostMessageContext *context);

    const int m_render_process_id;

    // Not owned by this object.
    ppapi::host::PpapiHost* m_ppapi_host;

    DISALLOW_COPY_AND_ASSIGN(PepperIsolatedFileSystemMessageFilter);
};

}  // namespace QtWebEngineCore

#endif  // PEPPER_ISOLATED_FILE_SYSTEM_MESSAGE_FILTER_H
