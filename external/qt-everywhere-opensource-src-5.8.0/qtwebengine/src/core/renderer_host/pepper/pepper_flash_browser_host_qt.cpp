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

// This is based on chrome/browser/renderer_host/pepper/pepper_flash_browser_host.cc:
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pepper_flash_browser_host_qt.h"

#include "base/time/time.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "ipc/ipc_message_macros.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"
#include "ppapi/shared_impl/time_conversion.h"
#include "qtwebenginecoreglobal_p.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_MACOSX)
#include <CoreServices/CoreServices.h>
#endif

using content::BrowserPpapiHost;
using content::BrowserThread;
using content::RenderProcessHost;

namespace QtWebEngineCore {

PepperFlashBrowserHostQt::PepperFlashBrowserHostQt(BrowserPpapiHost* host,
                                                   PP_Instance instance,
                                                   PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      host_(host),
      weak_factory_(this)
{
    int unused;
    host->GetRenderFrameIDsForInstance(instance, &render_process_id_, &unused);
}

PepperFlashBrowserHostQt::~PepperFlashBrowserHostQt() {}

int32_t PepperFlashBrowserHostQt::OnResourceMessageReceived(
        const IPC::Message& msg,
        ppapi::host::HostMessageContext* context)
{
    PPAPI_BEGIN_MESSAGE_MAP(PepperFlashBrowserHostQt, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_Flash_UpdateActivity,
                                        OnUpdateActivity)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_Flash_GetLocalTimeZoneOffset,
                                      OnGetLocalTimeZoneOffset)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_Flash_GetLocalDataRestrictions,
                                        OnGetLocalDataRestrictions)
    PPAPI_END_MESSAGE_MAP()
    return PP_ERROR_FAILED;
}

int32_t PepperFlashBrowserHostQt::OnUpdateActivity(ppapi::host::HostMessageContext* host_context)
{
#if defined(OS_WIN)
    // Reading then writing back the same value to the screensaver timeout system
    // setting resets the countdown which prevents the screensaver from turning
    // on "for a while". As long as the plugin pings us with this message faster
    // than the screensaver timeout, it won't go on.
    int value = 0;
    if (SystemParametersInfo(SPI_GETSCREENSAVETIMEOUT, 0, &value, 0))
        SystemParametersInfo(SPI_SETSCREENSAVETIMEOUT, value, NULL, 0);
#elif defined(OS_MACOSX)
    UpdateSystemActivity(OverallAct);
#endif
    return PP_OK;
}

int32_t PepperFlashBrowserHostQt::OnGetLocalTimeZoneOffset(
        ppapi::host::HostMessageContext* host_context,
        const base::Time& t)
{
    // The reason for this processing being in the browser process is that on
    // Linux, the localtime calls require filesystem access prohibited by the
    // sandbox.
    host_context->reply_msg = PpapiPluginMsg_Flash_GetLocalTimeZoneOffsetReply(
                ppapi::PPGetLocalTimeZoneOffset(t));
    return PP_OK;
}

int32_t PepperFlashBrowserHostQt::OnGetLocalDataRestrictions(
        ppapi::host::HostMessageContext* context)
{
    QT_NOT_YET_IMPLEMENTED
    return PP_OK;
}

}  // namespace QtWebEngineCore
