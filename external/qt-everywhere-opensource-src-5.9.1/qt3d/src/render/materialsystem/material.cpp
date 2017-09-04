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

#include "material_p.h"
#include "graphicscontext_p.h"
#include "technique_p.h"
#include "effect_p.h"
#include "qparameter.h"
#include "qtechnique.h"
#include "qmaterial.h"
#include "qeffect.h"
#include <Qt3DRender/private/qmaterial_p.h>

#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>

using namespace Qt3DCore;

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
namespace Render {

Material::Material()
    : BackendNode()
{
}

Material::~Material()
{
    cleanup();
}

void Material::cleanup()
{
    QBackendNode::setEnabled(false);
    m_parameterPack.clear();
}

void Material::initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change)
{
    const auto typedChange = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<QMaterialData>>(change);
    const auto &data = typedChange->data;
    m_effectUuid = data.effectId;
    m_parameterPack.setParameters(data.parameterIds);
}

void Material::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{

    switch (e->type()) {
    case PropertyUpdated: {
        const auto change = qSharedPointerCast<QPropertyUpdatedChange>(e);
        if (change->propertyName() == QByteArrayLiteral("effect"))
            m_effectUuid = change->value().value<QNodeId>();
        break;
    }

    case PropertyValueAdded: {
        const auto change = qSharedPointerCast<QPropertyNodeAddedChange>(e);
        if (change->propertyName() == QByteArrayLiteral("parameter"))
            m_parameterPack.appendParameter(change->addedNodeId());
        break;
    }

    case PropertyValueRemoved: {
        const auto change = qSharedPointerCast<QPropertyNodeRemovedChange>(e);
        if (change->propertyName() == QByteArrayLiteral("parameter"))
            m_parameterPack.removeParameter(change->removedNodeId());
        break;
    }

    default:
        break;
    }
    markDirty(AbstractRenderer::AllDirty);

    BackendNode::sceneChangeEvent(e);
}

QVector<Qt3DCore::QNodeId> Material::parameters() const
{
    return m_parameterPack.parameters();
}

Qt3DCore::QNodeId Material::effect() const
{
    return m_effectUuid;
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
