/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
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

#include "manager_p.h"
#include "qlogicaspect.h"
#include <Qt3DCore/private/qabstractaspect_p.h>
#include <Qt3DCore/private/qaspectmanager_p.h>
#include <Qt3DLogic/private/executor_p.h>
#include <Qt3DLogic/private/managers_p.h>
#include <QtCore/qcoreapplication.h>

#include <QDebug>
#include <QThread>

QT_BEGIN_NAMESPACE

namespace Qt3DLogic {
namespace Logic {

Manager::Manager()
    : m_logicHandlerManager(new HandlerManager)
    , m_semaphore(1)
    , m_dt(0.0f)
{
    m_semaphore.acquire();
}

Manager::~Manager()
{
}

void Manager::setExecutor(Executor *executor)
{
    m_executor = executor;
    if (m_executor)
        m_executor->setSemephore(&m_semaphore);
}

void Manager::appendHandler(Handler *handler)
{
    HHandler handle = m_logicHandlerManager->lookupHandle(handler->peerId());
    m_logicHandlers.append(handle);
    m_logicComponentIds.append(handler->peerId());
}

void Manager::removeHandler(Qt3DCore::QNodeId id)
{
    HHandler handle = m_logicHandlerManager->lookupHandle(id);
    m_logicComponentIds.removeAll(id);
    m_logicHandlers.removeAll(handle);
    m_logicHandlerManager->releaseResource(id);
}

void Manager::triggerLogicFrameUpdates()
{
    Q_ASSERT(m_executor);

    // Don't use blocking queued connections to main thread if it is already
    // in the process of shutting down as that will deadlock.
    if (Qt3DCore::QAbstractAspectPrivate::get(m_logicAspect)->m_aspectManager->isShuttingDown())
        return;

    // Trigger the main thread to process logic frame updates for each
    // logic component and then wait until done. The Executor will
    // release the semaphore when it has completed its work.
    m_executor->enqueueLogicFrameUpdates(m_logicComponentIds);
    qApp->postEvent(m_executor, new FrameUpdateEvent(m_dt));
    m_semaphore.acquire();
}

} // namespace Logic
} // namespace Qt3DLogic

QT_END_NAMESPACE
