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

#include "clearbuffers_p.h"
#include <Qt3DRender/private/qclearbuffers_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {
namespace Render {

static QVector4D vec4dFromColor(const QColor &color)
{
    return QVector4D(color.redF(), color.greenF(), color.blueF(), color.alphaF());
}

ClearBuffers::ClearBuffers()
    : FrameGraphNode(FrameGraphNode::ClearBuffers)
    , m_type(QClearBuffers::None)
    , m_clearDepthValue(1.f)
    , m_clearStencilValue(0)
{
}

void ClearBuffers::initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change)
{
    FrameGraphNode::initializeFromPeer(change);
    const auto typedChange = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<QClearBuffersData>>(change);
    const auto &data = typedChange->data;
    m_type = data.buffersType;
    m_clearColorAsColor = data.clearColor;
    m_clearColor = vec4dFromColor(m_clearColorAsColor);
    m_clearDepthValue = data.clearDepthValue;
    m_clearStencilValue = data.clearStencilValue;
    m_colorBufferId = data.bufferId;
}

void ClearBuffers::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    if (e->type() == PropertyUpdated) {
        QPropertyUpdatedChangePtr propertyChange = qSharedPointerCast<QPropertyUpdatedChange>(e);
        if (propertyChange->propertyName() == QByteArrayLiteral("buffers")) {
            m_type = static_cast<QClearBuffers::BufferType>(propertyChange->value().toInt());
        } else if (propertyChange->propertyName() == QByteArrayLiteral("clearColor")) {
            m_clearColorAsColor = propertyChange->value().value<QColor>();
            m_clearColor = vec4dFromColor(m_clearColorAsColor);
        } else if (propertyChange->propertyName() == QByteArrayLiteral("clearDepthValue")) {
            m_clearDepthValue = propertyChange->value().toFloat();
        } else if (propertyChange->propertyName() == QByteArrayLiteral("clearStencilValue")) {
            m_clearStencilValue = propertyChange->value().toInt();
        } else if (propertyChange->propertyName() == QByteArrayLiteral("colorBuffer")) {
            m_colorBufferId = propertyChange->value().value<QNodeId>();
        }
        markDirty(AbstractRenderer::AllDirty);
    }
    FrameGraphNode::sceneChangeEvent(e);
}

QClearBuffers::BufferType ClearBuffers::type() const
{
    return m_type;
}

QColor ClearBuffers::clearColorAsColor() const
{
    return m_clearColorAsColor;
}

QVector4D ClearBuffers::clearColor() const
{
    return m_clearColor;
}

float ClearBuffers::clearDepthValue() const
{
    return m_clearDepthValue;
}

int ClearBuffers::clearStencilValue() const
{
    return m_clearStencilValue;
}

Qt3DCore::QNodeId ClearBuffers::bufferId() const
{
    return m_colorBufferId;
}

bool ClearBuffers::clearsAllColorBuffers() const
{
    return m_colorBufferId.isNull();
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
