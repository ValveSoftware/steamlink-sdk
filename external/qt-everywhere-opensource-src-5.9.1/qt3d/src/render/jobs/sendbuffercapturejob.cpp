/****************************************************************************
**
** Copyright (C) 2017 Juan José Casafranca
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

#include "sendbuffercapturejob_p.h"


#include "Qt3DRender/private/renderer_p.h"
#include "Qt3DRender/private/nodemanagers_p.h"
#include <Qt3DRender/private/job_common_p.h>
#include <Qt3DRender/private/buffer_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

namespace Render {

SendBufferCaptureJob::SendBufferCaptureJob()
    : Qt3DCore::QAspectJob()
{
    SET_JOB_RUN_STAT_TYPE(this, JobTypes::SendBufferCapture, 0);
}

SendBufferCaptureJob::~SendBufferCaptureJob()
{
}

void SendBufferCaptureJob::setManagers(NodeManagers *managers)
{
    m_managers = managers;
}

void SendBufferCaptureJob::addRequest(QPair<Buffer *, QByteArray> request)
{
    QMutexLocker locker(&m_mutex);
    m_pendingSendBufferCaptures.push_back(request);
}

void SendBufferCaptureJob::run()
{
    QMutexLocker locker(&m_mutex);
    for (const QPair<Buffer*, QByteArray> &pendingCapture : qAsConst(m_pendingSendBufferCaptures)) {
        pendingCapture.first->updateDataFromGPUToCPU(pendingCapture.second);
    }

    m_pendingSendBufferCaptures.clear();
}

} // Render

} // Qt3DRender

QT_END_NAMESPACE
