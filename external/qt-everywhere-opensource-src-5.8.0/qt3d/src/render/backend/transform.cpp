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

#include "transform_p.h"

#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/private/qchangearbiter_p.h>
#include <Qt3DCore/qtransform.h>
#include <Qt3DCore/private/qtransform_p.h>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {
namespace Render {

Transform::Transform()
    : BackendNode()
    , m_rotation()
    , m_scale(1.0f, 1.0f, 1.0f)
    , m_translation()
{
}

void Transform::cleanup()
{
    m_rotation = QQuaternion();
    m_scale = QVector3D();
    m_translation = QVector3D();
    m_transformMatrix = QMatrix4x4();
    QBackendNode::setEnabled(false);
}

void Transform::initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change)
{
    const auto typedChange = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<QTransformData>>(change);
    const auto &data = typedChange->data;
    m_rotation = data.rotation;
    m_scale = data.scale;
    m_translation = data.translation;
    updateMatrix();
}

QMatrix4x4 Transform::transformMatrix() const
{
    return m_transformMatrix;
}

QVector3D Transform::scale() const
{
    return m_scale;
}

QQuaternion Transform::rotation() const
{
    return m_rotation;
}

QVector3D Transform::translation() const
{
    return m_translation;
}

void Transform::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    // TODO: Flag the matrix as dirty and update all matrices batched in a job
    if (e->type() == PropertyUpdated) {
        const QPropertyUpdatedChangePtr &propertyChange = qSharedPointerCast<QPropertyUpdatedChange>(e);
        if (propertyChange->propertyName() == QByteArrayLiteral("scale3D")) {
            m_scale = propertyChange->value().value<QVector3D>();
            updateMatrix();
        } else if (propertyChange->propertyName() == QByteArrayLiteral("rotation")) {
            m_rotation = propertyChange->value().value<QQuaternion>();
            updateMatrix();
        } else if (propertyChange->propertyName() == QByteArrayLiteral("translation")) {
            m_translation = propertyChange->value().value<QVector3D>();
            updateMatrix();
        }
    }
    markDirty(AbstractRenderer::TransformDirty);

    BackendNode::sceneChangeEvent(e);
}

void Transform::updateMatrix()
{
    QMatrix4x4 m;
    m.translate(m_translation);
    m.rotate(m_rotation);
    m.scale(m_scale);
    m_transformMatrix = m;
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
