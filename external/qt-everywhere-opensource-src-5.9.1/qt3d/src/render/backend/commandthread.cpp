/****************************************************************************
**
** Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
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

#include "commandthread_p.h"
#include <QOpenGLContext>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

namespace Render {

CommandThread::CommandThread(Renderer *renderer)
    : QThread()
    , m_renderer(renderer)
    , m_waitForStartSemaphore(0)
    , m_initializedSemaphore(0)
{
}

CommandThread::~CommandThread()
{
}

// Called by RenderThread or MainThread (Scene3d)
void CommandThread::initialize(QOpenGLContext *mainContext)
{
    // Start the thread
    start();

    // Wait for thread to be started
    m_waitForStartSemaphore.acquire();

    m_mainContext = mainContext;
    Q_ASSERT(m_mainContext);

    // Allow thread to proceed
    m_initializedSemaphore.release();
}

// Called by RenderThread of MainThread (Scene3D)
void CommandThread::shutdown()
{
    // Tell thread to exit event loop
    QThread::quit();

    // Wait for thread to exit
    wait();

    // Reset semaphores (in case we ever want to restart)
    m_waitForStartSemaphore.release(m_waitForStartSemaphore.available());
    m_initializedSemaphore.release(m_initializedSemaphore.available());
    m_localContext.reset();
}

// Any thread can call this, this is a blocking command
void CommandThread::executeCommand(Command *command)
{
    if (!isRunning())
        return;
    QMetaObject::invokeMethod(this,
                              "executeCommandInternal",
                              Qt::BlockingQueuedConnection,
                              Q_ARG(Command*, command));
}

void CommandThread::run()
{
    // Allow initialize to proceed
    m_waitForStartSemaphore.release();

    // Wait for initialize to be completed
    m_initializedSemaphore.acquire();

    // Initialize shared context and resources for the thread
    m_localContext.reset(new QOpenGLContext());
    m_localContext->setShareContext(m_mainContext);

    // Launch exec loop
    QThread::exec();
}

// Executed in the Command Thread
void CommandThread::executeCommandInternal(Command *command)
{
    command->execute(m_renderer, m_localContext.data());
}

} // Render

} // Qt3DRender

QT_END_NAMESPACE
