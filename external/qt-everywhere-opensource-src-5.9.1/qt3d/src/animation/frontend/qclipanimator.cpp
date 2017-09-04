/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
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

#include "qclipanimator.h"
#include "qclipanimator_p.h"
#include <Qt3DAnimation/qabstractanimationclip.h>
#include <Qt3DAnimation/qchannelmapper.h>

QT_BEGIN_NAMESPACE

namespace Qt3DAnimation {

QClipAnimatorPrivate::QClipAnimatorPrivate()
    : Qt3DAnimation::QAbstractClipAnimatorPrivate()
    , m_clip(nullptr)
{
}

/*!
    \qmltype ClipAnimator
    \instantiates Qt3DAnimation::QClipAnimator
    \inqmlmodule Qt3D.Animation
    \since 5.9

    \brief ClipAnimator is a component providing simple animation playback capabilities.

    An instance of ClipAnimator can be aggregated by an Entity to add the ability to play back
    animation clips and to apply the calculated animation values to properties of QObjects.

    The animation key frame data is provided via the clip property. This can be created
    programmatically with AnimationClip or loaded from file with AnimationClipLoader.

    In order to apply the values played back from the channels of data in the animation clip, the
    clip animator needs to have a ChannelMapper object assigned to the channelMapper property.

    The properties for controlling the animator are provided by the AbstractClipAnimator base
    class.

    \sa AbstractClipAnimator, AbstractAnimationClip, ChannelMapper, BlendedClipAnimator
*/

/*!
    \class Qt3DAnimation::QClipAnimator
    \inherits Qt3DAnimation::QAbstractClipAnimator

    \inmodule Qt3DAnimation
    \since 5.9

    \brief QClipAnimator is a component providing simple animation playback capabilities.

    An instance of QClipAnimator can be aggregated by a QEntity to add the ability to play back
    animation clips and to apply the calculated animation values to properties of QObjects.

    The animation key frame data is provided via the clip property. This can be created
    programmatically with QAnimationClip or loaded from file with QAnimationClipLoader.

    In order to apply the values played back from the channels of data in the animation clip, the
    clip animator needs to have a QChannelMapper object assigned to the channelMapper property.

    The properties for controlling the animator are provided by the QAbstractClipAnimator base
    class.

    \sa QAbstractClipAnimator, QAbstractAnimationClip, QChannelMapper, QBlendedClipAnimator
*/

QClipAnimator::QClipAnimator(Qt3DCore::QNode *parent)
    : Qt3DAnimation::QAbstractClipAnimator(*new QClipAnimatorPrivate, parent)
{
}

/*! \internal */
QClipAnimator::QClipAnimator(QClipAnimatorPrivate &dd, Qt3DCore::QNode *parent)
    : Qt3DAnimation::QAbstractClipAnimator(dd, parent)
{
}

QClipAnimator::~QClipAnimator()
{
}

/*!
    \qmlproperty AbstractAnimationClip clip

    This property holds the animation clip which contains the key frame data to be played back.
    The key frame data can be specified in either an AnimationClip or AnimationClipLoader.
*/

/*!
    \property clip

    This property holds the animation clip which contains the key frame data to be played back.
    The key frame data can be specified in either a QAnimationClip or QAnimationClipLoader.
*/
QAbstractAnimationClip *QClipAnimator::clip() const
{
    Q_D(const QClipAnimator);
    return d->m_clip;
}

void QClipAnimator::setClip(QAbstractAnimationClip *clip)
{
    Q_D(QClipAnimator);
    if (d->m_clip == clip)
        return;

    if (d->m_clip)
        d->unregisterDestructionHelper(d->m_clip);

    if (clip && !clip->parent())
        clip->setParent(this);
    d->m_clip = clip;

    // Ensures proper bookkeeping
    if (d->m_clip)
        d->registerDestructionHelper(d->m_clip, &QClipAnimator::setClip, d->m_clip);
    emit clipChanged(clip);
}

/*! \internal */
Qt3DCore::QNodeCreatedChangeBasePtr QClipAnimator::createNodeCreationChange() const
{
    auto creationChange = Qt3DCore::QNodeCreatedChangePtr<QClipAnimatorData>::create(this);
    auto &data = creationChange->data;
    Q_D(const QClipAnimator);
    data.clipId = Qt3DCore::qIdForNode(d->m_clip);
    data.mapperId = Qt3DCore::qIdForNode(d->m_mapper);
    data.running = d->m_running;
    data.loops = d->m_loops;
    return creationChange;
}

} // namespace Qt3DAnimation

QT_END_NAMESPACE
