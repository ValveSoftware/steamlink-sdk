/****************************************************************************
**
** Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "lerpclipblend_p.h"
#include <Qt3DAnimation/qclipblendnodecreatedchange.h>
#include <Qt3DAnimation/private/qlerpclipblend_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DAnimation {

namespace Animation {

LerpClipBlend::LerpClipBlend()
    : ClipBlendNode(ClipBlendNode::LerpBlendType)
    , m_startClipId()
    , m_endClipId()
    , m_blendFactor(0.0f)
{
}

LerpClipBlend::~LerpClipBlend()
{
}

void LerpClipBlend::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    if (e->type() == Qt3DCore::PropertyUpdated) {
        Qt3DCore::QPropertyUpdatedChangePtr change = qSharedPointerCast<Qt3DCore::QPropertyUpdatedChange>(e);
        if (change->propertyName() == QByteArrayLiteral("blendFactor"))
            m_blendFactor = change->value().toFloat();
        else if (change->propertyName() == QByteArrayLiteral("startClip"))
            m_startClipId = change->value().value<Qt3DCore::QNodeId>();
        else if (change->propertyName() == QByteArrayLiteral("endClip"))
            m_endClipId = change->value().value<Qt3DCore::QNodeId>();
    }
}

ClipResults LerpClipBlend::doBlend(const QVector<ClipResults> &blendData) const
{
    Q_ASSERT(blendData.size() == 2);
    Q_ASSERT(blendData[0].size() == blendData[1].size());
    const int elementCount = blendData.first().size();
    ClipResults blendResults(elementCount);

    for (int i = 0; i < elementCount; ++i)
        blendResults[i] = (1.0f - m_blendFactor) * blendData[0][i] + (m_blendFactor * blendData[1][i]);

    return blendResults;
}

void LerpClipBlend::initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change)
{
    ClipBlendNode::initializeFromPeer(change);
    const auto creationChangeData = qSharedPointerCast<Qt3DAnimation::QClipBlendNodeCreatedChange<Qt3DAnimation::QLerpClipBlendData>>(change);
    const Qt3DAnimation::QLerpClipBlendData cloneData = creationChangeData->data;
    m_startClipId = cloneData.startClipId;
    m_endClipId = cloneData.endClipId;
    m_blendFactor = cloneData.blendFactor;
}

double LerpClipBlend::duration() const
{
    ClipBlendNode *startNode = clipBlendNodeManager()->lookupNode(m_startClipId);
    const double startNodeDuration = startNode ? startNode->duration() : 0.0;

    ClipBlendNode *endNode = clipBlendNodeManager()->lookupNode(m_endClipId);
    const double endNodeDuration = endNode ? endNode->duration() : 0.0;

    return (1.0f - m_blendFactor) * startNodeDuration + m_blendFactor * endNodeDuration;
}

} // Animation

} // Qt3DAnimation

QT_END_NAMESPACE
