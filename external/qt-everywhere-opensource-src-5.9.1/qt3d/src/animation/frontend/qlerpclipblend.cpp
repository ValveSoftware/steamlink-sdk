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

#include "qlerpclipblend.h"
#include "qlerpclipblend_p.h"
#include <Qt3DAnimation/qclipblendnodecreatedchange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DAnimation {

/*!
    \qmltype LerpBlend
    \instantiates Qt3DAnimation::QLerpClipBlend
    \inqmlmodule Qt3D.Animation

    \brief Performs a linear interpolation of two animation clips based on a
    normalized factor

    \since 5.9

    LerpBlend can be useful to create advanced animation effects based on
    individual animation clips. For instance, given a player character,, lerp
    blending could be used to combine a walking animation clip with an injured
    animation clip based on a blend factor that increases the more the player
    gets injured. This would then allow with blend factor == 0 to have a non
    injured walking player, with blend factor == 1 a fully injured player, with
    blend factor == 0.5 a partially walking and injured player.

    \sa BlendedClipAnimator
*/

/*!
    \class Qt3DAnimation::QLerpClipBlend
    \inmodule Qt3DAnimation
    \inherits Qt3DAnimation::QAbstractClipBlendNode

    \brief Performs a linear interpolation of two animation clips based on a
    normalized factor

    \since 5.9

    QLerpClipBlend can be useful to create advanced animation effects based on
    individual animation clips. For instance, given a player character,, lerp
    blending could be used to combine a walking animation clip with an injured
    animation clip based on a blend factor that increases the more the player
    gets injured. This would then allow with blend factor == 0 to have a non
    injured walking player, with blend factor == 1 a fully injured player, with
    blend factor == 0.5 a partially walking and injured player.

    \sa QBlendedClipAnimator
*/

QLerpClipBlendPrivate::QLerpClipBlendPrivate()
    : QAbstractClipBlendNodePrivate()
    , m_startClip(nullptr)
    , m_endClip(nullptr)
    , m_blendFactor(0.0f)
{
}

QLerpClipBlend::QLerpClipBlend(Qt3DCore::QNode *parent)
    : QAbstractClipBlendNode(*new QLerpClipBlendPrivate(), parent)
{
}

QLerpClipBlend::QLerpClipBlend(QLerpClipBlendPrivate &dd, Qt3DCore::QNode *parent)
    : QAbstractClipBlendNode(dd, parent)
{
}

QLerpClipBlend::~QLerpClipBlend()
{
}

Qt3DCore::QNodeCreatedChangeBasePtr QLerpClipBlend::createNodeCreationChange() const
{
    Q_D(const QLerpClipBlend);
    auto creationChange = QClipBlendNodeCreatedChangePtr<QLerpClipBlendData>::create(this);
    QLerpClipBlendData &data = creationChange->data;
    data.startClipId = Qt3DCore::qIdForNode(d->m_startClip);
    data.endClipId = Qt3DCore::qIdForNode(d->m_endClip);
    data.blendFactor = d->m_blendFactor;
    return creationChange;
}

/*!
    \qmlproperty real LerpBlend::blendFactor

    Specifies the blending factor between 0 and 1 to control the blending of
    two animation clips.
*/
/*!
    \property QLerpClipBlend::blendFactor

    Specifies the blending factor between 0 and 1 to control the blending of
    two animation clips.
 */
float QLerpClipBlend::blendFactor() const
{
    Q_D(const QLerpClipBlend);
    return d->m_blendFactor;
}

/*!
    \qmlproperty AbstractClipBlendNode LerpClipBlend::startClip

    Holds the sub-tree that should be used as the start clip for this
    lerp blend node. That is, the clip returned by this blend node when
    the blendFactor is set to a value of 0.
*/
/*!
    \property QLerpClipBlend::startClip

    Holds the sub-tree that should be used as the start clip for this
    lerp blend node. That is, the clip returned by this blend node when
    the blendFactor is set to a value of 0.
*/
Qt3DAnimation::QAbstractClipBlendNode *QLerpClipBlend::startClip() const
{
    Q_D(const QLerpClipBlend);
    return d->m_startClip;
}

/*!
    \qmlproperty AbstractClipBlendNode LerpClipBlend::endClip

    Holds the sub-tree that should be used as the end clip for this
    lerp blend node. That is, the clip returned by this blend node when
    the blendFactor is set to a value of 1.
*/
/*!
    \property QLerpClipBlend::endClip

    Holds the sub-tree that should be used as the start clip for this
    lerp blend node. That is, the clip returned by this blend node when
    the blendFactor is set to a value of 1.
*/
Qt3DAnimation::QAbstractClipBlendNode *QLerpClipBlend::endClip() const
{
    Q_D(const QLerpClipBlend);
    return d->m_endClip;
}

void QLerpClipBlend::setBlendFactor(float blendFactor)
{
    Q_D(QLerpClipBlend);

    if (d->m_blendFactor == blendFactor)
        return;

    d->m_blendFactor = blendFactor;
    emit blendFactorChanged(blendFactor);
}

void QLerpClipBlend::setStartClip(Qt3DAnimation::QAbstractClipBlendNode *startClip)
{
    Q_D(QLerpClipBlend);
    if (d->m_startClip == startClip)
        return;

    if (d->m_startClip)
        d->unregisterDestructionHelper(d->m_startClip);

    if (startClip && !startClip->parent())
        startClip->setParent(this);
    d->m_startClip = startClip;

    // Ensures proper bookkeeping
    if (d->m_startClip)
        d->registerDestructionHelper(d->m_startClip, &QLerpClipBlend::setStartClip, d->m_startClip);
    emit startClipChanged(startClip);
}

void QLerpClipBlend::setEndClip(Qt3DAnimation::QAbstractClipBlendNode *endClip)
{
    Q_D(QLerpClipBlend);
    if (d->m_endClip == endClip)
        return;

    if (d->m_endClip)
        d->unregisterDestructionHelper(d->m_endClip);

    if (endClip && !endClip->parent())
        endClip->setParent(this);
    d->m_endClip = endClip;

    // Ensures proper bookkeeping
    if (d->m_endClip)
        d->registerDestructionHelper(d->m_endClip, &QLerpClipBlend::setEndClip, d->m_endClip);
    emit endClipChanged(endClip);
}

/*!
    \qmlproperty list<AnimationClip> LerpBlend::clips

    Holds the list of AnimationClip nodes against which the blending is performed.

    \note Only the two first AnimationClip are used, subsequent ones are ignored
*/


/*!
    \fn void QLerpClipBlend::addClip(QAbstractAnimationClip *clip);
    Adds a \a clip to the blending node's clips list.

    \note Only the two first AnimationClip are used, subsequent ones are ignored
 */

/*!
    \fn void QLerpClipBlend::removeClip(QAbstractAnimationClip *clip);
    Removes a \a clip from the blending node's clips list.
 */

/*!
    \fn QVector<QAbstractAnimationClip *> QLerpClipBlend::clips() const;
    Returns the list of QAnimationClip against which the blending is performed.
 */

} // Qt3DAnimation

QT_END_NAMESPACE
