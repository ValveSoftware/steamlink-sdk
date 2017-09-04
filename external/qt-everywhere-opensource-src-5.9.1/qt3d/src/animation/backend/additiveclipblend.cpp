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

#include "additiveclipblend_p.h"
#include <Qt3DAnimation/qclipblendnodecreatedchange.h>
#include <Qt3DAnimation/qadditiveclipblend.h>
#include <Qt3DAnimation/private/qadditiveclipblend_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DAnimation {

namespace Animation {

AdditiveClipBlend::AdditiveClipBlend()
    : ClipBlendNode(ClipBlendNode::AdditiveBlendType)
    , m_baseClipId()
    , m_additiveClipId()
    , m_additiveFactor(0.0f)
{
}

AdditiveClipBlend::~AdditiveClipBlend()
{
}

void AdditiveClipBlend::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    if (e->type() == Qt3DCore::PropertyUpdated) {
        Qt3DCore::QPropertyUpdatedChangePtr change = qSharedPointerCast<Qt3DCore::QPropertyUpdatedChange>(e);
        if (change->propertyName() == QByteArrayLiteral("additiveFactor"))
            m_additiveFactor = change->value().toFloat();
        else if (change->propertyName() == QByteArrayLiteral("baseClip"))
            m_baseClipId = change->value().value<Qt3DCore::QNodeId>();
        else if (change->propertyName() == QByteArrayLiteral("additiveClip"))
            m_additiveClipId = change->value().value<Qt3DCore::QNodeId>();
    }
}

ClipResults AdditiveClipBlend::doBlend(const QVector<ClipResults> &blendData) const
{
    Q_ASSERT(blendData.size() == 2);
    Q_ASSERT(blendData[0].size() == blendData[1].size());
    const int elementCount = blendData.first().size();
    ClipResults blendResults(elementCount);

    for (int i = 0; i < elementCount; ++i)
        blendResults[i] = blendData[0][i] + m_additiveFactor * blendData[1][i];

    return blendResults;
}

void AdditiveClipBlend::initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change)
{
    ClipBlendNode::initializeFromPeer(change);
    const auto creationChangeData = qSharedPointerCast<Qt3DAnimation::QClipBlendNodeCreatedChange<Qt3DAnimation::QAdditiveClipBlendData>>(change);
    const Qt3DAnimation::QAdditiveClipBlendData cloneData = creationChangeData->data;
    m_baseClipId = cloneData.baseClipId;
    m_additiveClipId = cloneData.additiveClipId;
    m_additiveFactor = cloneData.additiveFactor;
}

} // Animation

} // Qt3DAnimation

QT_END_NAMESPACE
