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

#ifndef CHROMIUM_GPU_HELPER_H
#define CHROMIUM_GPU_HELPER_H

#include <QtGlobal> // We need this for the Q_OS_QNX define.
#include <QMap>

namespace base {
class MessageLoop;
}

namespace gpu {
struct Mailbox;
class SyncPointManager;
namespace gles2 {
class MailboxManager;
class TextureBase;
}
}

// These functions wrap code that needs to include headers that are
// incompatible with Qt GL headers.
// From the outside, types from incompatible headers referenced in these
// functions should only be forward-declared and considered as opaque types.

base::MessageLoop *gpu_message_loop();
gpu::SyncPointManager *sync_point_manager();
gpu::gles2::MailboxManager *mailbox_manager();

gpu::gles2::TextureBase* ConsumeTexture(gpu::gles2::MailboxManager *mailboxManager, unsigned target, const gpu::Mailbox& mailbox);
unsigned int service_id(gpu::gles2::TextureBase *tex);

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
