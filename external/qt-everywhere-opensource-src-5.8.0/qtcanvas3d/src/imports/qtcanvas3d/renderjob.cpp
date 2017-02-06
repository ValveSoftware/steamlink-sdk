/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCanvas3D module of the Qt Toolkit.
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

#include "renderjob_p.h"
#include "canvasrenderer_p.h"
#include "canvas3d_p.h"

#include <QtGui/QOpenGLContext>

QT_BEGIN_NAMESPACE
QT_CANVAS3D_BEGIN_NAMESPACE

/*!
 * The CanvasRenderJob class is for synchronous render jobs used by QtCanvas3D.
 * It takes care of synchronization.
 */
CanvasRenderJob::CanvasRenderJob(GlSyncCommand *command,
                                 QMutex *mutex, QWaitCondition *condition,
                                 CanvasRenderer *renderer, bool *deleted)
    : m_command(command),
      m_mutex(mutex),
      m_condition(condition),
      m_renderer(renderer),
      m_deleted(deleted),
      m_creationThread(QThread::currentThread())
{
}

CanvasRenderJob::~CanvasRenderJob()
{
    if (m_creationThread != QThread::currentThread()) {
        notifyGuiThread();
    } else {
        // If the job is deleted in the thread it was created in, we can assume the scheduling
        // failed and notify the context that we got deleted via the m_deleted flag,
        // so it won't start waiting for job to complete.
        *m_deleted = true;
    }
}

void CanvasRenderJob::run()
{
    if (m_renderer && m_renderer->contextCreated()) {
        // Move commands to execution queue
        m_renderer->transferCommands();

        // Change to canvas context, if needed
        QOpenGLContext *oldContext(0);
        QSurface *oldSurface(0);
        if (!m_renderer->usingQtContext()) {
            oldContext = QOpenGLContext::currentContext();
            if (oldContext)
                oldSurface = oldContext->surface();

            m_renderer->makeCanvasContextCurrent();
        }

        // Execute pending queue
        m_renderer->executeCommandQueue();

        // Execute synchronous command (if there is one)
        if (m_command)
            m_renderer->executeSyncCommand(*m_command);

        // Restore Qt context
        if (m_renderer->usingQtContext()) {
            m_renderer->resetQtOpenGLState();
        } else {
            if (oldContext && oldSurface) {
                if (!oldContext->makeCurrent(oldSurface)) {
                    qCWarning(canvas3drendering).nospace() << "CanvasRenderJob::" << __FUNCTION__
                                                           << " Failed to make old surface current";
                }
            }
        }
    }

    notifyGuiThread();
}

void CanvasRenderJob::notifyGuiThread()
{
    if (m_mutex) {
        m_mutex->lock();
        m_condition->wakeOne();
        m_mutex->unlock();
        m_mutex = 0;
    }
}

QT_CANVAS3D_END_NAMESPACE
QT_END_NAMESPACE
