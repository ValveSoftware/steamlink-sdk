/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
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

#include "attribute_p.h"
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DRender/qbuffer.h>
#include <Qt3DRender/private/qattribute_p.h>
#include <Qt3DRender/private/stringtoint_p.h>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {
namespace Render {

Attribute::Attribute()
    : BackendNode(ReadOnly)
    , m_nameId(0)
    , m_vertexBaseType(QAttribute::Float)
    , m_vertexSize(1)
    , m_count(0)
    , m_byteStride(0)
    , m_byteOffset(0)
    , m_divisor(0)
    , m_attributeType(QAttribute::VertexAttribute)
    , m_attributeDirty(false)
{
}

Attribute::~Attribute()
{
}

void Attribute::cleanup()
{
    m_vertexBaseType = QAttribute::Float;
    m_vertexSize = 1;
    m_count = 0;
    m_byteStride = 0;
    m_byteOffset = 0;
    m_divisor = 0;
    m_attributeType = QAttribute::VertexAttribute;
    m_bufferId = Qt3DCore::QNodeId();
    m_name.clear();
    m_attributeDirty = false;
    m_nameId = 0;
}

void Attribute::initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change)
{
    const auto typedChange = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<QAttributeData>>(change);
    const auto &data = typedChange->data;
    m_bufferId = data.bufferId;
    m_name = data.name;
    m_nameId = StringToInt::lookupId(m_name);
    m_vertexBaseType = data.vertexBaseType;
    m_vertexSize = data.vertexSize;
    m_count = data.count;
    m_byteStride = data.byteStride;
    m_byteOffset = data.byteOffset;
    m_divisor = data.divisor;
    m_attributeType = data.attributeType;
    m_attributeDirty = true;
}

void Attribute::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    switch (e->type()) {
    case PropertyUpdated: {
        QPropertyUpdatedChangePtr propertyChange = qSharedPointerCast<QPropertyUpdatedChange>(e);
        QByteArray propertyName = propertyChange->propertyName();

        if (propertyName == QByteArrayLiteral("name")) {
            m_name = propertyChange->value().value<QString>();
            m_nameId = StringToInt::lookupId(m_name);
            m_attributeDirty = true;
        } else if (propertyName == QByteArrayLiteral("vertexBaseType")) {
            m_vertexBaseType = static_cast<QAttribute::VertexBaseType>(propertyChange->value().value<int>());
            m_attributeDirty = true;
        } else if (propertyName == QByteArrayLiteral("vertexSize")) {
            m_vertexSize = propertyChange->value().value<uint>();
            m_attributeDirty = true;
        } else if (propertyName == QByteArrayLiteral("count")) {
            m_count = propertyChange->value().value<uint>();
            m_attributeDirty = true;
        } else if (propertyName == QByteArrayLiteral("byteStride")) {
            m_byteStride = propertyChange->value().value<uint>();
            m_attributeDirty = true;
        } else if (propertyName == QByteArrayLiteral("byteOffset")) {
            m_byteOffset = propertyChange->value().value<uint>();
            m_attributeDirty = true;
        } else if (propertyName == QByteArrayLiteral("divisor")) {
            m_divisor = propertyChange->value().value<uint>();
            m_attributeDirty = true;
        } else if (propertyName == QByteArrayLiteral("attributeType")) {
            m_attributeType = static_cast<QAttribute::AttributeType>(propertyChange->value().value<int>());
            m_attributeDirty = true;
        } else  if (propertyName == QByteArrayLiteral("buffer")) {
            m_bufferId = propertyChange->value().value<QNodeId>();
            m_attributeDirty = true;
        }
        markDirty(AbstractRenderer::AllDirty);
        break;
    }

    default:
        break;
    }
    BackendNode::sceneChangeEvent(e);
}

void Attribute::unsetDirty()
{
    m_attributeDirty = false;
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
