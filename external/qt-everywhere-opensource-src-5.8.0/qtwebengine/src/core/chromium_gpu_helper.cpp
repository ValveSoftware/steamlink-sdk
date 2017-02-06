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

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromium_gpu_helper.h"

// Including gpu/command_buffer headers before content/gpu headers makes sure that
// guards are defined to prevent duplicate definition errors with forward declared
// GL typedefs cascading through content header includes.
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"

#include "content/gpu/gpu_child_thread.h"
#include "gpu/ipc/service/gpu_channel_manager.h"

#ifdef Q_OS_QNX
#include "content/common/gpu/stream_texture_qnx.h"
#endif

// FIXME: Try using content::GpuChildThread::current()
base::MessageLoop *gpu_message_loop()
{
    return content::GpuChildThread::instance()->message_loop();
}

gpu::SyncPointManager *sync_point_manager()
{
    gpu::GpuChannelManager *gpuChannelManager = content::GpuChildThread::instance()->ChannelManager();
    return gpuChannelManager->sync_point_manager();
}

gpu::gles2::MailboxManager *mailbox_manager()
{
    gpu::GpuChannelManager *gpuChannelManager = content::GpuChildThread::instance()->ChannelManager();
    return gpuChannelManager->mailbox_manager();
}

gpu::gles2::Texture* ConsumeTexture(gpu::gles2::MailboxManager *mailboxManager, unsigned target, const gpu::Mailbox& mailbox)
{
    Q_UNUSED(target);
    return mailboxManager->ConsumeTexture(mailbox);
}

unsigned int service_id(gpu::gles2::Texture *tex)
{
    return tex->service_id();
}

#ifdef Q_OS_QNX
EGLStreamData eglstream_connect_consumer(gpu::gles2::Texture *tex)
{
    EGLStreamData egl_stream;
    content::StreamTexture* image = static_cast<content::StreamTexture *>(tex->GetLevelImage(GL_TEXTURE_EXTERNAL_OES, 0));
    if (image) {
        image->ConnectConsumerIfNeeded(&egl_stream.egl_display, &egl_stream.egl_str_handle);
    }
    return egl_stream;
}
#endif
