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

// This is based on chrome/browser/renderer_host/pepper/pepper_isolated_file_system_message_filter.cc:
// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pepper_isolated_file_system_message_filter.h"

#include "base/macros.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_view_host.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/file_system_util.h"
#include "storage/browser/fileapi/isolated_context.h"

namespace QtWebEngineCore {

// static
PepperIsolatedFileSystemMessageFilter* PepperIsolatedFileSystemMessageFilter::Create(PP_Instance instance, content::BrowserPpapiHost *host)
{
    int render_process_id;
    int unused_render_frame_id;
    if (!host->GetRenderFrameIDsForInstance(instance, &render_process_id, &unused_render_frame_id))
        return nullptr;
    return new PepperIsolatedFileSystemMessageFilter(render_process_id, host->GetPpapiHost());
}

PepperIsolatedFileSystemMessageFilter::PepperIsolatedFileSystemMessageFilter(int render_process_id,
                                                                             ppapi::host::PpapiHost *ppapi_host)
    : m_render_process_id(render_process_id),
      m_ppapi_host(ppapi_host)
{}

PepperIsolatedFileSystemMessageFilter::~PepperIsolatedFileSystemMessageFilter()
{}

scoped_refptr<base::TaskRunner> PepperIsolatedFileSystemMessageFilter::OverrideTaskRunnerForMessage(const IPC::Message &)
{
    // In order to reach ExtensionSystem, we need to get ProfileManager first.
    // ProfileManager lives in UI thread, so we need to do this in UI thread.
    return content::BrowserThread::GetMessageLoopProxyForThread(content::BrowserThread::UI);
}

int32_t PepperIsolatedFileSystemMessageFilter::OnResourceMessageReceived(const IPC::Message& msg, ppapi::host::HostMessageContext *context)
{
    PPAPI_BEGIN_MESSAGE_MAP(PepperIsolatedFileSystemMessageFilter, msg)
        PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_IsolatedFileSystem_BrowserOpen, OnOpenFileSystem)
    PPAPI_END_MESSAGE_MAP()
    return PP_ERROR_FAILED;
}

int32_t PepperIsolatedFileSystemMessageFilter::OnOpenFileSystem(ppapi::host::HostMessageContext *context,
                                                                PP_IsolatedFileSystemType_Private type)
{
    switch (type) {
    case PP_ISOLATEDFILESYSTEMTYPE_PRIVATE_INVALID:
        break;
    case PP_ISOLATEDFILESYSTEMTYPE_PRIVATE_CRX:
        return PP_ERROR_NOTSUPPORTED;
    case PP_ISOLATEDFILESYSTEMTYPE_PRIVATE_PLUGINPRIVATE:
        return OpenPluginPrivateFileSystem(context);
    }
    NOTREACHED();
    context->reply_msg = PpapiPluginMsg_IsolatedFileSystem_BrowserOpenReply(std::string());
    return PP_ERROR_FAILED;
}

int32_t PepperIsolatedFileSystemMessageFilter::OpenPluginPrivateFileSystem(ppapi::host::HostMessageContext *context)
{
    DCHECK(m_ppapi_host);
    // Only plugins with private permission can open the filesystem.
    if (!m_ppapi_host->permissions().HasPermission(ppapi::PERMISSION_PRIVATE))
        return PP_ERROR_NOACCESS;

    const std::string& root_name = ppapi::IsolatedFileSystemTypeToRootName(PP_ISOLATEDFILESYSTEMTYPE_PRIVATE_PLUGINPRIVATE);
    const std::string& fsid =
        storage::IsolatedContext::GetInstance()->RegisterFileSystemForVirtualPath(
            storage::kFileSystemTypePluginPrivate, root_name, base::FilePath());

    // Grant full access of isolated filesystem to renderer process.
    content::ChildProcessSecurityPolicy* policy = content::ChildProcessSecurityPolicy::GetInstance();
    policy->GrantCreateReadWriteFileSystem(m_render_process_id, fsid);

    context->reply_msg = PpapiPluginMsg_IsolatedFileSystem_BrowserOpenReply(fsid);
    return PP_OK;
}

}  // namespace chrome
