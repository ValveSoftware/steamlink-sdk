/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "sendrendercapturejob_p.h"

#include "Qt3DRender/private/renderer_p.h"
#include "Qt3DRender/private/nodemanagers_p.h"
#include "Qt3DRender/private/rendercapture_p.h"
#include <Qt3DRender/private/job_common_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

namespace Render {

SendRenderCaptureJob::SendRenderCaptureJob(Renderer *renderer)
    : Qt3DCore::QAspectJob()
    , m_renderer(renderer)
{
    SET_JOB_RUN_STAT_TYPE(this, JobTypes::SendRenderCapture, 0);
}

SendRenderCaptureJob::~SendRenderCaptureJob()
{

}

void SendRenderCaptureJob::setManagers(NodeManagers *managers)
{
    m_managers = managers;
}

void SendRenderCaptureJob::run()
{
    const auto pendingCaptures = m_renderer->takePendingRenderCaptureSendRequests();
    for (Qt3DCore::QNodeId id : pendingCaptures) {
        auto *node = static_cast<Qt3DRender::Render::RenderCapture *>
                                    (m_managers->frameGraphManager()->lookupNode(id));
        node->sendRenderCaptures();
    }
}

} // Render

} // Qt3DRender

QT_END_NAMESPACE
