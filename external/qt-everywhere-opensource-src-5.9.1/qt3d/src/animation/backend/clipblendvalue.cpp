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

#include "clipblendvalue_p.h"
#include <Qt3DAnimation/qclipblendnodecreatedchange.h>
#include <Qt3DAnimation/qclipblendvalue.h>
#include <Qt3DAnimation/private/qclipblendvalue_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DAnimation {
namespace Animation {

ClipBlendValue::ClipBlendValue()
    : ClipBlendNode(ValueType)
{
}

ClipBlendValue::~ClipBlendValue()
{
}

void ClipBlendValue::initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change)
{
    ClipBlendNode::initializeFromPeer(change);
    const auto creationChange
            = qSharedPointerCast<QClipBlendNodeCreatedChange<QClipBlendValueData>>(change);
    const Qt3DAnimation::QClipBlendValueData data = creationChange->data;
    m_clipId = data.clipId;
}

void ClipBlendValue::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    if (e->type() == Qt3DCore::PropertyUpdated) {
        Qt3DCore::QPropertyUpdatedChangePtr change = qSharedPointerCast<Qt3DCore::QPropertyUpdatedChange>(e);
        if (change->propertyName() == QByteArrayLiteral("clip"))
            m_clipId = change->value().value<Qt3DCore::QNodeId>();
    }
}

ClipResults ClipBlendValue::doBlend(const QVector<ClipResults> &blendData) const
{
    // Should never be called for the value node
    Q_UNUSED(blendData);
    Q_UNREACHABLE();
    return ClipResults();
}

double ClipBlendValue::duration() const
{
    if (m_clipId.isNull())
        return 0.0;
    AnimationClip *clip = m_handler->animationClipLoaderManager()->lookupResource(m_clipId);
    Q_ASSERT(clip);
    return clip->duration();
}

void ClipBlendValue::setFormatIndices(Qt3DCore::QNodeId animatorId, const ComponentIndices &formatIndices)
{
    // Do we already have an entry for this animator?
    const int animatorIndex = m_animatorIds.indexOf(animatorId);
    if (animatorIndex == -1) {
        // Nope, add it
        m_animatorIds.push_back(animatorId);
        m_formatIndicies.push_back(formatIndices);
    } else {
        m_formatIndicies[animatorIndex] = formatIndices;
    }
}

ComponentIndices ClipBlendValue::formatIndices(Qt3DCore::QNodeId animatorId)
{
    const int animatorIndex = m_animatorIds.indexOf(animatorId);
    if (animatorIndex != -1)
        return m_formatIndicies[animatorIndex];
    return ComponentIndices();
}

} // namespace Animation
} // namespace Qt3DAnimation

QT_END_NAMESPACE
