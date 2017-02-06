/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "light_p.h"
#include "qabstractlight.h"
#include "qabstractlight_p.h"
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <private/abstractrenderer_p.h>
#include <private/nodemanagers_p.h>
#include <private/qbackendnode_p.h>
#include <private/managers_p.h>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {
namespace Render {

QNodeId Light::shaderData() const
{
    return m_shaderDataId;
}

void Light::initializeFromPeer(const QNodeCreatedChangeBasePtr &change)
{
    const auto typedChange = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<QAbstractLightData>>(change);
    const auto &data = typedChange->data;
    m_shaderDataId = data.shaderDataId;
}

RenderLightFunctor::RenderLightFunctor(AbstractRenderer *renderer, NodeManagers *managers)
    : m_managers(managers)
    , m_renderer(renderer)
{
}

Qt3DCore::QBackendNode *RenderLightFunctor::create(const Qt3DCore::QNodeCreatedChangeBasePtr &change) const
{
    Light *backend = m_managers->lightManager()->getOrCreateResource(change->subjectId());
    backend->setRenderer(m_renderer);
    return backend;
}

Qt3DCore::QBackendNode *RenderLightFunctor::get(Qt3DCore::QNodeId id) const
{
    return m_managers->lightManager()->lookupResource(id);
}

void RenderLightFunctor::destroy(Qt3DCore::QNodeId id) const
{
    m_managers->lightManager()->releaseResource(id);
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
