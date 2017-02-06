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

#include <Qt3DRender/private/effect_p.h>
#include <Qt3DRender/private/renderer_p.h>
#include <Qt3DRender/qeffect.h>
#include <Qt3DRender/qparameter.h>
#include <Qt3DRender/private/qeffect_p.h>

#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>

#include <QVariant>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {
namespace Render {

Effect::Effect()
    : BackendNode()
{
}

Effect::~Effect()
{
    cleanup();
}

void Effect::cleanup()
{
    QBackendNode::setEnabled(false);
    m_parameterPack.clear();
    m_techniques.clear();
}

void Effect::initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change)
{
    const auto typedChange = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<QEffectData>>(change);
    const auto &data = typedChange->data;
    m_techniques = data.techniqueIds;
    m_parameterPack.setParameters(data.parameterIds);
}

void Effect::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    switch (e->type()) {
    case PropertyValueAdded: {
        const auto change = qSharedPointerCast<QPropertyNodeAddedChange>(e);
        if (change->propertyName() == QByteArrayLiteral("technique"))
            appendRenderTechnique(change->addedNodeId());
        else if (change->propertyName() == QByteArrayLiteral("parameter"))
            m_parameterPack.appendParameter(change->addedNodeId());
        break;
    }

    case PropertyValueRemoved: {
        const auto change = qSharedPointerCast<QPropertyNodeRemovedChange>(e);
        if (change->propertyName() == QByteArrayLiteral("technique"))
            m_techniques.removeOne(change->removedNodeId());
        else if (change->propertyName() == QByteArrayLiteral("parameter"))
            m_parameterPack.removeParameter(change->removedNodeId());
        break;
    }

    default:
        break;
    }

    markDirty(AbstractRenderer::AllDirty);
    BackendNode::sceneChangeEvent(e);
}

void Effect::appendRenderTechnique(Qt3DCore::QNodeId technique)
{
    if (!m_techniques.contains(technique))
        m_techniques.append(technique);
}

QVector<Qt3DCore::QNodeId> Effect::techniques() const
{
    return m_techniques;
}

QVector<Qt3DCore::QNodeId> Effect::parameters() const
{
    return m_parameterPack.parameters();
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
