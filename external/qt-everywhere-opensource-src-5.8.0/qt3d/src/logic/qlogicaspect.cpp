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

#include "qlogicaspect.h"
#include "qlogicaspect_p.h"
#include "executor_p.h"
#include "handler_p.h"
#include "manager_p.h"
#include "qframeaction.h"

#include <Qt3DCore/qnode.h>
#include <Qt3DCore/private/qchangearbiter_p.h>
#include <Qt3DCore/private/qscene_p.h>
#include <Qt3DCore/private/qservicelocator_p.h>

#include <QThread>
#include <QWindow>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DLogic {

/*!
 \class Qt3DLogic::QLogicAspect
 \inherits Qt3DCore::QAbstractAspect
 \inmodule Qt3DLogic
 \brief Responsible for handling frame synchronization jobs.
 \since 5.7
*/

QLogicAspectPrivate::QLogicAspectPrivate()
    : QAbstractAspectPrivate()
    , m_time(0)
    , m_initialized(false)
    , m_manager(new Logic::Manager)
    , m_executor(new Logic::Executor)
    , m_callbackJob(new Logic::CallbackJob)
{
    m_callbackJob->setManager(m_manager.data());
    m_manager->setExecutor(m_executor.data());
}

void QLogicAspectPrivate::onEngineAboutToShutdown()
{
    // Throw away any pending work that may deadlock during the shutdown procedure
    // when the main thread waits for any queued jobs to finish.
    m_executor->clearQueueAndProceed();
}

void QLogicAspectPrivate::registerBackendTypes()
{
    Q_Q(QLogicAspect);
    q->registerBackendType<QFrameAction>(QBackendNodeMapperPtr(new Logic::HandlerFunctor(m_manager.data())));
}

/*!
  Constructs a new Qt3DLogic::QLogicAspect instance with \a parent.
*/
QLogicAspect::QLogicAspect(QObject *parent)
    : QLogicAspect(*new QLogicAspectPrivate(), parent) {}

/*! \internal */
QLogicAspect::QLogicAspect(QLogicAspectPrivate &dd, QObject *parent)
    : QAbstractAspect(dd, parent)
{
    Q_D(QLogicAspect);
    setObjectName(QStringLiteral("Logic Aspect"));
    d->registerBackendTypes();
    d_func()->m_manager->setLogicAspect(this);
}

/*! \internal */
QLogicAspect::~QLogicAspect()
{
}

/*! \internal */
QVector<QAspectJobPtr> QLogicAspect::jobsToExecute(qint64 time)
{
    Q_D(QLogicAspect);
    const qint64 deltaTime = time - d->m_time;
    const float dt = static_cast<const float>(deltaTime) / 1.0e9;
    d->m_manager->setDeltaTime(dt);
    d->m_time = time;

    // Create jobs that will get exectued by the threadpool
    QVector<QAspectJobPtr> jobs;
    jobs.append(d->m_callbackJob);
    return jobs;
}

/*! \internal */
void QLogicAspect::onRegistered()
{
}

/*! \internal */
void QLogicAspect::onEngineStartup()
{
    Q_D(QLogicAspect);
    d->m_executor->setScene(d->m_arbiter->scene());
}

} // namespace Qt3DLogic

QT_END_NAMESPACE

QT3D_REGISTER_NAMESPACED_ASPECT("logic", QT_PREPEND_NAMESPACE(Qt3DLogic), QLogicAspect)

