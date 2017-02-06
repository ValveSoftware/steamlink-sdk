/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "testrenderer.h"

QT_BEGIN_NAMESPACE

TestRenderer::TestRenderer()
    : m_changes(0)
{
}

TestRenderer::~TestRenderer()
{
}

void TestRenderer::markDirty(Qt3DRender::Render::AbstractRenderer::BackendNodeDirtySet changes, Qt3DRender::Render::BackendNode *node)
{
    Q_UNUSED(node);
    m_changes |= changes;
}

Qt3DRender::Render::AbstractRenderer::BackendNodeDirtySet TestRenderer::dirtyBits()
{
    return m_changes;
}

void TestRenderer::clearDirtyBits(Qt3DRender::Render::AbstractRenderer::BackendNodeDirtySet changes)
{
    m_changes &= changes;
}

void TestRenderer::resetDirty()
{
    m_changes = 0;
}

QVariant TestRenderer::executeCommand(const QStringList &args)
{
    Q_UNUSED(args)
    return QVariant();
}

void TestRenderer::setOffscreenSurfaceHelper(Qt3DRender::Render::OffscreenSurfaceHelper *helper)
{
    Q_UNUSED(helper);
}

QSurfaceFormat TestRenderer::format()
{
    return QSurfaceFormat();
}

QT_END_NAMESPACE
