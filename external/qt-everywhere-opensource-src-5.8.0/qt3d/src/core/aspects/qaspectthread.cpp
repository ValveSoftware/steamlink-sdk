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

#include "qaspectthread_p.h"
#include "qaspectmanager_p.h"

#include <Qt3DCore/private/corelogging_p.h>
#include <QMutexLocker>

QT_BEGIN_NAMESPACE

namespace Qt3DCore {

QAspectThread::QAspectThread(QObject *parent)
    : QThread(parent),
      m_aspectManager(nullptr),
      m_semaphore(0)
{
    qCDebug(Aspects) << Q_FUNC_INFO;
}

QAspectThread::~QAspectThread()
{
}

void QAspectThread::waitForStart(Priority priority)
{
    qCDebug(Aspects) << "Starting QAspectThread and going to sleep until it is ready for us...";
    start(priority);
    m_semaphore.acquire();
    qCDebug(Aspects) << "QAspectThead is now ready & calling thread is now awake again";
}

void QAspectThread::run()
{
    qCDebug(Aspects) << "Entering void QAspectThread::run()";

    m_aspectManager = new QAspectManager;

    // Load and initialize the aspects and any other core services
    // Done before releasing condition to make sure that Qml Components
    // Are exposed prior to Qml Engine source being set
    m_aspectManager->initialize();

    // Wake up the calling thread now that our worker objects are ready for action
    m_semaphore.release();

    // Enter the main loop
    m_aspectManager->exec();

    // Clean up
    m_aspectManager->shutdown();

    // Delete the aspect manager while we're still in the thread
    delete m_aspectManager;

    qCDebug(Aspects) << "Exiting void QAspectThread::run()";
}

} // namespace Qt3DCore

QT_END_NAMESPACE
