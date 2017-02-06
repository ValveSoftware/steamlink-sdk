/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
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

#include "framegraphvisitor_p.h"

#include "framegraphnode_p.h"
#include <Qt3DRender/private/renderer_p.h>
#include <Qt3DRender/private/managers_p.h>
#include <Qt3DRender/private/renderviewbuilder_p.h>
#include <QThreadPool>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {
namespace Render {

FrameGraphVisitor::FrameGraphVisitor(Renderer *renderer,
                                     const FrameGraphManager *manager)
    : m_renderer(renderer)
    , m_manager(manager)
    , m_jobs(nullptr)
    , m_renderviewIndex(0)

{
}

void FrameGraphVisitor::traverse(FrameGraphNode *root,
                                 QVector<Qt3DCore::QAspectJobPtr> *jobs)
{
    m_jobs = jobs;
    m_renderviewIndex = 0;

    Q_ASSERT(m_renderer);
    Q_ASSERT(m_jobs);
    Q_ASSERT_X(root, Q_FUNC_INFO, "The FrameGraphRoot is null");

    // Kick off the traversal
    Render::FrameGraphNode *node = root;
    if (node == nullptr)
        qCritical() << Q_FUNC_INFO << "FrameGraph is null";
    visit(node);
}

void FrameGraphVisitor::visit(Render::FrameGraphNode *node)
{
    // TO DO: check if node is a subtree selector
    // in which case, we only visit the subtrees returned
    // by the selector functor and not all the children
    // as we would otherwise do

    // Recurse to children (if we have any), otherwise if this is a leaf node,
    // initiate a rendering from the current camera
    const QVector<Qt3DCore::QNodeId> fgChildIds = node->childrenIds();

    for (const Qt3DCore::QNodeId fgChildId : fgChildIds)
        visit(m_manager->lookupNode(fgChildId));

    // Leaf node - create a RenderView ready to be populated
    // TODO: Pass in only framegraph config that has changed from previous
    // index RenderViewJob.
    if (fgChildIds.empty()) {
        RenderViewBuilder builder(node, m_renderviewIndex++, m_renderer);
        m_jobs->append(builder.buildJobHierachy());
    }
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
