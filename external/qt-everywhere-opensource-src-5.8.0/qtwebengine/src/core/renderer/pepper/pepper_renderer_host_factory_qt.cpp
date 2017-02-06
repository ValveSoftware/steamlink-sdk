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

// This is based on chrome/renderer/pepper/chrome_renderer_pepper_host_factory.cc:
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pepper_renderer_host_factory_qt.h"
#include "pepper_flash_renderer_host_qt.h"

#include "base/memory/ptr_util.h"
#include "chrome/renderer/pepper/pepper_flash_font_file_host.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppapi_message_utils.h"
#include "ppapi/shared_impl/ppapi_permissions.h"


namespace QtWebEngineCore {

PepperRendererHostFactoryQt::PepperRendererHostFactoryQt(content::RendererPpapiHost* host)
    : host_(host)
{
}

PepperRendererHostFactoryQt::~PepperRendererHostFactoryQt()
{
}

std::unique_ptr<ppapi::host::ResourceHost> PepperRendererHostFactoryQt::CreateResourceHost(
        ppapi::host::PpapiHost* host,
        PP_Resource resource,
        PP_Instance instance,
        const IPC::Message& message)
{
    DCHECK_EQ(host_->GetPpapiHost(), host);

    if (!host_->IsValidInstance(instance))
        return nullptr;

    if (host_->GetPpapiHost()->permissions().HasPermission(ppapi::PERMISSION_FLASH)) {
        switch (message.type()) {
        case PpapiHostMsg_Flash_Create::ID:
            return base::WrapUnique(new PepperFlashRendererHostQt(host_, instance, resource));
        case PpapiHostMsg_FlashFullscreen_Create::ID:
        case PpapiHostMsg_FlashMenu_Create::ID:
            // Not implemented
            break;
        }
    }

    // TODO(raymes): PDF also needs access to the FlashFontFileHost currently.
    // We should either rename PPB_FlashFont_File to PPB_FontFile_Private or get
    // rid of its use in PDF if possible.
    if (host_->GetPpapiHost()->permissions().HasPermission(ppapi::PERMISSION_FLASH)
        || host_->GetPpapiHost()->permissions().HasPermission(ppapi::PERMISSION_PRIVATE)) {
        switch (message.type()) {
        case PpapiHostMsg_FlashFontFile_Create::ID: {
            ppapi::proxy::SerializedFontDescription description;
            PP_PrivateFontCharset charset;
            if (ppapi::UnpackMessage<PpapiHostMsg_FlashFontFile_Create>(message, &description, &charset))
                return base::WrapUnique(new PepperFlashFontFileHost(host_, instance, resource, description, charset));
            break;
        }
        case PpapiHostMsg_FlashDRM_Create::ID:
            // Not implemented
            break;
        }
    }

    if (host_->GetPpapiHost()->permissions().HasPermission(ppapi::PERMISSION_PRIVATE)) {
        switch (message.type()) {
        case PpapiHostMsg_PDF_Create::ID:
            // Not implemented
            break;
        }
    }

    return nullptr;
}

} // QtWebEngineCore
