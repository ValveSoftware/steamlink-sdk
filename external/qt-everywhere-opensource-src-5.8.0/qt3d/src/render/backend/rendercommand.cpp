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

#include "rendercommand_p.h"

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
namespace Render {

RenderCommand::RenderCommand()
    : m_stateSet(nullptr)
    , m_depth(0.0f)
    , m_changeCost(0)
    , m_type(RenderCommand::Draw)
    , m_sortBackToFront(false)
    , m_primitiveCount(0)
    , m_primitiveType(QGeometryRenderer::Triangles)
    , m_restartIndexValue(-1)
    , m_firstInstance(0)
    , m_firstVertex(0)
    , m_verticesPerPatch(0)
    , m_instanceCount(0)
    , m_indexOffset(0)
    , m_indexAttributeByteOffset(0)
    , m_indexAttributeDataType(GL_UNSIGNED_SHORT)
    , m_drawIndexed(false)
    , m_primitiveRestartEnabled(false)
    , m_isValid(false)
{
   m_sortingType.global = 0;
   m_workGroups[0] = 0;
   m_workGroups[1] = 0;
   m_workGroups[2] = 0;
}

bool compareCommands(RenderCommand *r1, RenderCommand *r2)
{
    // The smaller m_depth is, the closer it is to the eye.
    if (r1->m_sortBackToFront && r2->m_sortBackToFront)
        return r1->m_depth > r2->m_depth;

    return r1->m_sortingType.global < r2->m_sortingType.global;
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
