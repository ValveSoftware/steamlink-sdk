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

#ifndef CHROMIUM_GPU_HELPER_H
#define CHROMIUM_GPU_HELPER_H

#include <QtGlobal> // We need this for the Q_OS_QNX define.
#include <QMap>

#include "base/callback.h"
#include "ui/gl/gl_fence.h"

namespace base {
class MessageLoop;
}

namespace content {
class SyncPointManager;
}

namespace gpu {
struct Mailbox;
namespace gles2 {
class MailboxManager;
class Texture;
}
}

// These functions wrap code that needs to include headers that are
// incompatible with Qt GL headers.
// From the outside, types from incompatible headers referenced in these
// functions should only be forward-declared and considered as opaque types.

QMap<uint32, gfx::TransferableFence> transferFences();
base::MessageLoop *gpu_message_loop();
content::SyncPointManager *sync_point_manager();
gpu::gles2::MailboxManager *mailbox_manager();

void AddSyncPointCallbackOnGpuThread(base::MessageLoop *gpuMessageLoop, content::SyncPointManager *syncPointManager, uint32 sync_point, const base::Closure& callback);
gpu::gles2::Texture* ConsumeTexture(gpu::gles2::MailboxManager *mailboxManager, unsigned target, const gpu::Mailbox& mailbox);
unsigned int service_id(gpu::gles2::Texture *tex);

#ifdef Q_OS_QNX
typedef void* EGLDisplay;
typedef void* EGLStreamKHR;

struct EGLStreamData {
    EGLDisplay egl_display;
    EGLStreamKHR egl_str_handle;

    EGLStreamData(): egl_display(NULL), egl_str_handle(NULL) {}
};

EGLStreamData eglstream_connect_consumer(gpu::gles2::Texture *tex);
#endif

#endif // CHROMIUM_GPU_HELPER_H
