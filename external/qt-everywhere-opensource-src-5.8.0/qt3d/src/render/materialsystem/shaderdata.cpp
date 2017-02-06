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

#include "shaderdata_p.h"
#include "qshaderdata.h"
#include "qshaderdata_p.h"
#include <QMetaProperty>
#include <QMetaObject>
#include <Qt3DCore/qdynamicpropertyupdatedchange.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <private/graphicscontext_p.h>
#include <private/qbackendnode_p.h>
#include <private/glbuffer_p.h>
#include <private/managers_p.h>
#include <private/nodemanagers_p.h>
#include <private/renderviewjobutils_p.h>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {
namespace Render {

namespace {

const int qNodeIdTypeId = qMetaTypeId<Qt3DCore::QNodeId>();

}

QVector<Qt3DCore::QNodeId> ShaderData::m_updatedShaderData;

ShaderData::ShaderData()
    : m_managers(nullptr)
{
}

ShaderData::~ShaderData()
{
}

void ShaderData::setManagers(NodeManagers *managers)
{
    m_managers = managers;
}

void ShaderData::initializeFromPeer(const QNodeCreatedChangeBasePtr &change)
{
    const auto typedChange = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<QShaderDataData>>(change);
    const QShaderDataData &data = typedChange->data;

    m_propertyReader = data.propertyReader;

    for (const QPair<QByteArray, QVariant> &entry : data.properties) {
        if (entry.first == QByteArrayLiteral("data") ||
                entry.first == QByteArrayLiteral("childNodes")) // We don't handle default Node properties
            continue;
        const QVariant &propertyValue = entry.second;
        const QString propertyName = QString::fromLatin1(entry.first);

        m_originalProperties.insert(propertyName, propertyValue);

        // We check if the property is a QNodeId or QVector<QNodeId> so that we can
        // check nested QShaderData for update
        if (propertyValue.userType() == qNodeIdTypeId) {
            m_nestedShaderDataProperties.insert(propertyName, propertyValue);
        } else if (propertyValue.userType() == QMetaType::QVariantList) {
            QVariantList list = propertyValue.value<QVariantList>();
            if (list.count() > 0 && list.at(0).userType() == qNodeIdTypeId)
                m_nestedShaderDataProperties.insert(propertyName, propertyValue);
        }
    }

    // We look for transformed properties once the complete hash of
    // originalProperties is available
    QHash<QString, QVariant>::iterator it = m_originalProperties.begin();
    const QHash<QString, QVariant>::iterator end = m_originalProperties.end();

    while (it != end) {
        if (it.value().type() == QVariant::Vector3D) {
            // if there is a matching QShaderData::TransformType propertyTransformed
            QVariant value = m_originalProperties.value(it.key() + QLatin1String("Transformed"));
            // if that's the case, we apply a space transformation to the property
            if (value.isValid() && value.type() == QVariant::Int)
                m_transformedProperties.insert(it.key(), static_cast<TransformType>(value.toInt()));
        }
        ++it;
    }
}


ShaderData *ShaderData::lookupResource(NodeManagers *managers, QNodeId id)
{
    return managers->shaderDataManager()->lookupResource(id);
}

ShaderData *ShaderData::lookupResource(QNodeId id)
{
    return ShaderData::lookupResource(m_managers, id);
}

// Call by cleanup job (single thread)
void ShaderData::clearUpdatedProperties()
{
    // DISABLED: Is only useful when building UBO from a ShaderData, which is disable since 5.7
    //    const QHash<QString, QVariant>::const_iterator end = m_nestedShaderDataProperties.end();
    //    QHash<QString, QVariant>::const_iterator it = m_nestedShaderDataProperties.begin();

    //    while (it != end) {
    //        if (it.value().userType() == QMetaType::QVariantList) {
    //            const auto values = it.value().value<QVariantList>();
    //            for (const QVariant &v : values) {
    //                ShaderData *nested = lookupResource(v.value<QNodeId>());
    //                if (nested != nullptr)
    //                    nested->clearUpdatedProperties();
    //            }
    //        } else {
    //            ShaderData *nested = lookupResource(it.value().value<QNodeId>());
    //            if (nested != nullptr)
    //                nested->clearUpdatedProperties();
    //        }
    //        ++it;
    //    }
}

void ShaderData::cleanup(NodeManagers *managers)
{
    Q_UNUSED(managers)
    // DISABLED: Is only useful when building UBO from a ShaderData, which is disable since 5.7
    //    for (Qt3DCore::QNodeId id : qAsConst(m_updatedShaderData)) {
    //        ShaderData *shaderData = ShaderData::lookupResource(managers, id);
    //        if (shaderData)
    //            shaderData->clearUpdatedProperties();
    //    }
    m_updatedShaderData.clear();
}

QVariant ShaderData::getTransformedProperty(const QString &name, const QMatrix4x4 &viewMatrix)
{
    // Note protecting m_worldMatrix at this point as we assume all world updates
    // have been performed when reaching this point
    auto it = m_transformedProperties.find(name);
    if (it != m_transformedProperties.end()) {
        const TransformType transformType = it.value();
        switch (transformType) {
        case ModelToEye:
            return QVariant::fromValue(viewMatrix * m_worldMatrix * m_originalProperties.value(name).value<QVector3D>());
        case ModelToWorld:
            return QVariant::fromValue(m_worldMatrix * m_originalProperties.value(it.key()).value<QVector3D>());
        case ModelToWorldDirection:
            return QVariant::fromValue((m_worldMatrix * QVector4D(m_originalProperties.value(it.key()).value<QVector3D>(), 0.0f)).toVector3D());
        case NoTransform:
            break;
        }
    }
    return QVariant();
}

// Called by FramePreparationJob or by RenderView when dealing with lights
void ShaderData::updateWorldTransform(const QMatrix4x4 &worldMatrix)
{
    QMutexLocker lock(&m_mutex);
    if (m_worldMatrix != worldMatrix) {
        m_worldMatrix = worldMatrix;
    }
}

// This will add the ShaderData to be cleared from updates at the end of the frame
// by the cleanup job
// Called by renderview jobs (several concurrent threads)
void ShaderData::markDirty()
{
    QMutexLocker lock(&m_mutex);
    if (!ShaderData::m_updatedShaderData.contains(peerId()))
        ShaderData::m_updatedShaderData.append(peerId());
}

/*!
  \internal
  Lookup if the current ShaderData or a nested ShaderData has updated properties.
  UpdateProperties contains either the value of the propertie of a QNodeId if it's another ShaderData.
  Transformed properties are updated for all of ShaderData that have ones at the point.

  \note This needs to be performed for every top level ShaderData every time it is used.
  As we don't know if the transformed properties use the same viewMatrix for all RenderViews.
 */

ShaderData::TransformType ShaderData::propertyTransformType(const QString &name) const
{
    return m_transformedProperties.value(name, TransformType::NoTransform);
}

void ShaderData::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    if (!m_propertyReader.isNull() && e->type() == PropertyUpdated) {
        QString propertyName;
        QVariant propertyValue;

        if (auto propertyChange = qSharedPointerDynamicCast<QPropertyUpdatedChange>(e)) {
            propertyName = QString::fromLatin1(propertyChange->propertyName());
            propertyValue =  m_propertyReader->readProperty(propertyChange->value());
        } else if (auto propertyChange = qSharedPointerDynamicCast<QDynamicPropertyUpdatedChange>(e)) {
            propertyName = QString::fromLatin1(propertyChange->propertyName());
            propertyValue = m_propertyReader->readProperty(propertyChange->value());
        } else {
            Q_UNREACHABLE();
        }

        // Note we aren't notified about nested QShaderData in this call
        // only scalar / vec properties
        m_originalProperties.insert(propertyName, propertyValue);
        BackendNode::markDirty(AbstractRenderer::AllDirty);
    }

    BackendNode::sceneChangeEvent(e);
}

RenderShaderDataFunctor::RenderShaderDataFunctor(AbstractRenderer *renderer, NodeManagers *managers)
    : m_managers(managers)
    , m_renderer(renderer)
{
}

Qt3DCore::QBackendNode *RenderShaderDataFunctor::create(const Qt3DCore::QNodeCreatedChangeBasePtr &change) const
{
    ShaderData *backend = m_managers->shaderDataManager()->getOrCreateResource(change->subjectId());
    backend->setManagers(m_managers);
    backend->setRenderer(m_renderer);
    return backend;
}

Qt3DCore::QBackendNode *RenderShaderDataFunctor::get(Qt3DCore::QNodeId id) const
{
    return m_managers->shaderDataManager()->lookupResource(id);
}

void RenderShaderDataFunctor::destroy(Qt3DCore::QNodeId id) const
{
    m_managers->shaderDataManager()->releaseResource(id);
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
