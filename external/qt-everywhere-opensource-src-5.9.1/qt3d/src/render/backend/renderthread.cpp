/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include "renderthread_p.h"
#include <Qt3DRender/private/renderer_p.h>
#include <Qt3DRender/private/renderview_p.h>

#include <Qt3DRender/private/renderlogging_p.h>
#include <QEventLoop>
#include <QTime>
#include <QMutexLocker>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

namespace Render {

RenderThread::RenderThread(Renderer *renderer)
    : QThread(),
      m_renderer(renderer),
      m_semaphore(0)
{
}

RenderThread::~RenderThread()
{
}

// Called by Renderer in the context of the Aspect Thread
void RenderThread::waitForStart( Priority priority )
{
    qCDebug(Render::Backend) << "Starting Render thread and then going to sleep until it is ready for us...";
    start( priority );
    m_semaphore.acquire();
    qCDebug(Render::Backend) << "Render thread is now ready & calling thread is now awake again";
}

// RenderThread context
void RenderThread::run()
{
    // We lock the renderer's mutex here before returning control to the calling
    // thread (the Aspect Thread). This is
    // to ensure that the Renderer's initialize() waitCondition is reached before
    // other threads try to wake it up. This is guaranteed by having the
    // Renderer::setSurface() function try to lock the renderer's mutex too.
    // That function will block until the mutex is unlocked by the wait condition
    // in the initialize() call below.
    QMutexLocker locker(m_renderer->mutex());

    // Now we have ensured we will reach the wait condition as described above,
    // return control to the aspect thread that created us.
    m_semaphore.release();

    // This call to Renderer::initialize() waits for a surface to be set on the
    // renderer in the context of the Aspect Thread
    m_renderer->initialize();
    locker.unlock();

    // Enter the main OpenGL submission loop.
    m_renderer->render();

    // Clean up any OpenGL resources
    m_renderer->releaseGraphicsResources();

    qCDebug(Render::Backend) << "Exiting RenderThread";
}

} // namespace Render

} // namespace Qt3DRender

QT_END_NAMESPACE
