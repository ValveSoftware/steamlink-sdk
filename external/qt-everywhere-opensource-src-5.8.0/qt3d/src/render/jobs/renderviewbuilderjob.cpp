/****************************************************************************
**
** Copyright (C) 2016 Paul Lemire
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

#include "renderviewbuilderjob_p.h"
#include <Qt3DRender/private/job_common_p.h>
#include <Qt3DRender/private/renderer_p.h>
#include <Qt3DRender/private/renderview_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

namespace Render {

namespace {
int renderViewInstanceCounter = 0;
} // anonymous

RenderViewBuilderJob::RenderViewBuilderJob()
    : Qt3DCore::QAspectJob(),
      m_renderView(nullptr)
{
    SET_JOB_RUN_STAT_TYPE(this, JobTypes::RenderViewBuilder, renderViewInstanceCounter++);
}

void RenderViewBuilderJob::run()
{
    // Build RenderCommand should perform the culling as we have no way to determine
    // if a child has a mesh in the view frustum while its parent isn't contained in it.
    if (!m_renderView->noDraw()) {
#if defined(QT3D_RENDER_VIEW_JOB_TIMINGS)
        gatherLightsTime = timer.nsecsElapsed();
        timer.restart();
#endif
    if (!m_renderView->isCompute())
        m_commands = m_renderView->buildDrawRenderCommands(m_renderables);
    else
        m_commands = m_renderView->buildComputeRenderCommands(m_renderables);
#if defined(QT3D_RENDER_VIEW_JOB_TIMINGS)
        buildCommandsTime = timer.nsecsElapsed();
        timer.restart();
#endif
    }

#if defined(QT3D_RENDER_VIEW_JOB_TIMINGS)
    qint64 creationTime = timer.nsecsElapsed();
    timer.restart();
#endif

#if defined(QT3D_RENDER_VIEW_JOB_TIMINGS)
    qint64 sortTime = timer.nsecsElapsed();
#endif

#if defined(QT3D_RENDER_VIEW_JOB_TIMINGS)
    qDebug() << m_index
             << "state:" << gatherStateTime / 1.0e6
             << "lights:" << gatherLightsTime / 1.0e6
             << "build commands:" << buildCommandsTime / 1.0e6
             << "sort:" << sortTime / 1.0e6;
#endif


}

} // Render

} // Qt3DRender

QT_END_NAMESPACE
