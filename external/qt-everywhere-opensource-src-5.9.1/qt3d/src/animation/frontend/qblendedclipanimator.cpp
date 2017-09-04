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
#include "qblendedclipanimator.h"
#include "qblendedclipanimator_p.h"
#include <Qt3DAnimation/qabstractclipblendnode.h>
#include <Qt3DAnimation/qchannelmapper.h>

QT_BEGIN_NAMESPACE

namespace Qt3DAnimation {

QBlendedClipAnimatorPrivate::QBlendedClipAnimatorPrivate()
    : Qt3DAnimation::QAbstractClipAnimatorPrivate()
    , m_blendTreeRoot(nullptr)
{
}

/*!
    \qmltype BlendedClipAnimator
    \instantiates Qt3DAnimation::QBlendedClipAnimator
    \inqmlmodule Qt3D.Animation
    \since 5.9

    \brief BlendedClipAnimator is a component providing animation playback capabilities of a tree
    of blend nodes.

    An instance of BlendedClipAnimator can be aggregated by an Entity to add the ability to play
    back animation clips and to apply the calculated animation values to properties of QObjects.

    Whereas a ClipAnimator gets its animation data from a single animation clip,
    BlendedClipAnimator can blend together multiple clips. The animation data is obtained by
    evaluating a so called \e {blend tree}. A blend tree is a hierarchical tree structure where the
    leaf nodes are value nodes that encapsulate an animation clip (AbstractAnimationClip); and the
    internal nodes represent blending operations that operate on the nodes pointed to by their
    operand properties.

    To associate a blend tree with a BlendedClipAnimator, set the animator's blendTree property to
    point at the root node of your blend tree:

    \badcode
    BlendedClipAnimator {
        blendTree: AdditiveClipBlend {
            ....
        }
    }
    \endcode

    A blend tree can be constructed from the following node types:

    \note The blend node tree should only be edited when the animator is not running.

    \list
    \li Qt3D.Animation.ClipBlendValue
    \li Qt3D.Animation.LerpClipBlend
    \li Qt3D.Animation.AdditiveClipBlend
    \endlist

    Additional node types will be added over time.

    As an example consider the following blend tree:

    \badcode
        Clip0----
                |
                Lerp Node----
                |           |
        Clip1----           Additive Node
                            |
                    Clip2----
    \endcode

    This can be created and used as follows:

    \badcode
    BlendedClipAnimator {
        blendTree: AdditiveClipBlend {
            baseClip: LerpClipBlend {
                startClip: ClipBlendValue {
                    clip: AnimationClipLoader { source: "walk.json" }
                }

                endClip: ClipBlendValue {
                    clip: AnimationClipLoader { source: "run.json" }
                }
            }

            additiveClip: ClipBlendValue {
                clip: AnimationClipLoader { source: "wave-arm.json" }
            }
        }

        channelMapper: ChannelMapper {...}
        running: true
    }
    \endcode

    By authoring a set of animation clips and blending between them dynamically at runtime with a
    blend tree, we open up a huge set of possible resulting animations. As some simple examples of
    the above blend tree, where alpha is the additive factor and beta is the lerp blend factor we
    can get a 2D continuum of possible animations:

    \badcode
    (alpha = 0, beta = 1) Running, No arm waving --- (alpha = 1, beta = 1) Running, Arm waving
            |                                               |
            |                                               |
            |                                               |
    (alpha = 0, beta = 0) Walking, No arm waving --- (alpha = 0, beta = 1) Running, No arm waving
    \endcode

    More complex blend trees offer even more flexibility for combining your animation clips. Note
    that the values used to control the blend tree (alpha and beta above) are simple properties on
    the blend nodes. This means, that these properties themselves can also be controlled by
    the animation framework.
*/

/*!
    \class Qt3DAnimation::QBlendedClipAnimator
    \inherits Qt3DAnimation::QAbstractClipAnimator

    \inmodule Qt3DAnimation
    \since 5.9

    \brief QBlendedClipAnimator is a component providing animation playback capabilities of a tree
    of blend nodes.

    An instance of QBlendedClipAnimator can be aggregated by a QEntity to add the ability to play
    back animation clips and to apply the calculated animation values to properties of QObjects.

    Whereas a QClipAnimator gets its animation data from a single animation clip,
    QBlendedClipAnimator can blend together multiple clips. The animation data is obtained by
    evaluating a so called \e {blend tree}. A blend tree is a hierarchical tree structure where the
    leaf nodes are value nodes that encapsulate an animation clip (QAbstractAnimationClip); and the
    internal nodes represent blending operations that operate on the nodes pointed to by their
    operand properties.

    To associate a blend tree with a QBlendedClipAnimator, set the animator's blendTree property to
    point at the root node of your blend tree:

    \badcode
    auto blendTreeRoot = new QAdditiveClipBlend();
    ...
    auto animator = new QBlendedClipAnimator();
    animator->setBlendTree(blendTreeRoot);
    \endcode

    A blend tree can be constructed from the following node types:

    \note The blend node tree should only be edited when the animator is not running.

    \list
    \li Qt3DAnimation::QClipBlendValue
    \li Qt3DAnimation::QLerpClipBlend
    \li Qt3DAnimation::QAdditiveClipBlend
    \endlist

    Additional node types will be added over time.

    As an example consider the following blend tree:

    \badcode
        Clip0----
                |
                Lerp Node----
                |           |
        Clip1----           Additive Node
                            |
                    Clip2----
    \endcode

    This can be created and used as follows:

    \code
    // Create leaf nodes of blend tree
    auto clip0 = new QClipBlendValue(
        new QAnimationClipLoader(QUrl::fromLocalFile("walk.json")));
    auto clip1 = new QClipBlendValue(
        new QAnimationClipLoader(QUrl::fromLocalFile("run.json")));
    auto clip2 = new QClipBlendValue(
        new QAnimationClipLoader(QUrl::fromLocalFile("wave-arm.json")));

    // Create blend tree inner nodes
    auto lerpNode = new QLerpClipBlend();
    lerpNode->setStartClip(clip0);
    lerpNode->setEndClip(clip1);
    lerpNode->setBlendFactor(0.5f); // Half-walk, half-run

    auto additiveNode = new QAdditiveClipBlend();
    additiveNode->setBaseClip(lerpNode); // Comes from lerp sub-tree
    additiveNode->setAdditiveClip(clip2);
    additiveNode->setAdditiveFactor(1.0f); // Wave arm fully

    // Run the animator
    auto animator = new QBlendedClipAnimator();
    animator->setBlendTree(additiveNode);
    animator->setChannelMapper(...);
    animator->setRunning(true);
    \endcode

    By authoring a set of animation clips and blending between them dynamically at runtime with a
    blend tree, we open up a huge set of possible resulting animations. As some simple examples of
    the above blend tree, where alpha is the additive factor and beta is the lerp blend factor we
    can get a 2D continuum of possible animations:

    \badcode
    (alpha = 0, beta = 1) Running, No arm waving --- (alpha = 1, beta = 1) Running, Arm waving
            |                                               |
            |                                               |
            |                                               |
    (alpha = 0, beta = 0) Walking, No arm waving --- (alpha = 0, beta = 1) Running, No arm waving
    \endcode

    More complex blend trees offer even more flexibility for combining your animation clips. Note
    that the values used to control the blend tree (alpha and beta above) are simple properties on
    the blend nodes. This means, that these properties themselves can also be controlled by
    the animation framework.

*/
QBlendedClipAnimator::QBlendedClipAnimator(Qt3DCore::QNode *parent)
    : Qt3DAnimation::QAbstractClipAnimator(*new QBlendedClipAnimatorPrivate, parent)
{
}

/*! \internal */
QBlendedClipAnimator::QBlendedClipAnimator(QBlendedClipAnimatorPrivate &dd, Qt3DCore::QNode *parent)
    : Qt3DAnimation::QAbstractClipAnimator(dd, parent)
{
}

QBlendedClipAnimator::~QBlendedClipAnimator()
{
}

/*!
    \qmlproperty AbstractClipBlendNode blendTree

    This property holds the root of the animation blend tree that will be evaluated before being
    interpolated by the animator.
*/

/*!
    \property blendTree

    This property holds the root of the animation blend tree that will be evaluated before being
    interpolated by the animator.
*/
QAbstractClipBlendNode *QBlendedClipAnimator::blendTree() const
{
    Q_D(const QBlendedClipAnimator);
    return d->m_blendTreeRoot;
}

void QBlendedClipAnimator::setBlendTree(QAbstractClipBlendNode *blendTree)
{
    Q_D(QBlendedClipAnimator);
    if (d->m_blendTreeRoot == blendTree)
        return;

    if (d->m_blendTreeRoot)
        d->unregisterDestructionHelper(d->m_blendTreeRoot);

    if (blendTree != nullptr && blendTree->parent() == nullptr)
        blendTree->setParent(this);

    d->m_blendTreeRoot = blendTree;

    if (d->m_blendTreeRoot)
        d->registerDestructionHelper(d->m_blendTreeRoot, &QBlendedClipAnimator::setBlendTree, d->m_blendTreeRoot);

    emit blendTreeChanged(blendTree);
}

/*! \internal */
Qt3DCore::QNodeCreatedChangeBasePtr QBlendedClipAnimator::createNodeCreationChange() const
{
    auto creationChange = Qt3DCore::QNodeCreatedChangePtr<QBlendedClipAnimatorData>::create(this);
    QBlendedClipAnimatorData &data = creationChange->data;
    Q_D(const QBlendedClipAnimator);
    data.blendTreeRootId = Qt3DCore::qIdForNode(d->m_blendTreeRoot);
    data.mapperId = Qt3DCore::qIdForNode(d->m_mapper);
    data.running = d->m_running;
    data.loops = d->m_loops;
    return creationChange;
}

} // namespace Qt3DAnimation

QT_END_NAMESPACE
