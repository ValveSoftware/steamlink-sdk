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

#include "viewportnode_p.h"
#include <Qt3DRender/qviewport.h>
#include <Qt3DRender/private/qviewport_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {
namespace Render {

ViewportNode::ViewportNode()
    : FrameGraphNode(FrameGraphNode::Viewport)
    , m_xMin(0.0f)
    , m_yMin(0.0f)
    , m_xMax(1.0f)
    , m_yMax(1.0f)
{
}

void ViewportNode::initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change)
{
    FrameGraphNode::initializeFromPeer(change);
    const auto typedChange = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<QViewportData>>(change);
    const auto &data = typedChange->data;
    m_xMin = data.normalizedRect.x();
    m_xMax = data.normalizedRect.width();
    m_yMin = data.normalizedRect.y();
    m_yMax = data.normalizedRect.height();
}

float ViewportNode::xMin() const
{
    return m_xMin;
}

void ViewportNode::setXMin(float xMin)
{
    m_xMin = xMin;
}
float ViewportNode::yMin() const
{
    return m_yMin;
}

void ViewportNode::setYMin(float yMin)
{
    m_yMin = yMin;
}
float ViewportNode::xMax() const
{
    return m_xMax;
}

void ViewportNode::setXMax(float xMax)
{
    m_xMax = xMax;
}
float ViewportNode::yMax() const
{
    return m_yMax;
}

void ViewportNode::setYMax(float yMax)
{
    m_yMax = yMax;
}

void ViewportNode::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    if (e->type() == PropertyUpdated) {
        QPropertyUpdatedChangePtr propertyChange = qSharedPointerCast<QPropertyUpdatedChange>(e);
        if (propertyChange->propertyName() == QByteArrayLiteral("normalizedRect")) {
            QRectF normalizedRect = propertyChange->value().value<QRectF>();
            setXMin(normalizedRect.x());
            setYMin(normalizedRect.y());
            setXMax(normalizedRect.width());
            setYMax(normalizedRect.height());
        }
        markDirty(AbstractRenderer::AllDirty);
    }
    FrameGraphNode::sceneChangeEvent(e);
}

QRectF computeViewport(const QRectF &childViewport, const ViewportNode *parentViewport)
{
    QRectF vp(parentViewport->xMin(),
              parentViewport->yMin(),
              parentViewport->xMax(),
              parentViewport->yMax());

    if (childViewport.isEmpty()) {
        return vp;
    } else {
        return QRectF(vp.x() + vp.width() * childViewport.x(),
                      vp.y() + vp.height() * childViewport.y(),
                      vp.width() * childViewport.width(),
                      vp.height() * childViewport.height());
    }
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
